#include "bbsolver/path/replacement/path_replacement_feature_layout_trial.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace bbsolver {

ReplacementFeatureLayoutTrialResult TryReplacementFeatureFractionLayout(
    const std::vector<std::vector<double>>& shape_flat_frames,
    int target_vertices,
    int source_min_vertices,
    const SolverConfig& config,
    const PathFrameFitOptions& frame_options,
    const ReplacementFractionLayoutTryFn& try_fraction_layout) {
  ReplacementFeatureLayoutTrialResult result;
  if (shape_flat_frames.empty() || !try_fraction_layout) {
    return result;
  }

  int feature_max_vertices =
      std::min(source_min_vertices - 1,
               std::max(target_vertices, source_min_vertices * 3 / 4));
  if (config.path_replacement_max_vertices > 0) {
    feature_max_vertices =
        std::min(feature_max_vertices, config.path_replacement_max_vertices);
  }

  constexpr int kMaxFeatureLayoutFrames = 8;
  constexpr int kMaxFeatureTargetRange = 2;
  const int feature_max_vertices_bounded =
      std::min(feature_max_vertices, target_vertices + kMaxFeatureTargetRange);

  const std::vector<std::vector<double>>* feature_frames_ptr =
      &shape_flat_frames;
  std::vector<std::vector<double>> feature_frames_subsampled;
  if (static_cast<int>(shape_flat_frames.size()) > kMaxFeatureLayoutFrames) {
    feature_frames_subsampled.reserve(
        static_cast<std::size_t>(kMaxFeatureLayoutFrames));
    const int n = static_cast<int>(shape_flat_frames.size());
    for (int fi = 0; fi < kMaxFeatureLayoutFrames; ++fi) {
      const int idx = fi * (n - 1) / (kMaxFeatureLayoutFrames - 1);
      feature_frames_subsampled.push_back(
          shape_flat_frames[static_cast<std::size_t>(idx)]);
    }
    feature_frames_ptr = &feature_frames_subsampled;
  }

  for (int feature_target = target_vertices;
       feature_target <= feature_max_vertices_bounded && !result.applied;
       ++feature_target) {
    const PathFeatureFractionLayoutResult layout =
        BuildShapeFlatFeatureFractionLayout(
            *feature_frames_ptr, feature_target, frame_options);
    ++result.targets_tried;
    result.feature_anchor_count =
        std::max(result.feature_anchor_count, layout.feature_count);
    if (!layout.warning.empty()) {
      result.warning = layout.warning;
    }
    if (!layout.ok) {
      continue;
    }
    if (try_fraction_layout(layout.outline_fractions,
                            /*seed_idx=*/-1,
                            /*adaptive_count=*/0)) {
      result.applied = true;
      result.target_vertices = feature_target;
    }
  }
  return result;
}

}  // namespace bbsolver
