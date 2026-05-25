#include "bbsolver/path/multimode/path_multimode_region_candidate.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/path/multimode/path_multimode_solver.hpp"

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
  std::cerr << "test_path_multimode_region_candidate: " << message << "\n";
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

bbsolver::PropertySamples MakeLinearSamples(int sample_count = 4) {
  bbsolver::PropertySamples samples;
  samples.property.id = "unit/path_multimode/region";
  samples.property.kind = bbsolver::ValueKind::Custom;
  samples.property.units_label = "shape_flat";
  samples.property.dimensions = 14;
  samples.t_start_sec = 0.0;
  samples.t_end_sec = static_cast<double>(sample_count - 1) / 24.0;
  samples.samples_per_frame = 1;
  for (int idx = 0; idx < sample_count; ++idx) {
    const double x = static_cast<double>(idx);
    samples.samples.push_back(
        {x / 24.0, ShapeFlatTwoVertex(x, 0.0, 10.0 + x, 5.0)});
  }
  return samples;
}

void TestRegionSegmentFeasible() {
  bbsolver::PropertySamples samples = MakeLinearSamples();
  Require(bbsolver::path_multimode::RegionSegmentFeasible(
              samples, 0, 3, {0, 2}, 1e-9),
          "linear segment should be feasible across full region");

  samples.samples[1].v[2] += 4.0;
  Require(!bbsolver::path_multimode::RegionSegmentFeasible(
              samples, 0, 3, {0, 2}, 0.1),
          "nonlinear middle sample should fail tolerance");
}

void TestSolveRegionAnchorsAndBudget() {
  bbsolver::PropertySamples samples = MakeLinearSamples();
  bbsolver::ShapeFlatMultiModeOptions options;
  options.region_tolerance = 1e-9;
  int checks = 0;
  bbsolver::path_multimode::RegionSolveResult result =
      bbsolver::path_multimode::SolveRegionAnchors(
          samples, {0, 2}, options, 3, &checks);
  Require(!result.budget_exceeded, "linear solve should stay within budget");
  Require(result.anchors == std::vector<int>({0, 3}),
          "linear solve should use endpoint anchors");
  Require(checks > 0, "segment checks should be counted");

  options.max_region_segment_checks = 1;
  checks = 0;
  bbsolver::path_multimode::RegionSolveResult budget =
      bbsolver::path_multimode::SolveRegionAnchors(
          samples, {0, 2}, options, 3, &checks);
  Require(budget.budget_exceeded, "low segment budget should stop solve");
}

void TestCandidateAssemblyAndBudget() {
  const bbsolver::PropertySamples samples = MakeLinearSamples();
  const std::vector<int> anchors = {0, 3};
  bbsolver::PropertyKeys keys =
      bbsolver::path_multimode::BuildCandidate(samples, anchors);
  Require(keys.property_id == samples.property.id, "candidate preserves property id");
  Require(!keys.converged, "candidate starts unconverged before validation");
  Require(keys.keys.size() == 2, "candidate creates one key per anchor");
  Require(keys.segments.size() == 1, "candidate creates segment reports");
  Require(keys.keys.front().interp_in == bbsolver::InterpType::Bezier,
          "first key keeps Bezier incoming interp");
  Require(keys.keys.front().interp_out == bbsolver::InterpType::Linear,
          "first key linear outgoing interp");
  Require(keys.keys.back().interp_out == bbsolver::InterpType::Bezier,
          "last key keeps Bezier outgoing interp");
  Require(keys.segments.front().reason ==
              "replacement_shape_multimode_linear_union",
          "candidate segment reason preserved");
  Require(!bbsolver::path_multimode::CandidateKeyBudgetExceeded(2, 4, 0.5),
          "budget comparison allows exact ratio");
  Require(bbsolver::path_multimode::CandidateKeyBudgetExceeded(3, 4, 0.5),
          "budget comparison rejects over-ratio candidates");
}

void TestLinearInterpolateShapeFlat() {
  const std::vector<double> a = ShapeFlatTwoVertex(0.0, 0.0, 10.0, 0.0);
  const std::vector<double> b = ShapeFlatTwoVertex(2.0, 4.0, 14.0, 8.0);
  const std::vector<double> mid =
      bbsolver::path_multimode::LinearInterpolateShapeFlat(a, b, 0.5);
  Require(mid.size() == a.size(), "interpolated shape size preserved");
  Require(std::abs(mid[2] - 1.0) < 1e-9, "first vertex x interpolated");
  Require(std::abs(mid[3] - 2.0) < 1e-9, "first vertex y interpolated");
  Require(std::abs(mid[8] - 12.0) < 1e-9, "second vertex x interpolated");
  Require(std::abs(mid[9] - 4.0) < 1e-9, "second vertex y interpolated");
}

}  // namespace

int main() {
  TestRegionSegmentFeasible();
  TestSolveRegionAnchorsAndBudget();
  TestCandidateAssemblyAndBudget();
  TestLinearInterpolateShapeFlat();
  return 0;
}
