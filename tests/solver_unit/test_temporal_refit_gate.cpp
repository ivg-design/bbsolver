#include "bbsolver/temporal/refit/temporal_refit_gate.hpp"
#include "bbsolver/domain.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    std::abort();
  }
}

bbsolver::PropertySamples BaselineSamples() {
  bbsolver::PropertySamples property_samples;
  property_samples.property.kind = bbsolver::ValueKind::Scalar;
  property_samples.property.units_label = "";
  property_samples.samples_per_frame = 1;
  return property_samples;
}

bbsolver::PropertyKeys BaselineKeys() {
  bbsolver::PropertyKeys keys;
  keys.converged = true;
  keys.keys = {bbsolver::Key{}, bbsolver::Key{}, bbsolver::Key{}};
  return keys;
}

bbsolver::SolverConfig ConfigForMode(const std::string& mode) {
  bbsolver::SolverConfig config;
  config.solve_optimization_mode = mode;
  return config;
}

void TestBaselineAllowsTemporalRefit() {
  Require(bbsolver::PipelineAllowsTemporalRefit(
              BaselineSamples(), BaselineKeys(), ConfigForMode("full")),
          "baseline scalar converged full-mode result must allow temporal refit");
}

void TestTemporalModeDisabledRejectsRefit() {
  Require(!bbsolver::PipelineAllowsTemporalRefit(
              BaselineSamples(), BaselineKeys(), ConfigForMode("vertex_only")),
          "vertex_only mode must reject temporal refit");
}

void TestMotionSmoothModeRejectsRefit() {
  Require(!bbsolver::PipelineAllowsTemporalRefit(
              BaselineSamples(), BaselineKeys(), ConfigForMode("motion_smooth")),
          "motion_smooth mode must reject temporal refit");
}

void TestCustomNonShapeRejectsRefit() {
  bbsolver::PropertySamples property_samples = BaselineSamples();
  property_samples.property.kind = bbsolver::ValueKind::Custom;
  property_samples.property.units_label = "not_shape_flat";
  Require(!bbsolver::PipelineAllowsTemporalRefit(
              property_samples, BaselineKeys(), ConfigForMode("full")),
          "custom non-shape property must reject temporal refit");
}

void TestCustomShapeFlatAllowsRefit() {
  bbsolver::PropertySamples property_samples = BaselineSamples();
  property_samples.property.kind = bbsolver::ValueKind::Custom;
  property_samples.property.units_label = "shape_flat";
  Require(bbsolver::PipelineAllowsTemporalRefit(
              property_samples, BaselineKeys(), ConfigForMode("full")),
          "custom shape_flat property must allow temporal refit");
}

void TestNonUnitSamplesPerFrameRejectsRefit() {
  bbsolver::PropertySamples property_samples = BaselineSamples();
  property_samples.samples_per_frame = 2;
  Require(!bbsolver::PipelineAllowsTemporalRefit(
              property_samples, BaselineKeys(), ConfigForMode("full")),
          "samples_per_frame other than one must reject temporal refit");
}

void TestUnconvergedKeysRejectRefit() {
  bbsolver::PropertyKeys keys = BaselineKeys();
  keys.converged = false;
  Require(!bbsolver::PipelineAllowsTemporalRefit(
              BaselineSamples(), keys, ConfigForMode("full")),
          "unconverged keys must reject temporal refit");
}

void TestFewerThanThreeKeysRejectRefit() {
  bbsolver::PropertyKeys keys = BaselineKeys();
  keys.keys.resize(2);
  Require(!bbsolver::PipelineAllowsTemporalRefit(
              BaselineSamples(), keys, ConfigForMode("full")),
          "fewer than three keys must reject temporal refit");
}

}  // namespace

int main() {
  TestBaselineAllowsTemporalRefit();
  TestTemporalModeDisabledRejectsRefit();
  TestMotionSmoothModeRejectsRefit();
  TestCustomNonShapeRejectsRefit();
  TestCustomShapeFlatAllowsRefit();
  TestNonUnitSamplesPerFrameRejectsRefit();
  TestUnconvergedKeysRejectRefit();
  TestFewerThanThreeKeysRejectRefit();
  std::cout << "[PASS] test_temporal_refit_gate\n";
  return 0;
}
