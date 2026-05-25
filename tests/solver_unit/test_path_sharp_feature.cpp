#include "bbsolver/path/geometry/path_sharp_feature.hpp"

#include <cassert>
#include <cstddef>
#include <utility>
#include <vector>

#include "bbsolver/path/frame_fit/path_frame_fit_geometry.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

namespace {

using bbsolver::pff_geom::DecodedShape;
using bbsolver::pff_geom::DecodeShapeFlat;
using bbsolver::pff_sharp::DetectSharpFeatureAtSourceVertex;
using bbsolver::pff_sharp::DetectSharpSourceVertices;
using bbsolver::pff_sharp::DetectTangentLockedSourceVertices;
using bbsolver::pff_sharp::SharpFeatureDecision;
using bbsolver::pff_sharp::TurnAngleAtSourceVertex;

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

void TestTurnAngleStraightLineIsZero() {
  // Three colinear vertices: interior angle = π, turn = 0.
  const std::vector<double> open = ShapeFlatPolygon(
      {{0.0, 0.0}, {1.0, 0.0}, {2.0, 0.0}}, false);
  const DecodedShape decoded = DecodeShapeFlat(open);
  // Middle vertex is the only interior one for an open path.
  assert(std::abs(TurnAngleAtSourceVertex(open, 1, decoded)) < 1e-9);
}

void TestTurnAngleRightAngleSquare() {
  // Closed unit square: every corner has a 90° turn.
  const std::vector<double> square = ShapeFlatPolygon(
      {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}, true);
  const DecodedShape decoded = DecodeShapeFlat(square);
  for (int i = 0; i < 4; ++i) {
    const double t = TurnAngleAtSourceVertex(square, i, decoded);
    assert(std::abs(t - bbsolver::pff_geom::kPi / 2.0) < 1e-9);
    assert(t >= kSharpTurnRadians);  // 90° is well past the sharp threshold
  }
}

void TestOpenEndpointTurnIsInfinity() {
  const std::vector<double> open = ShapeFlatPolygon(
      {{0.0, 0.0}, {1.0, 0.0}, {2.0, 0.0}}, false);
  const DecodedShape decoded = DecodeShapeFlat(open);
  assert(std::isinf(TurnAngleAtSourceVertex(open, 0, decoded)));
  assert(std::isinf(TurnAngleAtSourceVertex(open, decoded.vertex_count - 1, decoded)));
}

void TestZeroTangentsClassifier() {
  // ShapeFlatPolygon emits zero tangents by construction.
  const std::vector<double> square = ShapeFlatPolygon(
      {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}, true);
  const DecodedShape decoded = DecodeShapeFlat(square);
  for (int i = 0; i < 4; ++i) {
    assert(HasZeroTangentsAtSourceVertex(square, i, decoded));
  }
  // Out-of-range returns false.
  assert(!HasZeroTangentsAtSourceVertex(square, -1, decoded));
  assert(!HasZeroTangentsAtSourceVertex(square, decoded.vertex_count, decoded));
}

void TestDetectSharpRespectsSemanticAnchorOption() {
  const std::vector<double> square = ShapeFlatPolygon(
      {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}, true);
  const DecodedShape decoded = DecodeShapeFlat(square);

  bbsolver::PathFrameFitOptions opts;
  opts.source_vertices_are_semantic_anchors = true;
  // Square corner is required (geometric corner past kSharpTurnRadians).
  const SharpFeatureDecision required =
      DetectSharpFeatureAtSourceVertex(square, 1, decoded, opts);
  assert(required.required);
  assert(required.turn_radians >= kSharpTurnRadians);
  assert(required.zero_tangent_cue);  // tangents are zero by construction

  // Opt out: visible-outline-style. Decision must be default-empty.
  opts.source_vertices_are_semantic_anchors = false;
  const SharpFeatureDecision opt_out =
      DetectSharpFeatureAtSourceVertex(square, 1, decoded, opts);
  assert(!opt_out.required);
  assert(opt_out.turn_radians == 0.0);
  assert(!opt_out.zero_tangent_cue);
}

void TestDetectSharpSourceVerticesMarksOpenEndpoints() {
  const std::vector<double> open = ShapeFlatPolygon(
      {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}, {0.0, 2.0}}, false);
  const DecodedShape decoded = DecodeShapeFlat(open);
  bbsolver::PathFrameFitOptions opts;
  opts.source_vertices_are_semantic_anchors = true;

  const std::vector<bool> sharp =
      DetectSharpSourceVertices(open, decoded, opts);
  assert(sharp.size() == static_cast<std::size_t>(decoded.vertex_count));
  // Endpoints are always sharp for open paths.
  assert(sharp.front());
  assert(sharp.back());
  // Interior corners (the three middle vertices form right angles) are sharp.
  for (std::size_t i = 1; i + 1 < sharp.size(); ++i) {
    assert(sharp[i]);
  }
}

void TestDetectTangentLockedRespectsHardCutoff() {
  // Right-angle corner = π/2 ≈ 1.571 rad, well past kHardTangentLockTurnRadians (0.95).
  const std::vector<double> square = ShapeFlatPolygon(
      {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}, true);
  const DecodedShape decoded = DecodeShapeFlat(square);
  bbsolver::PathFrameFitOptions opts;
  opts.source_vertices_are_semantic_anchors = true;

  const std::vector<bool> locked =
      DetectTangentLockedSourceVertices(square, decoded, opts);
  for (bool flag : locked) {
    assert(flag);
  }
}

void TestSemanticAnchorOptOutEmptyResults() {
  const std::vector<double> square = ShapeFlatPolygon(
      {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}, true);
  const DecodedShape decoded = DecodeShapeFlat(square);
  bbsolver::PathFrameFitOptions opts;
  opts.source_vertices_are_semantic_anchors = false;

  // Sharp / locked classifiers return all-false when semantic anchors are off.
  const std::vector<bool> sharp =
      DetectSharpSourceVertices(square, decoded, opts);
  const std::vector<bool> locked =
      DetectTangentLockedSourceVertices(square, decoded, opts);
  for (bool flag : sharp) { assert(!flag); }
  for (bool flag : locked) { assert(!flag); }
}

}  // namespace

int main() {
  TestTurnAngleStraightLineIsZero();
  TestTurnAngleRightAngleSquare();
  TestOpenEndpointTurnIsInfinity();
  TestZeroTangentsClassifier();
  TestDetectSharpRespectsSemanticAnchorOption();
  TestDetectSharpSourceVerticesMarksOpenEndpoints();
  TestDetectTangentLockedRespectsHardCutoff();
  TestSemanticAnchorOptOutEmptyResults();
  return 0;
}
