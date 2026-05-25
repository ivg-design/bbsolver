#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace bbsolver {

std::string BridgePruneDisabledNote();

std::string BridgePruneAtMinVerticesNote();

std::string BuildBridgePruneAcceptedNote(
    int source_vertices,
    int fitted_vertices,
    std::size_t key_count,
    int pruned_vertices,
    int passes,
    int candidate_rounds,
    int batch_pruned_vertices,
    int attempts,
    double best_error,
    int protected_corner_skips,
    int fit_failures,
    int validation_failures,
    int sharp_failures,
    int accepted_candidates,
    bool preserve_sharp_corners,
    const std::vector<std::string>& accepted_steps);

std::string BuildBridgePruneRejectedNote(
    int source_vertices,
    std::size_t key_count,
    int attempts,
    int protected_corner_skips,
    int fit_failures,
    int validation_failures,
    int sharp_failures,
    int accepted_candidates,
    bool preserve_sharp_corners,
    const std::vector<std::string>& failures);

}  // namespace bbsolver
