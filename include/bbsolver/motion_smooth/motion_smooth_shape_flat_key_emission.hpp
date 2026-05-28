#pragma once

#include <vector>

#include "bbsolver/domain.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_rove_schedule.hpp"

namespace bbsolver {

// build the output `Key` vector from the rove
// schedule and (when ease is requested) apply the Motion Smooth
// Bezier ease curve. First/last keys always pin to `InterpType::Linear`
// on their outer edge regardless of `use_ease`; interior edges follow
// `use_ease` to choose Linear vs Bezier. Every key carries
// `DefaultEasesForProperty(property_samples)` as its neutral ease
// arrays, and the `temporal_continuous` / `temporal_auto_bezier`
// flags mirror `use_ease`.
//
// Returns the populated key vector; the orchestrator stitches it into
// the PropertyKeys bundle. `ApplyMotionSmoothBezierEase` is invoked
// in-place once `use_ease` is true.
std::vector<Key> EmitMotionSmoothShapeFlatKeysFromRoveSchedule(
    const ShapeMotionRoveSchedule& rove_schedule,
    const PropertySamples& property_samples,
    const SolverConfig& config,
    int dims,
    bool use_ease);

}  // namespace bbsolver
