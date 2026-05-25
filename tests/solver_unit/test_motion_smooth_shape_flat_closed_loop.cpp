// MS40 focused test: the MS24-extracted closed-loop resample helper.
//
// BuildShapeFlatClosedLoopAdaptiveResample has four behaviour bands
// depending on the (closed_loop, motion_smooth_source_fidelity)
// cross-product:
//
//   * (false, false): trivial schedule pass-through. schedule_values
//     = smooth_result.smoothed_values, no constraint indices, no
//     adaptive resample, no override.
//   * (false, true):  trivial schedule pass-through with fidelity.
//     schedule_values = smooth_result.original_values, constraint
//     indices = all-true (so the downstream tangent-lock skips
//     every key).
//   * (true, false):  adaptive loop runs, schedule_times derived
//     from EvenTimesForValueCount, constraint indices empty.
//   * (true, true):   adaptive loop runs + source_pose_interval
//     schedule populated + constraint indices derived from
//     adaptive_loop.params round tolerance (1e-9).
//
// Both `closed_loop=false` branches are pure pass-through and easy to
// assert. The `closed_loop=true` branches invoke
// BuildAdaptiveClosedLoopShapeSamples (MS3) — they're harder to bind
// to exact outputs, so the tests there assert structural invariants
// (resample flag set, schedule sized, override populated) without
// pinning exact numerical values.

#include "bbsolver/motion_smooth/motion_smooth_shape_flat_closed_loop.hpp"

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "bbsolver/domain.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_trajectory_smooth.hpp"

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

// Shape-flat single-vertex value with position (px, py) and zero
// tangents — same layout as MS34/MS35 fixtures. Total size 8.
std::vector<double> VertexAt(double px, double py) {
  return {0.0, 1.0, px, py, 0.0, 0.0, 0.0, 0.0};
}

bbsolver::SolverConfig MakeConfig(bool fidelity) {
  bbsolver::SolverConfig config;
  config.motion_smooth_source_fidelity = fidelity;
  return config;
}

// Build a minimal ShapeMotionTrajectorySmoothResult with N frames.
// original_values and smoothed_values are distinct (different
// positions) so we can verify which one feeds the pass-through.
bbsolver::ShapeMotionTrajectorySmoothResult MakeSmoothResult(int frames) {
  bbsolver::ShapeMotionTrajectorySmoothResult r;
  r.original_values.reserve(static_cast<std::size_t>(frames));
  r.smoothed_values.reserve(static_cast<std::size_t>(frames));
  for (int i = 0; i < frames; ++i) {
    // Originals trace a square: (0,0) → (10,0) → (10,10) → (0,10) → (0,0).
    const double angle = static_cast<double>(i) * 90.0;
    r.original_values.push_back(
        VertexAt(std::cos(angle * 3.14159 / 180.0) * 10.0,
                 std::sin(angle * 3.14159 / 180.0) * 10.0));
    // Smoothed values are uniformly +1 in y to discriminate from
    // originals — any pass-through test that returns smoothed
    // will see y-coord != originals'.
    std::vector<double> smoothed_value = r.original_values.back();
    smoothed_value[3] += 1.0;  // bump pos_y at vertex 0 (index 3)
    r.smoothed_values.push_back(smoothed_value);
  }
  r.smoothing_passes = 1;
  r.max_turn_before_deg = 75.0;
  r.max_turn_after_deg = 50.0;
  return r;
}

// ---------------------------------------------------------------------------
// closed_loop = false branches
// ---------------------------------------------------------------------------

void TestClosedLoopFalseFidelityOffPassesSmoothedThrough() {
  // (false, false): schedule_values must be smoothed_values, not
  // original_values; no constraint indices; no adaptive resample;
  // no turn override.
  const bbsolver::ShapeMotionTrajectorySmoothResult smooth = MakeSmoothResult(5);
  const bbsolver::SolverConfig config = MakeConfig(/*fidelity=*/false);
  const std::vector<double> key_times = {0.0, 0.25, 0.5, 0.75, 1.0};
  const bbsolver::ClosedLoopAdaptiveResampleResult r =
      bbsolver::BuildShapeFlatClosedLoopAdaptiveResample(
          smooth, key_times, config,
          /*vertex_count=*/1, /*dims=*/8, /*strength=*/1.0,
          /*closed_loop=*/false);
  Require(!r.adaptive_loop_resample,
          "closed_loop=false: adaptive_loop_resample must be false");
  Require(!r.trajectory_turn_after_overridden,
          "closed_loop=false: turn-after must not be overridden");
  Require(r.source_pose_constraint_indices.empty(),
          "closed_loop=false, fidelity=off: constraint indices must be empty");
  Require(r.schedule_times == key_times,
          "schedule_times must round-trip key_times exactly");
  Require(r.schedule_values.size() == smooth.smoothed_values.size(),
          "schedule_values size must match smoothed_values size");
  // Verify schedule_values == smoothed_values, not original_values.
  // Smoothed values have y-coord +1 vs original at index 3 (vertex 0 pos_y).
  Require(AlmostEqual(r.schedule_values.front()[3],
                      smooth.smoothed_values.front()[3]),
          "schedule_values must be smoothed_values (y-coord matches smoothed)");
}

void TestClosedLoopFalseFidelityOnPassesOriginalsThrough() {
  // (false, true): schedule_values must be original_values; constraint
  // indices = all-true; no adaptive resample; no turn override.
  const bbsolver::ShapeMotionTrajectorySmoothResult smooth = MakeSmoothResult(5);
  const bbsolver::SolverConfig config = MakeConfig(/*fidelity=*/true);
  const std::vector<double> key_times = {0.0, 0.25, 0.5, 0.75, 1.0};
  const bbsolver::ClosedLoopAdaptiveResampleResult r =
      bbsolver::BuildShapeFlatClosedLoopAdaptiveResample(
          smooth, key_times, config,
          /*vertex_count=*/1, /*dims=*/8, /*strength=*/1.0,
          /*closed_loop=*/false);
  Require(!r.adaptive_loop_resample,
          "closed_loop=false: adaptive_loop_resample must be false");
  Require(!r.trajectory_turn_after_overridden,
          "closed_loop=false: turn-after must not be overridden");
  Require(r.source_pose_constraint_indices.size() ==
              smooth.original_values.size(),
          "fidelity=on: constraint indices must size to schedule_values");
  for (bool flag : r.source_pose_constraint_indices) {
    Require(flag,
            "closed_loop=false, fidelity=on: every constraint index must be true");
  }
  Require(AlmostEqual(r.schedule_values.front()[3],
                      smooth.original_values.front()[3]),
          "schedule_values must be original_values (y-coord matches originals)");
}

// ---------------------------------------------------------------------------
// closed_loop = true branches
// ---------------------------------------------------------------------------

void TestClosedLoopTrueFidelityOffInvokesAdaptiveResample() {
  // (true, false): adaptive_loop_resample=true; trajectory_turn_after
  // overridden; schedule_times come from EvenTimesForValueCount (not
  // key_times); constraint indices empty.
  const bbsolver::ShapeMotionTrajectorySmoothResult smooth = MakeSmoothResult(5);
  const bbsolver::SolverConfig config = MakeConfig(/*fidelity=*/false);
  const std::vector<double> key_times = {0.0, 0.25, 0.5, 0.75, 1.0};
  const bbsolver::ClosedLoopAdaptiveResampleResult r =
      bbsolver::BuildShapeFlatClosedLoopAdaptiveResample(
          smooth, key_times, config,
          /*vertex_count=*/1, /*dims=*/8, /*strength=*/1.0,
          /*closed_loop=*/true);
  Require(r.adaptive_loop_resample,
          "closed_loop=true: adaptive_loop_resample must be true");
  Require(r.trajectory_turn_after_overridden,
          "closed_loop=true: turn-after must be overridden");
  Require(r.source_pose_constraint_indices.empty(),
          "closed_loop=true, fidelity=off: constraint indices must be empty");
  Require(r.schedule_times.size() == r.schedule_values.size(),
          "schedule_times and schedule_values must have matching size");
  // EvenTimesForValueCount on [0.0, 1.0] with size N produces times
  // [0.0, 1/(N-1), 2/(N-1), ..., 1.0]. First and last must be the
  // original anchors regardless of N.
  Require(AlmostEqual(r.schedule_times.front(), 0.0),
          "EvenTimesForValueCount must pin the start anchor");
  Require(AlmostEqual(r.schedule_times.back(), 1.0),
          "EvenTimesForValueCount must pin the end anchor");
}

void TestClosedLoopTrueFidelityOnPopulatesIntervalSchedule() {
  // (true, true): source_pose_interval_schedule.times populated;
  // constraint indices computed from adaptive_loop.params (true for
  // params within 1e-9 of an integer).
  const bbsolver::ShapeMotionTrajectorySmoothResult smooth = MakeSmoothResult(5);
  const bbsolver::SolverConfig config = MakeConfig(/*fidelity=*/true);
  const std::vector<double> key_times = {0.0, 0.25, 0.5, 0.75, 1.0};
  const bbsolver::ClosedLoopAdaptiveResampleResult r =
      bbsolver::BuildShapeFlatClosedLoopAdaptiveResample(
          smooth, key_times, config,
          /*vertex_count=*/1, /*dims=*/8, /*strength=*/1.0,
          /*closed_loop=*/true);
  Require(r.adaptive_loop_resample,
          "closed_loop=true: adaptive_loop_resample must be true");
  Require(r.trajectory_turn_after_overridden,
          "closed_loop=true: turn-after must be overridden");
  Require(!r.source_pose_constraint_indices.empty(),
          "closed_loop=true, fidelity=on: constraint indices must be populated");
  Require(r.source_pose_constraint_indices.size() == r.schedule_values.size(),
          "constraint indices must size to schedule_values");
  // At least one constraint index must be true — the original key
  // positions (params at integer values) always survive into the
  // adaptive_loop.params array.
  bool any_true = false;
  for (bool flag : r.source_pose_constraint_indices) {
    if (flag) { any_true = true; break; }
  }
  Require(any_true,
          "closed_loop=true, fidelity=on: at least one constraint index must "
          "be true (integer-param keys preserved)");
  Require(r.source_pose_interval_schedule.times.size() ==
              r.schedule_times.size(),
          "source_pose_interval_schedule must size to schedule_times");
}

}  // namespace

int main() {
  TestClosedLoopFalseFidelityOffPassesSmoothedThrough();
  TestClosedLoopFalseFidelityOnPassesOriginalsThrough();
  TestClosedLoopTrueFidelityOffInvokesAdaptiveResample();
  TestClosedLoopTrueFidelityOnPopulatesIntervalSchedule();
  std::cout << "[PASS] test_motion_smooth_shape_flat_closed_loop\n";
  return 0;
}
