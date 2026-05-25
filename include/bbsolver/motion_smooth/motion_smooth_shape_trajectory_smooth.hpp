#pragma once

#include <vector>

#include "bbsolver/domain.hpp"

namespace bbsolver {

struct ShapeMotionTrajectorySmoothResult {
  std::vector<std::vector<double>> original_values;
  std::vector<std::vector<double>> smoothed_values;
  int smoothing_passes = 0;
  double smoothing_blend = 0.0;
  double max_shape_displacement = 0.0;
  double max_control_displacement = 0.0;
  double max_turn_before_deg = 0.0;
  double max_turn_after_deg = 0.0;
  double displacement_limit = 0.0;
  bool source_fidelity_enabled = false;
  int source_fidelity_samples = 0;
};

// Build a smoothed trajectory by least-squares fitting a cubic Bezier
// through the (optionally fidelity-extended) observations, then
// blending each control with the smoothed cubic by
// `smoothing_blend = clamp(strength / (strength + 2.0), 0, 0.90)` and
// capping per-control displacement at
// `clamp(max(strength*24, extent*0.04*strength), 6, extent*0.35)`.
// The first/last shape keys keep their original control points; the
// anchor (dims 0/1) is never touched. Returns both the original and
// smoothed value sequences plus diagnostic counters.
ShapeMotionTrajectorySmoothResult BuildShapeMotionTrajectorySmoothValues(
    const PropertySamples& property_samples,
    const std::vector<double>& source_key_times,
    const std::vector<std::vector<double>>& raw,
    int vertex_count,
    int dims,
    double strength,
    const std::vector<double>* fidelity_times = nullptr,
    const std::vector<std::vector<double>>* fidelity_values = nullptr);

}  // namespace bbsolver
