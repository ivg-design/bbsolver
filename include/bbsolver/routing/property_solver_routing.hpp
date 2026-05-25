#pragma once

namespace bbsolver {

enum class PropertySolveRoute {
  PreserveSourceKeys,
  MotionSmooth,
  FrameKeyFallback,
  ReplacementShapeFlatTemporal,
  PathDecomposed,
  PlainTemporal,
};

struct PropertySolveRouteInput {
  bool preserve_source_keys = false;
  bool motion_smooth_enabled = false;
  bool temporal_optimization_enabled = true;
  bool path_temporal_reduced_by_fit = false;
  bool decompose_paths = false;
  bool decompose_candidate_is_shape_flat = false;
};

PropertySolveRoute ChoosePropertySolveRoute(
    const PropertySolveRouteInput& input);

}  // namespace bbsolver
