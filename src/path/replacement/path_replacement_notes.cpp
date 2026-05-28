#include "bbsolver/path/replacement/path_replacement_notes.hpp"

#include <limits>
#include <string>

#include "bbsolver/path/temporal/path_temporal_validation.hpp"
#include "bbsolver/path/replacement/path_replacement_feature_layout_trial.hpp"
#include "bbsolver/shape/sharp_corner_policy.hpp"

namespace bbsolver {
namespace {

std::string BestAttemptErrorText(double best_attempt_error) {
  return best_attempt_error < std::numeric_limits<double>::infinity()
             ? std::to_string(best_attempt_error)
: "none";
}

std::string BuildFeatureAnchorSuffix(
    const ReplacementFeatureLayoutTrialResult& feature_layout_trial) {
  if (feature_layout_trial.feature_anchor_count <= 0 &&
      feature_layout_trial.warning.empty()) {
    return {};
  }
  return "; feature_fraction_layout=" +
         std::string(feature_layout_trial.applied ? "applied_feature_layout"
: "rejected") +
         "; feature_anchors=" +
         std::to_string(feature_layout_trial.feature_anchor_count) +
         "; feature_targets_tried=" +
         std::to_string(feature_layout_trial.targets_tried) +
         (feature_layout_trial.target_vertices > 0
              ? "; feature_target_vertices=" +
                    std::to_string(feature_layout_trial.target_vertices)
: std::string{}) +
         (feature_layout_trial.warning.empty()
              ? std::string{}
: "; feature_layout_warning=" + feature_layout_trial.warning);
}

std::string BuildMedianLayoutSuffix(
    const ReplacementFractionCoherenceNoteInput& input) {
  if (!input.median_fraction_layout_tried) {
    return {};
  }
  return "; median_fraction_layout=" +
         std::string(input.median_fraction_layout_applied ? "applied"
: "rejected");
}

std::string BuildPhase2Suffix(
    const ReplacementFractionCoherenceNoteInput& input) {
  return input.phase2_ok
             ? std::string{}
: "; phase2_target_fit=rejected; phase2_target_warning=" +
                   input.phase2_warning;
}

void AppendNotePart(std::string& combined, const std::string& note) {
  if (note.empty()) {
    return;
  }
  if (combined.empty()) {
    combined = note;
  } else {
    combined += "; " + note;
  }
}

}  // namespace

std::string BuildReplacementFractionCoherenceNote(
    const ReplacementFractionCoherenceNoteInput& input,
    const ReplacementFeatureLayoutTrialResult& feature_layout_trial) {
  const std::string feature_anchor_suffix =
      BuildFeatureAnchorSuffix(feature_layout_trial);
  const std::string median_layout_suffix = BuildMedianLayoutSuffix(input);
  const std::string phase2_suffix = BuildPhase2Suffix(input);

  if (input.seed_indices_empty && !feature_layout_trial.applied) {
    return "skipped_no_fractions" + median_layout_suffix +
           feature_anchor_suffix + phase2_suffix;
  }
  if (input.fraction_coherence_applied) {
    return (feature_layout_trial.applied ? "applied_feature_layout"
: input.median_fraction_layout_applied
                                               ? "applied_median_layout"
: "applied") +
           std::string("; fraction_seeds_tried=") +
           std::to_string(input.fraction_seed_count) +
           "; best_seed_phase2_idx=" +
           std::to_string(input.best_seed_phase2_idx) +
           "; adaptive_fraction_insertions=" +
           std::to_string(input.adaptive_insertions) +
           "; adaptive_fraction_evaluations=" +
           std::to_string(input.adaptive_evaluations) +
           "; adaptive_vertices=" + std::to_string(input.coherent_vertices) +
           "; fraction_max_err=" +
           std::to_string(input.best_fraction_error) + median_layout_suffix +
           feature_anchor_suffix + phase2_suffix;
  }
  return "fallback_tolerance; fraction_seeds_tried=" +
         std::to_string(input.fraction_seed_count) +
         "; adaptive_fraction_evaluations=" +
         std::to_string(input.adaptive_evaluations) + "; best_attempt_err=" +
         BestAttemptErrorText(input.best_attempt_error) +
         median_layout_suffix + feature_anchor_suffix + phase2_suffix;
}

std::string BuildReplacementSuccessNote(
    const ReplacementSuccessNoteInput& input) {
  return "path_replacement_fit; source_vertices=" +
         (input.source_min_vertices == input.source_max_vertices
              ? std::to_string(input.source_min_vertices)
: std::to_string(input.source_min_vertices) + "-" +
                    std::to_string(input.source_max_vertices)) +
         "; auto_frame_vertices=" + std::to_string(input.auto_min_vertices) +
         "-" + std::to_string(input.auto_max_vertices) +
         "; target_vertices=" + std::to_string(input.target_vertices) +
         "; fitted_vertices=" + std::to_string(input.fitted_vertices) +
         "; fraction_coherence=" + input.fraction_coherence_note +
         "; estimated_candidate_keys=" +
         std::to_string(input.estimated_candidate_keys) +
         "; estimated_original_keys=" +
         std::to_string(input.estimated_original_keys) +
         "; frames=" + std::to_string(input.frame_count) +
         "; frame_outline_error=" +
         std::to_string(input.frame_outline_error);
}

std::string BuildReplacementHostEquivalentNote(int candidate_key_count,
                                               int source_sample_count) {
  if (source_sample_count <= 2 ||
      candidate_key_count * 10 < source_sample_count * 9) {
    return {};
  }
  return "key_reduction_host_equivalent=true"
         "; candidate_keys=" +
         std::to_string(candidate_key_count) +
         "; source_samples=" + std::to_string(source_sample_count);
}

std::string BuildReplacementFastVertexPreferenceNote(
    const ReplacementFastVertexPreferenceNoteInput& input) {
  return "temporal_validation_error=" +
         std::to_string(input.candidate_max_err) +
         "; source_outline_validation: " + input.source_validation_notes +
         "; source_outline_reference=" +
         std::string(input.visible_outline_reference ? "visible_outline"
: "source") +
         "; sharp_corner_reference=" +
         std::string(input.visible_outline_reference ? "visible_outline"
: "source") +
         (input.sharp_validation_enabled
              ? "; " + input.sharp_validation_notes
: std::string{}) +
         "; path_replacement_accepted_fast_vertex_preference"
         "; baseline_temporal_skipped=true"
         "; post_solve_vertex_reduction_skipped=already_stable_replacement"
         "; candidate_keys=" +
         std::to_string(input.candidate_key_count) +
         "; source_samples=" + std::to_string(input.source_sample_count) +
         "; estimated_candidate_keys=" +
         std::to_string(input.estimated_candidate_keys) +
         "; estimated_original_keys=" +
         std::to_string(input.estimated_original_keys) +
         "; fast_estimated_key_gate=true"
         "; candidate_fitted_vertices=" +
         std::to_string(input.fitted_vertices) +
         "; original_source_vertices=" +
         std::to_string(input.source_vertices_for_ratio) +
         "; vertex_reduction_ratio=" +
         std::to_string(input.vertex_reduction_ratio);
}

std::string BuildReplacementSourceValidationNote(
    const ReplacementSourceValidationNoteInput& input) {
  const std::string reference =
      input.visible_outline_reference ? "visible_outline": "source";
  if (input.validation_samples_checked > 0) {
    return "temporal_validation_error=" +
           std::to_string(input.validation_max_outline_error) +
           "; source_outline_validation: " + input.validation_notes +
           "; source_outline_reference=" + reference +
           "; sharp_corner_reference=" + reference +
           (input.sharp_validation_enabled
                ? "; " + input.sharp_validation_notes
: std::string{});
  }
  if (!input.sharp_validation_enabled) {
    return {};
  }
  return "sharp_corner_reference=" + reference + "; " +
         input.sharp_validation_notes;
}

ReplacementSourceValidationNoteInput BuildReplacementSourceValidationNoteInput(
    const PathTemporalValidationResult& validation,
    bool visible_outline_reference,
    const SharpCornerValidationResult& sharp_validation) {
  ReplacementSourceValidationNoteInput input;
  input.validation_samples_checked = validation.samples_checked;
  input.validation_max_outline_error = validation.max_outline_error;
  input.validation_notes = validation.notes;
  input.visible_outline_reference = visible_outline_reference;
  input.sharp_validation_enabled = sharp_validation.enabled;
  input.sharp_validation_notes = sharp_validation.notes;
  return input;
}

std::string BuildReplacementRejectedFallbackNote(
    const std::string& decision_note,
    const std::string& source_validation_note,
    int original_temporal_keys) {
  std::string combined = decision_note;
  AppendNotePart(combined, source_validation_note);
  AppendNotePart(
      combined,
      "replacement_rejected; temporal_fallback_used; original_temporal_keys=" +
          std::to_string(original_temporal_keys));
  return combined;
}

std::string BuildReplacementAcceptedInitialNote(
    const std::string& source_validation_note,
    const std::string& decision_note) {
  std::string combined = source_validation_note;
  AppendNotePart(combined, decision_note);
  return combined;
}

std::string BuildReplacementRetryFirstNote(double candidate_max_err,
                                           int fitted_vertices) {
  return "replacement_retry_first_err=" + std::to_string(candidate_max_err) +
         "; replacement_retry_first_vertices=" +
         std::to_string(fitted_vertices);
}

std::string BuildReplacementRetryFitFailedNote(
    int retry,
    int retry_target,
    const std::string& fit_note) {
  return "replacement_retry_" + std::to_string(retry) +
         "=fit_failed; retry_target=" + std::to_string(retry_target) +
         "; retry_fit_note=" + fit_note;
}

std::string BuildReplacementRetryNoImprovementNote(int retry,
                                                   int retry_target) {
  return "replacement_retry_" + std::to_string(retry) +
         "=skipped_no_improvement; retry_target=" +
         std::to_string(retry_target);
}

ReplacementRetryResultNoteInput BuildReplacementRetryResultNoteInput(
    int retry,
    int retry_target,
    int retry_vertices,
    double retry_temporal_tolerance,
    double retry_temporal_validation_error,
    bool visible_outline_reference,
    const SharpCornerValidationResult& sharp_validation) {
  ReplacementRetryResultNoteInput input;
  input.retry = retry;
  input.retry_target = retry_target;
  input.retry_vertices = retry_vertices;
  input.retry_temporal_tolerance = retry_temporal_tolerance;
  input.retry_temporal_validation_error = retry_temporal_validation_error;
  input.visible_outline_reference = visible_outline_reference;
  input.sharp_validation_enabled = sharp_validation.enabled;
  input.sharp_validation_notes = sharp_validation.notes;
  return input;
}

std::string BuildReplacementRetryResultNote(
    const ReplacementRetryResultNoteInput& input) {
  return "replacement_retry_" + std::to_string(input.retry) +
         "; retry_target=" + std::to_string(input.retry_target) +
         "; retry_vertices=" + std::to_string(input.retry_vertices) +
         "; retry_temporal_tolerance=" +
         std::to_string(input.retry_temporal_tolerance) +
         "; retry_temporal_validation_error=" +
         std::to_string(input.retry_temporal_validation_error) +
         "; retry_sharp_corner_reference=" +
         std::string(input.visible_outline_reference ? "visible_outline"
: "source") +
         (input.sharp_validation_enabled
              ? "; " + input.sharp_validation_notes
: std::string{});
}

std::string BuildReplacementRetryAcceptedNote(
    double retry_max_err,
    const std::string& decision_note) {
  return "temporal_validation_error=" + std::to_string(retry_max_err) +
         "; " + decision_note + "; replacement_retry_accepted";
}

std::string BuildReplacementRetrySkippedNoLadderNote() {
  return "replacement_retry=skipped_no_ladder";
}

std::string BuildReplacementRetrySkippedSharpCornerGateNote(
    int candidate_key_count,
    int original_temporal_keys) {
  return "replacement_retry=skipped_sharp_corner_gate"
         "; replacement_candidate_keys=" +
         std::to_string(candidate_key_count) +
         "; original_temporal_keys=" +
         std::to_string(original_temporal_keys);
}

std::string BuildReplacementRetrySkippedKeyGateNote(
    int candidate_key_count,
    int original_temporal_keys) {
  return "replacement_retry=skipped_key_gate; replacement_candidate_keys=" +
         std::to_string(candidate_key_count) +
         "; original_temporal_keys=" +
         std::to_string(original_temporal_keys);
}

}  // namespace bbsolver
