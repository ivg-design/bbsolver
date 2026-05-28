//  focused test: the -extracted adaptive sampler.
//
// BuildAdaptiveClosedLoopShapeSamples has two behaviour bands:
//
//   1. Early-return (when closed_values.size() < 4, dims <= 2, or
//      vertex_count <= 0):
//      * result.values = closed_values (verbatim)
//      * result.params = [0, 1,..., size-1]
//      * result.quality computed via ShapeMotionQuality
//      * All strength-derived fields stay at default (target_turn_deg=0,
//        chord_error_tolerance=0, max_keys=0, refinement_passes=0, etc.)
//
//   2. Main 16-pass refinement (when all guards pass):
//      * target_turn_deg = source_pose
//          ? clamp((48 - strength*3) * 0.65, 18, 32)
//: clamp((48 - strength*3),        26, 42)
//      * chord_error_tolerance = source_pose
//          ? max(0.25, strength * 0.35)
//: max(0.5,  strength * 0.55)
//      * max_per_segment = source_pose
//          ? clamp(strength*4 + 10, 16, 28)
//: clamp(strength*3 + 4,   8, 18)
//      * max_keys = unique_count * max_per_segment + 1
//      * refinement_passes capped at 16
//      * budget_hit flagged when split_count > split_budget
//      * splits counter non-negative monotonic
//
//  is the final motion_smooth surface to gain direct coverage;
// after this all MS surfaces in the scoreboard are covered.

#include "bbsolver/motion_smooth/motion_smooth_shape_loop_adaptive.hpp"

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    std::abort();
  }
}

bool AlmostEqual(double a, double b, double eps = 1e-9) {
  return std::abs(a - b) <= eps;
}

// Shape-flat single-vertex value at (px, py) with zero tangents.
// Same layout as /// fixtures. Size 8.
std::vector<double> VertexAt(double px, double py) {
  return {0.0, 1.0, px, py, 0.0, 0.0, 0.0, 0.0};
}

// Build a closed-loop value sequence: N points around a square with
// the (N+1)-th entry duplicating the first (the "closed wrap").
// unique_count = N (the function computes this as size - 1).
std::vector<std::vector<double>> ClosedSquareLoop(int unique_count) {
  std::vector<std::vector<double>> values;
  values.reserve(static_cast<std::size_t>(unique_count + 1));
  const double radius = 10.0;
  for (int i = 0; i < unique_count; ++i) {
    const double angle =
        2.0 * 3.14159265358979 *
        static_cast<double>(i) / static_cast<double>(unique_count);
    values.push_back(VertexAt(std::cos(angle) * radius,
                              std::sin(angle) * radius));
  }
  // Closed-loop wrap: last entry duplicates the first.
  values.push_back(values.front());
  return values;
}

// ---------------------------------------------------------------------------
// Early-return paths
// ---------------------------------------------------------------------------

void TestEarlyReturnSizeBelowFour() {
  // closed_values.size() < 4 → early return; params=[0,1,..,size-1];
  // strength-derived fields stay at default 0.
  const std::vector<std::vector<double>> three_frames = {
      VertexAt(0.0, 0.0),
      VertexAt(1.0, 0.0),
      VertexAt(0.0, 1.0),
  };
  const bbsolver::AdaptiveClosedLoopShapeSamples r =
      bbsolver::BuildAdaptiveClosedLoopShapeSamples(
          three_frames, /*dims=*/8, /*vertex_count=*/1,
          /*strength=*/1.0, /*source_pose=*/false);
  Require(r.values.size() == 3,
          "size<4 early-return: result.values must match input size");
  Require(r.params.size() == 3 &&
              AlmostEqual(r.params[0], 0.0) &&
              AlmostEqual(r.params[1], 1.0) &&
              AlmostEqual(r.params[2], 2.0),
          "size<4 early-return: params must be [0,1,2]");
  Require(AlmostEqual(r.target_turn_deg, 0.0),
          "size<4 early-return: target_turn_deg stays at default 0");
  Require(AlmostEqual(r.chord_error_tolerance, 0.0),
          "size<4 early-return: chord_error_tolerance stays at default 0");
  Require(r.max_keys == 0,
          "size<4 early-return: max_keys stays at default 0");
  Require(r.refinement_passes == 0,
          "size<4 early-return: refinement_passes stays at default 0");
  Require(!r.budget_hit,
          "size<4 early-return: budget_hit stays false");
}

void TestEarlyReturnDimsAtOrBelowTwo() {
  // dims <= 2 → early return even with 4+ frames.
  const std::vector<std::vector<double>> four_frames = {
      VertexAt(0.0, 0.0), VertexAt(1.0, 0.0),
      VertexAt(2.0, 0.0), VertexAt(3.0, 0.0),
  };
  const bbsolver::AdaptiveClosedLoopShapeSamples r =
      bbsolver::BuildAdaptiveClosedLoopShapeSamples(
          four_frames, /*dims=*/2, /*vertex_count=*/1,
          /*strength=*/1.0, /*source_pose=*/false);
  Require(r.values.size() == 4,
          "dims<=2 early-return: result.values must match input size");
  Require(r.max_keys == 0,
          "dims<=2 early-return: max_keys stays at default 0");
}

void TestEarlyReturnVertexCountAtOrBelowZero() {
  // vertex_count <= 0 → early return.
  const std::vector<std::vector<double>> four_frames = {
      VertexAt(0.0, 0.0), VertexAt(1.0, 0.0),
      VertexAt(2.0, 0.0), VertexAt(3.0, 0.0),
  };
  const bbsolver::AdaptiveClosedLoopShapeSamples r =
      bbsolver::BuildAdaptiveClosedLoopShapeSamples(
          four_frames, /*dims=*/8, /*vertex_count=*/0,
          /*strength=*/1.0, /*source_pose=*/false);
  Require(r.values.size() == 4,
          "vertex_count<=0 early-return: result.values must match input");
  Require(r.max_keys == 0,
          "vertex_count<=0 early-return: max_keys stays at default 0");
}

// ---------------------------------------------------------------------------
// Main-path strength-derived formulas
// ---------------------------------------------------------------------------

void TestTargetTurnDegFormulaNonSourcePose() {
  // target_turn_deg = clamp(48 - strength*3, 26, 42).
  //   strength=0  → 48 → clamped to 42
  //   strength=2  → 42 → at upper boundary
  //   strength=7  → 27 → in clamp range
  //   strength=20 → -12 → clamped to 26
  const std::vector<std::vector<double>> loop = ClosedSquareLoop(4);

  const bbsolver::AdaptiveClosedLoopShapeSamples r0 =
      bbsolver::BuildAdaptiveClosedLoopShapeSamples(
          loop, 8, 1, /*strength=*/0.0, /*source_pose=*/false);
  Require(AlmostEqual(r0.target_turn_deg, 42.0, 1e-9),
          "strength=0 (raw 48) must clamp target_turn_deg to upper 42");

  const bbsolver::AdaptiveClosedLoopShapeSamples r7 =
      bbsolver::BuildAdaptiveClosedLoopShapeSamples(
          loop, 8, 1, /*strength=*/7.0, /*source_pose=*/false);
  Require(AlmostEqual(r7.target_turn_deg, 27.0, 1e-9),
          "strength=7 (raw 27) must yield target_turn_deg=27 inside clamp");

  const bbsolver::AdaptiveClosedLoopShapeSamples r20 =
      bbsolver::BuildAdaptiveClosedLoopShapeSamples(
          loop, 8, 1, /*strength=*/20.0, /*source_pose=*/false);
  Require(AlmostEqual(r20.target_turn_deg, 26.0, 1e-9),
          "strength=20 (raw -12) must clamp target_turn_deg to lower 26");
}

void TestTargetTurnDegFormulaSourcePose() {
  // source_pose: target_turn_deg = clamp((48 - strength*3) * 0.65, 18, 32).
  //   strength=0   → 31.2 → in clamp
  //   strength=5   → 21.45 → in clamp
  //   strength=20  → -12 * 0.65 → -7.8 → clamped to 18
  //   strength=-10 → 78 * 0.65 = 50.7 → clamped to 32
  const std::vector<std::vector<double>> loop = ClosedSquareLoop(4);

  const bbsolver::AdaptiveClosedLoopShapeSamples r0 =
      bbsolver::BuildAdaptiveClosedLoopShapeSamples(
          loop, 8, 1, /*strength=*/0.0, /*source_pose=*/true);
  Require(AlmostEqual(r0.target_turn_deg, 31.2, 1e-9),
          "strength=0 source_pose must yield 48*0.65 = 31.2");

  const bbsolver::AdaptiveClosedLoopShapeSamples r20 =
      bbsolver::BuildAdaptiveClosedLoopShapeSamples(
          loop, 8, 1, /*strength=*/20.0, /*source_pose=*/true);
  Require(AlmostEqual(r20.target_turn_deg, 18.0, 1e-9),
          "strength=20 source_pose must clamp target_turn_deg to lower 18");
}

void TestChordErrorToleranceFormula() {
  // Non-source-pose: max(0.5, strength * 0.55).
  //   strength=0   → max(0.5, 0)    = 0.5
  //   strength=2   → max(0.5, 1.1)  = 1.1
  // Source-pose: max(0.25, strength * 0.35).
  //   strength=0   → max(0.25, 0)   = 0.25
  //   strength=2   → max(0.25, 0.7) = 0.7
  const std::vector<std::vector<double>> loop = ClosedSquareLoop(4);

  const bbsolver::AdaptiveClosedLoopShapeSamples r_off_low =
      bbsolver::BuildAdaptiveClosedLoopShapeSamples(
          loop, 8, 1, /*strength=*/0.0, /*source_pose=*/false);
  Require(AlmostEqual(r_off_low.chord_error_tolerance, 0.5, 1e-12),
          "non-source-pose strength=0 must clamp chord_error_tolerance to 0.5");

  const bbsolver::AdaptiveClosedLoopShapeSamples r_off_high =
      bbsolver::BuildAdaptiveClosedLoopShapeSamples(
          loop, 8, 1, /*strength=*/2.0, /*source_pose=*/false);
  Require(AlmostEqual(r_off_high.chord_error_tolerance, 1.1, 1e-12),
          "non-source-pose strength=2 must yield chord_error_tolerance=1.1");

  const bbsolver::AdaptiveClosedLoopShapeSamples r_on_low =
      bbsolver::BuildAdaptiveClosedLoopShapeSamples(
          loop, 8, 1, /*strength=*/0.0, /*source_pose=*/true);
  Require(AlmostEqual(r_on_low.chord_error_tolerance, 0.25, 1e-12),
          "source-pose strength=0 must clamp chord_error_tolerance to 0.25");

  const bbsolver::AdaptiveClosedLoopShapeSamples r_on_high =
      bbsolver::BuildAdaptiveClosedLoopShapeSamples(
          loop, 8, 1, /*strength=*/2.0, /*source_pose=*/true);
  Require(AlmostEqual(r_on_high.chord_error_tolerance, 0.7, 1e-12),
          "source-pose strength=2 must yield chord_error_tolerance=0.7");
}

void TestMaxKeysFormula() {
  // max_keys = unique_count * max_per_segment + 1.
  // Non-source-pose max_per_segment = clamp(round(strength*3) + 4, 8, 18).
  //   strength=1 → round(3) + 4 = 7 → clamp to 8.
  //   strength=4 → round(12) + 4 = 16 → in clamp.
  //   strength=10 → round(30) + 4 = 34 → clamp to 18.
  // unique_count = closed_values.size() - 1. For 4-frame closed loop
  // (3 unique points + 1 wrap), unique_count = 3.
  const std::vector<std::vector<double>> loop3 = ClosedSquareLoop(3);

  const bbsolver::AdaptiveClosedLoopShapeSamples r1 =
      bbsolver::BuildAdaptiveClosedLoopShapeSamples(
          loop3, 8, 1, /*strength=*/1.0, /*source_pose=*/false);
  Require(r1.max_keys == 3 * 8 + 1,  // = 25
          "non-source-pose strength=1 max_keys must = unique_count*8 + 1");

  const bbsolver::AdaptiveClosedLoopShapeSamples r10 =
      bbsolver::BuildAdaptiveClosedLoopShapeSamples(
          loop3, 8, 1, /*strength=*/10.0, /*source_pose=*/false);
  Require(r10.max_keys == 3 * 18 + 1,  // = 55 (max_per_segment clamped to 18)
          "non-source-pose strength=10 max_keys must = unique_count*18 + 1");
}

// ---------------------------------------------------------------------------
// Main-path structural invariants
// ---------------------------------------------------------------------------

void TestRefinementPassesNeverExceedsSixteen() {
  // The refinement loop is hard-capped at 16 iterations. Even with
  // strength tuned aggressively, refinement_passes must stay ≤ 16.
  const std::vector<std::vector<double>> loop = ClosedSquareLoop(6);
  const bbsolver::AdaptiveClosedLoopShapeSamples r =
      bbsolver::BuildAdaptiveClosedLoopShapeSamples(
          loop, 8, 1, /*strength=*/5.0, /*source_pose=*/false);
  Require(r.refinement_passes >= 0 && r.refinement_passes <= 16,
          "refinement_passes must remain in [0, 16]");
  Require(r.splits >= 0,
          "splits counter must be non-negative");
  Require(r.max_keys > 0,
          "main-path execution must populate max_keys > 0");
}

void TestQualityFieldPopulatedOnMainPath() {
  // The quality field is computed via ShapeMotionQuality(values, vertex_count).
  // On the main path it must be valid (since values come from
  // EvaluateClosedLoopShapeAtParam and vertex_count > 0).
  const std::vector<std::vector<double>> loop = ClosedSquareLoop(5);
  const bbsolver::AdaptiveClosedLoopShapeSamples r =
      bbsolver::BuildAdaptiveClosedLoopShapeSamples(
          loop, 8, 1, /*strength=*/1.0, /*source_pose=*/false);
  Require(r.quality.valid,
          "main-path quality must be valid");
  Require(r.quality.vertex_count == 1,
          "main-path quality.vertex_count must echo input vertex_count");
  Require(r.values.size() >= loop.size(),
          "main-path result.values must contain at least the original entries");
  Require(r.values.size() <= static_cast<std::size_t>(r.max_keys),
          "main-path result.values size must respect max_keys cap");
}

}  // namespace

int main() {
  TestEarlyReturnSizeBelowFour();
  TestEarlyReturnDimsAtOrBelowTwo();
  TestEarlyReturnVertexCountAtOrBelowZero();
  TestTargetTurnDegFormulaNonSourcePose();
  TestTargetTurnDegFormulaSourcePose();
  TestChordErrorToleranceFormula();
  TestMaxKeysFormula();
  TestRefinementPassesNeverExceedsSixteen();
  TestQualityFieldPopulatedOnMainPath();
  std::cout << "[PASS] test_motion_smooth_shape_loop_adaptive\n";
  return 0;
}
