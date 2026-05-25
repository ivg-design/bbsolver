#pragma once

#include "bbsolver/domain.hpp"

#include <functional>
#include <string>
#include <vector>

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

namespace bbsolver {

using ReplacementFractionLayoutTryFn =
    std::function<bool(const std::vector<double>& fractions,
                       int seed_idx,
                       int adaptive_count)>;

struct ReplacementFeatureLayoutTrialResult {
  bool applied = false;
  int feature_anchor_count = 0;
  int targets_tried = 0;
  int target_vertices = 0;
  std::string warning;
};

ReplacementFeatureLayoutTrialResult TryReplacementFeatureFractionLayout(
    const std::vector<std::vector<double>>& shape_flat_frames,
    int target_vertices,
    int source_min_vertices,
    const SolverConfig& config,
    const PathFrameFitOptions& frame_options,
    const ReplacementFractionLayoutTryFn& try_fraction_layout);

}  // namespace bbsolver
