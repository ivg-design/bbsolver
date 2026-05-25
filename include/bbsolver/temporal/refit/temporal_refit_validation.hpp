#pragma once

#include "bbsolver/domain.hpp"
#include "bbsolver/temporal/refit/temporal_refit.hpp"

namespace bbsolver {

bool ValidateRefitAgainstSource(
    const PropertyKeys& candidate,
    const PropertySamples& source,
    const SolverConfig& config,
    const CompInfo& comp,
    TemporalRefitOptions::BudgetMode budget_mode,
    double budget_relative_ceiling,
    double* max_err_out,
    double* max_err_screen_px_out);

}  // namespace bbsolver
