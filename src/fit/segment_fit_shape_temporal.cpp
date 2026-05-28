#include "bbsolver/fit/segment_fit_shape_temporal.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>

#include <ceres/ceres.h>
#include <cstddef>
#include <functional>
#include <vector>

#include "bbsolver/metrics/ae_curve.hpp"
#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/metrics/error_metrics.hpp"
#include "bbsolver/fit/segment_fit_bezier.hpp"
#include "bbsolver/fit/segment_fit_policy.hpp"
#include "bbsolver/fit/segment_fit_samples.hpp"

namespace bbsolver::segment_fit {
namespace {

struct ShapeFlatProgressResidual {
  double t;
  double t0;
  double t1;
  double target_progress;
  double weight;

  template <typename T>
  bool operator()(const T* const influence, T* residual) const {
    const T q = SolveTemporalParamT(T(t), t0, influence[0], t1, influence[1]);
    const T out_value = T(0.0);
    const T in_value = T(1.0);
    const T value = CubicBezierT(q,
                                 T(0.0),
                                 out_value,
                                 in_value,
                                 T(1.0));
    residual[0] = T(weight) * (value - T(target_progress));
    return true;
  }
};

}  // namespace

double ShapeFlatProjectedProgress(const PropertySamples& ps,
                                  int i,
                                  int j,
                                  int sample_idx) {
  const std::vector<double> v0 = SampleVector(ps, i);
  const std::vector<double> v1 = SampleVector(ps, j);
  const std::vector<double> vk = SampleVector(ps, sample_idx);
  const int dims = Dimensions(ps);
  double numerator = 0.0;
  double denominator = 0.0;
  for (int d = 2; d < dims; ++d) {
    const int shape_channel = (d - 2) % 6;
    // The path outline is anchored by vertex positions. Tangents matter for
    // curvature, but weighting them equally can let noisy fitted handles drown
    // out the stable vertex-slot motion that should drive temporal progress.
    const double channel_weight = shape_channel < 2 ? 1.0: 0.25;
    const double delta = ComponentOrZero(v1, static_cast<std::size_t>(d)) -
                         ComponentOrZero(v0, static_cast<std::size_t>(d));
    numerator += channel_weight *
                 (ComponentOrZero(vk, static_cast<std::size_t>(d)) -
                  ComponentOrZero(v0, static_cast<std::size_t>(d))) *
                 delta;
    denominator += channel_weight * delta * delta;
  }
  if (!(denominator > 1e-18)) {
    return 0.0;
  }
  return numerator / denominator;
}

double EvalShapeFlatTemporalProgress(double t,
                                     double t0,
                                     TemporalEase out_ease,
                                     double t1,
                                     TemporalEase in_ease) {
  return EvalTemporalBezier(t, t0, 0.0, out_ease, t1, 1.0, in_ease);
}

std::vector<double> ReconstructShapeFlatTemporalBezier(
    const PropertySamples& ps,
    int i,
    int j,
    TemporalEase ease_out,
    TemporalEase ease_in,
    double t) {
  const std::vector<double> v0 = SampleVector(ps, i);
  const std::vector<double> v1 = SampleVector(ps, j);
  std::vector<double> out(static_cast<std::size_t>(Dimensions(ps)), 0.0);
  const double u = EvalShapeFlatTemporalProgress(
      t, SampleTime(ps, i), ease_out, SampleTime(ps, j), ease_in);
  for (int d = 0; d < Dimensions(ps); ++d) {
    const double a = ComponentOrZero(v0, static_cast<std::size_t>(d));
    const double b = ComponentOrZero(v1, static_cast<std::size_t>(d));
    out[static_cast<std::size_t>(d)] = a + (b - a) * u;
  }
  return out;
}

std::vector<double> ReconstructShapeFlatKeyBezier(const PropertySamples& ps,
                                                  int i,
                                                  int j,
                                                  TemporalEase ease_out,
                                                  TemporalEase ease_in,
                                                  double t) {
  std::vector<double> out(static_cast<std::size_t>(Dimensions(ps)), 0.0);
  for (int d = 0; d < Dimensions(ps); ++d) {
    out[static_cast<std::size_t>(d)] =
        EvalTemporalBezier(t,
                           SampleTime(ps, i),
                           SampleValue(ps, i, d),
                           ease_out,
                           SampleTime(ps, j),
                           SampleValue(ps, j, d),
                           ease_in);
  }
  return out;
}

ErrorReport ComputeShapeFlatOutlineError(
    const PropertySamples& ps,
    int i,
    int j,
    const std::function<std::vector<double>(double t)>& reconstruct,
    const SolverConfig& cfg,
    int* evaluations,
    double* outline_wall_ms) {
  ErrorReport report;
  if (ps.samples.empty() || i < 0 || j < i ||
      j >= static_cast<int>(ps.samples.size())) {
    report.max_err = std::numeric_limits<double>::infinity();
    report.max_err_screen_px = report.max_err;
    return report;
  }

  PathFrameFitOptions options;
  options.outline_tolerance = std::max(cfg.tolerance, 0.0);

  double sum_sq = 0.0;
  int count = 0;
  for (int sample_idx = i; sample_idx <= j; ++sample_idx) {
    const Sample& sample = ps.samples[static_cast<std::size_t>(sample_idx)];
    const std::vector<double> actual = reconstruct(sample.t_sec);
    const auto outline_start = std::chrono::steady_clock::now();
    const double err = ShapeFlatFrameOutlineError(sample.v, actual, options);
    const auto outline_end = std::chrono::steady_clock::now();
    if (outline_wall_ms != nullptr) {
      *outline_wall_ms += ElapsedMs(outline_start, outline_end);
    }
    if (evaluations != nullptr) {
      ++(*evaluations);
    }
    if (err > report.max_err || !std::isfinite(err)) {
      report.max_err = err;
      report.max_err_screen_px = err;
      report.worst_sample_idx = sample_idx;
    }
    sum_sq += err * err;
    ++count;
  }
  report.rms_err =
      count > 0 ? std::sqrt(sum_sq / static_cast<double>(count)): 0.0;
  if (report.max_err_screen_px == 0.0) {
    report.max_err_screen_px = report.max_err;
  }
  return report;
}

SegmentFitResult TryShapeFlatTemporalBezier(int i,
                                            int j,
                                            const PropertySamples& ps,
                                            const SolverConfig& cfg,
                                            const CompInfo& comp) {
  const auto total_start = std::chrono::steady_clock::now();
  SegmentFitResult result;
  result.fit_shape_temporal_attempts = 1;
  result.interp = InterpType::Bezier;
  result.reason = "infeasible_shape_temporal_bezier";
  result.ease_out_at_i = {{0.0, ClampInfluence(kDefaultInfluence, cfg)}};
  result.ease_in_at_j = {{0.0, ClampInfluence(kDefaultInfluence, cfg)}};

  const double t0 = SampleTime(ps, i);
  const double t1 = SampleTime(ps, j);
  const double dt = t1 - t0;
  const double default_influence = ClampInfluence(kDefaultInfluence, cfg);
  result.ease_out_at_i = {{0.0, default_influence}};
  result.ease_in_at_j = {{0.0, default_influence}};
  if (!(dt > 0.0) || j <= i + 1) {
    const ErrorReport report = ComputeShapeFlatOutlineError(
        ps,
        i,
        j,
        [&](double t) {
          return ReconstructShapeFlatKeyBezier(ps,
                                               i,
                                               j,
                                               result.ease_out_at_i.front(),
                                               result.ease_in_at_j.front(),
                                               t);
        },
        cfg,
        &result.fit_shape_temporal_outline_evaluations,
        &result.fit_shape_temporal_outline_wall_ms);
    CopyError(result, report);
    result.feasible = Passes(report, cfg);
    result.reason = result.feasible ? "shape_temporal_bezier_ok"
: result.reason;
    result.fit_shape_temporal_total_wall_ms =
        ElapsedMs(total_start, std::chrono::steady_clock::now());
    return result;
  }

  std::array<double, 2> influence = {
      default_influence,
      default_influence,
  };

  ceres::Problem problem;
  problem.AddParameterBlock(influence.data(), 2);
  const double min_influence = std::max(0.1, cfg.min_influence);
  const double max_influence =
      std::min(100.0, std::max(min_influence, cfg.max_influence));
  problem.SetParameterLowerBound(influence.data(), 0, min_influence);
  problem.SetParameterUpperBound(influence.data(), 0, max_influence);
  problem.SetParameterLowerBound(influence.data(), 1, min_influence);
  problem.SetParameterUpperBound(influence.data(), 1, max_influence);

  for (int sample_idx = i + 1; sample_idx < j; ++sample_idx) {
    auto* cost =
        new ceres::AutoDiffCostFunction<ShapeFlatProgressResidual, 1, 2>(
            new ShapeFlatProgressResidual{SampleTime(ps, sample_idx),
                                          t0,
                                          t1,
                                          ShapeFlatProjectedProgress(
                                              ps, i, j, sample_idx),
                                          1.0});
    problem.AddResidualBlock(cost, nullptr, influence.data());
  }

  ceres::Solver::Options options = CeresOptions(cfg);
  options.max_num_iterations = std::max(cfg.max_iters_per_segment, 80);
  ceres::Solver::Summary summary;
  const auto ceres_start = std::chrono::steady_clock::now();
  ceres::Solve(options, &problem, &summary);
  const auto ceres_end = std::chrono::steady_clock::now();
  result.fit_shape_temporal_ceres_wall_ms +=
      ElapsedMs(ceres_start, ceres_end);
  result.iters = static_cast<int>(summary.iterations.size());
  result.ease_out_at_i = {{0.0, ClampInfluence(influence[0], cfg)}};
  result.ease_in_at_j = {{0.0, ClampInfluence(influence[1], cfg)}};

  const ErrorReport report = ComputeShapeFlatOutlineError(
      ps,
      i,
      j,
      [&](double t) {
        return ReconstructShapeFlatKeyBezier(ps,
                                             i,
                                             j,
                                             result.ease_out_at_i.front(),
                                             result.ease_in_at_j.front(),
                                             t);
      },
      cfg,
      &result.fit_shape_temporal_outline_evaluations,
      &result.fit_shape_temporal_outline_wall_ms);
  CopyError(result, report);
  result.feasible = Passes(report, cfg);
  result.reason =
      result.feasible ? "shape_temporal_bezier_ok"
: "infeasible_shape_temporal_bezier";
  result.fit_shape_temporal_total_wall_ms =
      ElapsedMs(total_start, std::chrono::steady_clock::now());
  return result;
}

}  // namespace bbsolver::segment_fit
