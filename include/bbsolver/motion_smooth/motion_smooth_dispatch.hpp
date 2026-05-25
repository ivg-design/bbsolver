#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/app/cli_options.hpp"

namespace bbsolver {

PropertyKeys MotionSmoothKeys(const PropertySamples& property_samples,
                              const SolverConfig& config,
                              const CompInfo& comp,
                              const SolveOptions& options);

}  // namespace bbsolver
