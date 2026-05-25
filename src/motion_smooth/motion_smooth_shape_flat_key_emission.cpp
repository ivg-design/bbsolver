#include "bbsolver/motion_smooth/motion_smooth_shape_flat_key_emission.hpp"

#include <cstddef>
#include <utility>
#include <vector>

#include "bbsolver/domain.hpp"
#include "bbsolver/motion_smooth/motion_smooth_bezier_ease.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_rove_schedule.hpp"
#include "bbsolver/samples/sample_key_timing.hpp"

namespace bbsolver {

std::vector<Key> EmitMotionSmoothShapeFlatKeysFromRoveSchedule(
    const ShapeMotionRoveSchedule& rove_schedule,
    const PropertySamples& property_samples,
    const SolverConfig& config,
    int dims,
    bool use_ease) {
  const std::vector<TemporalEase> neutral_ease =
      DefaultEasesForProperty(property_samples);
  std::vector<Key> keys;
  keys.reserve(rove_schedule.times.size());
  for (std::size_t ki = 0; ki < rove_schedule.times.size(); ++ki) {
    Key key;
    key.t_sec = rove_schedule.times[ki];
    key.v = rove_schedule.values[ki];
    key.interp_in = ki == 0
        ? InterpType::Linear
        : (use_ease ? InterpType::Bezier
                    : InterpType::Linear);
    key.interp_out = ki + 1 == rove_schedule.times.size()
        ? InterpType::Linear
        : (use_ease ? InterpType::Bezier
                    : InterpType::Linear);
    key.temporal_ease_in = neutral_ease;
    key.temporal_ease_out = neutral_ease;
    key.temporal_continuous = use_ease;
    key.temporal_auto_bezier = use_ease;
    keys.push_back(std::move(key));
  }
  if (use_ease) {
    ApplyMotionSmoothBezierEase(property_samples, config, dims, &keys);
  }
  return keys;
}

}  // namespace bbsolver
