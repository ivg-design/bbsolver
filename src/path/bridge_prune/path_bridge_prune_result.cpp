#include "bbsolver/path/bridge_prune/path_bridge_prune_result.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/path/bridge_prune/path_bridge_prune.hpp"
#include "bbsolver/path/bridge_prune/path_bridge_prune_notes.hpp"
#include "bbsolver/path/reduction/path_vertex_reduction.hpp"

#include <utility>
#include <string>

namespace bbsolver {

PostSolvePathVertexReductionResult BuildBridgePruneAcceptedResult(
    PropertyKeys keys,
    const BridgePruneResultSummary& summary) {
  PostSolvePathVertexReductionResult result;
  result.accepted = true;
  result.attempted = true;
  result.keys = std::move(keys);
  result.source_vertices = summary.reported_source_vertices;
  result.fitted_vertices = MaxShapeFlatKeyVertexCount(result.keys);
  result.max_outline_error = summary.best_error;
  result.notes = BuildBridgePruneAcceptedNote(
      summary.reported_source_vertices, result.fitted_vertices,
      result.keys.keys.size(), summary.pruned_vertices, summary.passes,
      summary.candidate_rounds, summary.batch_pruned_vertices,
      summary.attempts, summary.best_error, summary.protected_corner_skips,
      summary.outcomes.fit_failures, summary.outcomes.validation_failures,
      summary.outcomes.sharp_failures, summary.outcomes.accepted_candidates,
      summary.preserve_sharp_corners, summary.accepted_steps);
  return result;
}

std::string BuildBridgePruneRejectedResultNote(
    const PropertyKeys& solved_keys,
    const BridgePruneResultSummary& summary) {
  return BuildBridgePruneRejectedNote(
      summary.reported_source_vertices, solved_keys.keys.size(),
      summary.attempts, summary.protected_corner_skips,
      summary.outcomes.fit_failures, summary.outcomes.validation_failures,
      summary.outcomes.sharp_failures, summary.outcomes.accepted_candidates,
      summary.preserve_sharp_corners, summary.outcomes.failures);
}

}  // namespace bbsolver
