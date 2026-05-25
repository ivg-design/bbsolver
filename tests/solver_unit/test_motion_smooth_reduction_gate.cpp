#include "bbsolver/motion_smooth/motion_smooth_reduction_gate.hpp"
#include "bbsolver/domain.hpp"

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

#include "bbsolver/shape/shape_flat_topology.hpp"

namespace {

void Require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

std::vector<double> Square(double offset = 0.0) {
  return bbsolver::ShapeFlatFromVertices({
      {0.0 + offset, 0.0, 0.0, 0.0, 0.0, 0.0},
      {100.0 + offset, 0.0, 0.0, 0.0, 0.0, 0.0},
      {100.0 + offset, 100.0, 0.0, 0.0, 0.0, 0.0},
      {0.0 + offset, 100.0, 0.0, 0.0, 0.0, 0.0},
  }, true);
}

bbsolver::PropertySamples ShapeFlatSamples(int count) {
  bbsolver::PropertySamples samples;
  samples.property.id = "shape";
  samples.property.kind = bbsolver::ValueKind::Custom;
  samples.property.units_label = "shape_flat";
  samples.property.dimensions = static_cast<int>(Square().size());
  samples.property.source_key_times = {0.0, 1.0};
  for (int i = 0; i < count; ++i) {
    bbsolver::Sample sample;
    sample.t_sec = static_cast<double>(i) / 24.0;
    sample.v = Square(static_cast<double>(i) * 0.01);
    samples.samples.push_back(std::move(sample));
  }
  return samples;
}

bbsolver::PropertyKeys CandidateKeys(int count) {
  bbsolver::PropertyKeys keys;
  keys.property_id = "shape";
  keys.converged = true;
  for (int i = 0; i < count; ++i) {
    bbsolver::Key key;
    key.t_sec = static_cast<double>(i);
    key.v = Square(static_cast<double>(i));
    keys.keys.push_back(std::move(key));
  }
  return keys;
}

void TestGateIgnoresNonShapeFlatProperties() {
  bbsolver::PropertySamples samples;
  samples.property.kind = bbsolver::ValueKind::Scalar;
  samples.property.units_label = "px";

  const bbsolver::ShapeMotionReductionGateResult result =
      bbsolver::GateShapeMotionQualityRegression(
          samples, CandidateKeys(3), bbsolver::SolverConfig{});

  Require(!result.attempted, "non-shape property must not attempt gate");
  Require(!result.rejected, "non-shape property must not reject");
}

void TestGateIgnoresSmallCandidateKeysets() {
  const bbsolver::ShapeMotionReductionGateResult result =
      bbsolver::GateShapeMotionQualityRegression(
          ShapeFlatSamples(12), CandidateKeys(2), bbsolver::SolverConfig{});

  Require(!result.attempted, "two-key candidate must not attempt gate");
  Require(result.note.empty(), "early gate skip must not add notes");
}

void TestNearOptimalFastPathHonorsReplacementModeSkip() {
  bbsolver::SolverConfig config;
  config.allow_path_replacement_fit = true;

  const bbsolver::ShapeFlatNearOptimalResult result =
      bbsolver::TryShapeFlatAlreadyNearOptimalFastPath(
          ShapeFlatSamples(120), config, bbsolver::CompInfo{});

  Require(!result.applied, "replacement mode must skip near-optimal fast path");
  Require(result.source_samples == 120, "source sample count must be reported");
}

}  // namespace

int main() {
  TestGateIgnoresNonShapeFlatProperties();
  TestGateIgnoresSmallCandidateKeysets();
  TestNearOptimalFastPathHonorsReplacementModeSkip();
  return 0;
}
