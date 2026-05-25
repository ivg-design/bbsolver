#pragma once

#include <vector>

#include "bbsolver/motion_smooth/motion_smooth_shape_quality.hpp"

namespace bbsolver {

struct AdaptiveClosedLoopShapeSamples {
  std::vector<std::vector<double>> values;
  std::vector<double> params;
  int refinement_passes = 0;
  int splits = 0;
  int max_keys = 0;
  bool budget_hit = false;
  double target_turn_deg = 0.0;
  double chord_error_tolerance = 0.0;
  ShapeMotionQualityMetrics quality;
};

// Adaptively subdivide a closed-loop shape sample sequence until the
// per-segment chord error and per-vertex turn angle fall under the
// strength-derived tolerances, subject to a per-segment refinement
// budget. The input `closed_values` must include a duplicate wrap
// entry at the end (so closed_values.size() == unique_count + 1).
AdaptiveClosedLoopShapeSamples BuildAdaptiveClosedLoopShapeSamples(
    const std::vector<std::vector<double>>& closed_values,
    int dims,
    int vertex_count,
    double strength,
    bool source_pose_constraints = false);

}  // namespace bbsolver
