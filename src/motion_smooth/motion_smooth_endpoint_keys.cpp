#include "bbsolver/motion_smooth/motion_smooth_endpoint_keys.hpp"

#include <cstddef>
#include <string>
#include <vector>

#include "bbsolver/domain.hpp"
#include "bbsolver/routing/property_classification.hpp"
#include "bbsolver/samples/raw_frame_keys.hpp"
#include "bbsolver/samples/sample_key_timing.hpp"
#include "bbsolver/samples/sample_value_helpers.hpp"
#include "bbsolver/shape/shape_flat_topology.hpp"

namespace bbsolver {

PropertyKeys MotionSmoothEndpointKeys(const PropertySamples& property_samples,
                                      const SolverConfig& config) {
  PropertyKeys keys;
  keys.property_id = property_samples.property.id;
  keys.converged = true;
  keys.max_err = 0.0;
  keys.max_err_screen_px = 0.0;
  if (property_samples.samples.empty()) {
    keys.converged = false;
    keys.notes = "solve_mode_motion_smooth; no_samples";
    return keys;
  }

  const Sample& first_sample = property_samples.samples.front();
  const Sample& last_sample = property_samples.samples.back();
  if (IsShapeFlatPath(property_samples)) {
    const int first_vertices = ShapeFlatVertexCountFromValues(first_sample.v);
    const int last_vertices = ShapeFlatVertexCountFromValues(last_sample.v);
    if (first_vertices <= 0 || first_vertices != last_vertices) {
      return ShapeFlatFrameKeyFallback(
          property_samples,
          "solve_mode_motion_smooth_skipped: endpoint_topology_mismatch");
    }
  }

  const bool use_ease =
      config.motion_smooth_use_ease && config.allow_bezier;
  const InterpType interp = InterpType::Bezier;
  auto make_key = [&](const Sample& sample) {
    Key key;
    key.t_sec = sample.t_sec;
    key.v = SampleVectorOrZeros(property_samples, sample);
    key.interp_in = interp;
    key.interp_out = interp;
    key.temporal_ease_in = DefaultEasesForProperty(property_samples);
    key.temporal_ease_out = DefaultEasesForProperty(property_samples);
    if (property_samples.property.is_spatial) {
      key.spatial_continuous = true;
      key.spatial_auto_bezier = true;
    }
    return key;
  };

  keys.keys.push_back(make_key(first_sample));
  if (last_sample.t_sec > first_sample.t_sec + 1e-9) {
    keys.keys.push_back(make_key(last_sample));
  }
  if (!keys.keys.empty()) {
    keys.keys.front().interp_in = InterpType::Linear;
    keys.keys.back().interp_out = InterpType::Linear;
  }
  keys.notes =
      std::string("solve_mode_motion_smooth") +
      "; endpoint_keys=" + std::to_string(keys.keys.size()) +
      "; motion_smooth_ease=" + (use_ease ? "on" : "off") +
      "; source_error_not_evaluated=true";
  if (property_samples.samples.size() > 1) {
    SegmentReport segment;
    segment.start_idx = 0;
    segment.end_idx = static_cast<int>(property_samples.samples.size()) - 1;
    segment.reason = "motion_smooth_endpoint_interpolation";
    keys.segments.push_back(segment);
  }
  return keys;
}

}  // namespace bbsolver
