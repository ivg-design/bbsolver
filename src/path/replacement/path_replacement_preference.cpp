#include "bbsolver/path/replacement/path_replacement_preference.hpp"
#include "bbsolver/shape/sharp_corner_policy.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"
#include "bbsolver/path/replacement/path_replacement_notes.hpp"
#include "bbsolver/path/replacement/path_replacement_acceptance.hpp"

#include <algorithm>

namespace bbsolver {

ReplacementFastVertexPreferenceInput BuildReplacementFastVertexPreferenceInput(
    bool prefer_vertices,
    int source_sample_count,
    int candidate_key_count,
    int estimated_original_keys,
    int validation_samples_checked,
    const ReplacementValidationSummary& validation_summary,
    double path_tolerance,
    int fitted_vertices,
    int source_vertices_for_ratio,
    double min_vertex_reduction_ratio) {
  ReplacementFastVertexPreferenceInput input;
  input.prefer_vertices = prefer_vertices;
  input.source_sample_count = source_sample_count;
  input.candidate_key_count = candidate_key_count;
  input.estimated_original_keys = estimated_original_keys;
  input.validation_samples_checked = validation_samples_checked;
  input.candidate_converged = validation_summary.candidate_converged;
  input.candidate_max_err = validation_summary.candidate_max_err;
  input.path_tolerance = path_tolerance;
  input.fitted_vertices = fitted_vertices;
  input.source_vertices_for_ratio = source_vertices_for_ratio;
  input.min_vertex_reduction_ratio = min_vertex_reduction_ratio;
  return input;
}

ReplacementFastVertexPreferenceVerdict EvaluateReplacementFastVertexPreference(
    const ReplacementFastVertexPreferenceInput& input) {
  ReplacementFastVertexPreferenceVerdict verdict;
  verdict.candidate_reduces_keys =
      input.source_sample_count <= 2 ||
      input.candidate_key_count * 10 < input.source_sample_count * 9;
  verdict.estimated_key_gate =
      input.estimated_original_keys > 0 &&
      input.candidate_key_count <= input.estimated_original_keys;
  verdict.vertex_reduction_ratio =
      input.source_vertices_for_ratio > 0
          ? static_cast<double>(input.source_vertices_for_ratio -
                                input.fitted_vertices) /
                static_cast<double>(input.source_vertices_for_ratio)
          : 0.0;
  verdict.accept =
      input.prefer_vertices &&
      input.source_sample_count >= 8 &&
      verdict.estimated_key_gate &&
      input.validation_samples_checked > 0 &&
      input.candidate_converged &&
      input.candidate_max_err <= input.path_tolerance + 1e-9 &&
      verdict.candidate_reduces_keys &&
      input.fitted_vertices > 0 &&
      input.fitted_vertices < input.source_vertices_for_ratio &&
      verdict.vertex_reduction_ratio >=
          std::max(0.0, input.min_vertex_reduction_ratio);
  return verdict;
}

ReplacementFastVertexPreferenceNoteInput
BuildReplacementFastVertexPreferenceNoteInput(
    double candidate_max_err,
    const PathTemporalValidationResult& source_validation,
    bool visible_outline_reference,
    const SharpCornerValidationResult& sharp_validation,
    int candidate_key_count,
    int source_sample_count,
    int estimated_candidate_keys,
    int estimated_original_keys,
    int fitted_vertices,
    int source_vertices_for_ratio,
    const ReplacementFastVertexPreferenceVerdict& preference) {
  ReplacementFastVertexPreferenceNoteInput input;
  input.candidate_max_err = candidate_max_err;
  input.source_validation_notes = source_validation.notes;
  input.visible_outline_reference = visible_outline_reference;
  input.sharp_validation_enabled = sharp_validation.enabled;
  input.sharp_validation_notes = sharp_validation.notes;
  input.candidate_key_count = candidate_key_count;
  input.source_sample_count = source_sample_count;
  input.estimated_candidate_keys = estimated_candidate_keys;
  input.estimated_original_keys = estimated_original_keys;
  input.fitted_vertices = fitted_vertices;
  input.source_vertices_for_ratio = source_vertices_for_ratio;
  input.vertex_reduction_ratio = preference.vertex_reduction_ratio;
  return input;
}

}  // namespace bbsolver
