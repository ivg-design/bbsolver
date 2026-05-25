// MS9 focused test: the RDP keep-mask helper
// (ShapeMotionSourceKeyRdpKeep) was file-local to
// motion_smooth_shape_schedule.cpp before MS6. MS6 promoted it to a
// public bbsolver:: surface (motion_smooth_shape_source_key_schedule.hpp)
// so the schedule builder TU and any future direct callers can share
// the same recursion. This test exercises the helper and the builder
// directly to lock the simplification behavior that previously was
// only exercised transitively through the shape_flat integration test.

#include "bbsolver/motion_smooth/motion_smooth_shape_source_key_schedule.hpp"

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

// 6-dim values: anchor (x, y) at dims 0/1, four scalar channels at
// dims 2..5. Keeps the RDP perpendicular-distance math simple — any
// chord with a significant interior bow on dim 2 should keep the bow
// vertex.
std::vector<double> ScalarValue(double dim2,
                                double dim3 = 0.0,
                                double dim4 = 0.0,
                                double dim5 = 0.0) {
  return {0.0, 0.0, dim2, dim3, dim4, dim5};
}

void TestRdpKeepReturnsImmediatelyForTrivialIntervals() {
  // A degenerate `[first, last]` interval with no interior points must
  // leave the keep mask untouched. Pre-condition for the RDP recursion
  // base case.
  const std::vector<double> times = {0.0, 1.0};
  const std::vector<std::vector<double>> values = {ScalarValue(0.0),
                                                   ScalarValue(1.0)};
  std::vector<bool> keep = {true, true};
  bbsolver::ShapeMotionSourceKeyRdpKeep(times, values, 0, 1, 0.1, &keep);
  Require(keep.size() == 2,
          "keep mask size must be preserved for trivial intervals");
  Require(keep[0] && keep[1],
          "endpoint markers must remain true after trivial-interval call");
}

void TestRdpKeepHonorsNullPointerGuard() {
  // The helper's contract is to bail when `keep == nullptr`. A
  // regression that dropped the guard would crash on this call.
  const std::vector<double> times = {0.0, 1.0, 2.0};
  const std::vector<std::vector<double>> values = {
      ScalarValue(0.0), ScalarValue(1.0), ScalarValue(2.0)};
  bbsolver::ShapeMotionSourceKeyRdpKeep(times, values, 0, 2, 0.1, nullptr);
  // No assertion here beyond "did not crash"; the test succeeding
  // means the early return fired.
}

void TestRdpKeepMarksLargeInteriorBow() {
  // Straight-line trajectory on dim 2 (values 0, 0.5, 1) at uniform
  // times — interior point is on the chord, so RDP should keep
  // nothing.
  const std::vector<double> straight_times = {0.0, 1.0, 2.0};
  const std::vector<std::vector<double>> straight_values = {
      ScalarValue(0.0), ScalarValue(0.5), ScalarValue(1.0)};
  std::vector<bool> straight_keep = {true, false, true};
  bbsolver::ShapeMotionSourceKeyRdpKeep(
      straight_times, straight_values, 0, 2, 0.1, &straight_keep);
  Require(!straight_keep[1],
          "straight-line interior must not be kept above tolerance 0.1");

  // Now a bowed trajectory: the interior point is at dim2=5 while
  // the chord midpoint is 0.5 — distance ~= 4.5, well above tolerance.
  const std::vector<double> bowed_times = {0.0, 1.0, 2.0};
  const std::vector<std::vector<double>> bowed_values = {
      ScalarValue(0.0), ScalarValue(5.0), ScalarValue(1.0)};
  std::vector<bool> bowed_keep = {true, false, true};
  bbsolver::ShapeMotionSourceKeyRdpKeep(
      bowed_times, bowed_values, 0, 2, 0.1, &bowed_keep);
  Require(bowed_keep[1],
          "bowed interior point must be kept when distance exceeds tolerance");
}

void TestRdpKeepRecursesIntoSubIntervals() {
  // Five samples, large bows at indices 1 and 3, small at 2. RDP
  // should keep indices 1 and 3 (recursing into [0,2] and [2,4] after
  // picking 2 first if 2 is the global max, or directly otherwise).
  const std::vector<double> times = {0.0, 1.0, 2.0, 3.0, 4.0};
  const std::vector<std::vector<double>> values = {
      ScalarValue(0.0),
      ScalarValue(10.0),
      ScalarValue(0.5),
      ScalarValue(-10.0),
      ScalarValue(0.0)};
  std::vector<bool> keep = {true, false, false, false, true};
  bbsolver::ShapeMotionSourceKeyRdpKeep(times, values, 0, 4, 0.5, &keep);
  // At least one of the high-bow interior points must be kept; the
  // recursive sub-interval must also reach the other.
  Require(keep[1] || keep[3],
          "at least one high-bow interior point must be kept on first pass");
  Require(keep[1] && keep[3],
          "RDP must recurse into both sub-intervals and keep both bow indices");
  Require(!keep[2],
          "small-bow interior should not be kept above tolerance 0.5");
}

bbsolver::PropertySamples MakePropertySamples(int dim_count,
                                              double anchor_x,
                                              double anchor_y) {
  bbsolver::PropertySamples ps;
  ps.property.dimensions = dim_count;
  bbsolver::Sample sample;
  sample.t_sec = 0.0;
  sample.v.assign(static_cast<std::size_t>(dim_count), 0.0);
  sample.v[0] = anchor_x;
  sample.v[1] = anchor_y;
  ps.samples.push_back(std::move(sample));
  return ps;
}

void TestBuildShapeMotionSourceKeyScheduleAnchorPinning() {
  // The builder must pin dims 0/1 of every raw_value to the property's
  // first sample anchor, regardless of what MotionSmoothInterpolatedVector
  // returned. This is the "anchor (first two dims) is never touched"
  // contract called out in the MS6-MS10 docs entry.
  const int dims = 6;
  const bbsolver::PropertySamples property_samples =
      MakePropertySamples(dims, 42.0, -7.5);

  // A two-key schedule short-circuits the simplification path; the
  // builder returns the raw schedule directly. Good for testing the
  // anchor-pinning invariant in isolation.
  const std::vector<double> times = {0.0, 1.0};
  const std::vector<std::vector<double>> raw = {
      ScalarValue(0.0), ScalarValue(0.0)};
  const bbsolver::ShapeMotionSourceKeySchedule schedule =
      bbsolver::BuildShapeMotionSourceKeySchedule(
          property_samples, times, raw, dims, 1.0);

  Require(schedule.raw_count == 2,
          "raw_count must reflect input key count");
  Require(schedule.simplified_count == 2,
          "two-key schedule must short-circuit simplification");
  Require(!schedule.simplification_enabled,
          "simplification gate must stay off for <= 2 keys");
  Require(schedule.raw_values.size() == 2,
          "raw_values must mirror input size");
  for (const std::vector<double>& value : schedule.raw_values) {
    Require(value.size() >= 2,
            "every raw_value must be at least 2-D for anchor pin");
    Require(std::abs(value[0] - 42.0) < 1e-12,
            "raw_value dim 0 must pin to property anchor X");
    Require(std::abs(value[1] - (-7.5)) < 1e-12,
            "raw_value dim 1 must pin to property anchor Y");
  }
}

void TestBuildShapeMotionSourceKeyScheduleToleranceClamp() {
  // The simplify tolerance is clamped to [0.75, 3.0] regardless of
  // strength input. Verify the clamp at both extremes.
  const int dims = 6;
  const bbsolver::PropertySamples property_samples =
      MakePropertySamples(dims, 0.0, 0.0);
  // A 4-key schedule lets simplification trigger (size > 2). The
  // tolerance is `clamp(strength * 0.5, 0.75, 3.0)`.
  const std::vector<double> times = {0.0, 1.0, 2.0, 3.0};
  const std::vector<std::vector<double>> raw = {
      ScalarValue(0.0), ScalarValue(0.0), ScalarValue(0.0), ScalarValue(0.0)};

  // strength=0 → 0*0.5 = 0, clamped up to 0.75.
  const bbsolver::ShapeMotionSourceKeySchedule low =
      bbsolver::BuildShapeMotionSourceKeySchedule(
          property_samples, times, raw, dims, 0.0);
  Require(std::abs(low.simplify_tolerance - 0.75) < 1e-12,
          "low strength must clamp simplify_tolerance to 0.75 floor");

  // strength=10 → 5.0, clamped down to 3.0.
  const bbsolver::ShapeMotionSourceKeySchedule high =
      bbsolver::BuildShapeMotionSourceKeySchedule(
          property_samples, times, raw, dims, 10.0);
  Require(std::abs(high.simplify_tolerance - 3.0) < 1e-12,
          "high strength must clamp simplify_tolerance to 3.0 ceiling");

  // Mid strength=2 → 1.0, inside the clamp.
  const bbsolver::ShapeMotionSourceKeySchedule mid =
      bbsolver::BuildShapeMotionSourceKeySchedule(
          property_samples, times, raw, dims, 2.0);
  Require(std::abs(mid.simplify_tolerance - 1.0) < 1e-12,
          "mid strength must produce strength*0.5 inside clamp");
}

}  // namespace

int main() {
  TestRdpKeepReturnsImmediatelyForTrivialIntervals();
  TestRdpKeepHonorsNullPointerGuard();
  TestRdpKeepMarksLargeInteriorBow();
  TestRdpKeepRecursesIntoSubIntervals();
  TestBuildShapeMotionSourceKeyScheduleAnchorPinning();
  TestBuildShapeMotionSourceKeyScheduleToleranceClamp();
  std::cout << "[PASS] test_motion_smooth_source_key_schedule\n";
  return 0;
}
