#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

#include <cassert>
#include <utility>
#include <vector>

namespace {

std::vector<double> ShapeFlatPolygon(
    const std::vector<std::pair<double, double>>& vertices,
    bool closed = true) {
  std::vector<double> out;
  out.reserve(2 + 6 * vertices.size());
  out.push_back(closed ? 1.0: 0.0);
  out.push_back(static_cast<double>(vertices.size()));
  for (const auto& v: vertices) {
    out.push_back(v.first);
    out.push_back(v.second);
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
  }
  return out;
}

void TestBuildPolylineRejectsMalformed() {
  const std::vector<double> bad = {1.0, 4.0, 0.0, 0.0};  // too short
  const bbsolver::ShapeFlatOutlinePolyline poly =
      bbsolver::BuildShapeFlatOutlinePolyline(bad, bbsolver::PathFrameFitOptions{});
  assert(!poly.ok);
  assert(poly.points.empty());
}

void TestBuildPolylineGoodFrame() {
  const std::vector<double> square = ShapeFlatPolygon(
      {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}, true);
  const bbsolver::ShapeFlatOutlinePolyline poly =
      bbsolver::BuildShapeFlatOutlinePolyline(square, bbsolver::PathFrameFitOptions{});
  assert(poly.ok);
  assert(poly.closed);
  assert(!poly.points.empty());
}

void TestIdenticalFramesYieldZeroError() {
  const std::vector<double> square = ShapeFlatPolygon(
      {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}, true);
  const double err = bbsolver::ShapeFlatFrameOutlineError(
      square, square, bbsolver::PathFrameFitOptions{});
  assert(err <= 1e-9);
}

void TestMixedOpenClosedYieldsInfinity() {
  const std::vector<double> closed_square = ShapeFlatPolygon(
      {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}, true);
  std::vector<double> open_square = closed_square;
  open_square[0] = 0.0;
  const double err = bbsolver::ShapeFlatFrameOutlineError(
      closed_square, open_square, bbsolver::PathFrameFitOptions{});
  assert(std::isinf(err));
}

void TestMalformedSourceOrFittedYieldsInfinity() {
  const std::vector<double> bad = {1.0, 4.0, 0.0, 0.0};
  const std::vector<double> good = ShapeFlatPolygon(
      {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}, true);
  assert(std::isinf(bbsolver::ShapeFlatFrameOutlineError(
      bad, good, bbsolver::PathFrameFitOptions{})));
  assert(std::isinf(bbsolver::ShapeFlatFrameOutlineError(
      good, bad, bbsolver::PathFrameFitOptions{})));
}

void TestNonZeroErrorBetweenShiftedSquares() {
  const std::vector<double> src = ShapeFlatPolygon(
      {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}, true);
  // Shift the second frame by 0.5 in x; outline error should be > 0.
  const std::vector<double> shifted = ShapeFlatPolygon(
      {{0.5, 0.0}, {1.5, 0.0}, {1.5, 1.0}, {0.5, 1.0}}, true);
  const double err = bbsolver::ShapeFlatFrameOutlineError(
      src, shifted, bbsolver::PathFrameFitOptions{});
  assert(err > 0.0);
  // Bound: the symmetric distance from a shifted unit square ≤ shift (=0.5).
  assert(err <= 0.5 + 1e-9);
}

void TestCutoffShortCircuit() {
  const std::vector<double> src = ShapeFlatPolygon(
      {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}, true);
  const std::vector<double> shifted = ShapeFlatPolygon(
      {{0.5, 0.0}, {1.5, 0.0}, {1.5, 1.0}, {0.5, 1.0}}, true);
  // Cutoff 0.1; error is ~0.5, so cutoff should return > 0.1 quickly.
  const double err = bbsolver::ShapeFlatFrameOutlineError(
      src, shifted, bbsolver::PathFrameFitOptions{}, /*cutoff_error=*/0.1);
  assert(err > 0.1);
}

void TestErrorFromPolylinesMatchesEndToEnd() {
  const std::vector<double> a = ShapeFlatPolygon(
      {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}, true);
  const std::vector<double> b = ShapeFlatPolygon(
      {{0.5, 0.0}, {1.5, 0.0}, {1.5, 1.0}, {0.5, 1.0}}, true);
  const bbsolver::PathFrameFitOptions opts;
  const bbsolver::ShapeFlatOutlinePolyline pa =
      bbsolver::BuildShapeFlatOutlinePolyline(a, opts);
  const bbsolver::ShapeFlatOutlinePolyline pb =
      bbsolver::BuildShapeFlatOutlinePolyline(b, opts);
  const double from_polylines =
      bbsolver::ShapeFlatFrameOutlineErrorFromPolylines(pa, pb);
  const double end_to_end = bbsolver::ShapeFlatFrameOutlineError(a, b, opts);
  assert(std::abs(from_polylines - end_to_end) <= 1e-9);
}

}  // namespace

int main() {
  TestBuildPolylineRejectsMalformed();
  TestBuildPolylineGoodFrame();
  TestIdenticalFramesYieldZeroError();
  TestMixedOpenClosedYieldsInfinity();
  TestMalformedSourceOrFittedYieldsInfinity();
  TestNonZeroErrorBetweenShiftedSquares();
  TestCutoffShortCircuit();
  TestErrorFromPolylinesMatchesEndToEnd();
  return 0;
}
