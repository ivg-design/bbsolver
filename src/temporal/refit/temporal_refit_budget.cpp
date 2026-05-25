#include "bbsolver/temporal/refit/temporal_refit_budget.hpp"

#include "bbsolver/domain.hpp"

#include <algorithm>

namespace bbsolver {

double StrictPropertyCeiling(const SolverConfig& config) {
  return std::max(0.0, config.tolerance);
}

bool TemporalRefitScreenGateEnabled(const SolverConfig& config) {
  return config.tolerance_screen_px > 0.0 || config.weight_screen > 0.0;
}

double StrictScreenCeiling(const SolverConfig& config) {
  return config.tolerance_screen_px > 0.0 ? config.tolerance_screen_px
                                          : config.tolerance;
}

double RelativeCeilingFromBaseline(double max_err,
                                   double max_err_screen_px,
                                   const SolverConfig& config,
                                   double relative_eps) {
  double ceiling = max_err;
  if (TemporalRefitScreenGateEnabled(config)) {
    ceiling = std::max(ceiling, max_err_screen_px);
  }
  return ceiling + std::max(0.0, relative_eps);
}

}  // namespace bbsolver
