#include "bbsolver/path/bridge_prune/path_bridge_prune.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/path/bridge_prune/path_bridge_prune_batch.hpp"
#include "bbsolver/path/bridge_prune/path_bridge_prune_batch_attempt.hpp"
#include "bbsolver/path/bridge_prune/path_bridge_prune_candidate.hpp"
#include "bbsolver/path/bridge_prune/path_bridge_prune_notes.hpp"
#include "bbsolver/path/bridge_prune/path_bridge_prune_plan.hpp"
#include "bbsolver/path/bridge_prune/path_bridge_prune_progress.hpp"
#include "bbsolver/path/bridge_prune/path_bridge_prune_result.hpp"
#include "bbsolver/path/bridge_prune/path_bridge_prune_round.hpp"
#include "bbsolver/path/bridge_prune/path_bridge_prune_selection.hpp"
#include "bbsolver/shape/shape_flat_topology.hpp"

#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <vector>
#include <cstddef>
#include "bbsolver/metrics/error_metrics.hpp"
#include "nlohmann/json_fwd.hpp"
#include "bbsolver/path/reduction/path_vertex_reduction.hpp"
#include <nlohmann/json.hpp>

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    std::abort();
  }
}

std::vector<double> Flat(int vertices, bool closed = true) {
  std::vector<bbsolver::ShapeFlatVertex> shape_vertices;
  shape_vertices.reserve(static_cast<std::size_t>(vertices));
  for (int i = 0; i < vertices; ++i) {
    shape_vertices.push_back({
        static_cast<double>(i),
        static_cast<double>(i % 2),
        0.0,
        0.0,
        0.0,
        0.0,
    });
  }
  return bbsolver::ShapeFlatFromVertices(shape_vertices, closed);
}

bbsolver::PropertyKeys KeysWithFlat(const std::vector<double>& flat) {
  bbsolver::PropertyKeys keys;
  bbsolver::Key key;
  key.v = flat;
  keys.keys.push_back(key);
  keys.converged = true;
  keys.max_err = 0.25;
  keys.max_err_screen_px = 0.25;
  return keys;
}

bbsolver::PropertySamples BridgePrunePropertySamples() {
  bbsolver::PropertySamples samples;
  samples.property.id = "shape/path";
  samples.property.display_name = "Path";
  return samples;
}

void TestPostSolvePathVertexReductionResultDefaults() {
  const bbsolver::PostSolvePathVertexReductionResult result;
  Require(!result.accepted,
          "post-solve bridge-prune result must default accepted=false");
  Require(!result.attempted,
          "post-solve bridge-prune result must default attempted=false");
  Require(result.keys.keys.empty(),
          "post-solve bridge-prune result must default to empty keys");
  Require(result.notes.empty(),
          "post-solve bridge-prune result must default to empty notes");
  Require(result.source_vertices == 0 && result.fitted_vertices == 0,
          "post-solve bridge-prune result must default vertex counts to zero");
  Require(result.max_outline_error == 0.0,
          "post-solve bridge-prune result must default outline error to zero");
}

void TestBridgePruneDisabledEarlyExitCopiesKeys() {
  const bbsolver::PropertySamples original;
  const bbsolver::PropertyKeys keys = KeysWithFlat(Flat(5));
  bbsolver::SolverConfig config;
  config.path_replacement_prefer_vertices = false;
  const bbsolver::CompInfo comp;

  const bbsolver::PostSolvePathVertexReductionResult result =
      bbsolver::TryPostTemporalBridgePrune(original, keys, config, comp);
  Require(!result.accepted && !result.attempted,
          "disabled bridge prune must not accept or mark attempted");
  Require(result.keys.keys.size() == keys.keys.size(),
          "disabled bridge prune must copy input keys");
  Require(result.source_vertices == 5 && result.fitted_vertices == 5,
          "disabled bridge prune must report the initial max vertex count");
  Require(result.notes ==
              "post_solve_vertex_reduction_skipped: "
              "post_temporal_bridge_prune_disabled",
          "disabled bridge prune must preserve the exact skipped note");
}

void TestBridgePruneAtMinVerticesEarlyExit() {
  const bbsolver::PropertySamples original;
  const bbsolver::PropertyKeys keys = KeysWithFlat(Flat(4));
  bbsolver::SolverConfig config;
  config.path_replacement_prefer_vertices = true;
  config.path_replacement_min_vertices = 4;
  const bbsolver::CompInfo comp;

  const bbsolver::PostSolvePathVertexReductionResult result =
      bbsolver::TryPostTemporalBridgePrune(original, keys, config, comp);
  Require(!result.accepted && !result.attempted,
          "at-min bridge prune must not accept or mark attempted");
  Require(result.source_vertices == 4 && result.fitted_vertices == 4,
          "at-min bridge prune must report the initial max vertex count");
  Require(result.notes ==
              "post_solve_vertex_reduction_skipped: "
              "source_already_at_min_vertices",
          "at-min bridge prune must preserve the exact skipped note");
}

void TestBridgePruneCancelSentinelBeforeFirstPass() {
  const bbsolver::PropertySamples original;
  const bbsolver::PropertyKeys keys = KeysWithFlat(Flat(5));
  bbsolver::SolverConfig config;
  config.path_replacement_prefer_vertices = true;
  config.path_replacement_min_vertices = 4;
  const bbsolver::CompInfo comp;

  const bbsolver::PostSolvePathVertexReductionResult result =
      bbsolver::TryPostTemporalBridgePrune(
          original,
          keys,
          config,
          comp,
          nullptr,
          0,
          1,
          0,
          [] { return true; });
  Require(result.notes == "cancelled",
          "bridge prune must preserve the cancellation sentinel note");
}

void TestBridgePruneKeyValidationPropertyTolerance() {
  bbsolver::SolverConfig config;
  config.tolerance = 1.0;
  bbsolver::ErrorReport report;
  report.max_err = 1.0 + 0.5e-9;
  report.max_err_screen_px = 100.0;
  Require(bbsolver::PassesBridgePruneKeyValidation(report, config),
          "bridge-prune validation must tolerate property epsilon");

  report.max_err = 1.0 + 2e-9;
  Require(!bbsolver::PassesBridgePruneKeyValidation(report, config),
          "bridge-prune validation must reject property errors above epsilon");
}

void TestBridgePruneKeyValidationScreenBranches() {
  bbsolver::SolverConfig config;
  config.tolerance = 1.0;
  bbsolver::ErrorReport report;
  report.max_err = 0.5;
  report.max_err_screen_px = 10.0;
  Require(bbsolver::PassesBridgePruneKeyValidation(report, config),
          "bridge-prune validation must ignore screen error when disabled");

  config.weight_screen = 1.0;
  Require(!bbsolver::PassesBridgePruneKeyValidation(report, config),
          "weighted screen validation must use property tolerance fallback");

  config.weight_screen = 0.0;
  config.tolerance_screen_px = 12.0;
  Require(bbsolver::PassesBridgePruneKeyValidation(report, config),
          "positive screen tolerance must allow screen errors within tolerance");
  config.tolerance_screen_px = 8.0;
  Require(!bbsolver::PassesBridgePruneKeyValidation(report, config),
          "positive screen tolerance must reject screen errors above tolerance");
}

void TestBridgePruneCandidateFitFailureNote() {
  const bbsolver::PropertySamples original;
  const bbsolver::PropertyKeys current = KeysWithFlat(Flat(3));
  bbsolver::SolverConfig config;
  const bbsolver::CompInfo comp;

  const bbsolver::BridgePruneCandidateEvaluation evaluation =
      bbsolver::EvaluateBridgePruneCandidate(
          original, current, config, comp, 5, 2, true);
  Require(!evaluation.accepted && !evaluation.fit_ok,
          "bridge-prune candidate helper must report fit failure");
  Require(evaluation.result_vertices == 4 && evaluation.removed_index == 2,
          "bridge-prune candidate helper must preserve candidate identity");
  Require(evaluation.failure_note == "5v_remove_2:bridge_fit_failed",
          "bridge-prune candidate helper must preserve fit failure note");
}

void TestBridgePruneBatchCandidateFitFailureNote() {
  const bbsolver::PropertySamples original;
  const bbsolver::PropertyKeys current = KeysWithFlat(Flat(3));
  bbsolver::SolverConfig config;
  const bbsolver::CompInfo comp;

  const bbsolver::BridgePruneCandidateEvaluation evaluation =
      bbsolver::EvaluateBridgePruneBatchCandidate(
          original, current, config, comp, 5, 2, true);
  Require(!evaluation.accepted && !evaluation.fit_ok,
          "bridge-prune batch helper must report fit failure");
  Require(evaluation.result_vertices == 4 && evaluation.removed_index == 2,
          "bridge-prune batch helper must preserve candidate identity");
  Require(evaluation.failure_note == "5v_batch_remove_2:bridge_fit_failed",
          "bridge-prune batch helper must preserve fit failure note");
}

void TestBridgePruneNoteHelpersSkippedNotes() {
  Require(bbsolver::BridgePruneDisabledNote() ==
              "post_solve_vertex_reduction_skipped: "
              "post_temporal_bridge_prune_disabled",
          "bridge-prune disabled note helper must preserve exact text");
  Require(bbsolver::BridgePruneAtMinVerticesNote() ==
              "post_solve_vertex_reduction_skipped: "
              "source_already_at_min_vertices",
          "bridge-prune at-min note helper must preserve exact text");
}

void TestBridgePruneAcceptedNoteHelper() {
  const std::string note = bbsolver::BuildBridgePruneAcceptedNote(
      10, 8, 3, 2, 2, 4, 1, 9, 0.125, 1, 2, 3, 4, 5, true,
      {"10->9@idx2/keys3", "9->8@idx4/keys3/batch"});
  Require(note ==
              "post_solve_vertex_reduction_accepted"
              "; mode=post_temporal_bridge_prune"
              "; source_vertices=10"
              "; fitted_vertices=8"
              "; keys=3"
              "; pruned_vertices=2"
              "; bridge_prune_passes=2"
              "; bridge_prune_rounds=4"
              "; bridge_prune_batch_vertices=1"
              "; attempts=9"
              "; temporal_validation_error=0.125000"
              "; protected_corner_skips=1"
              "; bridge_prune_fit_failures=2"
              "; bridge_prune_validation_failures=3"
              "; bridge_prune_sharp_failures=4"
              "; bridge_prune_accepted_candidates=5"
              "; sharp_corner_preserve=on"
              "; bridge_prune_steps=10->9@idx2/keys3 | "
              "9->8@idx4/keys3/batch",
          "bridge-prune accepted note helper must preserve exact telemetry");
}

void TestBridgePruneRejectedNoteHelper() {
  const std::string with_failures = bbsolver::BuildBridgePruneRejectedNote(
      10, 3, 9, 1, 2, 3, 4, 5, true, {"fit", "", "sharp"});
  Require(with_failures ==
              "post_solve_vertex_reduction_rejected"
              "; mode=post_temporal_bridge_prune"
              "; source_vertices=10"
              "; keys=3"
              "; attempts=9"
              "; protected_corner_skips=1"
              "; bridge_prune_fit_failures=2"
              "; bridge_prune_validation_failures=3"
              "; bridge_prune_sharp_failures=4"
              "; bridge_prune_accepted_candidates=5"
              "; sharp_corner_preserve=on"
              "; failures=fit | sharp",
          "bridge-prune rejected note helper must preserve failures suffix");

  const std::string without_failures = bbsolver::BuildBridgePruneRejectedNote(
      10, 3, 9, 1, 2, 3, 4, 5, false, {});
  Require(without_failures ==
              "post_solve_vertex_reduction_rejected"
              "; mode=post_temporal_bridge_prune"
              "; source_vertices=10"
              "; keys=3"
              "; attempts=9"
              "; protected_corner_skips=1"
              "; bridge_prune_fit_failures=2"
              "; bridge_prune_validation_failures=3"
              "; bridge_prune_sharp_failures=4"
              "; bridge_prune_accepted_candidates=5",
          "bridge-prune rejected note helper must omit optional suffixes");
}

void TestBridgePruneAcceptedResultHelper() {
  bbsolver::BridgePruneResultSummary summary;
  summary.reported_source_vertices = 8;
  summary.pruned_vertices = 2;
  summary.passes = 2;
  summary.candidate_rounds = 3;
  summary.batch_pruned_vertices = 1;
  summary.attempts = 7;
  summary.protected_corner_skips = 1;
  summary.best_error = 0.125;
  summary.preserve_sharp_corners = true;
  summary.outcomes.fit_failures = 2;
  summary.outcomes.validation_failures = 3;
  summary.outcomes.sharp_failures = 4;
  summary.outcomes.accepted_candidates = 5;
  summary.accepted_steps = {"8->7@idx2/keys1", "7->6@idx3/keys1/batch"};

  const bbsolver::PostSolvePathVertexReductionResult result =
      bbsolver::BuildBridgePruneAcceptedResult(
          KeysWithFlat(Flat(6)), summary);
  Require(result.accepted && result.attempted,
          "accepted result helper must set accepted and attempted");
  Require(result.source_vertices == 8 && result.fitted_vertices == 6,
          "accepted result helper must preserve source and fitted counts");
  Require(result.keys.keys.size() == 1,
          "accepted result helper must preserve output keys");
  Require(result.max_outline_error == 0.125,
          "accepted result helper must preserve max outline error");
  Require(result.notes.find("post_solve_vertex_reduction_accepted") !=
              std::string::npos,
          "accepted result helper must preserve accepted note prefix");
  Require(result.notes.find("bridge_prune_steps=8->7@idx2/keys1 | "
                            "7->6@idx3/keys1/batch") != std::string::npos,
          "accepted result helper must preserve accepted step notes");
}

void TestBridgePruneRejectedResultNoteHelper() {
  bbsolver::BridgePruneResultSummary summary;
  summary.reported_source_vertices = 9;
  summary.attempts = 4;
  summary.protected_corner_skips = 2;
  summary.preserve_sharp_corners = true;
  summary.outcomes.fit_failures = 1;
  summary.outcomes.validation_failures = 2;
  summary.outcomes.sharp_failures = 3;
  summary.outcomes.accepted_candidates = 4;
  summary.outcomes.failures = {"9v_remove_2:bridge_fit_failed"};

  const std::string note = bbsolver::BuildBridgePruneRejectedResultNote(
      KeysWithFlat(Flat(9)), summary);
  Require(note.find("post_solve_vertex_reduction_rejected") !=
              std::string::npos,
          "rejected result helper must preserve rejected note prefix");
  Require(note.find("source_vertices=9") != std::string::npos,
          "rejected result helper must preserve source vertex count");
  Require(note.find("keys=1") != std::string::npos,
          "rejected result helper must preserve key count");
  Require(note.find("failures=9v_remove_2:bridge_fit_failed") !=
              std::string::npos,
          "rejected result helper must preserve failure notes");
}

void TestBridgePruneRemovalPlanWithoutSemanticAnchors() {
  bbsolver::SolverConfig config;
  config.path_preserve_sharp_corners = true;
  const bbsolver::PropertyKeys keys = KeysWithFlat(Flat(5));
  const bbsolver::BridgePruneRemovalPlan plan =
      bbsolver::BuildBridgePruneRemovalPlan(keys, 5, config, false);
  Require(plan.removed_indices == std::vector<int>({1, 2, 3, 4}),
          "bridge-prune removal plan must include all removable indexes "
          "when semantic anchors are disabled");
  Require(plan.protected_corner_skips == 0 && !plan.attempted,
          "bridge-prune removal plan must not mark protected skips without "
          "semantic anchors");
  Require(!bbsolver::BridgePruneIndexIsProtected(keys, 5, 1, config, false),
          "bridge-prune protected helper must honor semantic-anchor disable");
}

void TestBridgePruneShiftedIndexMathAndRange() {
  Require(bbsolver::ShiftBridgePruneRemovedIndex(5, {2, 4}) == 3,
          "bridge-prune shifted index must decrement for prior lower removals");
  Require(bbsolver::ShiftBridgePruneRemovedIndex(2, {2, 4}) == 2,
          "bridge-prune shifted index must not decrement for equal removals");
  Require(!bbsolver::BridgePruneShiftedIndexInRange(0, 5),
          "bridge-prune shifted index must reject endpoint zero");
  Require(!bbsolver::BridgePruneShiftedIndexInRange(5, 5),
          "bridge-prune shifted index must reject endpoint count");
  Require(bbsolver::BridgePruneShiftedIndexInRange(4, 5),
          "bridge-prune shifted index must accept interior indexes");
}

void TestBridgePruneAcceptedStepNoteHelper() {
  Require(bbsolver::BridgePruneAcceptedStepNote(8, 7, 3, 4, false) ==
              "8->7@idx3/keys4",
          "bridge-prune accepted step note must preserve non-batch format");
  Require(bbsolver::BridgePruneAcceptedStepNote(7, 6, 2, 5, true) ==
              "7->6@idx2/keys5/batch",
          "bridge-prune accepted step note must preserve batch suffix");
}

void TestBridgePruneOutcomeStatsRecordingAndLimit() {
  bbsolver::BridgePruneOutcomeStats stats;
  bbsolver::BridgePruneCandidateEvaluation fit_failed;
  fit_failed.failure_note = "fit";
  bbsolver::RecordBridgePruneEvaluationOutcome(&stats, fit_failed, 2);

  bbsolver::BridgePruneCandidateEvaluation validation_failed;
  validation_failed.fit_ok = true;
  validation_failed.failure_note = "validation";
  bbsolver::RecordBridgePruneEvaluationOutcome(
      &stats, validation_failed, 2);

  bbsolver::BridgePruneCandidateEvaluation sharp_failed;
  sharp_failed.fit_ok = true;
  sharp_failed.validation_ok = true;
  sharp_failed.failure_note = "sharp";
  bbsolver::RecordBridgePruneEvaluationOutcome(&stats, sharp_failed, 2);

  bbsolver::BridgePruneCandidateEvaluation accepted;
  accepted.accepted = true;
  bbsolver::RecordBridgePruneEvaluationOutcome(&stats, accepted, 2);

  Require(stats.fit_failures == 1 && stats.validation_failures == 1 &&
              stats.sharp_failures == 1 && stats.accepted_candidates == 1,
          "bridge-prune outcome stats must classify all candidate outcomes");
  Require(stats.failures == std::vector<std::string>({"fit", "validation"}),
          "bridge-prune outcome stats must retain failure notes up to limit");
}

void TestBridgePruneTimedOutcomeRecordsTimingAndStats() {
  bbsolver::BridgePruneTimingTotals timing;
  bbsolver::BridgePruneOutcomeStats stats;
  bbsolver::BridgePruneCandidateEvaluation evaluation;
  evaluation.fit_ok = true;
  evaluation.validation_wall_ms = 3.5;
  evaluation.failure_note = "validation";

  bbsolver::RecordBridgePruneTimedEvaluationOutcome(
      &timing, &stats, evaluation, true);
  Require(timing.rejected_validation_wall_ms == 3.5 &&
              timing.batch_rejected_validation_wall_ms == 3.5,
          "timed bridge-prune outcome must accumulate timing");
  Require(stats.validation_failures == 1 &&
              stats.failures == std::vector<std::string>({"validation"}),
          "timed bridge-prune outcome must record failure stats");
}

void TestBridgePruneOutcomeStatsMerge() {
  bbsolver::BridgePruneOutcomeStats totals;
  totals.failures.push_back("existing");
  bbsolver::BridgePruneOutcomeStats delta;
  delta.fit_failures = 1;
  delta.validation_failures = 2;
  delta.sharp_failures = 3;
  delta.accepted_candidates = 4;
  delta.failures = {"new", "extra"};
  bbsolver::MergeBridgePruneOutcomeStats(&totals, delta, 2);
  Require(totals.fit_failures == 1 && totals.validation_failures == 2 &&
              totals.sharp_failures == 3 && totals.accepted_candidates == 4,
          "bridge-prune outcome merge must add counters");
  Require(totals.failures == std::vector<std::string>({"existing", "new"}),
          "bridge-prune outcome merge must respect failure note limit");
}

void TestBridgePruneCandidateSelectionOrdersAcceptedCandidates() {
  std::vector<bbsolver::BridgePruneCandidateEvaluation> evaluations(4);
  evaluations[0].accepted = true;
  evaluations[0].max_err = 1.0;
  evaluations[0].max_err_screen_px = 1.0;
  evaluations[0].result_vertices = 4;
  evaluations[0].removed_index = 3;

  evaluations[1].fit_ok = true;
  evaluations[1].failure_note = "validation";

  evaluations[2].accepted = true;
  evaluations[2].max_err = 0.5;
  evaluations[2].max_err_screen_px = 1.0;
  evaluations[2].result_vertices = 4;
  evaluations[2].removed_index = 2;

  evaluations[3].accepted = true;
  evaluations[3].max_err = 0.5;
  evaluations[3].max_err_screen_px = 1.0;
  evaluations[3].result_vertices = 4;
  evaluations[3].removed_index = 1;

  const bbsolver::BridgePruneCandidateSelection selection =
      bbsolver::SelectBridgePruneCandidates(evaluations);
  Require(selection.best_index == 3,
          "bridge-prune selection must choose deterministic best candidate");
  Require(selection.accepted_order == std::vector<int>({3, 2, 0}),
          "bridge-prune selection must sort accepted candidates by comparator");
  Require(selection.outcomes.accepted_candidates == 3 &&
              selection.outcomes.validation_failures == 1,
          "bridge-prune selection must summarize accepted and failed outcomes");
  Require(selection.outcomes.failures ==
              std::vector<std::string>({"validation"}),
          "bridge-prune selection must retain failure notes");
}

void TestBridgePruneRoundEvaluationFitFailure() {
  const bbsolver::PropertySamples original;
  const bbsolver::PropertyKeys current = KeysWithFlat(Flat(3));
  bbsolver::SolverConfig config;
  const bbsolver::CompInfo comp;
  bbsolver::BridgePruneTimingTotals timing;
  timing.accepted_fit_wall_ms = 2.0;

  const bbsolver::BridgePruneRoundEvaluationResult result =
      bbsolver::EvaluateBridgePruneCandidateRound(
          original, current, config, comp, nullptr, 0, 1, 5, 4, 5, {2},
          1, timing, {}, true);
  Require(!result.cancelled,
          "bridge-prune round helper must not cancel without cancel_fn");
  Require(result.evaluations.size() == 1,
          "bridge-prune round helper must evaluate each candidate slot");
  Require(result.evaluations[0].failure_note ==
              "5v_remove_2:bridge_fit_failed",
          "bridge-prune round helper must preserve candidate failure notes");
  Require(result.timing.accepted_fit_wall_ms == 2.0,
          "bridge-prune round helper must preserve incoming timing totals");
}

void TestBridgePruneRoundEvaluationCancellation() {
  const bbsolver::PropertySamples original;
  const bbsolver::PropertyKeys current = KeysWithFlat(Flat(3));
  bbsolver::SolverConfig config;
  const bbsolver::CompInfo comp;
  const bbsolver::BridgePruneRoundEvaluationResult result =
      bbsolver::EvaluateBridgePruneCandidateRound(
          original, current, config, comp, nullptr, 0, 1, 5, 4, 5, {2},
          1, {}, [] { return true; }, true);
  Require(result.cancelled,
          "bridge-prune round helper must preserve cancellation sentinel");
  Require(result.evaluations.size() == 1,
          "bridge-prune cancelled round must keep pre-sized result slots");
}

void TestBridgePruneBatchAttemptCancellationAndStop() {
  const bbsolver::BridgePruneBatchCandidateAttempt cancelled =
      bbsolver::EvaluateBridgePruneBatchCandidateAttempt(
          bbsolver::PropertySamples{}, KeysWithFlat(Flat(5)),
          bbsolver::SolverConfig{}, bbsolver::CompInfo{}, 5, 4, 2, {},
          [] { return true; }, true);
  Require(cancelled.cancelled,
          "bridge-prune batch attempt must preserve cancellation sentinel");

  const bbsolver::BridgePruneBatchCandidateAttempt stopped =
      bbsolver::EvaluateBridgePruneBatchCandidateAttempt(
          bbsolver::PropertySamples{}, KeysWithFlat(Flat(5)),
          bbsolver::SolverConfig{}, bbsolver::CompInfo{}, 4, 4, 2, {}, {},
          true);
  Require(stopped.stop && !stopped.evaluated,
          "bridge-prune batch attempt must stop at min target before fitting");
}

void TestBridgePruneBatchAttemptIndexAndFitFailure() {
  const bbsolver::BridgePruneBatchCandidateAttempt out_of_range =
      bbsolver::EvaluateBridgePruneBatchCandidateAttempt(
          bbsolver::PropertySamples{}, KeysWithFlat(Flat(5)),
          bbsolver::SolverConfig{}, bbsolver::CompInfo{}, 5, 4, 5, {}, {},
          true);
  Require(!out_of_range.cancelled && !out_of_range.stop &&
              !out_of_range.evaluated,
          "bridge-prune batch attempt must skip shifted indexes out of range");

  const bbsolver::BridgePruneBatchCandidateAttempt evaluated =
      bbsolver::EvaluateBridgePruneBatchCandidateAttempt(
          bbsolver::PropertySamples{}, KeysWithFlat(Flat(3)),
          bbsolver::SolverConfig{}, bbsolver::CompInfo{}, 5, 4, 2, {}, {},
          true);
  Require(evaluated.evaluated && !evaluated.evaluation.accepted,
          "bridge-prune batch attempt must evaluate valid shifted candidates");
  Require(evaluated.evaluation.failure_note ==
              "5v_batch_remove_2:bridge_fit_failed",
          "bridge-prune batch attempt must preserve candidate failure notes");
}

void TestBridgePruneBatchApplyCancellation() {
  std::vector<bbsolver::BridgePruneCandidateEvaluation> evaluations(2);
  evaluations[0].removed_index = 1;
  evaluations[1].removed_index = 2;
  bbsolver::BridgePruneCandidateSelection selection;
  selection.best_index = 0;
  selection.accepted_order = {0, 1};

  const bbsolver::BridgePruneBatchApplyResult result =
      bbsolver::ApplyBridgePruneAcceptedBatchRemovals(
          bbsolver::PropertySamples{},
          KeysWithFlat(Flat(4)),
          bbsolver::SolverConfig{},
          bbsolver::CompInfo{},
          nullptr,
          0,
          1,
          5,
          3,
          5,
          evaluations,
          selection,
          {},
          3,
          [] { return true; },
          true);
  Require(result.cancelled,
          "bridge-prune batch apply helper must preserve cancellation");
  Require(result.attempts == 3 && result.passes == 0,
          "cancelled batch apply must not advance attempts or passes");
}

void TestBridgePruneBatchApplyFitFailure() {
  std::vector<bbsolver::BridgePruneCandidateEvaluation> evaluations(2);
  evaluations[0].removed_index = 1;
  evaluations[1].removed_index = 2;
  bbsolver::BridgePruneCandidateSelection selection;
  selection.best_index = 0;
  selection.accepted_order = {0, 1};
  bbsolver::BridgePruneTimingTotals timing;
  timing.accepted_fit_wall_ms = 2.0;

  const bbsolver::BridgePruneBatchApplyResult result =
      bbsolver::ApplyBridgePruneAcceptedBatchRemovals(
          bbsolver::PropertySamples{},
          KeysWithFlat(Flat(3)),
          bbsolver::SolverConfig{},
          bbsolver::CompInfo{},
          nullptr,
          0,
          1,
          5,
          3,
          5,
          evaluations,
          selection,
          timing,
          3,
          {},
          false);
  Require(!result.cancelled,
          "bridge-prune batch fit failure must not cancel");
  Require(result.attempts == 4 && result.passes == 0 &&
              result.pruned_vertices == 0,
          "bridge-prune batch fit failure must count only the attempted batch");
  Require(result.outcomes.fit_failures == 1 &&
              result.outcomes.failures ==
                  std::vector<std::string>({
                      "4v_batch_remove_1:bridge_fit_failed"}),
          "bridge-prune batch fit failure must preserve failure telemetry");
  Require(result.timing.accepted_fit_wall_ms == 2.0,
          "bridge-prune batch helper must preserve incoming timing totals");
}

void TestBridgePruneBatchApplySuccess() {
  std::vector<bbsolver::BridgePruneCandidateEvaluation> evaluations(2);
  evaluations[0].removed_index = 1;
  evaluations[1].removed_index = 2;
  bbsolver::BridgePruneCandidateSelection selection;
  selection.best_index = 0;
  selection.accepted_order = {0, 1};
  bbsolver::PropertyKeys expected = KeysWithFlat(Flat(4));
  int affected_keys = 0;
  Require(bbsolver::BridgePruneShapeFlatKeyClass(
              &expected, 4, 1, &affected_keys),
          "bridge-prune batch success fixture must be reducible");
  bbsolver::PropertySamples original;
  bbsolver::Sample sample;
  sample.v = expected.keys[0].v;
  original.samples.push_back(sample);

  const bbsolver::BridgePruneBatchApplyResult result =
      bbsolver::ApplyBridgePruneAcceptedBatchRemovals(
          original,
          KeysWithFlat(Flat(4)),
          bbsolver::SolverConfig{},
          bbsolver::CompInfo{},
          nullptr,
          0,
          1,
          5,
          3,
          5,
          evaluations,
          selection,
          {},
          3,
          {},
          false);
  Require(!result.cancelled,
          "bridge-prune batch success must not cancel");
  Require(result.attempts == 4 && result.passes == 1 &&
              result.pruned_vertices == 1 && result.batch_pruned_vertices == 1,
          "bridge-prune batch success must report one accepted batch removal");
  Require(bbsolver::ShapeFlatVertexCount(result.current.keys[0].v) == 3,
          "bridge-prune batch success must return the pruned key payload");
  Require(result.outcomes.accepted_candidates == 1,
          "bridge-prune batch success must record accepted telemetry");
  Require(result.accepted_steps ==
              std::vector<std::string>({"4->3@idx1/keys1/batch"}),
          "bridge-prune batch success must preserve accepted-step note");
}

void TestBridgePruneTimingDefaultsAndJsonFields() {
  const bbsolver::BridgePruneTimingTotals totals;
  Require(totals.accepted_fit_wall_ms == 0.0,
          "bridge-prune timing must default accepted fit to zero");
  Require(totals.rejected_validation_wall_ms == 0.0,
          "bridge-prune timing must default rejected validation to zero");
  Require(totals.batch_rejected_validation_wall_ms == 0.0,
          "bridge-prune timing must default batch rejected validation to zero");

  nlohmann::json event;
  bbsolver::AddBridgePruneTimingFields(event, totals);
  for (const char* key: {
           "bridge_prune_accepted_fit_wall_ms",
           "bridge_prune_accepted_validation_wall_ms",
           "bridge_prune_accepted_sharp_wall_ms",
           "bridge_prune_rejected_fit_wall_ms",
           "bridge_prune_rejected_validation_wall_ms",
           "bridge_prune_rejected_sharp_wall_ms",
           "bridge_prune_round_accepted_validation_wall_ms",
           "bridge_prune_round_rejected_validation_wall_ms",
           "bridge_prune_batch_accepted_validation_wall_ms",
           "bridge_prune_batch_rejected_validation_wall_ms",
       }) {
    Require(event.contains(key),
            std::string("bridge-prune timing event missing key ") + key);
    Require(event[key].get<double>() == 0.0,
            std::string("bridge-prune timing key must default to zero: ") + key);
  }
}

void TestBridgePruneTimingAccumulation() {
  bbsolver::BridgePruneTimingTotals totals;
  bbsolver::BridgePruneCandidateEvaluation fit_failed;
  fit_failed.fit_wall_ms = 1.5;
  bbsolver::AccumulateBridgePruneTiming(&totals, fit_failed, false);
  Require(totals.rejected_fit_wall_ms == 1.5,
          "fit failure must add rejected fit timing");

  bbsolver::BridgePruneCandidateEvaluation validation_failed;
  validation_failed.fit_ok = true;
  validation_failed.fit_wall_ms = 2.0;
  validation_failed.validation_wall_ms = 3.0;
  bbsolver::AccumulateBridgePruneTiming(&totals, validation_failed, false);
  Require(totals.rejected_fit_wall_ms == 3.5,
          "validation failure must add rejected fit timing");
  Require(totals.rejected_validation_wall_ms == 3.0,
          "validation failure must add rejected validation timing");
  Require(totals.round_rejected_validation_wall_ms == 3.0,
          "round validation failure must add round rejected timing");

  bbsolver::BridgePruneCandidateEvaluation sharp_failed;
  sharp_failed.fit_ok = true;
  sharp_failed.validation_ok = true;
  sharp_failed.fit_wall_ms = 4.0;
  sharp_failed.validation_wall_ms = 5.0;
  sharp_failed.sharp_wall_ms = 6.0;
  bbsolver::AccumulateBridgePruneTiming(&totals, sharp_failed, true);
  Require(totals.rejected_sharp_wall_ms == 6.0,
          "sharp failure must add rejected sharp timing");
  Require(totals.batch_rejected_validation_wall_ms == 5.0,
          "batch sharp failure must add batch rejected validation timing");

  bbsolver::BridgePruneCandidateEvaluation accepted;
  accepted.fit_ok = true;
  accepted.validation_ok = true;
  accepted.sharp_ok = true;
  accepted.fit_wall_ms = 7.0;
  accepted.validation_wall_ms = 8.0;
  accepted.sharp_wall_ms = 9.0;
  bbsolver::AccumulateBridgePruneTiming(&totals, accepted, true);
  Require(totals.accepted_fit_wall_ms == 7.0,
          "accepted candidate must add accepted fit timing");
  Require(totals.accepted_validation_wall_ms == 8.0,
          "accepted candidate must add accepted validation timing");
  Require(totals.accepted_sharp_wall_ms == 9.0,
          "accepted candidate must add accepted sharp timing");
  Require(totals.batch_accepted_validation_wall_ms == 8.0,
          "batch accepted candidate must add batch accepted validation timing");
}

void TestBridgePruneProgressEventBuilders() {
  const bbsolver::PropertySamples samples = BridgePrunePropertySamples();
  bbsolver::BridgePruneTimingTotals totals;
  totals.accepted_fit_wall_ms = 12.0;

  const nlohmann::json start = bbsolver::BridgePruneCandidateStartEvent(
      samples, 1, 3, 10, 4, 8, 2, 7, 11, totals);
  Require(start["event"] == "post_solve_vertex_bridge_prune_candidate",
          "candidate start event token must be stable");
  Require(start["phase"] == "Vertex pass: testing 8v candidates for Path",
          "candidate start phase must preserve wording");
  Require(start["id"] == "shape/path" && start["display_name"] == "Path",
          "candidate start event must include property identity");
  Require(start["target_vertices"] == 8 && start["removed_index"] == 2,
          "candidate start event must include target and removed index");
  Require(start["candidate_count"] == 7 && start["candidates_checked"] == 0,
          "candidate start event must include candidate counts");
  Require(start["attempt"] == 11,
          "candidate start event must include the first attempt");
  Require(start["bridge_prune_accepted_fit_wall_ms"] == 12.0,
          "candidate start event must include accumulated timing");

  const nlohmann::json progress = bbsolver::BridgePruneCandidateProgressEvent(
      samples, 1, 3, 10, 4, 8, 3, 7, 4, 0.5, 14, totals);
  Require(progress["event"] == "post_solve_vertex_bridge_prune_progress",
          "candidate progress event token must be stable");
  Require(progress["candidate_progress"] == 0.5,
          "candidate progress event must include candidate fraction");
  Require(progress["candidates_checked"] == 4,
          "candidate progress event must include checked count");

  const nlohmann::json accepted = bbsolver::BridgePruneAcceptedRemovalEvent(
      samples, 1, 3, 10, 4, 8, 5, 7, 18, totals);
  Require(accepted["phase"] == "Vertex pass: accepted removal for Path",
          "accepted removal phase must preserve wording");
  Require(accepted["target_vertices"] == 7,
          "accepted removal event must report the resulting vertex count");
  Require(accepted["candidates_checked"] == 7,
          "accepted removal event must report checked candidates");

  const nlohmann::json batch =
      bbsolver::BridgePruneAcceptedBatchRemovalEvent(
          samples, 1, 3, 10, 4, 7, 4, 19, totals);
  Require(batch["phase"] == "Vertex pass: accepted batched removal for Path",
          "accepted batch phase must preserve wording");
  Require(batch["target_vertices"] == 7 && batch["removed_index"] == 4,
          "accepted batch event must report current vertex count and index");
  Require(batch["batch"] == true,
          "accepted batch event must include the batch sentinel");
}

// The accepted/rejected orchestration path depends on ValidateKeys,
// sharp-corner preservation, and progress JSON emission over full fixtures.
// Those behavior contracts stay locked by solver_progress_policy.py and
// post_temporal_bridge_parallel_policy.py; this unit test covers the pure
// result surface and cheap early exits.

}  // namespace

int main() {
  TestPostSolvePathVertexReductionResultDefaults();
  TestBridgePruneDisabledEarlyExitCopiesKeys();
  TestBridgePruneAtMinVerticesEarlyExit();
  TestBridgePruneCancelSentinelBeforeFirstPass();
  TestBridgePruneKeyValidationPropertyTolerance();
  TestBridgePruneKeyValidationScreenBranches();
  TestBridgePruneCandidateFitFailureNote();
  TestBridgePruneBatchCandidateFitFailureNote();
  TestBridgePruneNoteHelpersSkippedNotes();
  TestBridgePruneAcceptedNoteHelper();
  TestBridgePruneRejectedNoteHelper();
  TestBridgePruneAcceptedResultHelper();
  TestBridgePruneRejectedResultNoteHelper();
  TestBridgePruneRemovalPlanWithoutSemanticAnchors();
  TestBridgePruneShiftedIndexMathAndRange();
  TestBridgePruneAcceptedStepNoteHelper();
  TestBridgePruneOutcomeStatsRecordingAndLimit();
  TestBridgePruneTimedOutcomeRecordsTimingAndStats();
  TestBridgePruneOutcomeStatsMerge();
  TestBridgePruneCandidateSelectionOrdersAcceptedCandidates();
  TestBridgePruneRoundEvaluationFitFailure();
  TestBridgePruneRoundEvaluationCancellation();
  TestBridgePruneBatchAttemptCancellationAndStop();
  TestBridgePruneBatchAttemptIndexAndFitFailure();
  TestBridgePruneBatchApplyCancellation();
  TestBridgePruneBatchApplyFitFailure();
  TestBridgePruneBatchApplySuccess();
  TestBridgePruneTimingDefaultsAndJsonFields();
  TestBridgePruneTimingAccumulation();
  TestBridgePruneProgressEventBuilders();
  std::cout << "[PASS] test_path_bridge_prune\n";
  return 0;
}
