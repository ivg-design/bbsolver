#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

#include <cassert>
#include <cstddef>
#include <utility>
#include <vector>

namespace {

constexpr int kClosedFlag = 1;

// Build a closed shape_flat polygon from {x,y} pairs with zero tangents.
std::vector<double> ShapeFlatPolygon(
    const std::vector<std::pair<double, double>>& vertices) {
  std::vector<double> out;
  out.reserve(2 + 6 * vertices.size());
  out.push_back(static_cast<double>(kClosedFlag));
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

void TestMalformedReturnsNotOk() {
  // Too few scalars to carry a single vertex.
  const std::vector<double> bad = {1.0, 4.0, 0.0, 0.0};
  const bbsolver::VisibleShapeFlatOutlineResult r =
      bbsolver::ExtractVisibleShapeFlatOutline(bad, bbsolver::PathFrameFitOptions{});
  assert(!r.ok);
  assert(!r.applied);
  assert(r.warning == "malformed shape_flat frame");
}

void TestOpenPathIsSkipped() {
  // 5 vertices but open. Visible-outline only operates on closed paths.
  std::vector<double> open_path = ShapeFlatPolygon(
      {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.5, 0.5}, {0.0, 1.0}});
  open_path[0] = 0.0;  // open
  const bbsolver::VisibleShapeFlatOutlineResult r =
      bbsolver::ExtractVisibleShapeFlatOutline(open_path, bbsolver::PathFrameFitOptions{});
  assert(r.ok);
  assert(!r.applied);
  assert(r.warning.find("open_or_too_small") != std::string::npos);
}

void TestTooFewVerticesIsSkipped() {
  // Closed triangle: visible-outline requires at least 4 source vertices.
  const std::vector<double> triangle =
      ShapeFlatPolygon({{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}});
  const bbsolver::VisibleShapeFlatOutlineResult r =
      bbsolver::ExtractVisibleShapeFlatOutline(triangle, bbsolver::PathFrameFitOptions{});
  assert(r.ok);
  assert(!r.applied);
  assert(r.warning.find("open_or_too_small") != std::string::npos);
}

void TestSimpleQuadHasNoSelfIntersection() {
  // Axis-aligned square, 4 vertices, no self-intersection. Result must be ok
  // but not applied; the source is already a simple polygon.
  const std::vector<double> square =
      ShapeFlatPolygon({{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}});
  const bbsolver::VisibleShapeFlatOutlineResult r =
      bbsolver::ExtractVisibleShapeFlatOutline(square, bbsolver::PathFrameFitOptions{});
  assert(r.ok);
  assert(r.closed);
  assert(r.source_vertex_count == 4);
  assert(!r.applied);
  assert(r.warning.find("no_self_intersections") != std::string::npos);
}

void TestFigureEightYieldsVisibleBoundary() {
  // Figure-eight ("bowtie") quadrilateral: vertex traversal order
  // (0,0) -> (1,1) -> (1,0) -> (0,1) crosses itself diagonally at (0.5, 0.5).
  // The visible outline of the filled region should be exactly one of the two
  // triangular lobes (the larger one by signed area), so the extractor must
  // return applied=true with a 3-vertex closed outline.
  const std::vector<double> bowtie = ShapeFlatPolygon(
      {{0.0, 0.0}, {1.0, 1.0}, {1.0, 0.0}, {0.0, 1.0}});

  const bbsolver::VisibleShapeFlatOutlineResult r =
      bbsolver::ExtractVisibleShapeFlatOutline(bowtie, bbsolver::PathFrameFitOptions{});

  assert(r.ok);
  assert(r.closed);
  assert(r.source_vertex_count == 4);
  assert(r.applied);
  assert(r.outline_vertex_count == 3);
  assert(r.warning.rfind("visible_outline; self_intersections=", 0) == 0);
  assert(r.warning.find("outline_vertices=3") != std::string::npos);

  // The emitted outline must itself be a valid closed shape_flat.
  assert(r.outline.size() == static_cast<std::size_t>(2 + 6 * r.outline_vertex_count));
  assert(static_cast<int>(std::llround(r.outline[0])) == 1);
  assert(static_cast<int>(std::llround(r.outline[1])) == r.outline_vertex_count);

  // Each emitted vertex should have zero tangents (the visible boundary is
  // re-fit later via FitShapeFlatFrame* and does not preserve source tangents).
  for (int vertex = 0; vertex < r.outline_vertex_count; ++vertex) {
    const std::size_t base = 2 + static_cast<std::size_t>(vertex) * 6;
    assert(r.outline[base + 2] == 0.0);
    assert(r.outline[base + 3] == 0.0);
    assert(r.outline[base + 4] == 0.0);
    assert(r.outline[base + 5] == 0.0);
  }
}

}  // namespace

int main() {
  TestMalformedReturnsNotOk();
  TestOpenPathIsSkipped();
  TestTooFewVerticesIsSkipped();
  TestSimpleQuadHasNoSelfIntersection();
  TestFigureEightYieldsVisibleBoundary();
  return 0;
}
