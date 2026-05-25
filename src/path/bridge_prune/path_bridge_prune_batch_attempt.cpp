#include "bbsolver/path/bridge_prune/path_bridge_prune_batch_attempt.hpp"
#include "bbsolver/domain.hpp"

#include <functional>
#include <vector>

#include "bbsolver/path/bridge_prune/path_bridge_prune_candidate.hpp"
#include "bbsolver/path/bridge_prune/path_bridge_prune_plan.hpp"
#include "bbsolver/path/reduction/path_vertex_reduction.hpp"

namespace bbsolver {

BridgePruneBatchCandidateAttempt EvaluateBridgePruneBatchCandidateAttempt(
    const PropertySamples& original,
    const PropertyKeys& current,
    const SolverConfig& config,
    const CompInfo& comp,
    int current_vertices,
    int min_target,
    int original_removed_index,
    const std::vector<int>& removed_original_indices,
    const std::function<bool()>& cancel_fn,
    bool source_vertices_are_semantic_anchors) {
  BridgePruneBatchCandidateAttempt result;
  result.original_removed_index = original_removed_index;
  if (BridgePruneCancelled(cancel_fn)) {
    result.cancelled = true;
    return result;
  }
  if (current_vertices <= min_target) {
    result.stop = true;
    return result;
  }

  result.shifted_removed_index = ShiftBridgePruneRemovedIndex(
      original_removed_index, removed_original_indices);
  if (!BridgePruneShiftedIndexInRange(
          result.shifted_removed_index, current_vertices)) {
    return result;
  }
  if (BridgePruneIndexIsProtected(
          current, current_vertices, result.shifted_removed_index, config,
          source_vertices_are_semantic_anchors)) {
    result.protected_corner_skip = true;
    return result;
  }

  result.evaluated = true;
  result.evaluation = EvaluateBridgePruneBatchCandidate(
      original, current, config, comp, current_vertices,
      result.shifted_removed_index, source_vertices_are_semantic_anchors);
  return result;
}

}  // namespace bbsolver
