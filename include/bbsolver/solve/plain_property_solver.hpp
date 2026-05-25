#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/app/cli_options.hpp"
#include "bbsolver/dp/dp_placer.hpp"

namespace bbsolver {

PropertyKeys SolvePlainProperty(const PropertySamples& property_samples,
                                const SolverConfig& config,
                                const CompInfo& comp,
                                const SolveOptions& options,
                                int max_gap_samples = 0,
                                PlacementProgressFn progress_fn = {});

}  // namespace bbsolver
