#pragma once

// façade: the original solver/src/motion_smooth_solver.cpp
// (360 LOC, eight independent responsibilities) was split into:
//   - motion_smooth_sample_points — IsMotionSmoothSpatialProperty,
//                                                SegmentEndpointValueOrSample,
//                                                MotionSmoothSourceKeyTimes,
//                                                MotionSmoothRawPoints,
//                                                MotionSmoothInterpolatedVector
//   - motion_smooth_bezier_ease — ApplyMotionSmoothBezierEase
//   - motion_smooth_endpoint_keys — MotionSmoothEndpointKeys
//   - motion_smooth_spatial_trajectory — MotionSmoothSpatialTrajectoryKeys
// This header re-exports the four sibling headers so existing consumers
// (main.cpp, motion_smooth_dispatch.cpp, motion_smooth_reduction_gate.cpp,
// motion_smooth_shape_flat.cpp, motion_smooth_shape_source_key_schedule.cpp,
// motion_smooth_shape_trajectory_smooth.cpp, test_motion_smooth_solver.cpp)
// keep compiling unchanged.

// Each re-export below carries `// IWYU pragma: keep` so clangd's
// include-cleaner accepts the façade architecture: the sub-headers
// are intentionally included for transitive symbol resolution by
// downstream consumers, not for direct use in this header body.
#include "bbsolver/motion_smooth/motion_smooth_bezier_ease.hpp"  // IWYU pragma: keep
#include "bbsolver/motion_smooth/motion_smooth_endpoint_keys.hpp"  // IWYU pragma: keep
#include "bbsolver/motion_smooth/motion_smooth_sample_points.hpp"  // IWYU pragma: keep
#include "bbsolver/motion_smooth/motion_smooth_spatial_trajectory.hpp"  // IWYU pragma: keep
