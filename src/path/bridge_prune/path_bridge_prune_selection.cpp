#include "bbsolver/path/bridge_prune/path_bridge_prune_selection.hpp"
#include "bbsolver/path/bridge_prune/path_bridge_prune_progress.hpp"
#include "bbsolver/path/reduction/path_vertex_reduction.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace bbsolver {

void RecordBridgePruneEvaluationOutcome(
    BridgePruneOutcomeStats* stats,
    const BridgePruneCandidateEvaluation& evaluation,
    std::size_t failure_limit) {
  if (stats == nullptr) {
    return;
  }
  if (evaluation.accepted) {
    ++stats->accepted_candidates;
    return;
  }
  if (!evaluation.fit_ok) {
    ++stats->fit_failures;
  } else if (!evaluation.validation_ok) {
    ++stats->validation_failures;
  } else if (!evaluation.sharp_ok) {
    ++stats->sharp_failures;
  }
  if (!evaluation.failure_note.empty() &&
      stats->failures.size() < failure_limit) {
    stats->failures.push_back(evaluation.failure_note);
  }
}

void RecordBridgePruneTimedEvaluationOutcome(
    BridgePruneTimingTotals* timing,
    BridgePruneOutcomeStats* stats,
    const BridgePruneCandidateEvaluation& evaluation,
    bool batch,
    std::size_t failure_limit) {
  AccumulateBridgePruneTiming(timing, evaluation, batch);
  RecordBridgePruneEvaluationOutcome(stats, evaluation, failure_limit);
}

void MergeBridgePruneOutcomeStats(BridgePruneOutcomeStats* totals,
                                  const BridgePruneOutcomeStats& delta,
                                  std::size_t failure_limit) {
  if (totals == nullptr) {
    return;
  }
  totals->fit_failures += delta.fit_failures;
  totals->validation_failures += delta.validation_failures;
  totals->sharp_failures += delta.sharp_failures;
  totals->accepted_candidates += delta.accepted_candidates;
  for (const std::string& failure : delta.failures) {
    if (!failure.empty() && totals->failures.size() < failure_limit) {
      totals->failures.push_back(failure);
    }
  }
}

BridgePruneCandidateSelection SelectBridgePruneCandidates(
    const std::vector<BridgePruneCandidateEvaluation>& evaluations,
    std::size_t failure_limit) {
  BridgePruneCandidateSelection selection;
  for (std::size_t candidate_idx = 0; candidate_idx < evaluations.size();
       ++candidate_idx) {
    const BridgePruneCandidateEvaluation& evaluation =
        evaluations[candidate_idx];
    RecordBridgePruneEvaluationOutcome(
        &selection.outcomes, evaluation, failure_limit);
    if (!evaluation.accepted) {
      continue;
    }
    selection.accepted_order.push_back(static_cast<int>(candidate_idx));
    if (selection.best_index < 0 ||
        BridgePruneCandidateIsBetter(
            evaluation,
            evaluations[static_cast<std::size_t>(selection.best_index)])) {
      selection.best_index = static_cast<int>(candidate_idx);
    }
  }
  std::sort(selection.accepted_order.begin(), selection.accepted_order.end(),
            [&](int lhs, int rhs) {
              return BridgePruneCandidateIsBetter(
                  evaluations[static_cast<std::size_t>(lhs)],
                  evaluations[static_cast<std::size_t>(rhs)]);
            });
  return selection;
}

}  // namespace bbsolver
