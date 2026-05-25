#include "bbsolver/path/replacement/path_replacement_notes.hpp"

#include "bbsolver/path/replacement/path_replacement_feature_layout_trial.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"
#include "bbsolver/shape/sharp_corner_policy.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void RequireContains(const std::string& text, const std::string& needle) {
  if (text.find(needle) == std::string::npos) {
    throw std::runtime_error("missing note token: " + needle);
  }
}

void TestAppliedMedianNotePreservesCounters() {
  bbsolver::ReplacementFractionCoherenceNoteInput input;
  input.fraction_coherence_applied = true;
  input.median_fraction_layout_tried = true;
  input.median_fraction_layout_applied = true;
  input.fraction_seed_count = 3;
  input.best_seed_phase2_idx = -2;
  input.adaptive_insertions = 0;
  input.adaptive_evaluations = 7;
  input.coherent_vertices = 5;
  input.best_fraction_error = 0.125;

  bbsolver::ReplacementFeatureLayoutTrialResult feature;
  const std::string note =
      bbsolver::BuildReplacementFractionCoherenceNote(input, feature);

  Require(note.rfind("applied_median_layout", 0) == 0,
          "median-applied prefix changed");
  RequireContains(note, "; fraction_seeds_tried=3");
  RequireContains(note, "; best_seed_phase2_idx=-2");
  RequireContains(note, "; adaptive_fraction_insertions=0");
  RequireContains(note, "; adaptive_fraction_evaluations=7");
  RequireContains(note, "; adaptive_vertices=5");
  RequireContains(note, "; fraction_max_err=0.125000");
  RequireContains(note, "; median_fraction_layout=applied");
}

void TestSkippedNoFractionsIncludesFeatureAndPhase2Suffixes() {
  bbsolver::ReplacementFractionCoherenceNoteInput input;
  input.seed_indices_empty = true;
  input.phase2_ok = false;
  input.phase2_warning = "target miss";

  bbsolver::ReplacementFeatureLayoutTrialResult feature;
  feature.feature_anchor_count = 4;
  feature.targets_tried = 2;
  feature.target_vertices = 6;
  feature.warning = "feature miss";

  const std::string note =
      bbsolver::BuildReplacementFractionCoherenceNote(input, feature);

  Require(note.rfind("skipped_no_fractions", 0) == 0,
          "skipped-no-fractions prefix changed");
  RequireContains(note, "; feature_fraction_layout=rejected");
  RequireContains(note, "; feature_anchors=4");
  RequireContains(note, "; feature_targets_tried=2");
  RequireContains(note, "; feature_target_vertices=6");
  RequireContains(note, "; feature_layout_warning=feature miss");
  RequireContains(note,
                  "; phase2_target_fit=rejected; phase2_target_warning=target miss");
}

void TestFallbackNoteUsesNoneForInfiniteBestAttempt() {
  bbsolver::ReplacementFractionCoherenceNoteInput input;
  input.fraction_seed_count = 1;
  input.adaptive_evaluations = 0;

  bbsolver::ReplacementFeatureLayoutTrialResult feature;
  const std::string note =
      bbsolver::BuildReplacementFractionCoherenceNote(input, feature);

  Require(note.rfind("fallback_tolerance", 0) == 0,
          "fallback prefix changed");
  RequireContains(note, "; fraction_seeds_tried=1");
  RequireContains(note, "; adaptive_fraction_evaluations=0");
  RequireContains(note, "; best_attempt_err=none");
}

void TestSuccessNotePreservesOrdering() {
  bbsolver::ReplacementSuccessNoteInput input;
  input.source_min_vertices = 5;
  input.source_max_vertices = 8;
  input.auto_min_vertices = 4;
  input.auto_max_vertices = 7;
  input.target_vertices = 6;
  input.fitted_vertices = 6;
  input.estimated_candidate_keys = 12;
  input.estimated_original_keys = 28;
  input.frame_count = 3;
  input.frame_outline_error = 0.25;
  input.fraction_coherence_note = "applied; fraction_seeds_tried=2";

  const std::string note = bbsolver::BuildReplacementSuccessNote(input);

  Require(note ==
              "path_replacement_fit; source_vertices=5-8"
              "; auto_frame_vertices=4-7"
              "; target_vertices=6"
              "; fitted_vertices=6"
              "; fraction_coherence=applied; fraction_seeds_tried=2"
              "; estimated_candidate_keys=12"
              "; estimated_original_keys=28"
              "; frames=3"
              "; frame_outline_error=0.250000",
          "success note ordering changed");
}

void TestHostEquivalentNoteUsesNinetyPercentKeyThreshold() {
  Require(bbsolver::BuildReplacementHostEquivalentNote(1, 2).empty(),
          "two-sample replacement should not be host-equivalent");
  Require(bbsolver::BuildReplacementHostEquivalentNote(8, 10).empty(),
          "less than ninety percent key ratio should not be host-equivalent");

  const std::string note =
      bbsolver::BuildReplacementHostEquivalentNote(9, 10);
  Require(note ==
              "key_reduction_host_equivalent=true"
              "; candidate_keys=9"
              "; source_samples=10",
          "host-equivalent note changed");
}

void TestFastVertexPreferenceNotePreservesTokens() {
  bbsolver::ReplacementFastVertexPreferenceNoteInput input;
  input.candidate_max_err = 0.25;
  input.source_validation_notes = "ok=true; samples_checked=12";
  input.visible_outline_reference = true;
  input.sharp_validation_enabled = true;
  input.sharp_validation_notes = "sharp_corners_ok=true";
  input.candidate_key_count = 9;
  input.source_sample_count = 20;
  input.estimated_candidate_keys = 9;
  input.estimated_original_keys = 12;
  input.fitted_vertices = 10;
  input.source_vertices_for_ratio = 20;
  input.vertex_reduction_ratio = 0.5;

  const std::string note =
      bbsolver::BuildReplacementFastVertexPreferenceNote(input);

  RequireContains(note, "temporal_validation_error=0.250000");
  RequireContains(note,
                  "; source_outline_validation: ok=true; samples_checked=12");
  RequireContains(note, "; source_outline_reference=visible_outline");
  RequireContains(note, "; sharp_corner_reference=visible_outline");
  RequireContains(note, "; sharp_corners_ok=true");
  RequireContains(note,
                  "; path_replacement_accepted_fast_vertex_preference");
  RequireContains(note, "; baseline_temporal_skipped=true");
  RequireContains(note,
                  "; post_solve_vertex_reduction_skipped=already_stable_replacement");
  RequireContains(note, "; candidate_keys=9");
  RequireContains(note, "; source_samples=20");
  RequireContains(note, "; estimated_candidate_keys=9");
  RequireContains(note, "; estimated_original_keys=12");
  RequireContains(note, "; fast_estimated_key_gate=true");
  RequireContains(note, "; candidate_fitted_vertices=10");
  RequireContains(note, "; original_source_vertices=20");
  RequireContains(note, "; vertex_reduction_ratio=0.500000");
}

void TestSourceValidationNotePreservesOutlineAndSharpTokens() {
  bbsolver::ReplacementSourceValidationNoteInput input;
  input.validation_samples_checked = 12;
  input.validation_max_outline_error = 0.375;
  input.validation_notes = "ok=true";
  input.visible_outline_reference = false;
  input.sharp_validation_enabled = true;
  input.sharp_validation_notes = "sharp_corners_ok=true";

  const std::string note =
      bbsolver::BuildReplacementSourceValidationNote(input);

  Require(note ==
              "temporal_validation_error=0.375000"
              "; source_outline_validation: ok=true"
              "; source_outline_reference=source"
              "; sharp_corner_reference=source"
              "; sharp_corners_ok=true",
          "source validation note changed");
}

void TestBuildSourceValidationNoteInputCopiesValidationFields() {
  bbsolver::PathTemporalValidationResult validation;
  validation.samples_checked = 12;
  validation.max_outline_error = 0.375;
  validation.notes = "ok=true";
  bbsolver::SharpCornerValidationResult sharp;
  sharp.enabled = true;
  sharp.notes = "sharp_corners_ok=true";

  const auto input = bbsolver::BuildReplacementSourceValidationNoteInput(
      validation, true, sharp);
  Require(input.validation_samples_checked == 12,
          "validation sample count changed");
  Require(input.validation_max_outline_error == 0.375,
          "validation max error changed");
  Require(input.validation_notes == "ok=true",
          "validation notes changed");
  Require(input.visible_outline_reference,
          "visible outline flag changed");
  Require(input.sharp_validation_enabled,
          "sharp validation enabled flag changed");
  Require(input.sharp_validation_notes == "sharp_corners_ok=true",
          "sharp validation notes changed");
}

void TestSourceValidationNoteFallsBackToSharpOnly() {
  bbsolver::ReplacementSourceValidationNoteInput input;
  input.visible_outline_reference = true;
  input.sharp_validation_enabled = true;
  input.sharp_validation_notes = "sharp_corners_ok=false";

  const std::string note =
      bbsolver::BuildReplacementSourceValidationNote(input);

  Require(note ==
              "sharp_corner_reference=visible_outline"
              "; sharp_corners_ok=false",
          "sharp-only validation note changed");
}

void TestSourceValidationNoteCanBeEmpty() {
  bbsolver::ReplacementSourceValidationNoteInput input;
  Require(bbsolver::BuildReplacementSourceValidationNote(input).empty(),
          "source validation note should be empty without outline or sharp data");
}

void TestRejectedFallbackNoteCombinesDecisionValidationAndFallback() {
  const std::string note = bbsolver::BuildReplacementRejectedFallbackNote(
      "candidate_rejected",
      "temporal_validation_error=0.500000",
      42);

  Require(note ==
              "candidate_rejected"
              "; temporal_validation_error=0.500000"
              "; replacement_rejected; temporal_fallback_used"
              "; original_temporal_keys=42",
          "rejected fallback note changed");
}

void TestRejectedFallbackNoteAlwaysIncludesFallback() {
  const std::string note =
      bbsolver::BuildReplacementRejectedFallbackNote("", "", 7);

  Require(note ==
              "replacement_rejected; temporal_fallback_used"
              "; original_temporal_keys=7",
          "fallback-only note changed");
}

void TestAcceptedInitialNoteCombinesValidationAndDecision() {
  const std::string note = bbsolver::BuildReplacementAcceptedInitialNote(
      "temporal_validation_error=0.125000",
      "path_replacement_accepted");

  Require(note ==
              "temporal_validation_error=0.125000"
              "; path_replacement_accepted",
          "accepted initial note changed");
}

void TestAcceptedInitialNoteCanBeEmpty() {
  Require(bbsolver::BuildReplacementAcceptedInitialNote("", "").empty(),
          "accepted initial note should be empty without inputs");
}

void TestReplacementRetryFirstNotePreservesTokens() {
  const std::string note =
      bbsolver::BuildReplacementRetryFirstNote(0.75, 9);

  Require(note ==
              "replacement_retry_first_err=0.750000"
              "; replacement_retry_first_vertices=9",
          "replacement retry first note changed");
}

void TestReplacementRetryFitFailureNotePreservesTokens() {
  const std::string note =
      bbsolver::BuildReplacementRetryFitFailedNote(2, 14, "fit detail");

  Require(note ==
              "replacement_retry_2=fit_failed"
              "; retry_target=14"
              "; retry_fit_note=fit detail",
          "replacement retry fit failure note changed");
}

void TestReplacementRetryNoImprovementNotePreservesTokens() {
  const std::string note =
      bbsolver::BuildReplacementRetryNoImprovementNote(3, 16);

  Require(note ==
              "replacement_retry_3=skipped_no_improvement"
              "; retry_target=16",
          "replacement retry no-improvement note changed");
}

void TestBuildReplacementRetryResultNoteInputCopiesValidationFields() {
  bbsolver::SharpCornerValidationResult sharp;
  sharp.enabled = true;
  sharp.notes = "sharp_corners_ok=true";

  const auto input = bbsolver::BuildReplacementRetryResultNoteInput(
      4, 18, 17, 1.25, 0.5, true, sharp);

  Require(input.retry == 4, "retry index changed");
  Require(input.retry_target == 18, "retry target changed");
  Require(input.retry_vertices == 17, "retry vertices changed");
  Require(input.retry_temporal_tolerance == 1.25,
          "retry temporal tolerance changed");
  Require(input.retry_temporal_validation_error == 0.5,
          "retry temporal validation error changed");
  Require(input.visible_outline_reference,
          "retry visible-outline reference changed");
  Require(input.sharp_validation_enabled,
          "retry sharp validation enabled flag changed");
  Require(input.sharp_validation_notes == "sharp_corners_ok=true",
          "retry sharp validation notes changed");
}

void TestReplacementRetryResultNotePreservesTokens() {
  bbsolver::ReplacementRetryResultNoteInput input;
  input.retry = 4;
  input.retry_target = 18;
  input.retry_vertices = 17;
  input.retry_temporal_tolerance = 1.25;
  input.retry_temporal_validation_error = 0.5;
  input.visible_outline_reference = true;
  input.sharp_validation_enabled = true;
  input.sharp_validation_notes = "sharp_corners_ok=true";

  const std::string note =
      bbsolver::BuildReplacementRetryResultNote(input);

  Require(note ==
              "replacement_retry_4"
              "; retry_target=18"
              "; retry_vertices=17"
              "; retry_temporal_tolerance=1.250000"
              "; retry_temporal_validation_error=0.500000"
              "; retry_sharp_corner_reference=visible_outline"
              "; sharp_corners_ok=true",
          "replacement retry result note changed");
}

void TestReplacementRetryAcceptedNotePreservesTokens() {
  const std::string note =
      bbsolver::BuildReplacementRetryAcceptedNote(0.25,
                                                 "path_replacement_accepted");

  Require(note ==
              "temporal_validation_error=0.250000"
              "; path_replacement_accepted"
              "; replacement_retry_accepted",
          "replacement retry accepted note changed");
}

void TestReplacementRetrySkippedNotesPreserveTokens() {
  Require(bbsolver::BuildReplacementRetrySkippedNoLadderNote() ==
              "replacement_retry=skipped_no_ladder",
          "replacement retry no-ladder note changed");
  Require(bbsolver::BuildReplacementRetrySkippedSharpCornerGateNote(8, 12) ==
              "replacement_retry=skipped_sharp_corner_gate"
              "; replacement_candidate_keys=8"
              "; original_temporal_keys=12",
          "replacement retry sharp-corner skip note changed");
  Require(bbsolver::BuildReplacementRetrySkippedKeyGateNote(13, 12) ==
              "replacement_retry=skipped_key_gate"
              "; replacement_candidate_keys=13"
              "; original_temporal_keys=12",
          "replacement retry key-gate skip note changed");
}

}  // namespace

int main() {
  TestAppliedMedianNotePreservesCounters();
  TestSkippedNoFractionsIncludesFeatureAndPhase2Suffixes();
  TestFallbackNoteUsesNoneForInfiniteBestAttempt();
  TestSuccessNotePreservesOrdering();
  TestHostEquivalentNoteUsesNinetyPercentKeyThreshold();
  TestFastVertexPreferenceNotePreservesTokens();
  TestSourceValidationNotePreservesOutlineAndSharpTokens();
  TestBuildSourceValidationNoteInputCopiesValidationFields();
  TestSourceValidationNoteFallsBackToSharpOnly();
  TestSourceValidationNoteCanBeEmpty();
  TestRejectedFallbackNoteCombinesDecisionValidationAndFallback();
  TestRejectedFallbackNoteAlwaysIncludesFallback();
  TestAcceptedInitialNoteCombinesValidationAndDecision();
  TestAcceptedInitialNoteCanBeEmpty();
  TestReplacementRetryFirstNotePreservesTokens();
  TestReplacementRetryFitFailureNotePreservesTokens();
  TestReplacementRetryNoImprovementNotePreservesTokens();
  TestBuildReplacementRetryResultNoteInputCopiesValidationFields();
  TestReplacementRetryResultNotePreservesTokens();
  TestReplacementRetryAcceptedNotePreservesTokens();
  TestReplacementRetrySkippedNotesPreserveTokens();
  std::cout << "[PASS] test_path_replacement_notes\n";
  return 0;
}
