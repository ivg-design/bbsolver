// MS35 focused test: the MS4-extracted closed-loop schedule helpers.
//
// Two pure functions live in motion_smooth_shape_loop_schedule:
//
//   1. TimesForClosedLoopParams(anchor_times, params)
//      Linear interpolation: each param in [0, unique_count] maps to
//      a wall-clock time inside the segment whose floor index it
//      falls into. Boundary params (<= 0 or >= unique_count) clamp
//      to the first / last anchor.
//
//   2. TimesForClosedLoopParamsByIntervalTravel(anchor_times, params,
//                                                values, vertex_count)
//      Refines the linear mapping by reshaping each anchor-bounded
//      interval proportionally to accumulated chord-distance travel.
//      Endpoints pin exactly. The result struct's `applied` flag is
//      `max_time_shift_sec > 1e-9`.
//
// Both functions are exercised only transitively today (via
// BuildAdaptiveClosedLoopShapeSamples → shape_flat orchestrator).
// This file locks their direct contracts.

#include "bbsolver/motion_smooth/motion_smooth_shape_loop_schedule.hpp"

#include <cmath>
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

// Shape-flat single-vertex value with position (px, py) and zero
// tangents. ShapeFlatControlDistance reduces to Euclidean position
// distance for zero-tangent values, making chord-travel arithmetic
// trivially assertable. Layout matches MS34's fixtures.
std::vector<double> VertexAt(double px, double py) {
  return {0.0, 1.0, px, py, 0.0, 0.0, 0.0, 0.0};
}

// ---------------------------------------------------------------------------
// TimesForClosedLoopParams
// ---------------------------------------------------------------------------

void TestTimesForParamsEmptyParamsReturnsEmpty() {
  // No params → no output, regardless of anchor count.
  const std::vector<double> result =
      bbsolver::TimesForClosedLoopParams({0.0, 1.0}, {});
  Require(result.empty(),
          "empty params input must yield empty time vector");
}

void TestTimesForParamsTooFewAnchorsReturnsEmpty() {
  // Fewer than 2 anchor times → cannot define a segment → empty.
  const std::vector<double> r0 =
      bbsolver::TimesForClosedLoopParams({}, {0.5});
  Require(r0.empty(),
          "empty anchor_times must yield empty result");
  const std::vector<double> r1 =
      bbsolver::TimesForClosedLoopParams({0.0}, {0.5});
  Require(r1.empty(),
          "single-anchor anchor_times must yield empty result");
}

void TestTimesForParamsBoundaryParamsClampToEndpoints() {
  // anchor_times = [0, 1, 2] → unique_count = 2. param <= 0 returns
  // anchor_times.front(); param >= unique_count returns
  // anchor_times.back().
  const std::vector<double> anchors = {0.0, 1.0, 2.0};
  const std::vector<double> times = bbsolver::TimesForClosedLoopParams(
      anchors, {-1.0, 0.0, 2.0, 3.0});
  Require(times.size() == 4,
          "output size must match params size");
  Require(AlmostEqual(times[0], 0.0),
          "param < 0 must clamp to anchor_times.front()");
  Require(AlmostEqual(times[1], 0.0),
          "param = 0 must clamp to anchor_times.front()");
  Require(AlmostEqual(times[2], 2.0),
          "param = unique_count must clamp to anchor_times.back()");
  Require(AlmostEqual(times[3], 2.0),
          "param > unique_count must clamp to anchor_times.back()");
}

void TestTimesForParamsInteriorLinearInterpolation() {
  // anchor_times = [10, 20, 50] → unique_count = 2.
  //   segment 0 spans [10, 20] for params in [0, 1].
  //   segment 1 spans [20, 50] for params in [1, 2].
  // param = 0.5 → 10 + 0.5*(20-10) = 15.
  // param = 1.0 → exactly 20 (segment boundary; floor 1, u 0).
  // param = 1.5 → 20 + 0.5*(50-20) = 35.
  const std::vector<double> anchors = {10.0, 20.0, 50.0};
  const std::vector<double> times = bbsolver::TimesForClosedLoopParams(
      anchors, {0.5, 1.0, 1.5});
  Require(AlmostEqual(times[0], 15.0),
          "param=0.5 must linearly interpolate to 15 in [10,20] segment");
  Require(AlmostEqual(times[1], 20.0),
          "param=1.0 must land on segment-boundary anchor 20");
  Require(AlmostEqual(times[2], 35.0),
          "param=1.5 must linearly interpolate to 35 in [20,50] segment");
}

// ---------------------------------------------------------------------------
// TimesForClosedLoopParamsByIntervalTravel
// ---------------------------------------------------------------------------

void TestIntervalTravelSizeMismatchFallsBackToLinear() {
  // When params.size() != values.size(), the helper bails after
  // populating the linear schedule. `applied=false` and the schedule
  // matches `TimesForClosedLoopParams`.
  const std::vector<double> anchors = {0.0, 1.0};
  const std::vector<double> params = {0.0, 0.5, 1.0};
  const std::vector<std::vector<double>> mismatched_values = {
      VertexAt(0.0, 0.0),
      VertexAt(1.0, 0.0),
  };  // size 2 vs params size 3
  const bbsolver::SourcePoseIntervalTimeSchedule s =
      bbsolver::TimesForClosedLoopParamsByIntervalTravel(
          anchors, params, mismatched_values, /*vertex_count=*/1);
  Require(s.times.size() == 3,
          "fallback must still populate linear times for all params");
  Require(!s.applied,
          "fallback path must not flag applied");
  Require(AlmostEqual(s.times[0], 0.0) &&
              AlmostEqual(s.times[1], 0.5) &&
              AlmostEqual(s.times[2], 1.0),
          "fallback times must match linear interpolation");
}

void TestIntervalTravelZeroVertexCountFallsBack() {
  // vertex_count <= 0 also triggers the fallback path.
  const std::vector<double> anchors = {0.0, 1.0};
  const std::vector<double> params = {0.0, 1.0};
  const std::vector<std::vector<double>> values = {
      VertexAt(0.0, 0.0),
      VertexAt(1.0, 0.0),
  };
  const bbsolver::SourcePoseIntervalTimeSchedule s =
      bbsolver::TimesForClosedLoopParamsByIntervalTravel(
          anchors, params, values, /*vertex_count=*/0);
  Require(!s.applied,
          "vertex_count=0 must not flag applied");
}

void TestIntervalTravelEndpointsPinExactly() {
  // Even when chord-travel retiming runs, the first and last anchor
  // times must round-trip exactly. Construct a 2-anchor schedule
  // with 3 params (boundary + interior + boundary).
  const std::vector<double> anchors = {0.0, 1.0};
  const std::vector<double> params = {0.0, 0.5, 1.0};
  const std::vector<std::vector<double>> values = {
      VertexAt(0.0, 0.0),
      VertexAt(3.0, 4.0),
      VertexAt(6.0, 0.0),
  };
  const bbsolver::SourcePoseIntervalTimeSchedule s =
      bbsolver::TimesForClosedLoopParamsByIntervalTravel(
          anchors, params, values, /*vertex_count=*/1);
  Require(s.times.size() == 3,
          "output must match params size");
  Require(AlmostEqual(s.times.front(), 0.0),
          "first endpoint must pin exactly to anchor_times.front()");
  Require(AlmostEqual(s.times.back(), 1.0),
          "last endpoint must pin exactly to anchor_times.back()");
}

void TestIntervalTravelInteriorRetimesByChordTravel() {
  // anchors = [0, 1] (single segment, unique_count = 1).
  // params = [0, 0.5, 1] (boundary + interior + boundary).
  // values: (0,0) → (3,4) → (6,4).
  //   segment 0→1: distance 5
  //   segment 1→2: distance 3
  //   total_travel = 8
  // Linear interior time at param=0.5 would be 0.5.
  // Chord-travel proportional interior time: cumulative travel at
  //   interior = 5; 5/8 = 0.625. Mapped to segment [0, 1] → 0.625.
  const std::vector<double> anchors = {0.0, 1.0};
  const std::vector<double> params = {0.0, 0.5, 1.0};
  const std::vector<std::vector<double>> values = {
      VertexAt(0.0, 0.0),
      VertexAt(3.0, 4.0),
      VertexAt(6.0, 4.0),
  };
  const bbsolver::SourcePoseIntervalTimeSchedule s =
      bbsolver::TimesForClosedLoopParamsByIntervalTravel(
          anchors, params, values, /*vertex_count=*/1);
  Require(s.times.size() == 3, "output size must equal params size");
  Require(AlmostEqual(s.times[0], 0.0),
          "interval-travel start endpoint must pin to 0.0");
  Require(AlmostEqual(s.times[1], 0.625, 1e-7),
          "interior key must retime to chord-travel proportion 5/8 = 0.625");
  Require(AlmostEqual(s.times[2], 1.0),
          "interval-travel end endpoint must pin to 1.0");
  Require(s.applied,
          "applied must flag true when max_time_shift > 1e-9");
  Require(s.max_time_shift_sec > 1e-9,
          "max_time_shift_sec must record the deviation from linear");
}

void TestIntervalTravelEvenChordTravelMatchesLinear() {
  // When chord travel is evenly distributed across the interior, the
  // retimed positions match the linear mapping → max_time_shift_sec
  // stays at ~0 → applied=false.
  // anchors=[0,1], params=[0, 0.5, 1], values move uniformly:
  //   (0,0) → (1,0) → (2,0). Each segment has travel 1; total 2;
  //   interior at travel 1/2 = 0.5 → matches linear at 0.5.
  const std::vector<double> anchors = {0.0, 1.0};
  const std::vector<double> params = {0.0, 0.5, 1.0};
  const std::vector<std::vector<double>> values = {
      VertexAt(0.0, 0.0),
      VertexAt(1.0, 0.0),
      VertexAt(2.0, 0.0),
  };
  const bbsolver::SourcePoseIntervalTimeSchedule s =
      bbsolver::TimesForClosedLoopParamsByIntervalTravel(
          anchors, params, values, /*vertex_count=*/1);
  Require(AlmostEqual(s.times[1], 0.5, 1e-7),
          "uniform-travel interior must match linear interp");
  Require(!s.applied,
          "applied must remain false when retiming yields no shift > 1e-9");
  Require(s.max_time_shift_sec <= 1e-9,
          "max_time_shift_sec must be at or below the applied threshold");
}

}  // namespace

int main() {
  TestTimesForParamsEmptyParamsReturnsEmpty();
  TestTimesForParamsTooFewAnchorsReturnsEmpty();
  TestTimesForParamsBoundaryParamsClampToEndpoints();
  TestTimesForParamsInteriorLinearInterpolation();
  TestIntervalTravelSizeMismatchFallsBackToLinear();
  TestIntervalTravelZeroVertexCountFallsBack();
  TestIntervalTravelEndpointsPinExactly();
  TestIntervalTravelInteriorRetimesByChordTravel();
  TestIntervalTravelEvenChordTravelMatchesLinear();
  std::cout << "[PASS] test_motion_smooth_shape_loop_schedule\n";
  return 0;
}
