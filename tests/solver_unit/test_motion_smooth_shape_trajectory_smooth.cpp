//  focused test: the -extracted trajectory smoother.
//
// BuildShapeMotionTrajectorySmoothValues has two behaviour bands:
//
//   1. Pre-LSQ scaffolding (always runs):
//      * smoothing_passes = source_key_times.size() > 2 ? 1: 0
//      * smoothing_blend  = clamp(strength / (strength + 2.0), 0, 0.90)
//      * original_values  = MotionSmoothInterpolatedVector(...) per key
//      * max_turn_before_deg = ShapeFlatSequenceMaxTurnDeg(original_values, dims)
//      * smoothed_values  = original_values (copy)
//      * max_turn_after_deg = max_turn_before_deg
//
//   2. Main LSQ smoothing (runs iff size > 2 AND dims > 2):
//      * displacement_limit computed from extent + strength
//      * cubic Bezier control-point LSQ fit
//      * smoothed_values updated with blended cubic per interior key
//      * max_turn_after_deg recomputed from smoothed_values
//
// The pre-LSQ contracts are testable as pure data transformations.
// The main LSQ output depends on the (s11, s12, s22) least-squares
// solve; binding exact numerical output is brittle, so the main-path
// tests assert structural invariants (displacement_limit > 0,
// max_turn_after may decrease, smoothed_values shape matches).

#include "bbsolver/motion_smooth/motion_smooth_shape_trajectory_smooth.hpp"

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "bbsolver/domain.hpp"

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
// Same layout as // fixtures. Size 8.
std::vector<double> VertexAt(double px, double py) {
  return {0.0, 1.0, px, py, 0.0, 0.0, 0.0, 0.0};
}

// PropertySamples wrapping `raw` value vectors at evenly-spaced
// times in [0, 1]. The smoother calls MotionSmoothInterpolatedVector
// against the property's samples — those need to be populated.
bbsolver::PropertySamples MakeProperty(
    const std::vector<std::vector<double>>& raw_values) {
  bbsolver::PropertySamples ps;
  ps.property.dimensions = raw_values.empty()
      ? 8
: static_cast<int>(raw_values.front().size());
  ps.t_start_sec = 0.0;
  ps.t_end_sec = 1.0;
  const std::size_t n = raw_values.size();
  ps.samples.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    bbsolver::Sample s;
    s.t_sec = n > 1
        ? static_cast<double>(i) / static_cast<double>(n - 1)
: 0.0;
    s.v = raw_values[i];
    ps.samples.push_back(std::move(s));
  }
  return ps;
}

// ---------------------------------------------------------------------------
// Pre-LSQ scaffolding (always runs)
// ---------------------------------------------------------------------------

void TestSmoothingPassesIsOneWhenSizeAboveTwo() {
  // smoothing_passes = source_key_times.size() > 2 ? 1: 0.
  const std::vector<std::vector<double>> raw = {
      VertexAt(0.0, 0.0),
      VertexAt(0.5, 0.0),
      VertexAt(1.0, 0.0),
  };
  const bbsolver::PropertySamples property = MakeProperty(raw);
  const std::vector<double> key_times = {0.0, 0.5, 1.0};
  const bbsolver::ShapeMotionTrajectorySmoothResult r =
      bbsolver::BuildShapeMotionTrajectorySmoothValues(
          property, key_times, raw, /*vertex_count=*/1, /*dims=*/8,
          /*strength=*/1.0);
  Require(r.smoothing_passes == 1,
          "3 source keys must yield smoothing_passes=1");
}

void TestSmoothingPassesIsZeroWhenSizeAtOrBelowTwo() {
  // 0, 1, and 2 source_key_times all yield smoothing_passes=0.
  const std::vector<std::vector<double>> raw_1 = {VertexAt(0.0, 0.0)};
  const std::vector<std::vector<double>> raw_2 = {
      VertexAt(0.0, 0.0), VertexAt(1.0, 1.0),
  };
  const bbsolver::PropertySamples p1 = MakeProperty(raw_1);
  const bbsolver::PropertySamples p2 = MakeProperty(raw_2);
  const bbsolver::ShapeMotionTrajectorySmoothResult r1 =
      bbsolver::BuildShapeMotionTrajectorySmoothValues(
          p1, /*source_key_times=*/{0.0}, raw_1, 1, 8, 1.0);
  Require(r1.smoothing_passes == 0,
          "1 source key must yield smoothing_passes=0");
  const bbsolver::ShapeMotionTrajectorySmoothResult r2 =
      bbsolver::BuildShapeMotionTrajectorySmoothValues(
          p2, /*source_key_times=*/{0.0, 1.0}, raw_2, 1, 8, 1.0);
  Require(r2.smoothing_passes == 0,
          "2 source keys must yield smoothing_passes=0");
}

void TestSmoothingBlendFormulaAtLowStrengthMidStrengthAndCeiling() {
  // smoothing_blend = clamp(strength / (strength + 2.0), 0, 0.90).
  // strength=0   → 0/2 = 0.0
  // strength=2   → 2/4 = 0.5
  // strength=18  → 18/20 = 0.9 (at ceiling)
  // strength=100 → 100/102 ≈ 0.9803 → clamped to 0.90
  const std::vector<std::vector<double>> raw = {
      VertexAt(0.0, 0.0), VertexAt(1.0, 0.0),
  };
  const bbsolver::PropertySamples property = MakeProperty(raw);
  const std::vector<double> key_times = {0.0, 1.0};

  const bbsolver::ShapeMotionTrajectorySmoothResult zero =
      bbsolver::BuildShapeMotionTrajectorySmoothValues(
          property, key_times, raw, 1, 8, /*strength=*/0.0);
  Require(AlmostEqual(zero.smoothing_blend, 0.0, 1e-12),
          "strength=0 must yield smoothing_blend=0.0");

  const bbsolver::ShapeMotionTrajectorySmoothResult mid =
      bbsolver::BuildShapeMotionTrajectorySmoothValues(
          property, key_times, raw, 1, 8, /*strength=*/2.0);
  Require(AlmostEqual(mid.smoothing_blend, 0.5, 1e-12),
          "strength=2 must yield smoothing_blend=0.5");

  const bbsolver::ShapeMotionTrajectorySmoothResult ceiling =
      bbsolver::BuildShapeMotionTrajectorySmoothValues(
          property, key_times, raw, 1, 8, /*strength=*/100.0);
  Require(AlmostEqual(ceiling.smoothing_blend, 0.90, 1e-12),
          "high strength must clamp smoothing_blend to 0.90");
}

// ---------------------------------------------------------------------------
// Early-return paths (size <= 2 OR dims <= 2)
// ---------------------------------------------------------------------------

void TestSizeAtOrBelowTwoSkipsMainSmoothing() {
  // size <= 2 → early return; smoothed_values == original_values;
  // max_turn_after_deg == max_turn_before_deg; displacement_limit
  // stays at default 0.0.
  const std::vector<std::vector<double>> raw = {
      VertexAt(0.0, 0.0), VertexAt(3.0, 4.0),
  };
  const bbsolver::PropertySamples property = MakeProperty(raw);
  const bbsolver::ShapeMotionTrajectorySmoothResult r =
      bbsolver::BuildShapeMotionTrajectorySmoothValues(
          property, /*source_key_times=*/{0.0, 1.0}, raw, 1, 8, 1.0);
  Require(r.original_values.size() == 2,
          "2-key smoother must produce 2 original_values");
  Require(r.smoothed_values == r.original_values,
          "size<=2 early return: smoothed_values must equal original_values");
  Require(AlmostEqual(r.max_turn_after_deg, r.max_turn_before_deg),
          "size<=2 early return: max_turn_after == max_turn_before");
  Require(AlmostEqual(r.displacement_limit, 0.0),
          "size<=2 early return: displacement_limit stays at default 0");
}

void TestDimsAtOrBelowTwoSkipsMainSmoothing() {
  // dims <= 2 → early return regardless of size. Use a 3-frame
  // trajectory with dims=2 (anchor only, no vertex).
  const std::vector<std::vector<double>> raw = {
      {0.0, 0.0}, {0.5, 0.0}, {1.0, 0.0},
  };
  const bbsolver::PropertySamples property = MakeProperty(raw);
  const bbsolver::ShapeMotionTrajectorySmoothResult r =
      bbsolver::BuildShapeMotionTrajectorySmoothValues(
          property, /*source_key_times=*/{0.0, 0.5, 1.0}, raw,
          /*vertex_count=*/0, /*dims=*/2, /*strength=*/1.0);
  Require(r.original_values.size() == 3,
          "3-key dims=2 smoother must produce 3 original_values");
  Require(r.smoothed_values == r.original_values,
          "dims<=2 early return: smoothed_values must equal original_values");
  Require(AlmostEqual(r.displacement_limit, 0.0),
          "dims<=2 early return: displacement_limit stays at default 0");
  Require(r.smoothing_passes == 1,
          "dims<=2 still sets smoothing_passes based on size>2 (size=3)");
}

// ---------------------------------------------------------------------------
// Main LSQ path
// ---------------------------------------------------------------------------

void TestMainPathPopulatesDisplacementLimit() {
  // Main path runs when size > 2 AND dims > 2. Verify
  // displacement_limit is > 0 (the formula is
  // `clamp(max(strength*24, extent*0.04*strength), 6, extent*0.35)`,
  // and the lower clamp at 6.0 guarantees positivity for any positive
  // strength and a non-degenerate extent).
  std::vector<std::vector<double>> raw;
  raw.push_back(VertexAt(0.0, 0.0));
  raw.push_back(VertexAt(10.0, 5.0));
  raw.push_back(VertexAt(20.0, -5.0));
  raw.push_back(VertexAt(30.0, 5.0));
  raw.push_back(VertexAt(40.0, 0.0));
  const bbsolver::PropertySamples property = MakeProperty(raw);
  const std::vector<double> key_times = {0.0, 0.25, 0.5, 0.75, 1.0};
  const bbsolver::ShapeMotionTrajectorySmoothResult r =
      bbsolver::BuildShapeMotionTrajectorySmoothValues(
          property, key_times, raw, /*vertex_count=*/1, /*dims=*/8,
          /*strength=*/1.0);
  Require(r.original_values.size() == 5,
          "5-key smoother must produce 5 original_values");
  Require(r.smoothed_values.size() == 5,
          "5-key smoother must produce 5 smoothed_values");
  Require(r.displacement_limit > 0.0,
          "main path must populate displacement_limit > 0");
  // smoothing_passes stays at 1 (set unconditionally by size > 2).
  Require(r.smoothing_passes == 1,
          "5-key smoother must set smoothing_passes=1");
}

void TestSourceFidelityFlagsRoundTripFromOptionalInputs() {
  // When fidelity_times/fidelity_values are both provided and their
  // size > source_key_times.size(), source_fidelity_enabled = true
  // and source_fidelity_samples = fidelity_times->size(). When either
  // is null, both flags stay default (false / 0).
  std::vector<std::vector<double>> raw;
  for (int i = 0; i < 5; ++i) {
    raw.push_back(VertexAt(static_cast<double>(i) * 5.0, 0.0));
  }
  const bbsolver::PropertySamples property = MakeProperty(raw);
  const std::vector<double> key_times = {0.0, 0.25, 0.5, 0.75, 1.0};

  // No fidelity inputs → flags stay default.
  const bbsolver::ShapeMotionTrajectorySmoothResult r_off =
      bbsolver::BuildShapeMotionTrajectorySmoothValues(
          property, key_times, raw, 1, 8, 1.0);
  Require(!r_off.source_fidelity_enabled,
          "no fidelity inputs: source_fidelity_enabled must be false");
  Require(r_off.source_fidelity_samples == 0,
          "no fidelity inputs: source_fidelity_samples must be 0");

  // Provide more fidelity samples than key_times.
  std::vector<double> fidelity_times;
  std::vector<std::vector<double>> fidelity_values;
  for (int i = 0; i < 9; ++i) {
    fidelity_times.push_back(static_cast<double>(i) / 8.0);
    fidelity_values.push_back(VertexAt(
        static_cast<double>(i) * 2.5, std::sin(static_cast<double>(i))));
  }
  const bbsolver::ShapeMotionTrajectorySmoothResult r_on =
      bbsolver::BuildShapeMotionTrajectorySmoothValues(
          property, key_times, raw, 1, 8, 1.0,
          &fidelity_times, &fidelity_values);
  Require(r_on.source_fidelity_enabled,
          "fidelity inputs (size > key_times.size()): "
          "source_fidelity_enabled must be true");
  Require(r_on.source_fidelity_samples == 9,
          "source_fidelity_samples must equal fidelity_times.size()");
}

}  // namespace

int main() {
  TestSmoothingPassesIsOneWhenSizeAboveTwo();
  TestSmoothingPassesIsZeroWhenSizeAtOrBelowTwo();
  TestSmoothingBlendFormulaAtLowStrengthMidStrengthAndCeiling();
  TestSizeAtOrBelowTwoSkipsMainSmoothing();
  TestDimsAtOrBelowTwoSkipsMainSmoothing();
  TestMainPathPopulatesDisplacementLimit();
  TestSourceFidelityFlagsRoundTripFromOptionalInputs();
  std::cout << "[PASS] test_motion_smooth_shape_trajectory_smooth\n";
  return 0;
}
