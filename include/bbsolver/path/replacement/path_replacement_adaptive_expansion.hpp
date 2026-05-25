#pragma once

#include "bbsolver/domain.hpp"

#include <functional>
#include <vector>

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/path/replacement/path_replacement_fraction_layout.hpp"

namespace bbsolver {

using ReplacementAdaptiveFractionTryFn =
    std::function<bool(const std::vector<double>& fractions,
                       int seed_idx,
                       int adaptive_count)>;

struct ReplacementAdaptiveExpansionResult {
  bool applied = false;
  int evaluations = 0;
  double best_attempt_error = 0.0;
};

ReplacementAdaptiveExpansionResult TryReplacementAdaptiveFractionExpansion(
    const std::vector<std::vector<double>>& shape_flat_frames,
    const std::vector<ReplacementFrameFitRecord>& phase2_records,
    const std::vector<int>& seed_indices,
    const SolverConfig& config,
    const PathFrameFitOptions& coherence_options,
    int source_min_vertices,
    int target_vertices,
    const ReplacementAdaptiveFractionTryFn& try_fraction_layout);

}  // namespace bbsolver
