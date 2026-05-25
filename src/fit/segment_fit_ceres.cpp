#include "bbsolver/fit/segment_fit_ceres.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <array>
#include <vector>

#include <ceres/ceres.h>
#include <cstddef>
#include <utility>

#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/metrics/error_metrics.hpp"
#include "bbsolver/fit/segment_fit_bezier.hpp"
#include "bbsolver/fit/segment_fit_policy.hpp"
#include "bbsolver/fit/segment_fit_samples.hpp"
#include "bbsolver/fit/segment_fit_unified_spatial.hpp"

namespace bbsolver::segment_fit {
namespace {

struct TemporalResidual {
  double t;
  double t0;
  double t1;
  double v0;
  double v1;
  double target;
  double weight;

  template <typename T>
  bool operator()(const T* const ease, T* residual) const {
    const T u = SolveTemporalParamT(T(t), t0, ease[1], t1, ease[3]);
    const T out_time = T(t0) + (ease[1] / T(100.0)) * T(t1 - t0);
    const T in_time = T(t1) - (ease[3] / T(100.0)) * T(t1 - t0);
    const T out_value = T(v0) + ease[0] * (out_time - T(t0));
    const T in_value = T(v1) - ease[2] * (T(t1) - in_time);
    const T value = CubicBezierT(u, T(v0), out_value, in_value, T(v1));
    residual[0] = T(weight) * (value - T(target));
    return true;
  }
};

struct SpatialResidual {
  double t;
  double t0;
  double t1;
  double v0;
  double v1;
  double target;
  double weight;

  template <typename T>
  bool operator()(const T* const ease,
                  const T* const spatial,
                  T* residual) const {
    const T u = SolveTemporalParamT(T(t), t0, ease[1], t1, ease[3]);
    const T value = CubicBezierT(
        u, T(v0), T(v0) + spatial[0], T(v1) + spatial[1], T(v1));
    residual[0] = T(weight) * (value - T(target));
    return true;
  }
};

}  // namespace

DimCeresResult RunSingleDimCeres(int dim,
                                 int i,
                                 int j,
                                 const PropertySamples& ps,
                                 const SolverConfig& cfg,
                                 const SegmentFitResult& seed) {
  const TemporalEase out = EaseForDim(seed.ease_out_at_i, dim);
  const TemporalEase in = EaseForDim(seed.ease_in_at_j, dim);
  std::array<double, 4> temporal = {
      out.speed,
      ClampInfluence(out.influence, cfg),
      in.speed,
      ClampInfluence(in.influence, cfg),
  };
  std::array<double, 2> spatial = {
      ComponentOrZero(seed.spatial_out_at_i, static_cast<std::size_t>(dim)),
      ComponentOrZero(seed.spatial_in_at_j, static_cast<std::size_t>(dim)),
  };

  ceres::Problem problem;
  const double t0 = SampleTime(ps, i);
  const double t1 = SampleTime(ps, j);
  const double min_influence = std::max(0.1, cfg.min_influence);
  const double max_influence =
      std::min(100.0, std::max(min_influence, cfg.max_influence));

  problem.AddParameterBlock(temporal.data(), 4);
  problem.SetParameterLowerBound(temporal.data(), 1, min_influence);
  problem.SetParameterUpperBound(temporal.data(), 1, max_influence);
  problem.SetParameterLowerBound(temporal.data(), 3, min_influence);
  problem.SetParameterUpperBound(temporal.data(), 3, max_influence);
  if (ps.property.is_spatial) {
    problem.AddParameterBlock(spatial.data(), 2);
  }

  const double weight = ResidualWeightForDim(ps, cfg, dim);
  for (int sample_idx = i + 1; sample_idx < j; ++sample_idx) {
    const double t = SampleTime(ps, sample_idx);
    if (ps.property.is_spatial) {
      auto* cost = new ceres::AutoDiffCostFunction<SpatialResidual, 1, 4, 2>(
          new SpatialResidual{t,
                              t0,
                              t1,
                              SampleValue(ps, i, dim),
                              SampleValue(ps, j, dim),
                              SampleValue(ps, sample_idx, dim),
                              weight});
      problem.AddResidualBlock(cost, nullptr, temporal.data(), spatial.data());
    } else {
      auto* cost = new ceres::AutoDiffCostFunction<TemporalResidual, 1, 4>(
          new TemporalResidual{t,
                               t0,
                               t1,
                               SampleValue(ps, i, dim),
                               SampleValue(ps, j, dim),
                               SampleValue(ps, sample_idx, dim),
                               weight});
      problem.AddResidualBlock(cost, nullptr, temporal.data());
    }
  }

  DimCeresResult result;
  if (j - i > 1) {
    ceres::Solver::Options options = CeresOptions(cfg);
    ceres::Solver::Summary summary;
    ceres::Solve(options, &problem, &summary);
    result.iters = static_cast<int>(summary.iterations.size());
  }

  result.ease_out = {temporal[0], ClampInfluence(temporal[1], cfg)};
  result.ease_in = {temporal[2], ClampInfluence(temporal[3], cfg)};
  result.spatial_out = spatial[0];
  result.spatial_in = spatial[1];
  return result;
}

SegmentFitResult TrySeparatedCeresBezier(int i,
                                         int j,
                                         const PropertySamples& ps,
                                         const SolverConfig& cfg,
                                         const CompInfo& comp,
                                         const SegmentFitResult& seed) {
  SegmentFitResult result = seed;
  result.reason = "infeasible_bezier_ceres";
  result.ease_out_at_i.clear();
  result.ease_in_at_j.clear();
  result.spatial_out_at_i.clear();
  result.spatial_in_at_j.clear();

  for (int d = 0; d < Dimensions(ps); ++d) {
    const DimCeresResult dim_result = RunSingleDimCeres(d, i, j, ps, cfg, seed);
    result.ease_out_at_i.push_back(dim_result.ease_out);
    result.ease_in_at_j.push_back(dim_result.ease_in);
    if (ps.property.is_spatial) {
      result.spatial_out_at_i.push_back(dim_result.spatial_out);
      result.spatial_in_at_j.push_back(dim_result.spatial_in);
    }
    result.iters += dim_result.iters;
  }

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
  if (result.feasible) {
    result.reason = "bezier_ok";
  }
  return result;
}

SegmentFitResult TryHermiteBezier(int i,
                                  int j,
                                  const PropertySamples& ps,
                                  const SolverConfig& cfg,
                                  const CompInfo& comp) {
  SegmentFitResult result;
  result.interp = InterpType::Bezier;
  result.ease_out_at_i = HermiteEase(ps, i, j, true, cfg);
  result.ease_in_at_j = HermiteEase(ps, i, j, false, cfg);
  if (ps.property.is_spatial) {
    result.spatial_out_at_i = HermiteSpatialTangents(ps, i, j, true, cfg);
    result.spatial_in_at_j = HermiteSpatialTangents(ps, i, j, false, cfg);
  }

  if (IsUnifiedSpatial(ps)) {
    return FitUnifiedSpatialTiming(i, j, ps, cfg, comp, std::move(result));
  }

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
      result.feasible ? "bezier_ok" : "infeasible_bezier_heuristic";
  return result;
}

SegmentFitResult TryCeresBezier(int i,
                                int j,
                                const PropertySamples& ps,
                                const SolverConfig& cfg,
                                const CompInfo& comp,
                                const SegmentFitResult& seed) {
  const int dims = Dimensions(ps);
  if (ps.property.is_separated) {
    return TrySeparatedCeresBezier(i, j, ps, cfg, comp, seed);
  }

  const int channels = TemporalChannels(ps);
  std::vector<std::array<double, 4>> temporal(
      static_cast<std::size_t>(channels));
  for (int c = 0; c < channels; ++c) {
    const TemporalEase out = EaseForDim(seed.ease_out_at_i, c);
    const TemporalEase in = EaseForDim(seed.ease_in_at_j, c);
    temporal[static_cast<std::size_t>(c)] = {
        out.speed,
        ClampInfluence(out.influence, cfg),
        in.speed,
        ClampInfluence(in.influence, cfg),
    };
  }

  std::vector<std::array<double, 2>> spatial(static_cast<std::size_t>(dims));
  for (int d = 0; d < dims; ++d) {
    spatial[static_cast<std::size_t>(d)] = {
        ComponentOrZero(seed.spatial_out_at_i, static_cast<std::size_t>(d)),
        ComponentOrZero(seed.spatial_in_at_j, static_cast<std::size_t>(d)),
    };
  }

  ceres::Problem problem;
  const double t0 = SampleTime(ps, i);
  const double t1 = SampleTime(ps, j);
  const double min_influence = std::max(0.1, cfg.min_influence);
  const double max_influence =
      std::min(100.0, std::max(min_influence, cfg.max_influence));

  for (auto& block : temporal) {
    problem.AddParameterBlock(block.data(), 4);
    problem.SetParameterLowerBound(block.data(), 1, min_influence);
    problem.SetParameterUpperBound(block.data(), 1, max_influence);
    problem.SetParameterLowerBound(block.data(), 3, min_influence);
    problem.SetParameterUpperBound(block.data(), 3, max_influence);
  }
  if (ps.property.is_spatial) {
    for (auto& block : spatial) {
      problem.AddParameterBlock(block.data(), 2);
    }
  }

  for (int sample_idx = i + 1; sample_idx < j; ++sample_idx) {
    const double t = SampleTime(ps, sample_idx);
    for (int d = 0; d < dims; ++d) {
      const int channel = ps.property.is_separated ? d : 0;
      const double weight = ResidualWeightForDim(ps, cfg, d);
      if (ps.property.is_spatial) {
        auto* cost =
            new ceres::AutoDiffCostFunction<SpatialResidual, 1, 4, 2>(
                new SpatialResidual{t,
                                    t0,
                                    t1,
                                    SampleValue(ps, i, d),
                                    SampleValue(ps, j, d),
                                    SampleValue(ps, sample_idx, d),
                                    weight});
        problem.AddResidualBlock(
            cost,
            nullptr,
            temporal[static_cast<std::size_t>(channel)].data(),
            spatial[static_cast<std::size_t>(d)].data());
      } else {
        auto* cost = new ceres::AutoDiffCostFunction<TemporalResidual, 1, 4>(
            new TemporalResidual{t,
                                 t0,
                                 t1,
                                 SampleValue(ps, i, d),
                                 SampleValue(ps, j, d),
                                 SampleValue(ps, sample_idx, d),
                                 weight});
        problem.AddResidualBlock(
            cost, nullptr, temporal[static_cast<std::size_t>(channel)].data());
      }
    }
  }

  SegmentFitResult result = seed;
  result.reason = "infeasible_bezier_ceres";
  if (j - i > 1) {
    ceres::Solver::Options options = CeresOptions(cfg);
    ceres::Solver::Summary summary;
    ceres::Solve(options, &problem, &summary);
    result.iters = static_cast<int>(summary.iterations.size());
  }

  result.ease_out_at_i.clear();
  result.ease_in_at_j.clear();
  for (const auto& block : temporal) {
    result.ease_out_at_i.push_back({block[0], ClampInfluence(block[1], cfg)});
    result.ease_in_at_j.push_back({block[2], ClampInfluence(block[3], cfg)});
  }
  if (ps.property.is_spatial) {
    result.spatial_out_at_i.clear();
    result.spatial_in_at_j.clear();
    for (const auto& block : spatial) {
      result.spatial_out_at_i.push_back(block[0]);
      result.spatial_in_at_j.push_back(block[1]);
    }
  }

  if (IsUnifiedSpatial(ps)) {
    return FitUnifiedSpatialTiming(i, j, ps, cfg, comp, std::move(result));
  }

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
  if (result.feasible) {
    result.reason = "bezier_ok";
  }
  return result;
}

}  // namespace bbsolver::segment_fit
