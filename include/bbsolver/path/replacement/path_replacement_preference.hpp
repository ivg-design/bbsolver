#pragma once

#include "bbsolver/path/replacement/path_replacement_acceptance.hpp"
#include "bbsolver/path/replacement/path_replacement_notes.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"
#include "bbsolver/shape/sharp_corner_policy.hpp"

namespace bbsolver {

struct ReplacementFastVertexPreferenceInput {
  bool prefer_vertices = false;
  int source_sample_count = 0;
  int candidate_key_count = 0;
  int estimated_original_keys = 0;
  int validation_samples_checked = 0;
  bool candidate_converged = false;
  double candidate_max_err = 0.0;
  double path_tolerance = 0.0;
  int fitted_vertices = 0;
  int source_vertices_for_ratio = 0;
  double min_vertex_reduction_ratio = 0.0;
};

struct ReplacementFastVertexPreferenceVerdict {
  bool accept = false;
  bool candidate_reduces_keys = false;
  bool estimated_key_gate = false;
  double vertex_reduction_ratio = 0.0;
};

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
    double min_vertex_reduction_ratio);

ReplacementFastVertexPreferenceVerdict EvaluateReplacementFastVertexPreference(
    const ReplacementFastVertexPreferenceInput& input);

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
    const ReplacementFastVertexPreferenceVerdict& preference);

}  // namespace bbsolver
