#include "bbsolver/fit/segment_fit_samples.hpp"
#include "ceres/types.h"
#include "ceres/solver.h"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstddef>
#include <ratio>
#include <vector>

namespace bbsolver::segment_fit {

int Dimensions(const PropertySamples& ps) {
  return std::max(ps.property.dimensions, 1);
}

int TemporalChannels(const PropertySamples& ps) {
  return ps.property.is_separated ? Dimensions(ps): 1;
}

bool IsUnifiedSpatial(const PropertySamples& ps) {
  return ps.property.is_spatial && !ps.property.is_separated;
}

bool IsShapeFlatPath(const PropertySamples& ps) {
  return ps.property.kind == ValueKind::Custom &&
         ps.property.units_label == "shape_flat";
}

double ComponentOrZero(const std::vector<double>& values, std::size_t idx) {
  return idx < values.size() ? values[idx]: 0.0;
}

double SampleValue(const PropertySamples& ps, int sample_idx, int dim) {
  if (sample_idx < 0 || sample_idx >= static_cast<int>(ps.samples.size())) {
    return 0.0;
  }
  return ComponentOrZero(
      ps.samples[static_cast<std::size_t>(sample_idx)].v,
      static_cast<std::size_t>(dim));
}

std::vector<double> SampleVector(const PropertySamples& ps, int sample_idx) {
  std::vector<double> values(static_cast<std::size_t>(Dimensions(ps)), 0.0);
  for (int d = 0; d < Dimensions(ps); ++d) {
    values[static_cast<std::size_t>(d)] = SampleValue(ps, sample_idx, d);
  }
  return values;
}

double SampleTime(const PropertySamples& ps, int sample_idx) {
  if (sample_idx < 0 || sample_idx >= static_cast<int>(ps.samples.size())) {
    return 0.0;
  }
  return ps.samples[static_cast<std::size_t>(sample_idx)].t_sec;
}

double EstimateSlope(const PropertySamples& ps, int a, int b, int dim) {
  const double ta = SampleTime(ps, a);
  const double tb = SampleTime(ps, b);
  if (!(tb > ta)) {
    return 0.0;
  }
  return (SampleValue(ps, b, dim) - SampleValue(ps, a, dim)) / (tb - ta);
}

double SampleDistance(const PropertySamples& ps, int a, int b) {
  if (a < 0 || b < 0 || a >= static_cast<int>(ps.samples.size()) ||
      b >= static_cast<int>(ps.samples.size())) {
    return 0.0;
  }
  double sum = 0.0;
  for (int d = 0; d < Dimensions(ps); ++d) {
    const double delta = SampleValue(ps, b, d) - SampleValue(ps, a, d);
    sum += delta * delta;
  }
  return std::sqrt(sum);
}

double EndpointSpatialSpeed(const PropertySamples& ps,
                            int i,
                            int j,
                            bool out_ease) {
  const int a = out_ease ? i: std::max(i, j - 1);
  const int b = out_ease ? std::min(j, i + 1): j;
  const double ta = SampleTime(ps, a);
  const double tb = SampleTime(ps, b);
  if (!(tb > ta)) {
    return 0.0;
  }
  return SampleDistance(ps, a, b) / (tb - ta);
}

double EndpointSlopeOut(const PropertySamples& ps, int i, int j, int dim) {
  if (i + 1 <= j) {
    return EstimateSlope(ps, i, i + 1, dim);
  }
  return 0.0;
}

double EndpointSlopeIn(const PropertySamples& ps, int i, int j, int dim) {
  if (j - 1 >= i) {
    return EstimateSlope(ps, j - 1, j, dim);
  }
  return 0.0;
}

double ClampInfluence(double value, const SolverConfig& cfg) {
  const double lo = std::max(0.1, cfg.min_influence);
  const double hi = std::min(100.0, std::max(lo, cfg.max_influence));
  if (!std::isfinite(value)) {
    return std::clamp(kDefaultInfluence, lo, hi);
  }
  return std::clamp(value, lo, hi);
}

std::vector<TemporalEase> DefaultEases(int count) {
  return std::vector<TemporalEase>(
      static_cast<std::size_t>(std::max(count, 1)),
      TemporalEase{0.0, kDefaultInfluence});
}

ceres::Solver::Options CeresOptions(const SolverConfig& cfg) {
  ceres::Solver::Options options;
  options.max_num_iterations = std::max(cfg.max_iters_per_segment, 200);
  options.minimizer_type = ceres::TRUST_REGION;
  options.trust_region_strategy_type = ceres::LEVENBERG_MARQUARDT;
  options.linear_solver_type = ceres::DENSE_QR;
  options.function_tolerance = 1e-10;
  options.gradient_tolerance = 1e-12;
  options.parameter_tolerance = 1e-12;
  options.minimizer_progress_to_stdout = false;
  options.logging_type = ceres::SILENT;
  return options;
}

bool UseFastUnifiedSpatialOnly(const PropertySamples& ps) {
  return ps.property.is_spatial && !ps.property.is_separated &&
         ps.samples.size() > 360;
}

double ElapsedMs(std::chrono::steady_clock::time_point start,
                 std::chrono::steady_clock::time_point end) {
  return std::chrono::duration<double, std::milli>(end - start).count();
}

}  // namespace bbsolver::segment_fit
