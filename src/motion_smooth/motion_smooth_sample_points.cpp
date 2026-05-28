#include "bbsolver/motion_smooth/motion_smooth_sample_points.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include "bbsolver/domain.hpp"
#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/motion_smooth/motion_smooth_geometry.hpp"
#include "bbsolver/routing/property_classification.hpp"
#include "bbsolver/samples/sample_value_helpers.hpp"

namespace bbsolver {

bool IsMotionSmoothSpatialProperty(const PropertySamples& property_samples) {
  return property_samples.property.is_spatial &&
         !property_samples.property.is_separated &&
         !IsShapeFlatPath(property_samples);
}

std::vector<double> SegmentEndpointValueOrSample(
    const PropertySamples& property_samples,
    const SegmentFitResult& fit,
    bool start_endpoint) {
  const std::vector<double>& fitted =
      start_endpoint ? fit.key_value_at_i: fit.key_value_at_j;
  if (!fitted.empty()) {
    return fitted;
  }
  const int sample_idx = start_endpoint
      ? 0
: static_cast<int>(property_samples.samples.size()) - 1;
  return SampleVectorOrZeros(
      property_samples,
      property_samples.samples[static_cast<std::size_t>(sample_idx)]);
}

std::vector<double> MotionSmoothSourceKeyTimes(
    const PropertySamples& property_samples) {
  std::vector<double> times;
  const double eps = 1e-6;
  for (double t: property_samples.property.source_key_times) {
    if (!std::isfinite(t)) {
      continue;
    }
    if (t < property_samples.t_start_sec - eps ||
        t > property_samples.t_end_sec + eps) {
      continue;
    }
    times.push_back(t);
  }
  std::sort(times.begin(), times.end());
  times.erase(std::unique(times.begin(),
                          times.end(),
                          [eps](double a, double b) {
                            return std::abs(a - b) <= eps;
                          }),
              times.end());
  return times;
}

std::vector<std::vector<double>> MotionSmoothRawPoints(
    const PropertySamples& property_samples,
    int dims) {
  std::vector<std::vector<double>> points;
  points.reserve(property_samples.samples.size());
  for (const Sample& sample: property_samples.samples) {
    std::vector<double> value = SampleVectorOrZeros(property_samples, sample);
    if (static_cast<int>(value.size()) < dims) {
      value.resize(static_cast<std::size_t>(dims), 0.0);
    }
    points.push_back(std::move(value));
  }
  return points;
}

std::vector<double> MotionSmoothInterpolatedVector(
    const PropertySamples& property_samples,
    const std::vector<std::vector<double>>& points,
    double t_sec,
    int dims) {
  std::vector<double> value =
      MotionSmoothInterpolatedPoint(property_samples, points, t_sec, dims);
  if (static_cast<int>(value.size()) < dims) {
    value.resize(static_cast<std::size_t>(dims), 0.0);
  }
  return value;
}

}  // namespace bbsolver
