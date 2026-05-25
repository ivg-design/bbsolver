#include "bbsolver/routing/property_solver_routing.hpp"

namespace bbsolver {

PropertySolveRoute ChoosePropertySolveRoute(
    const PropertySolveRouteInput& input) {
  if (input.preserve_source_keys) {
    return PropertySolveRoute::PreserveSourceKeys;
  }
  if (input.motion_smooth_enabled) {
    return PropertySolveRoute::MotionSmooth;
  }
  if (!input.temporal_optimization_enabled) {
    return PropertySolveRoute::FrameKeyFallback;
  }
  if (input.path_temporal_reduced_by_fit) {
    return PropertySolveRoute::ReplacementShapeFlatTemporal;
  }
  if (input.decompose_paths && input.decompose_candidate_is_shape_flat) {
    return PropertySolveRoute::PathDecomposed;
  }
  return PropertySolveRoute::PlainTemporal;
}

}  // namespace bbsolver
