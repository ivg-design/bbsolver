#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/path/bridge_prune/path_bridge_prune_progress.hpp"
#include "bbsolver/path/bridge_prune/path_bridge_prune_selection.hpp"
#include "bbsolver/path/reduction/path_vertex_reduction.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace bbsolver {

class ProgressWriter;

struct BridgePruneBatchApplyResult {
  bool cancelled = false;
  PropertyKeys current;
  int attempts = 0;
  int passes = 0;
  int pruned_vertices = 0;
  int batch_pruned_vertices = 0;
  int protected_corner_skips = 0;
  double best_error = 0.0;
  BridgePruneTimingTotals timing;
  BridgePruneOutcomeStats outcomes;
  std::vector<std::string> accepted_steps;
};

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
    bool source_vertices_are_semantic_anchors);

}  // namespace bbsolver
