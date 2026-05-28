#include "bbsolver/motion_smooth/motion_smooth_shape_flat_topology_gate.hpp"

#include <utility>
#include <vector>

#include "bbsolver/domain.hpp"
#include "bbsolver/motion_smooth/motion_smooth_sample_points.hpp"
#include "bbsolver/samples/raw_frame_keys.hpp"
#include "bbsolver/shape/shape_flat_topology.hpp"

namespace bbsolver {

MotionSmoothShapeFlatTopologyGateResult
ValidateMotionSmoothShapeFlatTopology(const PropertySamples& property_samples) {
  MotionSmoothShapeFlatTopologyGateResult result;
  PropertyKeys& keys = result.fallback_keys;
  keys.property_id = property_samples.property.id;
  keys.converged = true;
  keys.max_err = 0.0;
  keys.max_err_screen_px = 0.0;

  if (property_samples.samples.size() < 2) {
    keys.notes = "solve_mode_motion_smooth; no_shape_motion_span";
    return result;
  }

  const int vertex_count =
      ShapeFlatVertexCountFromValues(property_samples.samples.front().v);
  const int dims =
      static_cast<int>(property_samples.samples.front().v.size());
  if (vertex_count <= 0 || dims < 8) {
    result.fallback_keys = ShapeFlatFrameKeyFallback(
        property_samples,
        "solve_mode_motion_smooth_skipped: invalid_shape_topology");
    return result;
  }
  for (const Sample& sample: property_samples.samples) {
    if (ShapeFlatVertexCountFromValues(sample.v) != vertex_count ||
        static_cast<int>(sample.v.size()) != dims) {
      result.fallback_keys = ShapeFlatFrameKeyFallback(
          property_samples,
          "solve_mode_motion_smooth_skipped: variable_shape_topology");
      return result;
    }
  }

  std::vector<double> key_times = MotionSmoothSourceKeyTimes(property_samples);
  if (key_times.size() < 2) {
    result.fallback_keys = ShapeFlatFrameKeyFallback(
        property_samples,
        "solve_mode_motion_smooth_skipped: no_source_key_schedule");
    return result;
  }

  result.ok = true;
  result.vertex_count = vertex_count;
  result.dims = dims;
  result.key_times = std::move(key_times);
  return result;
}

}  // namespace bbsolver
