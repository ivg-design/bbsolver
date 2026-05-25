#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/path/reduction/path_vertex_reduction.hpp"

#include <cstddef>

#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>

namespace bbsolver {

struct BridgePruneTimingTotals {
  double accepted_fit_wall_ms = 0.0;
  double accepted_validation_wall_ms = 0.0;
  double accepted_sharp_wall_ms = 0.0;
  double rejected_fit_wall_ms = 0.0;
  double rejected_validation_wall_ms = 0.0;
  double rejected_sharp_wall_ms = 0.0;
  double round_accepted_validation_wall_ms = 0.0;
  double round_rejected_validation_wall_ms = 0.0;
  double batch_accepted_validation_wall_ms = 0.0;
  double batch_rejected_validation_wall_ms = 0.0;
};

void AccumulateBridgePruneTiming(
    BridgePruneTimingTotals* totals,
    const BridgePruneCandidateEvaluation& evaluation,
    bool batch);

void AddBridgePruneTimingFields(nlohmann::json& event,
                                const BridgePruneTimingTotals& totals);

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
    const BridgePruneTimingTotals& totals);

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
    const BridgePruneTimingTotals& totals);

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
    const BridgePruneTimingTotals& totals);

nlohmann::json BridgePruneAcceptedBatchRemovalEvent(
    const PropertySamples& original,
    std::size_t property_idx,
    std::size_t property_count,
    int initial_max_vertices,
    int min_target,
    int current_vertices,
    int shifted_removed_index,
    int attempts,
    const BridgePruneTimingTotals& totals);

}  // namespace bbsolver
