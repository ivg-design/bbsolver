#include "bbsolver/fit/segment_fit_unified_spatial.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include <ceres/ceres.h>
#include <cstddef>
#include <utility>
#include <vector>

#include "bbsolver/metrics/ae_curve.hpp"
#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/metrics/error_metrics.hpp"
#include "bbsolver/fit/segment_fit_bezier.hpp"
#include "bbsolver/fit/segment_fit_policy.hpp"
#include "bbsolver/fit/segment_fit_samples.hpp"
#include "bbsolver/metrics/unified_spatial.hpp"

namespace bbsolver::segment_fit {
namespace {

struct DistanceTimingResidual {
  double t;
  double t0;
  double t1;
  double path_length;
  double target_distance;
  double weight;

  template <typename T>
  bool operator()(const T* const temporal, T* residual) const {
    const T q = SolveTemporalParamT(T(t), t0, temporal[1], t1, temporal[3]);
    const T out_distance = temporal[0];
    const T in_distance = T(path_length) - temporal[2];
    const T value = CubicBezierT(q,
                                 T(0.0),
                                 out_distance,
                                 in_distance,
                                 T(path_length));
    residual[0] = T(weight) * (value - T(target_distance));
    return true;
  }
};

}  // namespace

std::vector<double> EvalPathPoint(double u,
                                  const std::vector<double>& v0,
                                  const std::vector<double>& spatial_out,
                                  const std::vector<double>& v1,
                                  const std::vector<double>& spatial_in) {
  std::vector<double> value(v0.size(), 0.0);
  const double cu = std::clamp(u, 0.0, 1.0);
  for (std::size_t d = 0; d < value.size(); ++d) {
    value[d] = EvalSpatialBezierU(cu,
                                  ComponentOrZero(v0, d),
                                  ComponentOrZero(spatial_out, d),
                                  ComponentOrZero(v1, d),
                                  ComponentOrZero(spatial_in, d));
  }
  return value;
}

double VectorDistanceSquared(const std::vector<double>& a,
                             const std::vector<double>& b) {
  const std::size_t n = std::max(a.size(), b.size());
  double sum = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    const double delta = ComponentOrZero(a, i) - ComponentOrZero(b, i);
    sum += delta * delta;
  }
  return sum;
}

PathProjectionLut BuildPathProjectionLut(
    const std::vector<double>& v0,
    const std::vector<double>& spatial_out,
    const std::vector<double>& v1,
    const std::vector<double>& spatial_in) {
  constexpr int kProjectionSteps = 512;
  PathProjectionLut lut;
  lut.points.reserve(static_cast<std::size_t>(kProjectionSteps + 1));
  lut.length.assign(static_cast<std::size_t>(kProjectionSteps + 1), 0.0);
  lut.points.push_back(EvalPathPoint(0.0, v0, spatial_out, v1, spatial_in));
  double total = 0.0;
  for (int step = 1; step <= kProjectionSteps; ++step) {
    const double u =
        static_cast<double>(step) / static_cast<double>(kProjectionSteps);
    lut.points.push_back(EvalPathPoint(u, v0, spatial_out, v1, spatial_in));
    total += std::sqrt(VectorDistanceSquared(
        lut.points[static_cast<std::size_t>(step - 1)],
        lut.points[static_cast<std::size_t>(step)]));
    lut.length[static_cast<std::size_t>(step)] = total;
  }
  return lut;
}

std::vector<std::pair<double, double>> ProjectSegmentSamplesToPathDistances(
    const PropertySamples& ps,
    int i,
    int j,
    const std::vector<double>& spatial_out,
    const std::vector<double>& spatial_in) {
  const std::vector<double> v0 = SampleVector(ps, i);
  const std::vector<double> v1 = SampleVector(ps, j);
  const PathProjectionLut lut =
      BuildPathProjectionLut(v0, spatial_out, v1, spatial_in);
  std::vector<std::pair<double, double>> targets;
  targets.reserve(static_cast<std::size_t>(std::max(0, j - i + 1)));
  if (lut.length.empty()) {
    return targets;
  }

  std::size_t min_step = 0;
  for (int sample_idx = i; sample_idx <= j; ++sample_idx) {
    double target_distance = 0.0;
    if (sample_idx == i) {
      target_distance = 0.0;
    } else if (sample_idx == j) {
      target_distance = lut.length.back();
      min_step = lut.length.size() - 1;
    } else {
      const std::vector<double> sample = SampleVector(ps, sample_idx);
      std::size_t best_step = min_step;
      double best_err = std::numeric_limits<double>::infinity();
      for (std::size_t step = min_step; step < lut.points.size(); ++step) {
        const double err = VectorDistanceSquared(sample, lut.points[step]);
        if (err < best_err) {
          best_err = err;
          best_step = step;
        }
      }
      min_step = best_step;
      target_distance = lut.length[best_step];
    }
    targets.push_back({SampleTime(ps, sample_idx), target_distance});
  }
  return targets;
}

double EaseHandleDistance(TemporalEase ease, double dt, double path_length) {
  const double influence = std::clamp(ease.influence, 0.1, 100.0) / 100.0;
  return std::clamp(std::max(0.0, ease.speed) * influence * dt,
                    0.0,
                    path_length);
}

double SpeedFromHandleDistance(double handle_distance,
                               double influence,
                               double dt) {
  const double handle_time =
      std::clamp(influence, 0.1, 100.0) / 100.0 * dt;
  if (!(handle_time > 1e-12)) {
    return 0.0;
  }
  return std::max(0.0, handle_distance) / handle_time;
}

SegmentFitResult FitUnifiedSpatialTiming(int i,
                                         int j,
                                         const PropertySamples& ps,
                                         const SolverConfig& cfg,
                                         const CompInfo& comp,
                                         SegmentFitResult result) {
  if (!IsUnifiedSpatial(ps)) {
    return result;
  }

  const std::vector<double> v0 = SampleVector(ps, i);
  const std::vector<double> v1 = SampleVector(ps, j);
  const double t0 = SampleTime(ps, i);
  const double t1 = SampleTime(ps, j);
  const double dt = t1 - t0;
  const double path_length = ApproxUnifiedSpatialPathLength(
      v0, result.spatial_out_at_i, v1, result.spatial_in_at_j);
  if (!(dt > 0.0) || !(path_length > 0.0)) {
    return result;
  }

  const std::vector<std::pair<double, double>> targets =
      ProjectSegmentSamplesToPathDistances(ps,
                                           i,
                                           j,
                                           result.spatial_out_at_i,
                                           result.spatial_in_at_j);
  if (targets.size() <= 2) {
    result.ease_out_at_i = {{0.0, ClampInfluence(kDefaultInfluence, cfg)}};
    result.ease_in_at_j = {{0.0, ClampInfluence(kDefaultInfluence, cfg)}};
    return result;
  }

  const TemporalEase seed_out = EaseForDim(result.ease_out_at_i, 0);
  const TemporalEase seed_in = EaseForDim(result.ease_in_at_j, 0);
  double out_handle = EaseHandleDistance(seed_out, dt, path_length);
  double in_handle = EaseHandleDistance(seed_in, dt, path_length);
  if (!(out_handle > 0.0) && targets.size() > 1) {
    const double sample_dt = targets[1].first - targets[0].first;
    if (sample_dt > 0.0) {
      const double speed =
          std::max(0.0, (targets[1].second - targets[0].second) / sample_dt);
      out_handle = std::clamp(
          speed * ClampInfluence(seed_out.influence, cfg) / 100.0 * dt,
          0.0,
          path_length);
    }
  }
  if (!(in_handle > 0.0) && targets.size() > 1) {
    const auto& a = targets[targets.size() - 2];
    const auto& b = targets[targets.size() - 1];
    const double sample_dt = b.first - a.first;
    if (sample_dt > 0.0) {
      const double speed =
          std::max(0.0, (b.second - a.second) / sample_dt);
      in_handle = std::clamp(
          speed * ClampInfluence(seed_in.influence, cfg) / 100.0 * dt,
          0.0,
          path_length);
    }
  }

  std::array<double, 4> temporal = {
      out_handle,
      ClampInfluence(seed_out.influence, cfg),
      in_handle,
      ClampInfluence(seed_in.influence, cfg),
  };

  ceres::Problem problem;
  problem.AddParameterBlock(temporal.data(), 4);
  const double min_influence = std::max(0.1, cfg.min_influence);
  const double max_influence =
      std::min(100.0, std::max(min_influence, cfg.max_influence));
  problem.SetParameterLowerBound(temporal.data(), 0, 0.0);
  problem.SetParameterUpperBound(temporal.data(), 0, path_length);
  problem.SetParameterLowerBound(temporal.data(), 1, min_influence);
  problem.SetParameterUpperBound(temporal.data(), 1, max_influence);
  problem.SetParameterLowerBound(temporal.data(), 2, 0.0);
  problem.SetParameterUpperBound(temporal.data(), 2, path_length);
  problem.SetParameterLowerBound(temporal.data(), 3, min_influence);
  problem.SetParameterUpperBound(temporal.data(), 3, max_influence);

  const double weight = 1.0 / std::max(path_length, 1.0);
  for (std::size_t target_idx = 1; target_idx + 1 < targets.size();
       ++target_idx) {
    auto* cost =
        new ceres::AutoDiffCostFunction<DistanceTimingResidual, 1, 4>(
            new DistanceTimingResidual{targets[target_idx].first,
                                       t0,
                                       t1,
                                       path_length,
                                       targets[target_idx].second,
                                       weight});
    problem.AddResidualBlock(cost, nullptr, temporal.data());
  }

  ceres::Solver::Options options = CeresOptions(cfg);
  options.max_num_iterations = std::max(cfg.max_iters_per_segment, 100);
  ceres::Solver::Summary summary;
  ceres::Solve(options, &problem, &summary);
  result.iters += static_cast<int>(summary.iterations.size());
  const double out_influence = ClampInfluence(temporal[1], cfg);
  const double in_influence = ClampInfluence(temporal[3], cfg);
  result.ease_out_at_i = {{
      SpeedFromHandleDistance(std::clamp(temporal[0], 0.0, path_length),
                              out_influence,
                              dt),
      out_influence,
  }};
  result.ease_in_at_j = {{
      SpeedFromHandleDistance(std::clamp(temporal[2], 0.0, path_length),
                              in_influence,
                              dt),
      in_influence,
  }};

  const ErrorReport report = ComputeError(
      ps,
      i,
      j,
      [&](double t) {
        return ReconstructBezier(ps,
                                 i,
                                 j,
                                 result.ease_out_at_i,
                                 result.ease_in_at_j,
                                 result.spatial_out_at_i,
                                 result.spatial_in_at_j,
                                 t);
      },
      cfg,
      comp,
      ps.layer_xform_at_start ? &*ps.layer_xform_at_start : nullptr);
  CopyError(result, report);
  result.feasible = Passes(report, cfg);
  result.reason =
      result.feasible ? "unified_spatial_speed_ok"
                      : "infeasible_unified_spatial_speed";
  return result;
}

}  // namespace bbsolver::segment_fit
