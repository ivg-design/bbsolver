#include "bbsolver/path/geometry/path_geometry_refinement.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

#include <cassert>
#include <cmath>
#include <string>
#include <utility>
#include <vector>
#include <cstddef>

namespace {

constexpr double kPi = 3.14159265358979323846;

// closed shape_flat: [closed_flag, n_vertices, (x, y, in_x, in_y, out_x, out_y) * n]
std::vector<double> ShapeFlatPolygon(
    const std::vector<std::pair<double, double>>& vertices) {
  std::vector<double> out;
  out.reserve(2 + 6 * vertices.size());
  out.push_back(1.0);
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

std::vector<double> ShapeFlatRect(double x, double y, double w, double h) {
  return ShapeFlatPolygon({{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}});
}

std::vector<double> ShapeFlatRegularPolygon(int n, double radius) {
  std::vector<std::pair<double, double>> vertices;
  vertices.reserve(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    const double angle = 2.0 * kPi * static_cast<double>(i) / static_cast<double>(n);
    vertices.emplace_back(radius * std::cos(angle), radius * std::sin(angle));
  }
  return ShapeFlatPolygon(vertices);
}

bbsolver::PropertySamples MakeShapeFlatSamples(
    const std::string& id,
    const std::vector<std::pair<double, std::vector<double>>>& frames) {
  bbsolver::PropertySamples ps;
  ps.property.id = id;
  ps.property.kind = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions = frames.empty()
      ? 0
      : static_cast<int>(frames.front().second.size());
  ps.t_start_sec = frames.empty() ? 0.0 : frames.front().first;
  ps.t_end_sec = frames.empty() ? 0.0 : frames.back().first;
  ps.samples_per_frame = 1;
  for (const auto& [t, v] : frames) {
    bbsolver::Sample s;
    s.t_sec = t;
    s.v = v;
    ps.samples.push_back(std::move(s));
  }
  return ps;
}

void TestEmptySourceReturnsNoSourceSamplesNote() {
  bbsolver::PropertySamples empty;
  empty.property.kind = bbsolver::ValueKind::Custom;
  empty.property.units_label = "shape_flat";

  const bbsolver::PathGeometryRefinementResult result =
      bbsolver::RefinePathGeometryAtFractions(
          empty, {0.0, 0.25, 0.5, 0.75}, bbsolver::PathFrameFitOptions{});

  assert(!result.ok);
  assert(!result.applied);
  assert(result.frames_refined == 0);
  assert(result.refined_max_error == 0.0);
  assert(result.notes == "no_source_samples");
}

void TestEmptyFractionsReturnsNoWinningFractionsNote() {
  const bbsolver::PropertySamples source = MakeShapeFlatSamples(
      "unit/refine/empty_fractions",
      {{0.0, ShapeFlatRect(0.0, 0.0, 1.0, 1.0)}});

  const bbsolver::PathGeometryRefinementResult result =
      bbsolver::RefinePathGeometryAtFractions(
          source, {}, bbsolver::PathFrameFitOptions{});

  assert(!result.ok);
  assert(!result.applied);
  assert(result.frames_refined == 0);
  assert(result.notes == "no_winning_fractions");
}

void TestMalformedSourceReportsFailureNote() {
  bbsolver::PropertySamples bad;
  bad.property.kind = bbsolver::ValueKind::Custom;
  bad.property.units_label = "shape_flat";
  // Claims 4 vertices but provides only 8 scalars; valid shape_flat needs 2 + 6*4 = 26.
  bbsolver::Sample s;
  s.t_sec = 0.125;
  s.v = {1.0, 4.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
  bad.samples.push_back(s);

  bbsolver::PathFrameFitOptions opts;
  opts.outline_tolerance = 1.0;

  const bbsolver::PathGeometryRefinementResult result =
      bbsolver::RefinePathGeometryAtFractions(
          bad, {0.0, 0.25, 0.5, 0.75}, opts);

  assert(!result.ok);
  assert(!result.applied);
  assert(result.frames_refined == 0);
  assert(result.notes.rfind("failed_malformed_source_at_t=", 0) == 0);
}

void TestSquareRefinementPopulatesRefinedSamplesAndStatusNote() {
  const std::vector<double> square = ShapeFlatRect(0.0, 0.0, 1.0, 1.0);
  const bbsolver::PropertySamples source = MakeShapeFlatSamples(
      "unit/refine/square",
      {{0.0, square}, {1.0 / 24.0, square}, {2.0 / 24.0, square}});
  const std::vector<double> corner_fractions = {0.0, 0.25, 0.5, 0.75};

  bbsolver::PathFrameFitOptions opts;
  opts.outline_tolerance = 0.5;

  const bbsolver::PathGeometryRefinementResult result =
      bbsolver::RefinePathGeometryAtFractions(source, corner_fractions, opts);

  assert(result.ok);
  assert(result.applied);
  assert(result.frames_refined == static_cast<int>(source.samples.size()));
  assert(result.refined_samples.samples.size() == source.samples.size());
  assert(result.refined_samples.property.dimensions ==
         2 + 6 * static_cast<int>(corner_fractions.size()));
  assert(result.refined_max_error <= opts.outline_tolerance + 1e-9);

  // Spelled status note: "ok; frames=N; refined_max_error=..."
  assert(result.notes.rfind("ok; frames=", 0) == 0);
  assert(result.notes.find("refined_max_error=") != std::string::npos);

  for (std::size_t i = 0; i < source.samples.size(); ++i) {
    const bbsolver::Sample& s = result.refined_samples.samples[i];
    assert(static_cast<int>(std::llround(s.v[1])) ==
           static_cast<int>(corner_fractions.size()));
    assert(s.v.size() == static_cast<std::size_t>(
                             2 + 6 * static_cast<int>(corner_fractions.size())));
    assert(std::abs(s.t_sec - source.samples[i].t_sec) < 1e-9);
  }
}

void TestToleranceFailureReportsExceedsToleranceNote() {
  // 8-vertex regular octagon reduced to 4 corner fractions cannot stay within
  // a 1e-3 outline tolerance: the chord between alternating vertices skips
  // intermediate vertices at a known geometric offset.
  const std::vector<double> octagon = ShapeFlatRegularPolygon(8, 1.0);
  const bbsolver::PropertySamples source = MakeShapeFlatSamples(
      "unit/refine/tolerance_fail", {{0.5, octagon}});

  bbsolver::PathFrameFitOptions opts;
  opts.outline_tolerance = 1e-3;

  const bbsolver::PathGeometryRefinementResult result =
      bbsolver::RefinePathGeometryAtFractions(
          source, {0.0, 0.25, 0.5, 0.75}, opts);

  assert(!result.ok);
  assert(!result.applied);
  assert(result.notes.rfind("failed_at_t=", 0) == 0);
  assert(result.notes.find("exceeds_tolerance") != std::string::npos);
  assert(result.notes.find("max_outline_error=") != std::string::npos);
}

}  // namespace

int main() {
  TestEmptySourceReturnsNoSourceSamplesNote();
  TestEmptyFractionsReturnsNoWinningFractionsNote();
  TestMalformedSourceReportsFailureNote();
  TestSquareRefinementPopulatesRefinedSamplesAndStatusNote();
  TestToleranceFailureReportsExceedsToleranceNote();
  return 0;
}
