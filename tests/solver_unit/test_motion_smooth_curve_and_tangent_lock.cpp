//  focused regression test:  tangent-lock +  curve primitives.
//
// These leaf-level building blocks (MotionSmoothCatmullRomValue,
// EvaluateClosedLoopShapeAtParam, PointTurnDeg, ShapeFlatVertexPoint,
// LockShapeFlatRotationalTangents and its multi-key + Except variants)
// are consumed by the adaptive sampler (motion_smooth_shape_loop_adaptive.cpp)
// and the shape-flat orchestrator. The pre-existing
// test_motion_smooth_shape_flat.cpp exercises them transitively through
// BuildMotionSmoothShapeFlatTrajectory, but never directly — so the
// foundational math (Catmull-Rom kernel, polyline turn angle,
// anchor-handle direction-inversion in tangent locking) had no
// regression coverage until this file.
//
// Each test is small, deterministic, and exercises one well-defined
// behaviour. A future refactor that silently changes one of the
// formulas would fail here before propagating to the integration test.

#include "bbsolver/motion_smooth/motion_smooth_shape_loop_curve.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_tangent_lock.hpp"

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
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

// Shape-flat value vector layout per shape_flat_topology.cpp:
//   value[0] = closed flag (0 or 1)
//   value[1] = vertex count N (read via std::llround)
//   value[2 + i*6 + 0] = vertex_i pos_x
//   value[2 + i*6 + 1] = vertex_i pos_y
//   value[2 + i*6 + 2] = vertex_i in_x
//   value[2 + i*6 + 3] = vertex_i in_y
//   value[2 + i*6 + 4] = vertex_i out_x
//   value[2 + i*6 + 5] = vertex_i out_y
// Total size for N vertices: 2 + N * 6.
//
// ShapeFlatVertexCountFromValues asserts both `value[1] == N` and
// `value.size() == 2 + N*6` — a mismatch returns -1, which makes
// LockShapeFlatRotationalTangents short-circuit. Fixtures must
// honour both invariants.
std::vector<double> SingleVertexValue(double pos_x,
                                      double pos_y,
                                      double in_x = 0.0,
                                      double in_y = 0.0,
                                      double out_x = 0.0,
                                      double out_y = 0.0) {
  return {0.0, 1.0, pos_x, pos_y, in_x, in_y, out_x, out_y};
  //      ^^^^  ^^^
  //      closed=0, vertex_count=1 (matches size 8 = 2 + 1*6).
}

// ---------------------------------------------------------------------------
//  — MotionSmoothCatmullRomValue
// ---------------------------------------------------------------------------

void TestCatmullRomReturnsP1AtUZero() {
  // Centripetal Catmull-Rom (Bezier-form derivative) at u=0 passes
  // through p1 by construction. Lock this identity.
  const double v = bbsolver::MotionSmoothCatmullRomValue(
      -1.0, 7.0, 13.0, 21.0, 0.0);
  Require(AlmostEqual(v, 7.0),
          "Catmull-Rom at u=0 must return p1 exactly");
}

void TestCatmullRomReturnsP2AtUOne() {
  // At u=1 the curve passes through p2 by construction.
  const double v = bbsolver::MotionSmoothCatmullRomValue(
      -1.0, 7.0, 13.0, 21.0, 1.0);
  Require(AlmostEqual(v, 13.0),
          "Catmull-Rom at u=1 must return p2 exactly");
}

void TestCatmullRomLinearOnAffinelyArrangedPoints() {
  // Four collinear, evenly-spaced points reduce to linear
  // interpolation between p1 and p2 at any u: 0.5*(p1+p2) at u=0.5.
  const double mid = bbsolver::MotionSmoothCatmullRomValue(
      0.0, 1.0, 2.0, 3.0, 0.5);
  Require(AlmostEqual(mid, 1.5),
          "Catmull-Rom on affinely-arranged points must be linear");
}

// ---------------------------------------------------------------------------
//  — PointTurnDeg
// ---------------------------------------------------------------------------

void TestPointTurnDegStraightLineIsZero() {
  // Straight line: prev → cur → next collinear, no turn.
  const double angle = bbsolver::PointTurnDeg(
      {0.0, 0.0}, {1.0, 0.0}, {2.0, 0.0});
  Require(AlmostEqual(angle, 0.0, 1e-7),
          "collinear triplet must yield 0° turn");
}

void TestPointTurnDegRightAngle() {
  // Right-angle turn: (0,0) → (1,0) → (1,1). The function returns
  // the angle between the leg directions: legs are (1,0) and (0,1) →
  // 90 degrees.
  const double angle = bbsolver::PointTurnDeg(
      {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0});
  Require(AlmostEqual(angle, 90.0, 1e-7),
          "perpendicular triplet must yield 90° turn");
}

void TestPointTurnDegReversal() {
  // Full reversal: (0,0) → (1,0) → (0,0). Legs are (1,0) and (-1,0),
  // dot product = -1, acos(-1) = 180 deg.
  const double angle = bbsolver::PointTurnDeg(
      {0.0, 0.0}, {1.0, 0.0}, {0.0, 0.0});
  Require(AlmostEqual(angle, 180.0, 1e-7),
          "reversal triplet must yield 180° turn");
}

void TestPointTurnDegDegenerateLegReturnsZero() {
  // When either leg has near-zero length, the formula's clamp would
  // otherwise produce NaN. The function returns 0 instead.
  const double zero_first_leg = bbsolver::PointTurnDeg(
      {1.0, 1.0}, {1.0, 1.0}, {2.0, 2.0});
  Require(AlmostEqual(zero_first_leg, 0.0, 1e-7),
          "zero-length first leg must yield 0° (degenerate guard)");
  const double zero_second_leg = bbsolver::PointTurnDeg(
      {0.0, 0.0}, {1.0, 1.0}, {1.0, 1.0});
  Require(AlmostEqual(zero_second_leg, 0.0, 1e-7),
          "zero-length second leg must yield 0° (degenerate guard)");
}

// ---------------------------------------------------------------------------
//  — ShapeFlatVertexPoint
// ---------------------------------------------------------------------------

void TestShapeFlatVertexPointReadsAtCorrectOffset() {
  // Per the shape-flat layout, vertex 0 position is at indices (2, 3),
  // vertex 1 at (8, 9), vertex 2 at (14, 15) — stride 6 per vertex.
  const std::vector<double> value = {
      0.0,  0.0,           // anchor
      1.0,  2.0,  0.0, 0.0, 0.0, 0.0,  // vertex 0
      3.0,  4.0,  0.0, 0.0, 0.0, 0.0,  // vertex 1
      5.0,  6.0,  0.0, 0.0, 0.0, 0.0,  // vertex 2
  };
  const std::pair<double, double> v0 = bbsolver::ShapeFlatVertexPoint(value, 0);
  Require(AlmostEqual(v0.first, 1.0) && AlmostEqual(v0.second, 2.0),
          "vertex 0 position must be read from indices (2, 3)");
  const std::pair<double, double> v1 = bbsolver::ShapeFlatVertexPoint(value, 1);
  Require(AlmostEqual(v1.first, 3.0) && AlmostEqual(v1.second, 4.0),
          "vertex 1 position must be read from indices (8, 9)");
  const std::pair<double, double> v2 = bbsolver::ShapeFlatVertexPoint(value, 2);
  Require(AlmostEqual(v2.first, 5.0) && AlmostEqual(v2.second, 6.0),
          "vertex 2 position must be read from indices (14, 15)");
}

// ---------------------------------------------------------------------------
//  — EvaluateClosedLoopShapeAtParam
// ---------------------------------------------------------------------------

void TestEvaluateClosedLoopReturnsFrontAtBoundaryParam() {
  // Per the docstring contract, when `param >= unique_count - 1e-12`
  // the function returns the wrapped front entry.
  // unique_count = closed_values.size() - 1 = 3.
  const std::vector<std::vector<double>> closed_values = {
      SingleVertexValue(0.0, 0.0),
      SingleVertexValue(1.0, 0.0),
      SingleVertexValue(1.0, 1.0),
      SingleVertexValue(0.0, 1.0),  // duplicate wrap (would close to 0)
      SingleVertexValue(0.0, 0.0),  // explicit duplicate of front
  };
  const std::vector<double> v = bbsolver::EvaluateClosedLoopShapeAtParam(
      closed_values, 8, 4.0);
  Require(!v.empty() && v.size() >= 4,
          "boundary param must return at least the value vector size");
  Require(AlmostEqual(v[2], closed_values.front()[2]) &&
              AlmostEqual(v[3], closed_values.front()[3]),
          "boundary param must return the front entry (closed-loop wrap)");
}

void TestEvaluateClosedLoopShortLoopReturnsFront() {
  // unique_count < 3 short-circuits to the front entry.
  const std::vector<std::vector<double>> short_loop = {
      SingleVertexValue(7.0, 9.0),
      SingleVertexValue(7.0, 9.0),
      SingleVertexValue(7.0, 9.0),
  };
  const std::vector<double> v = bbsolver::EvaluateClosedLoopShapeAtParam(
      short_loop, 8, 0.5);
  Require(!v.empty() && AlmostEqual(v[2], 7.0) && AlmostEqual(v[3], 9.0),
          "short loop (unique_count < 3) must return front entry");
}

// Note: an empty `closed_values` input is *not* a tested contract.
// The function's body computes `closed_values.size() - 1` as size_t,
// which underflows when the input is empty; the subsequent
// `unique_count < 3` guard relies on signed semantics and is bypassed
// for size_t = SIZE_MAX. Production callers never invoke the function
// with an empty container (the adaptive sampler always builds a
// closed loop with at least 4 entries), so this corner is left
// undocumented at the source layer and intentionally untested at the
// regression layer — exposing it would require a behaviour change
// that is out of scope for 's "preserve public behaviour" brief.

void TestEvaluateClosedLoopNegativeParamWraps() {
  // A negative param must wrap to a positive position in
  // [0, unique_count) via fmod. Construct a small loop and verify
  // that -0.5 maps to the segment near unique_count - 0.5 (i.e. the
  // segment between the last two distinct points).
  const std::vector<std::vector<double>> closed_values = {
      SingleVertexValue(0.0, 0.0),
      SingleVertexValue(10.0, 0.0),
      SingleVertexValue(10.0, 10.0),
      SingleVertexValue(0.0, 10.0),
      SingleVertexValue(0.0, 0.0),
  };
  const std::vector<double> neg = bbsolver::EvaluateClosedLoopShapeAtParam(
      closed_values, 8, -0.5);
  const std::vector<double> pos = bbsolver::EvaluateClosedLoopShapeAtParam(
      closed_values, 8, 3.5);  // unique_count=4, so 3.5 ~= -0.5 wrapped
  // The wrapping math: -0.5 → fmod(-0.5, 4) = -0.5, then +4 = 3.5,
  // then fmod again = 3.5. So neg should evaluate the same segment
  // as pos=3.5. Lock the equivalence on dims 2/3 (vertex position).
  Require(AlmostEqual(neg[2], pos[2], 1e-9) &&
              AlmostEqual(neg[3], pos[3], 1e-9),
          "negative param must wrap modulo unique_count");
}

// ---------------------------------------------------------------------------
//  — LockShapeFlatRotationalTangents
// ---------------------------------------------------------------------------

void TestLockTangentsNullPointerIsNoOp() {
  // Null pointer must return default-constructed stats without
  // crashing.
  const bbsolver::ShapeTangentLockStats stats =
      bbsolver::LockShapeFlatRotationalTangents(
          static_cast<std::vector<double>*>(nullptr));
  Require(stats.pairs_seen == 0 && stats.pairs_locked == 0 &&
              AlmostEqual(stats.max_deviation_before_deg, 0.0),
          "null pointer must yield zero-stat result");
}

void TestLockTangentsZeroVertexCountIsNoOp() {
  // A value vector too short to contain even one vertex must yield
  // zero stats. ShapeFlatVertexCountFromValues returns 0 for
  // sub-8-element vectors.
  std::vector<double> value = {0.0, 0.0};  // anchor only, no vertices
  const bbsolver::ShapeTangentLockStats stats =
      bbsolver::LockShapeFlatRotationalTangents(&value);
  Require(stats.pairs_seen == 0 && stats.pairs_locked == 0,
          "value with zero vertices must yield zero stats");
}

void TestLockTangentsAlreadyAlignedPair() {
  // A vertex with in-handle pointing exactly opposite to out-handle
  // (already anti-aligned, deviation 0°) must still be counted as
  // "seen" but no realignment needed.
  // in = (-1, 0), out = (1, 0) — already 180° apart.
  std::vector<double> value = SingleVertexValue(0.0, 0.0,
                                                /*in_x=*/-1.0, /*in_y=*/0.0,
                                                /*out_x=*/1.0,  /*out_y=*/0.0);
  const bbsolver::ShapeTangentLockStats stats =
      bbsolver::LockShapeFlatRotationalTangents(&value);
  Require(stats.pairs_seen == 1,
          "non-degenerate handle pair must be counted as seen");
  Require(stats.pairs_locked == 1,
          "non-degenerate handle pair must be locked (algorithm "
          "always applies the inversion regardless of starting alignment)");
  Require(AlmostEqual(stats.max_deviation_before_deg, 0.0, 1e-7),
          "already-aligned pair must report zero deviation");
  // Post-lock: lengths preserved, in/out remain anti-aligned.
  Require(AlmostEqual(value[4], -1.0, 1e-9) &&
              AlmostEqual(value[5], 0.0, 1e-9) &&
              AlmostEqual(value[6], 1.0, 1e-9) &&
              AlmostEqual(value[7], 0.0, 1e-9),
          "already-aligned pair must round-trip handle values exactly");
}

void TestLockTangentsRealignsMisalignedPair() {
  // Vertex with in-handle at (−1, 1) (length √2, 135° from +X) and
  // out-handle at (1, 1) (length √2, 45° from +X). Angle between
  // them is 90°, deviation from 180° is 90°. After locking, they
  // must be anti-aligned along the average of the original
  // directions (algorithm computes dir = out/|out| - in/|in|).
  std::vector<double> value = SingleVertexValue(0.0, 0.0,
                                                /*in_x=*/-1.0, /*in_y=*/1.0,
                                                /*out_x=*/1.0,  /*out_y=*/1.0);
  const bbsolver::ShapeTangentLockStats stats =
      bbsolver::LockShapeFlatRotationalTangents(&value);
  Require(stats.pairs_seen == 1 && stats.pairs_locked == 1,
          "misaligned pair must be counted and locked");
  Require(stats.max_deviation_before_deg > 89.0 &&
              stats.max_deviation_before_deg < 91.0,
          "90°-misaligned pair must report deviation near 90°");
  // After locking, in and out should be anti-parallel (their unit
  // vectors should sum to ~0). Check by computing dot product of
  // normalized handles.
  const double in_len = std::sqrt(value[4] * value[4] + value[5] * value[5]);
  const double out_len = std::sqrt(value[6] * value[6] + value[7] * value[7]);
  Require(in_len > 1e-9 && out_len > 1e-9,
          "handle lengths must be preserved (non-zero)");
  const double cos_after =
      (value[4] * value[6] + value[5] * value[7]) / (in_len * out_len);
  Require(cos_after < -0.99,
          "post-lock handles must be anti-parallel (cos ~= -1)");
}

void TestLockTangentsMultiKeyAggregatesStats() {
  // Multi-key overload sums pairs_seen and pairs_locked, and tracks
  // the max deviation across keys.
  std::vector<std::vector<double>> values = {
      SingleVertexValue(0.0, 0.0, -1.0,  0.0,  1.0,  0.0),  // aligned
      SingleVertexValue(0.0, 0.0, -1.0,  1.0,  1.0,  1.0),  // 90°
      SingleVertexValue(0.0, 0.0, -1.0,  0.0,  1.0,  0.0),  // aligned
  };
  const bbsolver::ShapeTangentLockStats total =
      bbsolver::LockShapeFlatRotationalTangents(&values);
  Require(total.pairs_seen == 3 && total.pairs_locked == 3,
          "multi-key overload must aggregate pair counts across keys");
  Require(total.max_deviation_before_deg > 89.0 &&
              total.max_deviation_before_deg < 91.0,
          "multi-key overload must track max deviation across keys");
}

void TestLockTangentsExceptHonorsSkipIndices() {
  // The "Except" variant skips keys whose skip_indices[i] is true.
  std::vector<std::vector<double>> values = {
      SingleVertexValue(0.0, 0.0, -1.0,  1.0,  1.0,  1.0),  // 90°
      SingleVertexValue(0.0, 0.0, -1.0,  1.0,  1.0,  1.0),  // 90° (skipped)
      SingleVertexValue(0.0, 0.0, -1.0,  1.0,  1.0,  1.0),  // 90°
  };
  const std::vector<bool> skip = {false, true, false};
  const bbsolver::ShapeTangentLockStats total =
      bbsolver::LockShapeFlatRotationalTangentsExcept(&values, skip);
  Require(total.pairs_seen == 2,
          "Except variant must skip flagged keys from pair count");
  Require(total.pairs_locked == 2,
          "Except variant must skip flagged keys from lock count");
  // Skipped key's handles must remain untouched. Original handles
  // in=(-1, 1) and out=(1, 1) have dot product 0 and lengths sqrt(2)
  // each — so cos(angle) = 0 (90° apart). Lock that the skip
  // preserved them exactly rather than locking them anti-parallel.
  const std::vector<double>& skipped = values[1];
  const double cos_skipped =
      (skipped[4] * skipped[6] + skipped[5] * skipped[7]) /
      (std::sqrt(skipped[4] * skipped[4] + skipped[5] * skipped[5]) *
       std::sqrt(skipped[6] * skipped[6] + skipped[7] * skipped[7]));
  Require(std::abs(cos_skipped) < 1e-7,
          "skipped key handles must remain at original 90° (cos ~= 0)");
}

void TestLockTangentsExceptShortSkipVectorIncludesAllKeys() {
  // A skip_indices vector shorter than the values vector treats the
  // missing entries as "do not skip" (default false). Lock the tail
  // gracefully.
  std::vector<std::vector<double>> values = {
      SingleVertexValue(0.0, 0.0, -1.0,  1.0,  1.0,  1.0),  // 90°
      SingleVertexValue(0.0, 0.0, -1.0,  1.0,  1.0,  1.0),  // 90°
  };
  const std::vector<bool> short_skip = {true};  // only first is skipped
  const bbsolver::ShapeTangentLockStats total =
      bbsolver::LockShapeFlatRotationalTangentsExcept(&values, short_skip);
  Require(total.pairs_seen == 1,
          "short skip vector must default missing entries to false");
}

}  // namespace

int main() {
  TestCatmullRomReturnsP1AtUZero();
  TestCatmullRomReturnsP2AtUOne();
  TestCatmullRomLinearOnAffinelyArrangedPoints();
  TestPointTurnDegStraightLineIsZero();
  TestPointTurnDegRightAngle();
  TestPointTurnDegReversal();
  TestPointTurnDegDegenerateLegReturnsZero();
  TestShapeFlatVertexPointReadsAtCorrectOffset();
  TestEvaluateClosedLoopReturnsFrontAtBoundaryParam();
  TestEvaluateClosedLoopShortLoopReturnsFront();
  TestEvaluateClosedLoopNegativeParamWraps();
  TestLockTangentsNullPointerIsNoOp();
  TestLockTangentsZeroVertexCountIsNoOp();
  TestLockTangentsAlreadyAlignedPair();
  TestLockTangentsRealignsMisalignedPair();
  TestLockTangentsMultiKeyAggregatesStats();
  TestLockTangentsExceptHonorsSkipIndices();
  TestLockTangentsExceptShortSkipVectorIncludesAllKeys();
  std::cout << "[PASS] test_motion_smooth_curve_and_tangent_lock\n";
  return 0;
}
