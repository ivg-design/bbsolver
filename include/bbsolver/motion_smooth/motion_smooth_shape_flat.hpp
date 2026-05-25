#pragma once

#include "bbsolver/domain.hpp"

namespace bbsolver {

PropertyKeys MotionSmoothShapeFlatTrajectoryKeys(
    const PropertySamples& property_samples,
    const SolverConfig& config);

}  // namespace bbsolver
