#include "bbsolver/path/temporal/path_temporal_progress.hpp"
#include "bbsolver/path/temporal/path_temporal_influence.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"

#include <cassert>
#include <cstddef>
#include <iostream>
#include <vector>

namespace {

std::vector<double> ShapeFlatLine(double x0, double x1) {
  return {
      0.0, 2.0,
      x0, 0.0, 0.0, 0.0, 0.0, 0.0,
      x1, 0.0, 0.0, 0.0, 0.0, 0.0,
  };
}

void TestShapeFlatSyntaxValidation() {
  assert(!bbsolver::PathTemporalShapeFlatIsValid({}));
  assert(!bbsolver::PathTemporalShapeFlatIsValid({0.0, 0.0}));
  assert(!bbsolver::PathTemporalShapeFlatIsValid({0.0, 2.0, 1.0}));
  assert(bbsolver::PathTemporalShapeFlatIsValid(ShapeFlatLine(0.0, 10.0)));
}

void TestLerpShapeFlatChordKeepsHeaderAndInterpolatesPayload() {
  const std::vector<double> a = ShapeFlatLine(0.0, 10.0);
  const std::vector<double> b = ShapeFlatLine(20.0, 40.0);
  const std::vector<double> mid = bbsolver::LerpShapeFlatChord(a, b, 0.25);
  assert(mid.size() == a.size());
  assert(mid[0] == a[0]);
  assert(mid[1] == a[1]);
  assert(std::abs(mid[2] - 5.0) < 1e-12);
  assert(std::abs(mid[8] - 17.5) < 1e-12);

  const std::vector<double> mismatched =
      bbsolver::LerpShapeFlatChord({1.0, 2.0, 3.0}, {1.0}, 0.5);
  assert(mismatched == std::vector<double>({0.0, 0.0, 0.0}));
}

void TestInfluenceClampAndProgressStepBounds() {
  assert(bbsolver::ClampShapeTemporalInfluencePercent(
             std::numeric_limits<double>::quiet_NaN(), 0.1, 100.0) == 33.3);
  assert(bbsolver::ClampShapeTemporalInfluencePercent(-5.0, 10.0, 90.0) == 10.0);
  assert(bbsolver::ClampShapeTemporalInfluencePercent(110.0, 10.0, 90.0) == 90.0);
  assert(bbsolver::ProgressStepForLinear(-0.1, 40) == 0);
  assert(bbsolver::ProgressStepForLinear(1.1, 40) == 40);
  assert(bbsolver::ProgressStepForLinear(0.5, 40) == 20);
}

void TestDefaultBezierProgressIsMonotoneAndEndpointStable() {
  double prev = bbsolver::DefaultShapeTemporalBezierProgress(0.0);
  assert(prev >= 0.0);
  for (int idx = 1; idx <= 20; ++idx) {
    const double alpha = static_cast<double>(idx) / 20.0;
    const double current = bbsolver::DefaultShapeTemporalBezierProgress(alpha);
    assert(current + 1e-12 >= prev);
    prev = current;
  }
  assert(std::abs(bbsolver::DefaultShapeTemporalBezierProgress(1.0) - 1.0) <
         1e-9);
  assert(bbsolver::ProgressStepForDefaultBezier(0.0, 40) == 0);
  assert(bbsolver::ProgressStepForDefaultBezier(1.0, 40) == 40);
}

void TestInitialInfluenceCandidatesAreDedupedAndClamped() {
  bbsolver::ShapeMorphProgressBandOptions options;
  options.min_bezier_influence = 20.0;
  options.max_bezier_influence = 80.0;
  options.bezier_influence_grid_steps = 3;

  const std::vector<bbsolver::ShapeTemporalInfluencePair> candidates =
      bbsolver::BuildInitialShapeTemporalInfluenceCandidates(options);
  assert(!candidates.empty());
  for (const bbsolver::ShapeTemporalInfluencePair& candidate : candidates) {
    assert(candidate.out_influence >= 20.0);
    assert(candidate.out_influence <= 80.0);
    assert(candidate.in_influence >= 20.0);
    assert(candidate.in_influence <= 80.0);
  }
  for (std::size_t i = 0; i < candidates.size(); ++i) {
    for (std::size_t j = i + 1; j < candidates.size(); ++j) {
      assert(!bbsolver::ShapeTemporalInfluencesAlmostSame(
                 candidates[i].out_influence, candidates[j].out_influence) ||
             !bbsolver::ShapeTemporalInfluencesAlmostSame(
                 candidates[i].in_influence, candidates[j].in_influence));
    }
  }
}

}  // namespace

int main() {
  TestShapeFlatSyntaxValidation();
  TestLerpShapeFlatChordKeepsHeaderAndInterpolatesPayload();
  TestInfluenceClampAndProgressStepBounds();
  TestDefaultBezierProgressIsMonotoneAndEndpointStable();
  TestInitialInfluenceCandidatesAreDedupedAndClamped();
  std::cout << "[PASS] test_path_temporal_progress\n";
  return 0;
}
