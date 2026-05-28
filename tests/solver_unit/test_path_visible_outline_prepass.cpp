#include "bbsolver/path/geometry/path_visible_outline_prepass.hpp"
#include "bbsolver/domain.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "bbsolver/shape/shape_flat_topology.hpp"

namespace {

void Require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

bbsolver::PropertySamples ShapeFlatSamples(
    const std::vector<std::vector<double>>& values) {
  bbsolver::PropertySamples samples;
  samples.property.id = "shape";
  samples.property.kind = bbsolver::ValueKind::Custom;
  samples.property.units_label = "shape_flat";
  samples.property.dimensions = values.empty() ? 1: static_cast<int>(values[0].size());
  for (std::size_t i = 0; i < values.size(); ++i) {
    bbsolver::Sample sample;
    sample.t_sec = static_cast<double>(i);
    sample.v = values[i];
    samples.samples.push_back(std::move(sample));
  }
  return samples;
}

std::vector<double> Square() {
  return bbsolver::ShapeFlatFromVertices({
      {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      {100.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      {100.0, 100.0, 0.0, 0.0, 0.0, 0.0},
      {0.0, 100.0, 0.0, 0.0, 0.0, 0.0},
  }, true);
}

void TestNonShapeFlatIsSkipped() {
  bbsolver::PropertySamples samples;
  samples.property.kind = bbsolver::ValueKind::Scalar;
  samples.property.units_label = "px";
  bbsolver::Sample sample;
  sample.t_sec = 0.0;
  sample.v = {1.0};
  samples.samples.push_back(sample);

  const bbsolver::VisibleOutlinePrepassResult result =
      bbsolver::TryVisibleOutlinePrepass(samples, bbsolver::SolverConfig{});

  Require(!result.applied, "non-shape property must not apply");
  Require(result.notes == "visible_outline_prepass_skipped: non_shape_flat",
          "non-shape note must be preserved");
  Require(result.samples.samples.size() == 1,
          "skipped result must retain original samples");
}

void TestMalformedShapeFlatIsSkipped() {
  bbsolver::PropertySamples samples = ShapeFlatSamples({{1.0, 4.0}});

  const bbsolver::VisibleOutlinePrepassResult result =
      bbsolver::TryVisibleOutlinePrepass(samples, bbsolver::SolverConfig{});

  Require(!result.applied, "malformed shape must not apply");
  Require(result.notes ==
              "visible_outline_prepass_skipped: malformed_shape_flat",
          "malformed note must be preserved");
}

void TestSimpleShapeWithoutVisibleOutlineBenefitIsSkipped() {
  bbsolver::PropertySamples samples = ShapeFlatSamples({Square(), Square()});

  const bbsolver::VisibleOutlinePrepassResult result =
      bbsolver::TryVisibleOutlinePrepass(samples, bbsolver::SolverConfig{});

  Require(!result.applied, "simple square must not apply visible-outline prepass");
  Require(result.notes.find("visible_outline_prepass_skipped:") == 0,
          "skip note must keep visible-outline prefix");
  Require(result.samples.samples.size() == samples.samples.size(),
          "skipped result must retain original sample count");
}

}  // namespace

int main() {
  TestNonShapeFlatIsSkipped();
  TestMalformedShapeFlatIsSkipped();
  TestSimpleShapeWithoutVisibleOutlineBenefitIsSkipped();
  return 0;
}
