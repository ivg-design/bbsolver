#include "bbsolver/path/bridge_prune/path_bridge_prune_plan.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/shape/sharp_corner_policy.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace bbsolver {

BridgePruneRemovalPlan BuildBridgePruneRemovalPlan(
    const PropertyKeys& keys,
    int target_vertices,
    const SolverConfig& config,
    bool source_vertices_are_semantic_anchors) {
  BridgePruneRemovalPlan plan;
  plan.removed_indices.reserve(
      static_cast<std::size_t>(std::max(0, target_vertices - 1)));
  for (int removed_index = 1; removed_index < target_vertices;
       ++removed_index) {
    if (BridgePruneIndexIsProtected(keys, target_vertices, removed_index,
                                    config,
                                    source_vertices_are_semantic_anchors)) {
      ++plan.protected_corner_skips;
      plan.attempted = true;
      continue;
    }
    plan.removed_indices.push_back(removed_index);
  }
  return plan;
}

bool BridgePruneIndexIsProtected(
    const PropertyKeys& keys,
    int target_vertices,
    int removed_index,
    const SolverConfig& config,
    bool source_vertices_are_semantic_anchors) {
  return source_vertices_are_semantic_anchors &&
         ShapeFlatKeyIndexIsProtectedCorner(
             keys, target_vertices, removed_index, config);
}

int ShiftBridgePruneRemovedIndex(
    int original_removed_index,
    const std::vector<int>& removed_original_indices) {
  int shifted_removed_index = original_removed_index;
  for (int removed_original_index: removed_original_indices) {
    if (removed_original_index < original_removed_index) {
      --shifted_removed_index;
    }
  }
  return shifted_removed_index;
}

bool BridgePruneShiftedIndexInRange(int shifted_removed_index,
                                    int current_vertices) {
  return shifted_removed_index > 0 &&
         shifted_removed_index < current_vertices;
}

std::string BridgePruneAcceptedStepNote(int source_vertices,
                                        int result_vertices,
                                        int removed_index,
                                        int affected_keys,
                                        bool batch) {
  return std::to_string(source_vertices) + "->" +
         std::to_string(result_vertices) + "@idx" +
         std::to_string(removed_index) + "/keys" +
         std::to_string(affected_keys) +
         (batch ? "/batch": std::string{});
}

}  // namespace bbsolver
