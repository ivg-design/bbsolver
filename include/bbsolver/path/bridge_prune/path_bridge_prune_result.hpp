#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/path/bridge_prune/path_bridge_prune.hpp"
#include "bbsolver/path/bridge_prune/path_bridge_prune_selection.hpp"

#include <string>
#include <vector>

namespace bbsolver {

struct BridgePruneResultSummary {
  int reported_source_vertices = 0;
  int pruned_vertices = 0;
  int passes = 0;
  int candidate_rounds = 0;
  int batch_pruned_vertices = 0;
  int attempts = 0;
  int protected_corner_skips = 0;
  double best_error = 0.0;
  bool preserve_sharp_corners = false;
  BridgePruneOutcomeStats outcomes;
  std::vector<std::string> accepted_steps;
};

PostSolvePathVertexReductionResult BuildBridgePruneAcceptedResult(
    PropertyKeys keys,
    const BridgePruneResultSummary& summary);

std::string BuildBridgePruneRejectedResultNote(
    const PropertyKeys& solved_keys,
    const BridgePruneResultSummary& summary);

}  // namespace bbsolver
