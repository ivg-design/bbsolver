#pragma once

#include <vector>

#include "bbsolver/domain.hpp"

namespace bbsolver {

// Apply the Motion Smooth Bezier ease curve to each adjacent key pair
// in `*keys`. The ease control points come from
// `config.motion_smooth_bezier_{x1,y1,x2,y2}` (clamped to valid
// ranges); `out_influence` and `in_influence` are derived from `x1`
// and `1 - x2` scaled into `[config.min_influence,
// config.max_influence]`. Speeds are scaled by the per-segment
// average velocity (`distance / dt`) along the segment.
//
// No-op when `keys == nullptr`, `keys->size() < 2`, or
// `config.motion_smooth_use_ease == false`. Touches only
// `temporal_ease_in/out`, `interp_in/out` (set to Bezier),
// `temporal_continuous` (set true), and `temporal_auto_bezier` (set
// false) on the affected keys.
void ApplyMotionSmoothBezierEase(const PropertySamples& property_samples,
                                 const SolverConfig& config,
                                 int dims,
                                 std::vector<Key>* keys);

}  // namespace bbsolver
