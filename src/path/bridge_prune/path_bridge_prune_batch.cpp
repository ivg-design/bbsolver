#include "bbsolver/path/bridge_prune/path_bridge_prune_batch.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/path/bridge_prune/path_bridge_prune_batch_attempt.hpp"
#include "bbsolver/path/bridge_prune/path_bridge_prune_plan.hpp"
#include "bbsolver/path/bridge_prune/path_bridge_prune_progress.hpp"
#include "bbsolver/path/bridge_prune/path_bridge_prune_selection.hpp"
#include "bbsolver/path/reduction/path_vertex_reduction.hpp"
#include "bbsolver/progress/progress.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace bbsolver {

BridgePruneBatchApplyResult ApplyBridgePruneAcceptedBatchRemovals(
    const PropertySamples& original,
    PropertyKeys current,
    const SolverConfig& config,
    const CompInfo& comp,
    const ProgressWriter* progress,
    std::size_t property_idx,
    std::size_t property_count,
    int initial_max_vertices,
    int min_target,
    int target_vertices,
    const std::vector<BridgePruneCandidateEvaluation>& evaluations,
    const BridgePruneCandidateSelection& selection,
    const BridgePruneTimingTotals& initial_timing,
    int attempts,
    const std::function<bool()>& cancel_fn,
    bool source_vertices_are_semantic_anchors) {
  BridgePruneBatchApplyResult result;
  result.current = std::move(current);
  result.attempts = attempts;
  result.timing = initial_timing;
  if (selection.best_index < 0) {
    return result;
  }

  const BridgePruneCandidateEvaluation& selected =
      evaluations[static_cast<std::size_t>(selection.best_index)];
  std::vector<int> removed_original_indices;
  removed_original_indices.push_back(selected.removed_index);

  for (int candidate_order_index : selection.accepted_order) {
    if (candidate_order_index == selection.best_index) {
      continue;
    }
    const BridgePruneCandidateEvaluation& prior_evaluation =
        evaluations[static_cast<std::size_t>(candidate_order_index)];
    const int original_removed_index = prior_evaluation.removed_index;
    const int current_vertices =
        target_vertices - static_cast<int>(removed_original_indices.size());
    BridgePruneBatchCandidateAttempt attempt =
        EvaluateBridgePruneBatchCandidateAttempt(
            original, result.current, config, comp, current_vertices,
            min_target, original_removed_index, removed_original_indices,
            cancel_fn, source_vertices_are_semantic_anchors);
    if (attempt.cancelled) {
      result.cancelled = true;
      return result;
    }
    if (attempt.stop) {
      break;
    }
    if (attempt.protected_corner_skip) {
      ++result.protected_corner_skips;
      continue;
    }
    if (!attempt.evaluated) {
      continue;
    }

    ++result.attempts;
    RecordBridgePruneTimedEvaluationOutcome(
        &result.timing, &result.outcomes, attempt.evaluation, true);
    if (!attempt.evaluation.accepted) {
      continue;
    }

    result.current = std::move(attempt.evaluation.candidate);
    removed_original_indices.push_back(original_removed_index);
    std::sort(removed_original_indices.begin(), removed_original_indices.end());
    ++result.passes;
    ++result.pruned_vertices;
    ++result.batch_pruned_vertices;
    result.best_error = attempt.evaluation.max_err;
    result.accepted_steps.push_back(BridgePruneAcceptedStepNote(
        current_vertices, current_vertices - 1, attempt.shifted_removed_index,
        attempt.evaluation.affected_keys, true));
    if (progress != nullptr) {
      progress->Emit(BridgePruneAcceptedBatchRemovalEvent(
          original, property_idx, property_count, initial_max_vertices,
          min_target, current_vertices, attempt.shifted_removed_index,
          result.attempts, result.timing));
    }
  }
  return result;
}

}  // namespace bbsolver
