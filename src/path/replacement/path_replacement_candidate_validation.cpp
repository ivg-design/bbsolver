#include "bbsolver/path/replacement/path_replacement_candidate_validation.hpp"

#include "bbsolver/path/replacement/path_replacement_acceptance.hpp"
#include "bbsolver/path/replacement/path_replacement_notes.hpp"
#include "bbsolver/path/replacement/path_replacement_progress.hpp"
#include "bbsolver/path/config/path_solver_config.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"
#include "bbsolver/progress/progress.hpp"
#include "bbsolver/shape/sharp_corner_policy.hpp"

namespace bbsolver {

ReplacementCandidateValidationResult ValidateReplacementCandidateAgainstBaseline(
    const ReplacementCandidateValidationRequest& request) {
  ReplacementCandidateValidationResult result;
  result.validation_options = ReplacementPathTemporalValidationOptions(
      *request.config, request.visible_outline_reference);
  request.progress->Emit(
      ReplacementValidationStartProgressEvent(
          *request.property_samples,
          request.property_idx,
          request.property_count));
  result.source_validation = ValidatePathTemporalCandidate(
      *request.original_property_samples,
      *request.property_keys,
      result.validation_options);
  result.source_sharp_validation = ValidateSharpCornerPreservation(
      *request.original_property_samples,
      *request.property_keys,
      *request.config,
      !request.visible_outline_reference);
  result.validation_summary = SummarizeReplacementCandidateValidation(
      result.source_validation,
      result.source_sharp_validation,
      *request.property_keys);
  result.verdict = EvaluateReplacementAcceptance(
      static_cast<int>(request.property_keys->keys.size()),
      result.validation_summary.candidate_max_err,
      result.validation_summary.candidate_converged,
      request.replacement_fitted_vertices,
      static_cast<int>(request.original_keys->keys.size()),
      request.original_keys->max_err,
      request.replacement_original_max_vertices,
      request.original_keys->converged,
      request.config->tolerance,
      request.config->path_replacement_prefer_vertices,
      request.config->path_replacement_max_key_growth_ratio,
      request.config->path_replacement_min_vertex_reduction_ratio,
      static_cast<int>(request.original_property_samples->samples.size()));
  request.progress->Emit(
      ReplacementValidationDoneProgressEvent(
          *request.property_samples,
          request.property_idx,
          request.property_count,
          result.validation_summary.candidate_converged,
          result.validation_summary.candidate_max_err,
          result.source_validation.samples_checked,
          request.property_keys->keys.size(),
          request.original_keys->keys.size(),
          result.source_sharp_validation.ok));
  result.source_validation_note = BuildReplacementSourceValidationNote(
      BuildReplacementSourceValidationNoteInput(
          result.source_validation,
          request.visible_outline_reference,
          result.source_sharp_validation));
  return result;
}

}  // namespace bbsolver
