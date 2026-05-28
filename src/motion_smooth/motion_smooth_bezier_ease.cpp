#include "bbsolver/motion_smooth/motion_smooth_bezier_ease.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "bbsolver/domain.hpp"
#include "bbsolver/motion_smooth/motion_smooth_geometry.hpp"
#include "bbsolver/samples/sample_key_timing.hpp"

namespace bbsolver {
namespace {

std::vector<TemporalEase> MotionSmoothEaseArray(
    const PropertySamples& property_samples,
    double speed,
    double influence) {
  std::vector<TemporalEase> ease = DefaultEasesForProperty(property_samples);
  for (TemporalEase& item: ease) {
    item.speed = speed;
    item.influence = influence;
  }
  return ease;
}

}  // namespace

void ApplyMotionSmoothBezierEase(const PropertySamples& property_samples,
                                 const SolverConfig& config,
                                 int dims,
                                 std::vector<Key>* keys) {
  if (!keys || keys->size() < 2 || !config.motion_smooth_use_ease) {
    return;
  }
  const double x1 = std::clamp(config.motion_smooth_bezier_x1, 0.001, 0.999);
  const double y1 = std::clamp(config.motion_smooth_bezier_y1, 0.0, 1.0);
  const double x2 = std::clamp(config.motion_smooth_bezier_x2, 0.001, 0.999);
  const double y2 = std::clamp(config.motion_smooth_bezier_y2, 0.0, 1.0);
  const double out_influence = std::clamp(
      x1 * 100.0, config.min_influence, config.max_influence);
  const double in_influence = std::clamp(
      (1.0 - x2) * 100.0, config.min_influence, config.max_influence);
  const double out_slope = y1 / std::max(x1, 1e-6);
  const double in_slope = (1.0 - y2) / std::max(1.0 - x2, 1e-6);
  for (std::size_t i = 0; i + 1 < keys->size(); ++i) {
    Key& left = (*keys)[i];
    Key& right = (*keys)[i + 1];
    const double dt = std::max(right.t_sec - left.t_sec, 1e-6);
    const double distance = MotionPointDistance(left.v, right.v, dims);
    const double avg_speed = distance / dt;
    left.temporal_ease_out = MotionSmoothEaseArray(
        property_samples, avg_speed * out_slope, out_influence);
    right.temporal_ease_in = MotionSmoothEaseArray(
        property_samples, avg_speed * in_slope, in_influence);
    left.interp_out = InterpType::Bezier;
    right.interp_in = InterpType::Bezier;
    left.temporal_continuous = true;
    right.temporal_continuous = true;
    left.temporal_auto_bezier = false;
    right.temporal_auto_bezier = false;
  }
}

}  // namespace bbsolver
