#include "bbsolver/path/replacement/path_replacement_fast_vertex_acceptance.hpp"

#include <string>

#include "bbsolver/domain.hpp"
#include "bbsolver/path/replacement/path_replacement_acceptance.hpp"
#include "bbsolver/path/replacement/path_replacement_notes.hpp"
#include "bbsolver/path/replacement/path_replacement_preference.hpp"
#include "bbsolver/path/replacement/path_replacement_progress.hpp"
#include "bbsolver/path/config/path_solver_config.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"
#include "bbsolver/path/reduction/path_vertex_reduction.hpp"
#include "bbsolver/progress/progress.hpp"
#include "bbsolver/shape/sharp_corner_policy.hpp"
#include "bbsolver/solve/solver_reporting.hpp"

namespace bbsolver {

ReplacementFastVertexAcceptanceResult
TryAcceptReplacementFastVertexPreference(
    const ReplacementFastVertexAcceptanceRequest& request,
    PropertyKeys* property_keys) {
  ReplacementFastVertexAcceptanceResult result;
  result.candidate_key_count =
      static_cast<int>(property_keys->keys.size());
  result.source_sample_count =
      static_cast<int>(request.original_property_samples->samples.size());

  const std::string host_equivalent_note =
      BuildReplacementHostEquivalentNote(result.candidate_key_count,
                                         result.source_sample_count);
  if (!host_equivalent_note.empty()) {
    AppendJoinedNote(property_keys->notes, host_equivalent_note);
  }

  const PathTemporalValidationOptions fast_validation_options =
      ReplacementPathTemporalValidationOptions(
          *request.config, request.visible_outline_reference);
  const PathTemporalValidationResult fast_source_validation =
      ValidatePathTemporalCandidate(*request.original_property_samples,
                                    *property_keys,
                                    fast_validation_options);
  const SharpCornerValidationResult fast_source_sharp_validation =
      ValidateSharpCornerPreservation(
          *request.original_property_samples,
          *property_keys,
          *request.config,
          !request.visible_outline_reference);
  const ReplacementValidationSummary fast_validation_summary =
      SummarizeReplacementCandidateValidation(
          fast_source_validation, fast_source_sharp_validation, *property_keys);
  const double path_tolerance = EffectivePathTolerance(*request.config);
  const int replacement_source_vertices_for_ratio =
      request.replacement_original_max_vertices > 0
          ? request.replacement_original_max_vertices
: MaxShapeFlatSampleVertexCount(*request.original_property_samples);
  const ReplacementFastVertexPreferenceInput fast_vertex_input =
      BuildReplacementFastVertexPreferenceInput(
          request.config->path_replacement_prefer_vertices,
          result.source_sample_count,
          result.candidate_key_count,
          request.replacement_estimated_original_keys,
          fast_source_validation.samples_checked,
          fast_validation_summary,
          path_tolerance,
          request.replacement_fitted_vertices,
          replacement_source_vertices_for_ratio,
          request.config->path_replacement_min_vertex_reduction_ratio);
  const ReplacementFastVertexPreferenceVerdict fast_vertex_preference =
      EvaluateReplacementFastVertexPreference(fast_vertex_input);

  if (!fast_vertex_preference.accept) {
    return result;
  }

  property_keys->max_err = fast_validation_summary.candidate_max_err;
  property_keys->max_err_screen_px =
      fast_validation_summary.candidate_max_err;
  property_keys->converged = true;
  const ReplacementFastVertexPreferenceNoteInput fast_note_input =
      BuildReplacementFastVertexPreferenceNoteInput(
          fast_validation_summary.candidate_max_err,
          fast_source_validation,
          request.visible_outline_reference,
          fast_source_sharp_validation,
          result.candidate_key_count,
          result.source_sample_count,
          request.replacement_estimated_candidate_keys,
          request.replacement_estimated_original_keys,
          request.replacement_fitted_vertices,
          replacement_source_vertices_for_ratio,
          fast_vertex_preference);
  AppendSolverNote(*property_keys,
                   BuildReplacementFastVertexPreferenceNote(fast_note_input));
  if (request.progress != nullptr) {
    request.progress->Emit(
        ReplacementFastVertexValidationDoneProgressEvent(
            *request.property_samples,
            request.property_idx,
            request.property_count,
            fast_validation_summary.candidate_max_err,
            fast_source_validation.samples_checked,
            property_keys->keys.size(),
            fast_source_sharp_validation.ok));
  }
  result.accepted = true;
  return result;
}

}  // namespace bbsolver
