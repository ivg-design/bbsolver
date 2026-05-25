#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/replacement_temporal/replacement_temporal_solver.hpp"

namespace bbsolver {
namespace replacement_temporal {

void PromoteValidatedAnchorFallback(
    const PropertySamples& original,
    const PropertySamples& reduced,
    const SolverConfig& config,
    const ReplacementTemporalSolverOptions& options,
    PropertyKeys& keys);

PropertyKeys MaybeUseAllSampleLinearPruneCandidate(
    const PropertySamples& original,
    const PropertySamples& reduced,
    const SolverConfig& config,
    const ReplacementTemporalSolverOptions& options,
    const PropertyKeys& current);

}  // namespace replacement_temporal
}  // namespace bbsolver
