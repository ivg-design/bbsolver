#pragma once

#include "bbsolver/path/bridge_prune/path_bridge_prune_progress.hpp"
#include "bbsolver/path/reduction/path_vertex_reduction.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace bbsolver {

struct BridgePruneOutcomeStats {
  int fit_failures = 0;
  int validation_failures = 0;
  int sharp_failures = 0;
  int accepted_candidates = 0;
  std::vector<std::string> failures;
};

struct BridgePruneCandidateSelection {
  int best_index = -1;
  std::vector<int> accepted_order;
  BridgePruneOutcomeStats outcomes;
};

void RecordBridgePruneEvaluationOutcome(
    BridgePruneOutcomeStats* stats,
    const BridgePruneCandidateEvaluation& evaluation,
    std::size_t failure_limit = 8);

void RecordBridgePruneTimedEvaluationOutcome(
    BridgePruneTimingTotals* timing,
    BridgePruneOutcomeStats* stats,
    const BridgePruneCandidateEvaluation& evaluation,
    bool batch,
    std::size_t failure_limit = 8);

void MergeBridgePruneOutcomeStats(BridgePruneOutcomeStats* totals,
                                  const BridgePruneOutcomeStats& delta,
                                  std::size_t failure_limit = 8);

BridgePruneCandidateSelection SelectBridgePruneCandidates(
    const std::vector<BridgePruneCandidateEvaluation>& evaluations,
    std::size_t failure_limit = 8);

}  // namespace bbsolver
