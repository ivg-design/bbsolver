#include "bbsolver/path/bridge_prune/path_bridge_prune_notes.hpp"

#include "bbsolver/path/reduction/path_vertex_reduction.hpp"

#include <string>
#include <cstddef>
#include <vector>

namespace bbsolver {

std::string BridgePruneDisabledNote() {
  return "post_solve_vertex_reduction_skipped: "
         "post_temporal_bridge_prune_disabled";
}

std::string BridgePruneAtMinVerticesNote() {
  return "post_solve_vertex_reduction_skipped: "
         "source_already_at_min_vertices";
}

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
    const std::vector<std::string>& accepted_steps) {
  return "post_solve_vertex_reduction_accepted"
         "; mode=post_temporal_bridge_prune"
         "; source_vertices=" + std::to_string(source_vertices) +
         "; fitted_vertices=" + std::to_string(fitted_vertices) +
         "; keys=" + std::to_string(key_count) +
         "; pruned_vertices=" + std::to_string(pruned_vertices) +
         "; bridge_prune_passes=" + std::to_string(passes) +
         "; bridge_prune_rounds=" + std::to_string(candidate_rounds) +
         "; bridge_prune_batch_vertices=" +
             std::to_string(batch_pruned_vertices) +
         "; attempts=" + std::to_string(attempts) +
         "; temporal_validation_error=" + std::to_string(best_error) +
         "; protected_corner_skips=" +
             std::to_string(protected_corner_skips) +
         BridgePruneTelemetryNotes(fit_failures, validation_failures,
                                   sharp_failures, accepted_candidates) +
         (preserve_sharp_corners ? "; sharp_corner_preserve=on":
                                   std::string{}) +
         "; bridge_prune_steps=" + JoinNotes(accepted_steps);
}

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
    const std::vector<std::string>& failures) {
  return "post_solve_vertex_reduction_rejected"
         "; mode=post_temporal_bridge_prune"
         "; source_vertices=" + std::to_string(source_vertices) +
         "; keys=" + std::to_string(key_count) +
         "; attempts=" + std::to_string(attempts) +
         "; protected_corner_skips=" +
             std::to_string(protected_corner_skips) +
         BridgePruneTelemetryNotes(fit_failures, validation_failures,
                                   sharp_failures, accepted_candidates) +
         (preserve_sharp_corners ? "; sharp_corner_preserve=on":
                                   std::string{}) +
         (failures.empty() ? std::string{}:
                             "; failures=" + JoinNotes(failures));
}

}  // namespace bbsolver
