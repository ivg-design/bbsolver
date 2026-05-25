#pragma once

#include <cstddef>
#include <limits>
#include <string>

namespace bbsolver {

struct ReplacementFeatureLayoutTrialResult;
struct PathTemporalValidationResult;
struct SharpCornerValidationResult;

struct ReplacementFractionCoherenceNoteInput {
  bool seed_indices_empty = false;
  bool fraction_coherence_applied = false;
  bool median_fraction_layout_tried = false;
  bool median_fraction_layout_applied = false;
  bool phase2_ok = true;
  int fraction_seed_count = 0;
  int best_seed_phase2_idx = -1;
  int adaptive_insertions = 0;
  int adaptive_evaluations = 0;
  int coherent_vertices = 0;
  double best_fraction_error = 0.0;
  double best_attempt_error = std::numeric_limits<double>::infinity();
  std::string phase2_warning;
};

std::string BuildReplacementFractionCoherenceNote(
    const ReplacementFractionCoherenceNoteInput& input,
    const ReplacementFeatureLayoutTrialResult& feature_layout_trial);

struct ReplacementSuccessNoteInput {
  int source_min_vertices = 0;
  int source_max_vertices = 0;
  int auto_min_vertices = 0;
  int auto_max_vertices = 0;
  int target_vertices = 0;
  int fitted_vertices = 0;
  int estimated_candidate_keys = 0;
  int estimated_original_keys = 0;
  std::size_t frame_count = 0;
  double frame_outline_error = 0.0;
  std::string fraction_coherence_note;
};

std::string BuildReplacementSuccessNote(
    const ReplacementSuccessNoteInput& input);

std::string BuildReplacementHostEquivalentNote(int candidate_key_count,
                                               int source_sample_count);

struct ReplacementFastVertexPreferenceNoteInput {
  double candidate_max_err = 0.0;
  std::string source_validation_notes;
  bool visible_outline_reference = false;
  bool sharp_validation_enabled = false;
  std::string sharp_validation_notes;
  int candidate_key_count = 0;
  int source_sample_count = 0;
  int estimated_candidate_keys = 0;
  int estimated_original_keys = 0;
  int fitted_vertices = 0;
  int source_vertices_for_ratio = 0;
  double vertex_reduction_ratio = 0.0;
};

std::string BuildReplacementFastVertexPreferenceNote(
    const ReplacementFastVertexPreferenceNoteInput& input);

struct ReplacementSourceValidationNoteInput {
  int validation_samples_checked = 0;
  double validation_max_outline_error = 0.0;
  std::string validation_notes;
  bool visible_outline_reference = false;
  bool sharp_validation_enabled = false;
  std::string sharp_validation_notes;
};

std::string BuildReplacementSourceValidationNote(
    const ReplacementSourceValidationNoteInput& input);

ReplacementSourceValidationNoteInput BuildReplacementSourceValidationNoteInput(
    const PathTemporalValidationResult& validation,
    bool visible_outline_reference,
    const SharpCornerValidationResult& sharp_validation);

std::string BuildReplacementRejectedFallbackNote(
    const std::string& decision_note,
    const std::string& source_validation_note,
    int original_temporal_keys);

std::string BuildReplacementAcceptedInitialNote(
    const std::string& source_validation_note,
    const std::string& decision_note);

std::string BuildReplacementRetryFirstNote(double candidate_max_err,
                                           int fitted_vertices);

std::string BuildReplacementRetryFitFailedNote(
    int retry,
    int retry_target,
    const std::string& fit_note);

std::string BuildReplacementRetryNoImprovementNote(int retry,
                                                   int retry_target);

struct ReplacementRetryResultNoteInput {
  int retry = 0;
  int retry_target = 0;
  int retry_vertices = 0;
  double retry_temporal_tolerance = 0.0;
  double retry_temporal_validation_error = 0.0;
  bool visible_outline_reference = false;
  bool sharp_validation_enabled = false;
  std::string sharp_validation_notes;
};

ReplacementRetryResultNoteInput BuildReplacementRetryResultNoteInput(
    int retry,
    int retry_target,
    int retry_vertices,
    double retry_temporal_tolerance,
    double retry_temporal_validation_error,
    bool visible_outline_reference,
    const SharpCornerValidationResult& sharp_validation);

std::string BuildReplacementRetryResultNote(
    const ReplacementRetryResultNoteInput& input);

std::string BuildReplacementRetryAcceptedNote(
    double retry_max_err,
    const std::string& decision_note);

std::string BuildReplacementRetrySkippedNoLadderNote();

std::string BuildReplacementRetrySkippedSharpCornerGateNote(
    int candidate_key_count,
    int original_temporal_keys);

std::string BuildReplacementRetrySkippedKeyGateNote(
    int candidate_key_count,
    int original_temporal_keys);

}  // namespace bbsolver
