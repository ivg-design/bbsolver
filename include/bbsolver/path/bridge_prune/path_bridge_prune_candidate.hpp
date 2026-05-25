#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/metrics/error_metrics.hpp"
#include "bbsolver/path/reduction/path_vertex_reduction.hpp"

namespace bbsolver {

bool PassesBridgePruneKeyValidation(const ErrorReport& report,
                                    const SolverConfig& config);

BridgePruneCandidateEvaluation EvaluateBridgePruneCandidate(
    const PropertySamples& original,
    const PropertyKeys& current,
    const SolverConfig& config,
    const CompInfo& comp,
    int target_vertices,
    int removed_index,
    bool source_vertices_are_semantic_anchors);

BridgePruneCandidateEvaluation EvaluateBridgePruneBatchCandidate(
    const PropertySamples& original,
    const PropertyKeys& current,
    const SolverConfig& config,
    const CompInfo& comp,
    int current_vertices,
    int shifted_removed_index,
    bool source_vertices_are_semantic_anchors);

}  // namespace bbsolver
