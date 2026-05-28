#include "bbsolver/path/temporal/path_temporal_band_helpers.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/path/temporal/path_temporal_influence.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"

#include <cassert>
#include <iostream>
#include <utility>
#include <vector>

namespace {

std::vector<double> ShapeFlatPolygon(
    const std::vector<std::pair<double, double>>& vertices) {
  std::vector<double> out;
  out.reserve(2 + 6 * vertices.size());
  out.push_back(1.0);
  out.push_back(static_cast<double>(vertices.size()));
  for (const auto& vertex: vertices) {
    out.push_back(vertex.first);
    out.push_back(vertex.second);
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
  }
  return out;
}

void TestBuildProgressIntervals() {
  const std::vector<bbsolver::ShapeMorphProgressInterval> intervals =
      bbsolver::BuildShapeMorphProgressIntervals(
          {false, true, true, false, true},
          4);

  assert(intervals.size() == 2);
  assert(intervals[0].first_step == 1);
  assert(intervals[0].last_step == 2);
  assert(intervals[0].min_u == 0.25);
  assert(intervals[0].max_u == 0.5);
  assert(intervals[1].first_step == 4);
  assert(intervals[1].last_step == 4);
  assert(intervals[1].min_u == 1.0);
  assert(intervals[1].max_u == 1.0);
}

void TestMonotoneProgressPath() {
  assert(bbsolver::ShapeMorphHasMonotoneProgressPath({
      {true, false, false},
      {false, true, false},
      {false, false, true},
  }));

  assert(!bbsolver::ShapeMorphHasMonotoneProgressPath({
      {true, false, false},
      {false, false, true},
      {false, true, false},
  }));
}

void TestInfluencePairMaxErrorForIdenticalEndpoints() {
  bbsolver::PropertySamples samples;
  samples.property.id = "unit/path_temporal_band_helpers";
  samples.property.kind = bbsolver::ValueKind::Custom;
  samples.property.units_label = "shape_flat";
  samples.property.dimensions = 26;

  const std::vector<double> square =
      ShapeFlatPolygon({{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}});
  for (double t: {0.0, 1.0 / 24.0, 2.0 / 24.0}) {
    bbsolver::Sample sample;
    sample.t_sec = t;
    sample.v = square;
    samples.samples.push_back(sample);
  }

  bbsolver::ShapeMorphProgressBandOptions options;
  options.frame_fit_options.outline_tolerance = 0.01;

  std::vector<bbsolver::ShapeFlatOutlinePolyline> source_outlines;
  source_outlines.reserve(samples.samples.size());
  for (const bbsolver::Sample& sample: samples.samples) {
    source_outlines.push_back(
        bbsolver::BuildShapeFlatOutlinePolyline(
            sample.v,
            options.frame_fit_options));
    assert(source_outlines.back().ok);
  }

  int evaluations = 0;
  double outline_ms = 0.0;
  const double err = bbsolver::EvaluateShapeTemporalInfluencePairMaxError(
      samples,
      0,
      static_cast<int>(samples.samples.size()) - 1,
      source_outlines,
      square,
      square,
      options,
      bbsolver::ShapeTemporalInfluencePair{33.3, 33.3},
      1.0,
      &evaluations,
      &outline_ms);

  assert(err <= options.frame_fit_options.outline_tolerance);
  assert(evaluations == static_cast<int>(samples.samples.size()));
  assert(outline_ms >= 0.0);
}

}  // namespace

int main() {
  TestBuildProgressIntervals();
  TestMonotoneProgressPath();
  TestInfluencePairMaxErrorForIdenticalEndpoints();
  std::cout << "[PASS] test_path_temporal_band_helpers\n";
  return 0;
}
