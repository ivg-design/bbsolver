#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/path/bridge_prune/path_bridge_prune_progress.hpp"
#include "bbsolver/path/reduction/path_vertex_reduction.hpp"

#include <cstddef>
#include <functional>
#include <vector>

namespace bbsolver {

class ProgressWriter;

struct BridgePruneRoundEvaluationResult {
  bool cancelled = false;
  std::vector<BridgePruneCandidateEvaluation> evaluations;
  BridgePruneTimingTotals timing;
};

BridgePruneRoundEvaluationResult EvaluateBridgePruneCandidateRound(
    const PropertySamples& original,
    const PropertyKeys& current,
    const SolverConfig& config,
    const CompInfo& comp,
    const ProgressWriter* progress,
    std::size_t property_idx,
    std::size_t property_count,
    int initial_max_vertices,
    int min_target,
    int target_vertices,
    const std::vector<int>& removal_candidates,
    int first_attempt,
    const BridgePruneTimingTotals& initial_timing,
    const std::function<bool()>& cancel_fn,
    bool source_vertices_are_semantic_anchors);

}  // namespace bbsolver
