// Implements bbsolver::ExtractShapeFlatFeatureAnchors (declared in
// path_frame_fit.hpp, public surface) and bbsolver::pff_anchor::SnapFractionsToFrameFeatureAnchors
// (declared in path_feature_anchor.hpp). PFF8 moves both bodies byte-faithfully
// out of path_frame_fit.cpp's anonymous namespace; no algorithmic change.
//
// Diagnostics decision: **none / pure layout**. The anchor extractor returns
// an empty vector for malformed inputs / zero-perimeter frames; the snap
// helper returns the input layout unchanged when no anchors apply. No
// DiagnosticsWriter, no progress, no cancellation, no operator state.
// Diagnostics ownership: caller-owned.

#include "bbsolver/path/geometry/path_feature_anchor.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "bbsolver/path/dense/path_dense_polyline.hpp"
#include "bbsolver/path/geometry/path_fraction_helpers.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit_geometry.hpp"
#include "bbsolver/path/geometry/path_sharp_feature.hpp"

namespace bbsolver {

std::vector<PathFeatureAnchor> ExtractShapeFlatFeatureAnchors(
    const std::vector<double>& shape_flat,
    const PathFrameFitOptions& options) {
  std::vector<PathFeatureAnchor> anchors;
  const pff_geom::DecodedShape decoded = pff_geom::DecodeShapeFlat(shape_flat);
  if (!decoded.ok || decoded.vertex_count <= 0) {
    return anchors;
  }

  std::vector<int> source_to_dense;
  const std::vector<DensePoint> dense =
      pff_dense::ShapeFlatToDensePolyline(shape_flat, options, &source_to_dense);
  if (dense.empty()) {
    return anchors;
  }
  const double total_length = pff_dense::DensePerimeter(dense, decoded.closed);
  if (!(total_length > 1e-9)) {
    return anchors;
  }
  const std::vector<double> arcs = pff_dense::DenseArcPositions(dense, decoded.closed);

  anchors.reserve(static_cast<std::size_t>(decoded.vertex_count));
  for (int source_index = 0; source_index < decoded.vertex_count; ++source_index) {
    const pff_sharp::SharpFeatureDecision decision =
        pff_sharp::DetectSharpFeatureAtSourceVertex(shape_flat, source_index, decoded, options);
    if (!decision.required ||
        source_index >= static_cast<int>(source_to_dense.size()) ||
        source_to_dense[static_cast<std::size_t>(source_index)] < 0) {
      continue;
    }
    PathFeatureAnchor anchor;
    anchor.source_vertex_index = source_index;
    anchor.outline_fraction =
        pff_dense::DenseFractionAtIndex(
            dense,
            arcs,
            decoded.closed,
            source_to_dense[static_cast<std::size_t>(source_index)],
            total_length);
    anchor.turn_radians = decision.turn_radians;
    anchor.zero_tangent_cue = decision.zero_tangent_cue;
    anchors.push_back(anchor);
  }

  std::sort(anchors.begin(), anchors.end(), [](const PathFeatureAnchor& a,
                                               const PathFeatureAnchor& b) {
    return a.outline_fraction < b.outline_fraction;
  });
  anchors.erase(
      std::unique(anchors.begin(), anchors.end(), [](const PathFeatureAnchor& a,
                                                     const PathFeatureAnchor& b) {
        return std::abs(a.outline_fraction - b.outline_fraction) <=
               pff_fractions::kFractionEpsilon;
      }),
      anchors.end());
  return anchors;
}

namespace pff_anchor {

std::vector<double> SnapFractionsToFrameFeatureAnchors(
    const std::vector<double>& canonical_fractions,
    const std::vector<double>& shape_flat,
    const PathFrameFitOptions& options,
    bool closed) {
  const std::vector<PathFeatureAnchor> anchors =
      ExtractShapeFlatFeatureAnchors(shape_flat, options);
  if (anchors.empty()) {
    return canonical_fractions;
  }

  const double snap_radius =
      FeatureClusterRadiusForCount(static_cast<int>(canonical_fractions.size()));
  std::vector<double> snapped = canonical_fractions;
  std::vector<bool> used(anchors.size(), false);
  for (std::size_t i = 0; i < canonical_fractions.size(); ++i) {
    if (!closed && (i == 0 || i + 1 == canonical_fractions.size())) {
      continue;
    }
    int best_anchor = -1;
    double best_distance = snap_radius;
    for (std::size_t anchor_index = 0; anchor_index < anchors.size(); ++anchor_index) {
      if (used[anchor_index]) {
        continue;
      }
      const double distance = pff_fractions::FractionDistance(
          canonical_fractions[i], anchors[anchor_index].outline_fraction, closed);
      if (distance <= best_distance) {
        best_distance = distance;
        best_anchor = static_cast<int>(anchor_index);
      }
    }
    if (best_anchor >= 0) {
      snapped[i] = anchors[static_cast<std::size_t>(best_anchor)].outline_fraction;
      used[static_cast<std::size_t>(best_anchor)] = true;
    }
  }

  std::vector<double> normalized;
  if (!pff_fractions::NormalizeOutlineFractions(snapped, closed, &normalized) ||
      normalized.size() != canonical_fractions.size()) {
    return canonical_fractions;
  }
  return normalized;
}

}  // namespace pff_anchor
}  // namespace bbsolver
