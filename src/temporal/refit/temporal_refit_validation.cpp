#include "bbsolver/temporal/refit/temporal_refit_validation.hpp"

#include "bbsolver/domain.hpp"
#include "bbsolver/metrics/error_metrics.hpp"
#include "bbsolver/temporal/refit/temporal_refit_budget.hpp"
#include "bbsolver/temporal/refit/temporal_refit_dimensions.hpp"
#include "bbsolver/temporal/refit/temporal_refit_shape.hpp"
#include "bbsolver/temporal/refit/temporal_refit.hpp"
#include "bbsolver/verify/verifier.hpp"

#include <limits>

namespace bbsolver {

bool ValidateRefitAgainstSource(
    const PropertyKeys& candidate,
    const PropertySamples& source,
    const SolverConfig& config,
    const CompInfo& comp,
    TemporalRefitOptions::BudgetMode budget_mode,
    double budget_relative_ceiling,
    double* max_err_out,
    double* max_err_screen_px_out) {
  double max_err = std::numeric_limits<double>::infinity();
  double max_err_screen_px = std::numeric_limits<double>::infinity();
  bool ok = false;

  if (TemporalRefitIsShapeFlatProperty(source)) {
    ok = ValidateShapeRefitAgainstSource(candidate, source, config,
                                         budget_mode,
                                         budget_relative_ceiling,
                                         &max_err,
                                         &max_err_screen_px);
  } else if (!source.samples.empty() &&
             !TemporalRefitIsCustomProperty(source) &&
             AllTemporalRefitCandidateKeysMatchDimensions(candidate, source)) {
    const ErrorReport report =
        ValidateKeys(source, candidate.keys, config, comp);
    max_err = report.max_err;
    max_err_screen_px = report.max_err_screen_px;

    const double property_ceiling =
        budget_mode == TemporalRefitOptions::BudgetMode::Relative
            ? budget_relative_ceiling
: StrictPropertyCeiling(config);
    const bool property_ok = max_err <= property_ceiling + 1e-9;

    bool screen_ok = true;
    if (TemporalRefitScreenGateEnabled(config)) {
      const double screen_ceiling =
          budget_mode == TemporalRefitOptions::BudgetMode::Relative
              ? budget_relative_ceiling
: StrictScreenCeiling(config);
      screen_ok = max_err_screen_px <= screen_ceiling + 1e-9;
    }
    ok = property_ok && screen_ok;
  }

  if (max_err_out) {
    *max_err_out = max_err;
  }
  if (max_err_screen_px_out) {
    *max_err_screen_px_out = max_err_screen_px;
  }
  return ok;
}

}  // namespace bbsolver
