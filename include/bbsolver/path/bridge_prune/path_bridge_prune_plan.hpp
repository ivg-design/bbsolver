#pragma once

#include "bbsolver/domain.hpp"

#include <string>
#include <vector>

namespace bbsolver {

struct BridgePruneRemovalPlan {
  std::vector<int> removed_indices;
  int protected_corner_skips = 0;
  bool attempted = false;
};

BridgePruneRemovalPlan BuildBridgePruneRemovalPlan(
    const PropertyKeys& keys,
    int target_vertices,
    const SolverConfig& config,
    bool source_vertices_are_semantic_anchors);

bool BridgePruneIndexIsProtected(
    const PropertyKeys& keys,
    int target_vertices,
    int removed_index,
    const SolverConfig& config,
    bool source_vertices_are_semantic_anchors);

int ShiftBridgePruneRemovedIndex(
    int original_removed_index,
    const std::vector<int>& removed_original_indices);

bool BridgePruneShiftedIndexInRange(int shifted_removed_index,
                                    int current_vertices);

std::string BridgePruneAcceptedStepNote(int source_vertices,
                                        int result_vertices,
                                        int removed_index,
                                        int affected_keys,
                                        bool batch);

}  // namespace bbsolver
