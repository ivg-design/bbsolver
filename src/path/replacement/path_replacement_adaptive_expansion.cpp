#include "bbsolver/path/replacement/path_replacement_adaptive_expansion.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/path/replacement/path_replacement_fraction_layout.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <vector>

namespace bbsolver {

ReplacementAdaptiveExpansionResult TryReplacementAdaptiveFractionExpansion(
    const std::vector<std::vector<double>>& shape_flat_frames,
    const std::vector<ReplacementFrameFitRecord>& phase2_records,
    const std::vector<int>& seed_indices,
    const SolverConfig& config,
    const PathFrameFitOptions& coherence_options,
    int source_min_vertices,
    int target_vertices,
    const ReplacementAdaptiveFractionTryFn& try_fraction_layout) {
  ReplacementAdaptiveExpansionResult result;
  result.best_attempt_error = std::numeric_limits<double>::infinity();
  if (seed_indices.empty() || !try_fraction_layout) {
    return result;
  }

  int adaptive_max_vertices =
      std::min(source_min_vertices - 1, target_vertices + 12);
  if (config.path_replacement_max_vertices > 0) {
    adaptive_max_vertices =
        std::min(adaptive_max_vertices, config.path_replacement_max_vertices);
  }
  if (adaptive_max_vertices <= target_vertices) {
    return result;
  }

  for (int seed_idx: seed_indices) {
    if (seed_idx < 0 || seed_idx >= static_cast<int>(phase2_records.size())) {
      continue;
    }
    PathFractionExpansionOptions expansion_options;
    expansion_options.max_fraction_count = adaptive_max_vertices;
    expansion_options.max_insertions =
        std::min(4, std::max(0, adaptive_max_vertices - target_vertices));
    expansion_options.max_candidate_gaps_per_pass = 2;
    expansion_options.min_error_improvement = 0.0;
    const PathFractionExpansionResult expanded =
        ExpandShapeFlatOutlineFractions(
            shape_flat_frames,
            phase2_records[static_cast<std::size_t>(seed_idx)]
.outline_fractions,
            coherence_options,
            expansion_options);
    result.evaluations += expanded.candidate_evaluations;
    if (expanded.ok &&
        expanded.final_max_outline_error < result.best_attempt_error) {
      result.best_attempt_error = expanded.final_max_outline_error;
    }
    if (expanded.ok && expanded.tolerance_met &&
        static_cast<int>(expanded.outline_fractions.size()) > target_vertices) {
      result.applied = try_fraction_layout(expanded.outline_fractions,
                                           seed_idx,
                                           expanded.insertions);
      if (result.applied) {
        break;
      }
    }
  }
  return result;
}

}  // namespace bbsolver
