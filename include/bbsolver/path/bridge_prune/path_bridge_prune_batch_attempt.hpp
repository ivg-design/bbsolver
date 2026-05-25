#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/path/reduction/path_vertex_reduction.hpp"

#include <functional>
#include <vector>

namespace bbsolver {

struct BridgePruneBatchCandidateAttempt {
  bool cancelled = false;
  bool stop = false;
  bool evaluated = false;
  bool protected_corner_skip = false;
  int original_removed_index = 0;
  int shifted_removed_index = 0;
  BridgePruneCandidateEvaluation evaluation;
};

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
    bool source_vertices_are_semantic_anchors);

}  // namespace bbsolver
