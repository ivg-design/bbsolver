// bbsolver post-cleanup temporal refit.
//
// The temporal-refit pass is deliberately narrow: scalar/vector/non-Custom
// properties use the existing verifier + DP fitter to attempt a smaller
// temporal key set, then validate the candidate against the original source
// samples. shape_flat custom properties use the same temporal refit loop but
// validate only through source-outline path temporal validation. Other custom
// properties remain deterministic no-ops.
//
// The helper modules under bbsolver/temporal/refit own the staged budget,
// resampling, structural validation, and candidate-acceptance logic.

#include "bbsolver/temporal/refit/temporal_refit.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/dp/dp_placer.hpp"

#include "bbsolver/fit/segment_fitter.hpp"
#include "bbsolver/temporal/refit/temporal_refit_budget.hpp"
#include "bbsolver/temporal/refit/temporal_refit_candidate.hpp"
#include "bbsolver/temporal/refit/temporal_refit_resample.hpp"
#include "bbsolver/temporal/refit/temporal_refit_structural.hpp"
#include "bbsolver/temporal/refit/temporal_refit_support.hpp"
#include "bbsolver/temporal/refit/temporal_refit_validation.hpp"

#include <cmath>
#include <string>
#include <utility>

namespace bbsolver {

TemporalRefitResult TryTemporalRefitKeyReduction(
    const PropertySamples& source,
    const PropertyKeys& accepted_keys,
    const SolverConfig& config,
    const CompInfo& comp,
    const TemporalRefitOptions& options) {
  TemporalRefitResult result;
  result.keys = accepted_keys;
  result.input_key_count = static_cast<int>(accepted_keys.keys.size());
  result.output_key_count = result.input_key_count;

  // Top-level cancel observation BEFORE eligibility — even an
  // ineligible property should bail clean if the operator is
  // already cancelling.
  if (TemporalRefitCancelled(options)) {
    result.rejection_reason = "cancelled";
    result.notes =
        BuildTemporalRefitNotes(result) + TemporalRefitValidationNote(source);
    return result;
  }

  const std::string structural_rejection =
      RefitStructuralRejection(source, accepted_keys);
  if (!structural_rejection.empty()) {
    result.rejection_reason = structural_rejection;
    result.notes =
        BuildTemporalRefitNotes(result) + TemporalRefitValidationNote(source);
    return result;
  }

  result.attempted = true;
  EmitTemporalRefitProgress(options, "temporal_refit_start", 0, 1);

  double budget_relative_ceiling = 0.0;
  if (options.budget_mode == TemporalRefitOptions::BudgetMode::Relative) {
    double baseline_max_err = 0.0;
    double baseline_max_err_screen_px = 0.0;
    ValidateRefitAgainstSource(accepted_keys, source, config, comp,
                               TemporalRefitOptions::BudgetMode::Strict,
                               0.0, &baseline_max_err,
                               &baseline_max_err_screen_px);
    budget_relative_ceiling =
        RelativeCeilingFromBaseline(baseline_max_err,
                                    baseline_max_err_screen_px,
                                    config,
                                    options.relative_eps);
  }

  PropertySamples resampled =
      ResampleAcceptedAtSourceTimes(accepted_keys, source);
  if (resampled.samples.size() != source.samples.size()) {
    result.rejection_reason = "degenerate";
    EmitTemporalRefitProgress(options, "temporal_refit_done", 1, 1);
    result.notes =
        BuildTemporalRefitNotes(result) + TemporalRefitValidationNote(source);
    return result;
  }
  if (TemporalRefitCancelled(options)) {
    result.rejection_reason = "cancelled";
    EmitTemporalRefitProgress(options, "temporal_refit_done", 1, 1);
    result.notes =
        BuildTemporalRefitNotes(result) + TemporalRefitValidationNote(source);
    return result;
  }

  const PlacementProgressFn progress_fn =
      options.progress_fn
          ? PlacementProgressFn([&](const PlacementProgress& progress) {
              PlacementProgress event = progress;
              event.stage = "temporal_refit_progress";
              options.progress_fn(event);
            })
          : PlacementProgressFn{};

  PropertyKeys candidate =
      SolveProperty(resampled, config, comp, FitSegment, options.cancel_fn,
                    options.max_gap_samples, progress_fn);
  if (candidate.keys.size() == 1 && resampled.samples.size() >= 2) {
    candidate = TwoEndpointCandidate(resampled);
  }
  if (TemporalRefitCancelled(options) || candidate.notes == "cancelled") {
    result.rejection_reason = "cancelled";
    EmitTemporalRefitProgress(options, "temporal_refit_done", 1, 1);
    result.notes =
        BuildTemporalRefitNotes(result) + TemporalRefitValidationNote(source);
    return result;
  }

  if (candidate.keys.size() < 2 ||
      std::abs(candidate.keys.front().t_sec - source.samples.front().t_sec) >
          1e-9 ||
      std::abs(candidate.keys.back().t_sec - source.samples.back().t_sec) >
          1e-9) {
    result.rejection_reason = "degenerate";
    EmitTemporalRefitProgress(options, "temporal_refit_done", 1, 1);
    result.notes =
        BuildTemporalRefitNotes(result) + TemporalRefitValidationNote(source);
    return result;
  }

  EmitTemporalRefitProgress(options, "temporal_refit_validate", 1, 2);
  const bool validation_ok =
      ValidateRefitAgainstSource(candidate, source, config, comp,
                                 options.budget_mode,
                                 budget_relative_ceiling,
                                 &result.max_err,
                                 &result.max_err_screen_px);
  const int candidate_key_count =
      static_cast<int>(candidate.keys.size());
  result.output_key_count = result.input_key_count;

  if (candidate_key_count >= result.input_key_count) {
    result.rejection_reason = "no_gain";
  } else if (!validation_ok) {
    result.rejection_reason = "over_budget";
  } else {
    result.accepted = true;
    candidate.max_err = result.max_err;
    candidate.max_err_screen_px = result.max_err_screen_px;
    candidate.converged = true;
    result.keys = std::move(candidate);
    result.output_key_count = candidate_key_count;
  }

  EmitTemporalRefitProgress(options, "temporal_refit_done", 1, 1);
  result.notes =
      BuildTemporalRefitNotes(result) + TemporalRefitValidationNote(source);
  return result;
}

}  // namespace bbsolver
