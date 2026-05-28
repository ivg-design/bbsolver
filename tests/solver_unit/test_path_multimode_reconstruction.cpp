#include "bbsolver/path/multimode/path_multimode_reconstruction.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/path/multimode/path_multimode_region_candidate.hpp"
#include "bbsolver/path/multimode/path_multimode_temporal.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"
#include "bbsolver/path/multimode/path_multimode_geometry.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
  if (condition) {
    return;
  }
  std::cerr << "test_path_multimode_reconstruction: " << message << "\n";
  std::exit(1);
}

std::vector<double> ShapeFlatTwoVertex(double x0, double y0,
                                       double x1, double y1) {
  return {
      0.0, 2.0,
      x0, y0, 0.0, 0.0, 0.0, 0.0,
      x1, y1, 0.0, 0.0, 0.0, 0.0,
  };
}

bbsolver::PropertySamples MakeSamples(bool nonlinear_middle) {
  bbsolver::PropertySamples samples;
  samples.property.id = "unit/path_multimode/reconstruction";
  samples.property.kind = bbsolver::ValueKind::Custom;
  samples.property.units_label = "shape_flat";
  samples.property.dimensions = 14;
  samples.t_start_sec = 0.0;
  samples.t_end_sec = 2.0 / 24.0;
  samples.samples_per_frame = 1;
  samples.samples.push_back({0.0, ShapeFlatTwoVertex(0.0, 0.0, 10.0, 0.0)});
  samples.samples.push_back(
      {1.0 / 24.0,
       ShapeFlatTwoVertex(1.0, nonlinear_middle ? 4.0: 2.0, 12.0, 4.0)});
  samples.samples.push_back({2.0 / 24.0,
                             ShapeFlatTwoVertex(2.0, 4.0, 14.0, 8.0)});
  return samples;
}

void TestReconstructionAndRefinement() {
  const bbsolver::path_multimode::VertexRegion full_region{0, 2};
  const bbsolver::PropertySamples linear = MakeSamples(false);
  const bbsolver::path_multimode::LandmarkSubpathReconstructionResult ok =
      bbsolver::path_multimode::EvaluateLandmarkSubpathReconstruction(
          linear, full_region, {0, 2}, 0.1);
  Require(ok.ok, "linear subpath reconstructs from endpoint anchors");
  Require(ok.samples_checked == 3, "all samples checked");

  const bbsolver::PropertySamples nonlinear = MakeSamples(true);
  const bbsolver::path_multimode::LandmarkSubpathReconstructionResult failed =
      bbsolver::path_multimode::EvaluateLandmarkSubpathReconstruction(
          nonlinear, full_region, {0, 2}, 0.1);
  Require(!failed.ok, "nonlinear middle sample fails endpoint reconstruction");
  Require(failed.worst_sample_idx == 1, "worst sample tracks nonlinear middle");

  const bbsolver::path_multimode::LandmarkSubpathRefinementResult refined =
      bbsolver::path_multimode::RefineLandmarkSubpathAnchors(
          nonlinear, full_region, {0, 2}, 0.1, {});
  Require(refined.ok, "refinement inserts nonlinear sample");
  Require(refined.inserted_samples == 1, "one sample inserted");
  Require(refined.anchors == std::vector<int>({0, 1, 2}),
          "refined anchors include worst sample");
}

void TestCandidateValidationAndTemporalReplay() {
  const bbsolver::PropertySamples samples = MakeSamples(false);
  bbsolver::PropertyKeys keys =
      bbsolver::path_multimode::BuildCandidate(samples, {0, 2});
  keys.converged = true;
  const bbsolver::path_multimode::LandmarkSubpathReconstructionResult validation =
      bbsolver::path_multimode::EvaluateLandmarkSubpathCandidate(
          samples, keys, 0.1);
  Require(validation.ok, "candidate validates against temporal oracle");

  const bbsolver::ShapeMorphProgressBandOptions band_options =
      bbsolver::path_multimode::LandmarkBandOptions(0.1, 2);
  const std::vector<double> mid =
      bbsolver::path_multimode::EvaluateTemporalShapeAtSample(
          samples, keys, 1, band_options);
  Require(mid.size() == samples.samples[1].v.size(), "temporal replay returns shape");
  Require(std::abs(mid[2] - 1.0) < 1e-9, "replayed first vertex x");
  Require(std::abs(mid[3] - 2.0) < 1e-9, "replayed first vertex y");
  Require(std::abs(mid[8] - 12.0) < 1e-9, "replayed second vertex x");
  Require(std::abs(mid[9] - 4.0) < 1e-9, "replayed second vertex y");
}

}  // namespace

int main() {
  TestReconstructionAndRefinement();
  TestCandidateValidationAndTemporalReplay();
  return 0;
}
