#include "bbsolver/path/bridge_prune/path_bridge_prune_progress.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/path/reduction/path_vertex_reduction.hpp"
#include "bbsolver/progress/progress.hpp"

#include <string>
#include <cstddef>
#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>

namespace bbsolver {

void AccumulateBridgePruneTiming(
    BridgePruneTimingTotals* totals,
    const BridgePruneCandidateEvaluation& evaluation,
    bool batch) {
  if (totals == nullptr) {
    return;
  }
  if (!evaluation.fit_ok) {
    totals->rejected_fit_wall_ms += evaluation.fit_wall_ms;
    return;
  }
  if (!evaluation.validation_ok) {
    totals->rejected_fit_wall_ms += evaluation.fit_wall_ms;
    totals->rejected_validation_wall_ms += evaluation.validation_wall_ms;
    if (batch) {
      totals->batch_rejected_validation_wall_ms +=
          evaluation.validation_wall_ms;
    } else {
      totals->round_rejected_validation_wall_ms +=
          evaluation.validation_wall_ms;
    }
    return;
  }
  if (!evaluation.sharp_ok) {
    totals->rejected_fit_wall_ms += evaluation.fit_wall_ms;
    totals->rejected_validation_wall_ms += evaluation.validation_wall_ms;
    totals->rejected_sharp_wall_ms += evaluation.sharp_wall_ms;
    if (batch) {
      totals->batch_rejected_validation_wall_ms +=
          evaluation.validation_wall_ms;
    } else {
      totals->round_rejected_validation_wall_ms +=
          evaluation.validation_wall_ms;
    }
    return;
  }
  totals->accepted_fit_wall_ms += evaluation.fit_wall_ms;
  totals->accepted_validation_wall_ms += evaluation.validation_wall_ms;
  totals->accepted_sharp_wall_ms += evaluation.sharp_wall_ms;
  if (batch) {
    totals->batch_accepted_validation_wall_ms +=
        evaluation.validation_wall_ms;
  } else {
    totals->round_accepted_validation_wall_ms +=
        evaluation.validation_wall_ms;
  }
}

void AddBridgePruneTimingFields(nlohmann::json& event,
                                const BridgePruneTimingTotals& totals) {
  event["bridge_prune_accepted_fit_wall_ms"] =
      totals.accepted_fit_wall_ms;
  event["bridge_prune_accepted_validation_wall_ms"] =
      totals.accepted_validation_wall_ms;
  event["bridge_prune_accepted_sharp_wall_ms"] =
      totals.accepted_sharp_wall_ms;
  event["bridge_prune_rejected_fit_wall_ms"] =
      totals.rejected_fit_wall_ms;
  event["bridge_prune_rejected_validation_wall_ms"] =
      totals.rejected_validation_wall_ms;
  event["bridge_prune_rejected_sharp_wall_ms"] =
      totals.rejected_sharp_wall_ms;
  event["bridge_prune_round_accepted_validation_wall_ms"] =
      totals.round_accepted_validation_wall_ms;
  event["bridge_prune_round_rejected_validation_wall_ms"] =
      totals.round_rejected_validation_wall_ms;
  event["bridge_prune_batch_accepted_validation_wall_ms"] =
      totals.batch_accepted_validation_wall_ms;
  event["bridge_prune_batch_rejected_validation_wall_ms"] =
      totals.batch_rejected_validation_wall_ms;
}

nlohmann::json BridgePruneCandidateStartEvent(
    const PropertySamples& original,
    std::size_t property_idx,
    std::size_t property_count,
    int initial_max_vertices,
    int min_target,
    int target_vertices,
    int removed_index,
    std::size_t candidate_count,
    int first_attempt,
    const BridgePruneTimingTotals& totals) {
  nlohmann::json event = {
      {"event", "post_solve_vertex_bridge_prune_candidate"},
      {"phase", "Vertex pass: testing " +
                    std::to_string(target_vertices) + "v candidates for " +
                    ProgressPropertyLabel(original)},
      {"progress", SolveProgressForPropertyStage(
                       property_idx,
                       property_count,
                       BridgePruneLocalProgress(initial_max_vertices,
                                                min_target,
                                                target_vertices,
                                                0.0))},
      {"id", original.property.id},
      {"display_name", ProgressPropertyLabel(original)},
      {"i", property_idx},
      {"n", property_count},
      {"target_vertices", target_vertices},
      {"removed_index", removed_index},
      {"candidate_count", candidate_count},
      {"candidates_checked", 0},
      {"attempt", first_attempt},
  };
  AddBridgePruneTimingFields(event, totals);
  return event;
}

nlohmann::json BridgePruneCandidateProgressEvent(
    const PropertySamples& original,
    std::size_t property_idx,
    std::size_t property_count,
    int initial_max_vertices,
    int min_target,
    int target_vertices,
    int removed_index,
    std::size_t candidate_count,
    std::size_t candidates_checked,
    double candidate_fraction,
    int attempt,
    const BridgePruneTimingTotals& totals) {
  nlohmann::json event = {
      {"event", "post_solve_vertex_bridge_prune_progress"},
      {"phase", "Vertex pass: testing " +
                    std::to_string(target_vertices) + "v candidates for " +
                    ProgressPropertyLabel(original)},
      {"progress", SolveProgressForPropertyStage(
                       property_idx,
                       property_count,
                       BridgePruneLocalProgress(initial_max_vertices,
                                                min_target,
                                                target_vertices,
                                                candidate_fraction))},
      {"id", original.property.id},
      {"display_name", ProgressPropertyLabel(original)},
      {"i", property_idx},
      {"n", property_count},
      {"target_vertices", target_vertices},
      {"removed_index", removed_index},
      {"candidate_count", candidate_count},
      {"candidates_checked", candidates_checked},
      {"candidate_progress", candidate_fraction},
      {"attempt", attempt},
  };
  AddBridgePruneTimingFields(event, totals);
  return event;
}

nlohmann::json BridgePruneAcceptedRemovalEvent(
    const PropertySamples& original,
    std::size_t property_idx,
    std::size_t property_count,
    int initial_max_vertices,
    int min_target,
    int target_vertices,
    int removed_index,
    std::size_t candidates_checked,
    int attempts,
    const BridgePruneTimingTotals& totals) {
  nlohmann::json event = {
      {"event", "post_solve_vertex_bridge_prune_candidate"},
      {"phase", "Vertex pass: accepted removal for " +
                    ProgressPropertyLabel(original)},
      {"progress", SolveProgressForPropertyStage(
                       property_idx,
                       property_count,
                       BridgePruneLocalProgress(initial_max_vertices,
                                                min_target,
                                                target_vertices - 1,
                                                0.0))},
      {"id", original.property.id},
      {"display_name", ProgressPropertyLabel(original)},
      {"i", property_idx},
      {"n", property_count},
      {"target_vertices", target_vertices - 1},
      {"removed_index", removed_index},
      {"candidate_count", 1},
      {"candidates_checked", candidates_checked},
      {"attempt", attempts},
  };
  AddBridgePruneTimingFields(event, totals);
  return event;
}

nlohmann::json BridgePruneAcceptedBatchRemovalEvent(
    const PropertySamples& original,
    std::size_t property_idx,
    std::size_t property_count,
    int initial_max_vertices,
    int min_target,
    int current_vertices,
    int shifted_removed_index,
    int attempts,
    const BridgePruneTimingTotals& totals) {
  nlohmann::json event = {
      {"event", "post_solve_vertex_bridge_prune_candidate"},
      {"phase", "Vertex pass: accepted batched removal for " +
                    ProgressPropertyLabel(original)},
      {"progress", SolveProgressForPropertyStage(
                       property_idx,
                       property_count,
                       BridgePruneLocalProgress(initial_max_vertices,
                                                min_target,
                                                current_vertices - 1,
                                                0.0))},
      {"id", original.property.id},
      {"display_name", ProgressPropertyLabel(original)},
      {"i", property_idx},
      {"n", property_count},
      {"target_vertices", current_vertices},
      {"removed_index", shifted_removed_index},
      {"candidate_count", 1},
      {"candidates_checked", 1},
      {"attempt", attempts},
      {"batch", true},
  };
  AddBridgePruneTimingFields(event, totals);
  return event;
}

}  // namespace bbsolver
