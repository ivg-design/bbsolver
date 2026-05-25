#include "bbsolver/path/geometry/path_feature_anchor.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace {

using bbsolver::pff_anchor::SnapFractionsToFrameFeatureAnchors;

std::vector<double> ShapeFlatPolygon(
    const std::vector<std::pair<double, double>>& vertices,
    bool closed = true) {
  std::vector<double> out;
  out.reserve(2 + 6 * vertices.size());
  out.push_back(closed ? 1.0 : 0.0);
  out.push_back(static_cast<double>(vertices.size()));
  for (const auto& v : vertices) {
    out.push_back(v.first);
    out.push_back(v.second);
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
  }
  return out;
}

void TestFeatureClusterRadiusForCountClamps() {
  // target <= 0 -> default 0.02.
  assert(std::abs(FeatureClusterRadiusForCount(0) - 0.02) < 1e-12);
  assert(std::abs(FeatureClusterRadiusForCount(-3) - 0.02) < 1e-12);
  // 0.5 / 4 = 0.125 -> clamped to upper bound 0.05.
  assert(std::abs(FeatureClusterRadiusForCount(4) - 0.05) < 1e-12);
  // 0.5 / 100 = 0.005 -> clamped to lower bound 0.015.
  assert(std::abs(FeatureClusterRadiusForCount(100) - 0.015) < 1e-12);
  // 0.5 / 12 ≈ 0.0417 -> inside [0.015, 0.05], passes through.
  assert(std::abs(FeatureClusterRadiusForCount(12) - 0.5 / 12.0) < 1e-12);
}

void TestExtractAnchorsMalformedReturnsEmpty() {
  const std::vector<double> bad = {1.0, 4.0, 0.0, 0.0};
  const std::vector<bbsolver::PathFeatureAnchor> anchors =
      bbsolver::ExtractShapeFlatFeatureAnchors(bad, bbsolver::PathFrameFitOptions{});
  assert(anchors.empty());
}

void TestExtractAnchorsClosedSquareFindsFourCorners() {
  // Closed unit square, zero tangents. Every corner is a sharp feature.
  const std::vector<double> square = ShapeFlatPolygon(
      {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}, true);

  bbsolver::PathFrameFitOptions opts;
  opts.source_vertices_are_semantic_anchors = true;
  const std::vector<bbsolver::PathFeatureAnchor> anchors =
      bbsolver::ExtractShapeFlatFeatureAnchors(square, opts);

  assert(anchors.size() == 4);
  // Fractions are strictly seam-ordered (sorted + deduped by the extractor).
  for (std::size_t i = 1; i < anchors.size(); ++i) {
    assert(anchors[i].outline_fraction > anchors[i - 1].outline_fraction);
  }
  // Each anchor carries a valid source vertex index (0..3) and zero-tangent cue.
  for (const auto& a : anchors) {
    assert(a.source_vertex_index >= 0);
    assert(a.source_vertex_index < 4);
    assert(a.turn_radians > 0.0);
    assert(a.zero_tangent_cue);
  }
}

void TestExtractAnchorsSemanticOptOutEmpty() {
  const std::vector<double> square = ShapeFlatPolygon(
      {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}, true);
  bbsolver::PathFrameFitOptions opts;
  opts.source_vertices_are_semantic_anchors = false;
  const std::vector<bbsolver::PathFeatureAnchor> anchors =
      bbsolver::ExtractShapeFlatFeatureAnchors(square, opts);
  assert(anchors.empty());
}

void TestSnapWithNoAnchorsReturnsInput() {
  // 4-vertex closed polygon with NO sharp anchors when semantic-anchors are off.
  const std::vector<double> square = ShapeFlatPolygon(
      {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}, true);
  bbsolver::PathFrameFitOptions opts;
  opts.source_vertices_are_semantic_anchors = false;
  const std::vector<double> canonical = {0.0, 0.27, 0.51, 0.74};
  const std::vector<double> snapped =
      SnapFractionsToFrameFeatureAnchors(canonical, square, opts, true);
  assert(snapped == canonical);
}

void TestSnapPullsCanonicalSlotsToNearbyAnchors() {
  // 4-vertex closed square: anchors land at {0.0, 0.25, 0.5, 0.75} along the
  // unit perimeter (each edge is 1.0, perimeter is 4.0, corner fractions are
  // multiples of 1/4 = 0.25). A canonical layout offset by 0.01 should snap
  // every slot to the exact corner fraction.
  const std::vector<double> square = ShapeFlatPolygon(
      {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}, true);
  bbsolver::PathFrameFitOptions opts;
  opts.source_vertices_are_semantic_anchors = true;

  const std::vector<double> anchors_fractions = {0.0, 0.25, 0.5, 0.75};
  // Use 3 slots so snap_radius is computed from FeatureClusterRadiusForCount(3).
  // canonical[0] = 0.0 is at seam, leave it; canonical[1] near 0.25, canonical[2] near 0.5.
  const std::vector<double> canonical = {0.0, 0.24, 0.51};
  const std::vector<double> snapped =
      SnapFractionsToFrameFeatureAnchors(canonical, square, opts, true);
  assert(snapped.size() == canonical.size());
  // Each slot should now lie on one of the corner fractions.
  for (double s : snapped) {
    bool matched = false;
    for (double a : anchors_fractions) {
      if (std::abs(s - a) <= 1e-6) {
        matched = true;
        break;
      }
    }
    assert(matched);
  }
}

void TestSnapPreservesOpenEndpoints() {
  // Open path: snap must NOT move the first/last slots.
  const std::vector<double> open = ShapeFlatPolygon(
      {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}, false);
  bbsolver::PathFrameFitOptions opts;
  opts.source_vertices_are_semantic_anchors = true;

  const std::vector<double> canonical = {0.0, 0.31, 0.62, 1.0};
  const std::vector<double> snapped =
      SnapFractionsToFrameFeatureAnchors(canonical, open, opts, false);
  // Endpoints unchanged.
  assert(snapped.front() == 0.0);
  assert(snapped.back() == 1.0);
}

void TestSnapNormalizationRejectionReturnsInput() {
  // Pathological: provide canonical fractions that would NOT normalize after
  // a snap (e.g., a singleton). NormalizeOutlineFractions rejects an empty
  // post-snap or out-of-order layout.
  const std::vector<double> square = ShapeFlatPolygon(
      {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}, true);
  bbsolver::PathFrameFitOptions opts;
  opts.source_vertices_are_semantic_anchors = true;
  // Empty input -> snapped stays empty; NormalizeOutlineFractions returns
  // false for an empty layout, so the helper returns the input unchanged.
  const std::vector<double> canonical = {};
  const std::vector<double> snapped =
      SnapFractionsToFrameFeatureAnchors(canonical, square, opts, true);
  assert(snapped.empty());
}

}  // namespace

int main() {
  TestFeatureClusterRadiusForCountClamps();
  TestExtractAnchorsMalformedReturnsEmpty();
  TestExtractAnchorsClosedSquareFindsFourCorners();
  TestExtractAnchorsSemanticOptOutEmpty();
  TestSnapWithNoAnchorsReturnsInput();
  TestSnapPullsCanonicalSlotsToNearbyAnchors();
  TestSnapPreservesOpenEndpoints();
  TestSnapNormalizationRejectionReturnsInput();
  return 0;
}
