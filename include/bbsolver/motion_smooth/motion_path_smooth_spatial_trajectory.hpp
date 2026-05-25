#pragma once

#include "bbsolver/domain.hpp"

namespace bbsolver {

// Spatial Position-style motion-path smoother. This is intentionally separate
// from MotionSmoothSpatialTrajectoryKeys: it reduces keys against a smoothed
// trajectory, while preserving optional keyed-frame and sharp-turn constraints.
PropertyKeys MotionPathSmoothSpatialTrajectoryKeys(
    const PropertySamples& property_samples,
    const SolverConfig& config);

}  // namespace bbsolver
