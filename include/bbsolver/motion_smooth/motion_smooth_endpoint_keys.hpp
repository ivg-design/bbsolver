#pragma once

#include "bbsolver/domain.hpp"

namespace bbsolver {

// Build an endpoint-only key bundle for the Motion Smooth route:
// first sample + last sample (when distinct in time by more than
// `1e-9` seconds). Each key carries the property's default temporal
// ease arrays, an `InterpType::Bezier` body interp, and Linear
// extrapolation on the bundle's outer edges.
//
// Shape-flat paths whose first/last samples have mismatched vertex
// counts are routed to ShapeFlatFrameKeyFallback with the note
// "solve_mode_motion_smooth_skipped: endpoint_topology_mismatch".
//
// Notes string is composed as:
//   "solve_mode_motion_smooth; endpoint_keys=<N>;
//    motion_smooth_ease=<on|off>;
//    source_error_not_evaluated=true".
//
// `config.motion_smooth_use_ease && config.allow_bezier` decides the
// motion_smooth_ease=on/off token (does not change the interp type
// here — see ApplyMotionSmoothBezierEase for the actual ease curve).
PropertyKeys MotionSmoothEndpointKeys(const PropertySamples& property_samples,
                                      const SolverConfig& config);

}  // namespace bbsolver
