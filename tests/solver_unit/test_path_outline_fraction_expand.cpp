#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace {

constexpr int kClosedFlag = 1;

std::vector<double> ShapeFlatPolygon(
    const std::vector<std::pair<double, double>>& vertices,
    bool closed = true) {
  std::vector<double> out;
  out.reserve(2 + 6 * vertices.size());
  out.push_back(closed ? static_cast<double>(kClosedFlag): 0.0);
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

// 8-vertex regular polygon on the unit circle (closed).
std::vector<double> ClosedOctagon() {
  std::vector<std::pair<double, double>> verts;
  for (int i = 0; i < 8; ++i) {
    const double a = 2.0 * 3.14159265358979323846 * static_cast<double>(i) / 8.0;
    verts.emplace_back(std::cos(a), std::sin(a));
  }
  return ShapeFlatPolygon(verts);
}

void TestEmptyFramesReturnsNoShapeFlatFrames() {
  const bbsolver::PathFractionExpansionResult r =
      bbsolver::ExpandShapeFlatOutlineFractions(
          {}, {0.0, 0.25, 0.5, 0.75},
          bbsolver::PathFrameFitOptions{}, bbsolver::PathFractionExpansionOptions{});
  assert(!r.ok);
  assert(!r.applied);
  assert(r.warning == "no shape_flat frames");
  // Result still mirrors the seed.
  assert(r.outline_fractions == std::vector<double>({0.0, 0.25, 0.5, 0.75}));
  assert(r.initial_fraction_count == 4);
  assert(r.final_fraction_count == 4);
}

void TestMalformedFrameReportsMalformed() {
  // Frame with too-few scalars: header says 4 vertices but only 8 doubles.
  std::vector<double> bad = {1.0, 4.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
  const bbsolver::PathFractionExpansionResult r =
      bbsolver::ExpandShapeFlatOutlineFractions(
          {bad}, {0.0, 0.25, 0.5, 0.75},
          bbsolver::PathFrameFitOptions{}, bbsolver::PathFractionExpansionOptions{});
  assert(!r.ok);
  assert(!r.applied);
  assert(r.warning == "malformed shape_flat frame");
}

void TestMixedOpenClosedReportsMix() {
  const std::vector<double> closed_quad = ShapeFlatPolygon(
      {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}, true);
  std::vector<double> open_quad = closed_quad;
  open_quad[0] = 0.0;  // flip closed flag for the second frame
  const bbsolver::PathFractionExpansionResult r =
      bbsolver::ExpandShapeFlatOutlineFractions(
          {closed_quad, open_quad}, {0.0, 0.25, 0.5, 0.75},
          bbsolver::PathFrameFitOptions{}, bbsolver::PathFractionExpansionOptions{});
  assert(!r.ok);
  assert(r.warning == "mixed open/closed shape_flat frames");
}

void TestInvalidSeedFractionsRejected() {
  const std::vector<double> octagon = ClosedOctagon();
  // Non-monotone fractions trip NormalizeOutlineFractions.
  const bbsolver::PathFractionExpansionResult r =
      bbsolver::ExpandShapeFlatOutlineFractions(
          {octagon}, {0.5, 0.25},
          bbsolver::PathFrameFitOptions{}, bbsolver::PathFractionExpansionOptions{});
  assert(!r.ok);
  assert(r.warning == "invalid outline fractions");
}

void TestSeedCountReachesSourceVerticesIsRejected() {
  // 4-vertex closed square; seeding with 4 fractions trips the
  // "outline fraction count reaches source vertex count" guard.
  const std::vector<double> square = ShapeFlatPolygon(
      {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}, true);
  const bbsolver::PathFractionExpansionResult r =
      bbsolver::ExpandShapeFlatOutlineFractions(
          {square}, {0.0, 0.25, 0.5, 0.75},
          bbsolver::PathFrameFitOptions{}, bbsolver::PathFractionExpansionOptions{});
  assert(!r.ok);
  assert(r.warning == "outline fraction count reaches source vertex count");
}

void TestClosedSeedMustStartAtSeam() {
  const std::vector<double> octagon = ClosedOctagon();
  // Closed seam contract: first fraction must be ~0.
  const bbsolver::PathFractionExpansionResult r =
      bbsolver::ExpandShapeFlatOutlineFractions(
          {octagon}, {0.1, 0.4, 0.7},
          bbsolver::PathFrameFitOptions{}, bbsolver::PathFractionExpansionOptions{});
  assert(!r.ok);
  assert(r.warning == "closed outline fractions must start at source seam");
}

void TestToleranceMetEarlyExitsWithNoInsertions() {
  // A 4-fraction seed against an 8-vertex octagon under a very lax
  // tolerance should pass the initial EvaluateFractionLayout check and
  // exit without inserting anything.
  const std::vector<double> octagon = ClosedOctagon();
  bbsolver::PathFrameFitOptions opts;
  opts.outline_tolerance = 5.0;  // larger than any meaningful chord error

  const bbsolver::PathFractionExpansionResult r =
      bbsolver::ExpandShapeFlatOutlineFractions(
          {octagon}, {0.0, 0.25, 0.5, 0.75}, opts,
          bbsolver::PathFractionExpansionOptions{});

  assert(r.ok);
  assert(!r.applied);  // tolerance was met immediately; no insertions
  assert(r.tolerance_met);
  assert(r.initial_fraction_count == 4);
  assert(r.final_fraction_count == 4);
  assert(r.insertions == 0);
  assert(r.closed);
  assert(r.outline_fractions.size() == 4);
  assert(std::abs(r.outline_fractions.front()) < 1e-9);
}

}  // namespace

int main() {
  TestEmptyFramesReturnsNoShapeFlatFrames();
  TestMalformedFrameReportsMalformed();
  TestMixedOpenClosedReportsMix();
  TestInvalidSeedFractionsRejected();
  TestSeedCountReachesSourceVerticesIsRejected();
  TestClosedSeedMustStartAtSeam();
  TestToleranceMetEarlyExitsWithNoInsertions();
  return 0;
}
