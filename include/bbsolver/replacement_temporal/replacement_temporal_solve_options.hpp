#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/app/cli_options.hpp"
#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/replacement_temporal/replacement_temporal_solver.hpp"

namespace bbsolver {

ReplacementTemporalSolverOptions ReplacementTemporalOptions(
    const SolverConfig& config,
    int max_gap_samples,
    const SolveOptions& options,
    PlacementProgressFn placement_progress_fn = {});

}  // namespace bbsolver
