#pragma once

#include "bbsolver/domain.hpp"
#include "bbsolver/temporal/refit/temporal_refit.hpp"

#include <vector>

namespace bbsolver {

bool TemporalRefitIsShapeFlatProperty(const PropertySamples& source);

bool IsValidTemporalRefitShapeFlatValue(const std::vector<double>& value);

bool TemporalRefitShapeFlatTopologyMatches(const std::vector<double>& a,
                                           const std::vector<double>& b);

bool AllTemporalRefitSourceSamplesAreValidShapeFlat(
    const PropertySamples& source);

bool AllTemporalRefitShapeFlatKeysHaveStableTopology(
    const PropertyKeys& keys);

PropertySamples ResampleShapeFlatAcceptedAtSourceTimes(
    const PropertyKeys& accepted_keys,
    const PropertySamples& source_template);

bool ValidateShapeRefitAgainstSource(
    const PropertyKeys& candidate,
    const PropertySamples& source,
    const SolverConfig& config,
    TemporalRefitOptions::BudgetMode budget_mode,
    double budget_relative_ceiling,
    double* max_err_out,
    double* max_err_screen_px_out);

}  // namespace bbsolver
