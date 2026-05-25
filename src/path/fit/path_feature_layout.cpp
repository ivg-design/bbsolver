// Implements bbsolver::BuildShapeFlatFeatureFractionLayout (declared in
// path_frame_fit.hpp). PFF9 moves the canonical-layout assembly out of
// path_frame_fit.cpp's anonymous namespace; behavior is byte-faithful.
// Uses pff_cluster (PFF9 helpers), pff_anchor (PFF8), pff_fractions (PFF2),
// pff_geom (PFF1), and EvaluateFractionLayout (PFF3) through their headers.
//
// Diagnostics decision: **none / pure layout**. Acceptance-style helper that
// returns a PathFeatureFractionLayoutResult with status strings in `notes`
// and `warning`. No DiagnosticsWriter, no progress, no cancellation, no
// operator state. Diagnostics ownership: caller-owned.

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "bbsolver/path/geometry/path_feature_anchor.hpp"
#include "bbsolver/path/geometry/path_feature_cluster.hpp"
#include "bbsolver/path/geometry/path_fraction_helpers.hpp"
#include "bbsolver/path/fit/path_fraction_layout_eval.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit_geometry.hpp"

namespace bbsolver {

PathFeatureFractionLayoutResult BuildShapeFlatFeatureFractionLayout(
    const std::vector<std::vector<double>>& shape_flat_frames,
    int target_count,
    const PathFrameFitOptions& options) {
  using pff_anchor::FeatureClusterRadiusForCount;
  using pff_cluster::ClusterFeatureObservations;
  using pff_cluster::FeatureClusterRecord;
  using pff_cluster::FeatureClusterScore;
  using pff_cluster::FeatureObservation;
  using pff_cluster::PersistentFeatureFrameThreshold;
  using pff_fractions::FractionGap;
  using pff_fractions::FractionSegmentsByDescendingGap;
  using pff_fractions::FractionsInStrictSeamOrder;
  using pff_fractions::InsertFractionValue;
  using pff_fractions::InsertSplitFraction;
  using pff_fractions::LargestFractionGapSegment;
  using pff_fractions::kFractionEpsilon;
  using pff_geom::DecodeShapeFlat;
  using pff_geom::DecodedShape;

  PathFeatureFractionLayoutResult result;
  result.target_count = target_count;
  result.frame_count = static_cast<int>(shape_flat_frames.size());

  if (shape_flat_frames.empty()) {
    result.warning = "no shape_flat frames";
    return result;
  }
  const DecodedShape first_decoded = DecodeShapeFlat(shape_flat_frames.front());
  if (!first_decoded.ok) {
    result.warning = "malformed shape_flat frame";
    return result;
  }
  result.closed = first_decoded.closed;
  const int min_vertices = result.closed ? 3 : 2;
  if (target_count < min_vertices) {
    result.warning = "target_count below shape minimum";
    return result;
  }

  std::vector<FeatureObservation> feature_observations;
  for (std::size_t frame_index = 0; frame_index < shape_flat_frames.size(); ++frame_index) {
    const std::vector<double>& frame = shape_flat_frames[frame_index];
    const DecodedShape decoded = DecodeShapeFlat(frame);
    if (!decoded.ok) {
      result.warning = "malformed shape_flat frame";
      return result;
    }
    if (decoded.closed != result.closed) {
      result.warning = "mixed open/closed shape_flat frames";
      return result;
    }
    const std::vector<PathFeatureAnchor> anchors =
        ExtractShapeFlatFeatureAnchors(frame, options);
    feature_observations.reserve(feature_observations.size() + anchors.size());
    for (const PathFeatureAnchor& anchor : anchors) {
      feature_observations.push_back({
          anchor.outline_fraction,
          static_cast<int>(frame_index),
          anchor.turn_radians,
          anchor.zero_tangent_cue,
      });
    }
  }

  const double cluster_radius = FeatureClusterRadiusForCount(target_count);
  std::vector<FeatureClusterRecord> feature_clusters =
      ClusterFeatureObservations(
          std::move(feature_observations), result.closed, cluster_radius);
  const int clustered_feature_count = static_cast<int>(feature_clusters.size());
  const int persistence_threshold =
      PersistentFeatureFrameThreshold(result.frame_count);

  std::vector<double> layout;
  layout.push_back(0.0);
  if (!result.closed) {
    layout.push_back(1.0);
  }

  const int feature_slot_capacity =
      std::max(0, target_count - static_cast<int>(layout.size()));
  std::vector<FeatureClusterRecord> selected_features;
  selected_features.reserve(static_cast<std::size_t>(feature_slot_capacity));
  std::vector<FeatureClusterRecord> required_features;
  std::vector<FeatureClusterRecord> optional_features;
  for (const FeatureClusterRecord& cluster : feature_clusters) {
    const bool single_frame_layout = result.frame_count == 1;
    const bool strong_zero_tangent = cluster.zero_tangent_count > 0;
    const bool persistent =
        cluster.observed_frame_count >= persistence_threshold;
    if (single_frame_layout || strong_zero_tangent || persistent) {
      required_features.push_back(cluster);
    } else {
      optional_features.push_back(cluster);
    }
  }
  std::sort(required_features.begin(), required_features.end(),
            [](const FeatureClusterRecord& a, const FeatureClusterRecord& b) {
    const bool a_zero = a.zero_tangent_count > 0;
    const bool b_zero = b.zero_tangent_count > 0;
    if (a_zero != b_zero) {
      return a_zero > b_zero;
    }
    return FeatureClusterScore(a) > FeatureClusterScore(b);
  });

  int dropped_required_features = 0;
  for (const FeatureClusterRecord& cluster : required_features) {
    if (static_cast<int>(selected_features.size()) < feature_slot_capacity) {
      selected_features.push_back(cluster);
    } else {
      ++dropped_required_features;
    }
  }

  result.feature_count = static_cast<int>(selected_features.size());
  if (dropped_required_features > 0 && result.frame_count == 1) {
    result.feature_count = static_cast<int>(required_features.size());
    result.warning = "feature anchor count exceeds target_count";
    return result;
  }
  const int zero_tangent_required = static_cast<int>(
      std::count_if(required_features.begin(), required_features.end(),
                    [](const FeatureClusterRecord& cluster) {
                      return cluster.zero_tangent_count > 0;
                    }));
  if (zero_tangent_required > feature_slot_capacity) {
    result.feature_count = zero_tangent_required;
    result.warning = "zero-tangent feature anchors exceed target_count";
    return result;
  }

  std::sort(selected_features.begin(), selected_features.end(),
            [](const FeatureClusterRecord& a, const FeatureClusterRecord& b) {
    return a.fraction < b.fraction;
  });
  std::vector<double> feature_fractions;
  feature_fractions.reserve(selected_features.size());
  for (const FeatureClusterRecord& cluster : selected_features) {
    feature_fractions.push_back(cluster.fraction);
  }

  for (double feature_fraction : feature_fractions) {
    InsertFractionValue(&layout, feature_fraction, result.closed);
  }
  if (static_cast<int>(layout.size()) > target_count) {
    result.warning = "feature anchors plus endpoints exceed target_count";
    return result;
  }

  std::vector<std::vector<double>> sampled_frames;
  const std::vector<std::vector<double>>* layout_frames = &shape_flat_frames;
  constexpr int kMaxFeatureLayoutEvalFrames = 12;
  if (static_cast<int>(shape_flat_frames.size()) > kMaxFeatureLayoutEvalFrames) {
    sampled_frames.reserve(static_cast<std::size_t>(kMaxFeatureLayoutEvalFrames));
    int previous_idx = -1;
    for (int i = 0; i < kMaxFeatureLayoutEvalFrames; ++i) {
      const int idx = static_cast<int>(std::llround(
          static_cast<double>(i) *
          static_cast<double>(shape_flat_frames.size() - 1) /
          static_cast<double>(kMaxFeatureLayoutEvalFrames - 1)));
      if (idx != previous_idx) {
        sampled_frames.push_back(shape_flat_frames[static_cast<std::size_t>(idx)]);
        previous_idx = idx;
      }
    }
    layout_frames = &sampled_frames;
  }

  if (!sampled_frames.empty()) {
    while (static_cast<int>(layout.size()) < target_count) {
      const int segment = LargestFractionGapSegment(layout, result.closed);
      if (segment < 0 ||
          FractionGap(layout, segment, result.closed) <= kFractionEpsilon) {
        result.warning = "could not distribute remaining outline slots";
        return result;
      }
      InsertSplitFraction(&layout, segment, result.closed);
      if (!FractionsInStrictSeamOrder(layout, result.closed)) {
        result.warning = "generated invalid outline fraction order";
        return result;
      }
    }

    result.ok = true;
    result.outline_fractions = std::move(layout);
    result.notes = "clustered " + std::to_string(result.feature_count) +
                   "/" + std::to_string(clustered_feature_count) +
                   " persistent feature anchors across " +
                   std::to_string(result.frame_count) +
                   " frames; optional_features_skipped=" +
                   std::to_string(static_cast<int>(optional_features.size()) +
                                  dropped_required_features) +
                   "; persistence_threshold=" +
                   std::to_string(persistence_threshold) +
                   "; layout_eval_frames=geometric_large_sequence";
    return result;
  }

  // Error-guided gap filling: at each step try inserting at the worst-error
  // outline fraction (from a cross-frame evaluation) and at the top-4 largest
  // arc-length gap splits, then commit whichever candidate gives the lowest
  // max error across all frames. Once tolerance is met, switch to cheap
  // geometric gap splits for any remaining slots so we always return exactly
  // target_count fractions without paying per-frame evaluation cost needlessly.
  FractionLayoutEvaluation current_eval =
      EvaluateFractionLayout(*layout_frames, layout, options);
  if (!current_eval.ok) {
    result.warning = current_eval.warning.empty()
                         ? "initial feature layout unreplayable"
                         : current_eval.warning;
    return result;
  }

  const double tolerance = std::max(options.outline_tolerance, 0.0);
  while (static_cast<int>(layout.size()) < target_count) {
    if (current_eval.max_error <= tolerance + 1e-9) {
      // Already within tolerance: fill remaining slots geometrically.
      const int segment = LargestFractionGapSegment(layout, result.closed);
      if (segment < 0 ||
          FractionGap(layout, segment, result.closed) <= kFractionEpsilon) {
        result.warning = "could not distribute remaining outline slots";
        return result;
      }
      InsertSplitFraction(&layout, segment, result.closed);
      if (!FractionsInStrictSeamOrder(layout, result.closed)) {
        result.warning = "generated invalid outline fraction order";
        return result;
      }
      continue;
    }

    bool found = false;
    std::vector<double> best_fractions;
    FractionLayoutEvaluation best_eval;

    auto try_candidate = [&](std::vector<double> candidate) {
      if (!FractionsInStrictSeamOrder(candidate, result.closed)) {
        return;
      }
      FractionLayoutEvaluation eval =
          EvaluateFractionLayout(*layout_frames, candidate, options);
      if (eval.ok && (!found || eval.max_error < best_eval.max_error)) {
        found = true;
        best_fractions = std::move(candidate);
        best_eval = std::move(eval);
      }
    };

    if (current_eval.has_worst_fraction) {
      std::vector<double> candidate = layout;
      if (InsertFractionValue(&candidate, current_eval.worst_fraction,
                              result.closed)) {
        try_candidate(std::move(candidate));
      }
    }
    for (int seg : FractionSegmentsByDescendingGap(
             layout, result.closed, /*max_segments=*/4)) {
      std::vector<double> candidate = layout;
      InsertSplitFraction(&candidate, seg, result.closed);
      try_candidate(std::move(candidate));
    }

    if (found) {
      layout = std::move(best_fractions);
      current_eval = std::move(best_eval);
    } else {
      // No improvement found; fall back to largest-gap split.
      const int segment = LargestFractionGapSegment(layout, result.closed);
      if (segment < 0 ||
          FractionGap(layout, segment, result.closed) <= kFractionEpsilon) {
        result.warning = "could not distribute remaining outline slots";
        return result;
      }
      InsertSplitFraction(&layout, segment, result.closed);
      if (!FractionsInStrictSeamOrder(layout, result.closed)) {
        result.warning = "generated invalid outline fraction order";
        return result;
      }
      current_eval = EvaluateFractionLayout(*layout_frames, layout, options);
      if (!current_eval.ok) {
        result.warning = current_eval.warning;
        return result;
      }
    }
  }

  if (!FractionsInStrictSeamOrder(layout, result.closed) ||
      static_cast<int>(layout.size()) != target_count) {
    result.warning = "generated invalid outline fraction layout";
    return result;
  }

  result.ok = true;
  result.outline_fractions = std::move(layout);
  result.notes = "clustered " + std::to_string(result.feature_count) +
                 "/" + std::to_string(clustered_feature_count) +
                 " persistent feature anchors across " +
                 std::to_string(result.frame_count) +
                 " frames; optional_features_skipped=" +
                 std::to_string(static_cast<int>(optional_features.size()) +
                                dropped_required_features) +
                 "; persistence_threshold=" +
                 std::to_string(persistence_threshold) +
                 "; layout_eval_frames=" +
                 std::to_string(layout_frames->size());
  return result;
}

}  // namespace bbsolver
