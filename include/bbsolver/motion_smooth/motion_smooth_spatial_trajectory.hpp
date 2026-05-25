#pragma once

#include "bbsolver/domain.hpp"

namespace bbsolver {

// Build a Motion Smooth spatial-trajectory key bundle for a 2D
// continuous property. Workflow:
//   1. Smooth the raw sample point cloud via
//      MotionSmoothFilteredPoints at the configured strength (clamped
//      to >= 1.0; falls back through tolerance_screen_px to tolerance).
//   2. Choose key times. Prefer the property's source-key times when
//      >= 2 of them fall in window; otherwise RDP-simplify the smoothed
//      point cloud at `max(0.75, strength * 0.75)` and use the kept
//      indices' sample times.
//   3. Sample the smoothed cloud at each key time. Endpoints sample
//      the raw cloud instead so the bundle starts/ends exactly on the
//      original first/last sample.
//   4. Compute per-key spatial tangents (1/3 outward delta at the
//      endpoints, 1/6 central-difference delta in the interior),
//      clamped to 45% of the adjacent segment length.
//   5. Apply Motion Smooth Bezier ease via ApplyMotionSmoothBezierEase
//      (no-op when `motion_smooth_use_ease` is false).
//
// The bundle's notes string includes every diagnostic counter the
// route emits: smoothing_passes, smoothing_strength,
// max_smoothing_displacement, key_schedule, source_key_count,
// input_samples, output_keys, motion_smooth_ease, motion_smooth_bezier.
PropertyKeys MotionSmoothSpatialTrajectoryKeys(
    const PropertySamples& property_samples,
    const SolverConfig& config);

}  // namespace bbsolver
