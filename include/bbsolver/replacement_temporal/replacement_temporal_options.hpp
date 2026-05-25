#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/replacement_temporal/replacement_temporal_solver.hpp"

namespace bbsolver {

ReplacementTemporalSolverOptions NormalizeReplacementTemporalOptions(
    ReplacementTemporalSolverOptions options,
    const SolverConfig& config);

}  // namespace bbsolver
