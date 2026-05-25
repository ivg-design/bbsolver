#pragma once

#include "bbsolver/domain.hpp"

namespace bbsolver {

double StrictPropertyCeiling(const SolverConfig& config);

bool TemporalRefitScreenGateEnabled(const SolverConfig& config);

double StrictScreenCeiling(const SolverConfig& config);

double RelativeCeilingFromBaseline(double max_err,
                                   double max_err_screen_px,
                                   const SolverConfig& config,
                                   double relative_eps);

}  // namespace bbsolver
