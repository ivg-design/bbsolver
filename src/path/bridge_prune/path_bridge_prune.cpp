#include "bbsolver/path/bridge_prune/path_bridge_prune.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/path/bridge_prune/path_bridge_prune_batch.hpp"
#include "bbsolver/path/bridge_prune/path_bridge_prune_notes.hpp"
#include "bbsolver/path/bridge_prune/path_bridge_prune_plan.hpp"
#include "bbsolver/path/bridge_prune/path_bridge_prune_progress.hpp"
#include "bbsolver/path/bridge_prune/path_bridge_prune_result.hpp"
#include "bbsolver/path/bridge_prune/path_bridge_prune_round.hpp"
#include "bbsolver/path/bridge_prune/path_bridge_prune_selection.hpp"
#include "bbsolver/path/reduction/path_vertex_reduction.hpp"
#include "bbsolver/progress/progress.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace bbsolver {

PostSolvePathVertexReductionResult TryPostTemporalBridgePrune(
    const PropertySamples& original,
    const PropertyKeys& solved_keys,
    const SolverConfig& config,
    const CompInfo& comp,
    const ProgressWriter* progress,
    std::size_t property_idx,
    std::size_t property_count,
    int source_vertices_override,
    std::function<bool()> cancel_fn,
    bool source_vertices_are_semantic_anchors) {
  PostSolvePathVertexReductionResult result;
  result.keys = solved_keys;
  const int initial_max_vertices = MaxShapeFlatKeyVertexCount(solved_keys);
  const int reported_source_vertices =
      source_vertices_override > 0 ? source_vertices_override: initial_max_vertices;
  result.source_vertices = reported_source_vertices;
  result.fitted_vertices = initial_max_vertices;

  if (!config.path_replacement_prefer_vertices) {
    result.notes = BridgePruneDisabledNote();
    return result;
  }

  const int min_target = std::max(config.path_replacement_min_vertices, 4);
  if (initial_max_vertices <= min_target) {
    result.notes = BridgePruneAtMinVerticesNote();
    return result;
  }

  PropertyKeys current = solved_keys;
  const int max_passes =
      PostTemporalBridgePrunePassBudget(config, initial_max_vertices, min_target);
  int passes = 0;
  int attempts = 0;
  int candidate_rounds = 0;
  int batch_pruned_vertices = 0;
  int protected_corner_skips = 0;
  int pruned_vertices = 0;
  BridgePruneOutcomeStats bridge_prune_outcomes;
  BridgePruneTimingTotals bridge_prune_timing;
  double best_error = solved_keys.max_err;
  std::vector<std::string> accepted_steps;

  for (int pass = 0; pass < max_passes; ++pass) {
    if (BridgePruneCancelled(cancel_fn)) {
      result.notes = "cancelled";
      return result;
    }
    const int target_vertices =
        DominantClosedShapeFlatKeyVertexCount(current, min_target);
    if (target_vertices <= min_target) {
      break;
    }

    const BridgePruneRemovalPlan removal_plan = BuildBridgePruneRemovalPlan(
        current, target_vertices, config, source_vertices_are_semantic_anchors);
    protected_corner_skips += removal_plan.protected_corner_skips;
    if (removal_plan.attempted) {
      result.attempted = true;
    }
    const std::vector<int>& removal_candidates =
        removal_plan.removed_indices;

    if (removal_candidates.empty()) {
      break;
    }

    if (BridgePruneCancelled(cancel_fn)) {
      result.notes = "cancelled";
      return result;
    }

    const int first_attempt = attempts + 1;
    attempts += static_cast<int>(removal_candidates.size());
    ++candidate_rounds;
    result.attempted = true;
    BridgePruneRoundEvaluationResult round_evaluation =
        EvaluateBridgePruneCandidateRound(
            original, current, config, comp, progress, property_idx,
            property_count, initial_max_vertices, min_target, target_vertices,
            removal_candidates, first_attempt, bridge_prune_timing, cancel_fn,
            source_vertices_are_semantic_anchors);
    if (round_evaluation.cancelled) {
      result.notes = "cancelled";
      return result;
    }
    bridge_prune_timing = round_evaluation.timing;

    const BridgePruneCandidateSelection selection =
        SelectBridgePruneCandidates(round_evaluation.evaluations);
    MergeBridgePruneOutcomeStats(&bridge_prune_outcomes, selection.outcomes);

    if (selection.best_index < 0) {
      break;
    }

    if (BridgePruneCancelled(cancel_fn)) {
      result.notes = "cancelled";
      return result;
    }

    BridgePruneCandidateEvaluation& selected =
        round_evaluation.evaluations[
            static_cast<std::size_t>(selection.best_index)];
    current = std::move(selected.candidate);
    ++passes;
    ++pruned_vertices;
    best_error = selected.max_err;
    if (progress != nullptr) {
      progress->Emit(BridgePruneAcceptedRemovalEvent(
          original, property_idx, property_count, initial_max_vertices, min_target,
          target_vertices, selected.removed_index, removal_candidates.size(),
          attempts, bridge_prune_timing));
    }
    accepted_steps.push_back(BridgePruneAcceptedStepNote(
        target_vertices, target_vertices - 1, selected.removed_index,
        selected.affected_keys, false));

    BridgePruneBatchApplyResult batch_result =
        ApplyBridgePruneAcceptedBatchRemovals(
            original, std::move(current), config, comp, progress, property_idx,
            property_count, initial_max_vertices, min_target, target_vertices,
            round_evaluation.evaluations, selection, bridge_prune_timing,
            attempts, cancel_fn, source_vertices_are_semantic_anchors);
    if (batch_result.cancelled) {
      result.notes = "cancelled";
      return result;
    }
    current = std::move(batch_result.current);
    attempts = batch_result.attempts;
    passes += batch_result.passes;
    pruned_vertices += batch_result.pruned_vertices;
    batch_pruned_vertices += batch_result.batch_pruned_vertices;
    protected_corner_skips += batch_result.protected_corner_skips;
    bridge_prune_timing = batch_result.timing;
    MergeBridgePruneOutcomeStats(&bridge_prune_outcomes,
                                 batch_result.outcomes);
    if (batch_result.pruned_vertices > 0) {
      best_error = batch_result.best_error;
      accepted_steps.insert(accepted_steps.end(),
                            batch_result.accepted_steps.begin(),
                            batch_result.accepted_steps.end());
    }
  }

  BridgePruneResultSummary summary;
  summary.reported_source_vertices = reported_source_vertices;
  summary.pruned_vertices = pruned_vertices;
  summary.passes = passes;
  summary.candidate_rounds = candidate_rounds;
  summary.batch_pruned_vertices = batch_pruned_vertices;
  summary.attempts = attempts;
  summary.protected_corner_skips = protected_corner_skips;
  summary.best_error = best_error;
  summary.preserve_sharp_corners = config.path_preserve_sharp_corners;
  summary.outcomes = bridge_prune_outcomes;
  summary.accepted_steps = accepted_steps;

  if (pruned_vertices > 0) {
    return BuildBridgePruneAcceptedResult(std::move(current), summary);
  }

  if (result.attempted) {
    result.notes = BuildBridgePruneRejectedResultNote(solved_keys, summary);
  }
  return result;
}

}  // namespace bbsolver
