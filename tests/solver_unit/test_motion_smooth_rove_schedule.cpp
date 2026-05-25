// MS34 focused test: the MS8-extracted rove schedule helper.
//
// BuildShapeMotionRoveScheduleFromValues builds the time-and-value
// schedule that drives the final motion-smooth shape_flat output. Its
// contract has three behaviour bands:
//
//   * Static-duplicate removal: interior keys whose
//     ShapeFlatControlDistance from the previous kept key is
//     <= 1e-7 are dropped from the schedule.
//   * Endpoint preservation: first/last sample times are always
//     kept verbatim, regardless of static-duplicate state or rove
//     application.
//   * Travel-proportional retiming (when `apply_rove=true`): interior
//     keys are reassigned times in proportion to cumulative chord
//     travel from start, clamped against `min_step` to keep the
//     sequence monotonic.
//
// MS18 policy locks the `1e-7` literal in this TU's source; this test
// locks the behaviour empirically — a future regression that changed
// `static_eps` to `1e-9` or `1e-5` would compile cleanly and likely
// pass MS18's grep, but would fail one of the static-duplicate or
// retiming sub-tests here.

#include "bbsolver/motion_smooth/motion_smooth_shape_rove_schedule.hpp"

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

// Shape-flat single-vertex value at a given position with zero
// tangents. ShapeFlatControlDistance sums squared distances across
// position + in + out (3 control channels). With zero tangents,
// distance reduces to the Euclidean distance between vertex
// positions, which makes assertion math trivial.
//
// Layout: [closed=0, vertex_count=1, pos_x, pos_y, in_x=0, in_y=0,
//          out_x=0, out_y=0]. Total size 8 = 2 + 1*6.
std::vector<double> VertexAt(double pos_x, double pos_y) {
  return {0.0, 1.0, pos_x, pos_y, 0.0, 0.0, 0.0, 0.0};
}

void TestRoveScheduleHandlesEmptyInput() {
  // Empty inputs must not crash; helper returns a default-constructed
  // result with source_key_count=0 and no times/values.
  const bbsolver::ShapeMotionRoveSchedule schedule =
      bbsolver::BuildShapeMotionRoveScheduleFromValues(
          /*source_key_times=*/{},
          /*source_values=*/{},
          /*vertex_count=*/1,
          /*apply_rove=*/true);
  Require(schedule.source_key_count == 0,
          "empty input must yield source_key_count=0");
  Require(schedule.times.empty() && schedule.values.empty(),
          "empty input must yield empty times/values");
  Require(!schedule.rove_applied,
          "empty input must not flag rove_applied");
}

void TestRoveScheduleSinglePointReturnsAsIs() {
  // Single-point schedule: the helper keeps the one entry as an
  // endpoint, but cannot retime (no segments). total_travel stays
  // zero and rove_applied stays false even when apply_rove=true.
  const std::vector<double> times = {0.5};
  const std::vector<std::vector<double>> values = {VertexAt(3.0, 4.0)};
  const bbsolver::ShapeMotionRoveSchedule schedule =
      bbsolver::BuildShapeMotionRoveScheduleFromValues(
          times, values, /*vertex_count=*/1, /*apply_rove=*/true);
  Require(schedule.source_key_count == 1,
          "single-point input must yield source_key_count=1");
  Require(schedule.times.size() == 1,
          "single-point schedule must carry exactly one time");
  Require(AlmostEqual(schedule.times.front(), 0.5),
          "single-point schedule must preserve the input time");
  Require(AlmostEqual(schedule.total_travel, 0.0),
          "single-point schedule has no segments — total_travel must be 0");
  Require(!schedule.rove_applied,
          "single-point schedule cannot rove (no interior keys)");
}

void TestRoveScheduleRemovesInteriorStaticDuplicates() {
  // 1e-7 static-duplicate epsilon: interior keys whose distance from
  // the previous kept key is <= 1e-7 are dropped. First/last keys
  // are endpoints and ALWAYS preserved, even if they would otherwise
  // be static duplicates.
  //
  // Construct: 5 source keys at times 0, 0.25, 0.5, 0.75, 1.0 with
  // positions (0,0), (0,0), (3,4), (3,4), (5,12). The 2nd is a
  // duplicate of the 1st (kept as endpoint), the 4th is a duplicate
  // of the 3rd (interior — must be dropped).
  const std::vector<double> times = {0.0, 0.25, 0.5, 0.75, 1.0};
  const std::vector<std::vector<double>> values = {
      VertexAt(0.0, 0.0),
      VertexAt(0.0, 0.0),
      VertexAt(3.0, 4.0),
      VertexAt(3.0, 4.0),
      VertexAt(5.0, 12.0),
  };
  const bbsolver::ShapeMotionRoveSchedule schedule =
      bbsolver::BuildShapeMotionRoveScheduleFromValues(
          times, values, /*vertex_count=*/1, /*apply_rove=*/false);
  Require(schedule.source_key_count == 5,
          "source_key_count must reflect input size");
  // Index 1 is interior (and a duplicate of index 0); index 3 is
  // interior (and a duplicate of index 2). Both should drop.
  // Index 0, 2, 4 should survive.
  Require(schedule.times.size() == 3,
          "two interior static-duplicates must drop, leaving 3 keys");
  Require(schedule.static_keys_removed == 2,
          "static_keys_removed must report 2");
  Require(AlmostEqual(schedule.times.front(), 0.0) &&
              AlmostEqual(schedule.times.back(), 1.0),
          "endpoints must be preserved at original times");
}

void TestRoveScheduleAlwaysPreservesEndpoints() {
  // Even when the first sample is the same as the second (or last is
  // same as second-to-last), endpoints must remain. Construct a
  // schedule where ALL values are identical — the result must still
  // carry the first and last entries.
  const std::vector<double> times = {0.0, 0.5, 1.0};
  const std::vector<std::vector<double>> values = {
      VertexAt(7.0, 11.0),
      VertexAt(7.0, 11.0),
      VertexAt(7.0, 11.0),
  };
  const bbsolver::ShapeMotionRoveSchedule schedule =
      bbsolver::BuildShapeMotionRoveScheduleFromValues(
          times, values, /*vertex_count=*/1, /*apply_rove=*/false);
  // The middle entry is a static duplicate of the first → dropped.
  // The last entry is a static duplicate of (now-kept) first → BUT
  // last is an endpoint, so it must survive.
  Require(schedule.times.size() == 2,
          "all-identical 3-key schedule must keep both endpoints (interior drops)");
  Require(AlmostEqual(schedule.times.front(), 0.0),
          "first endpoint must round-trip exactly");
  Require(AlmostEqual(schedule.times.back(), 1.0),
          "last endpoint must round-trip exactly");
  Require(schedule.static_keys_removed == 1,
          "only the interior duplicate counts as removed");
}

void TestRoveScheduleAppliesTravelProportionalRetiming() {
  // 3 distinct shape keys with apply_rove=true. Total travel
  // distance: (0,0)→(3,4)=5, (3,4)→(3,4) doesn't apply since we use
  // 3 distinct shapes. Let's use (0,0)→(3,4)→(6,4): segment 1 has
  // distance 5 (Euclidean), segment 2 has distance 3. Total = 8.
  //
  // Linear-time placement would put the middle key at t=0.5.
  // Travel-proportional placement puts it at t = 5/8 = 0.625.
  const std::vector<double> times = {0.0, 0.5, 1.0};
  const std::vector<std::vector<double>> values = {
      VertexAt(0.0, 0.0),
      VertexAt(3.0, 4.0),
      VertexAt(6.0, 4.0),
  };
  const bbsolver::ShapeMotionRoveSchedule schedule =
      bbsolver::BuildShapeMotionRoveScheduleFromValues(
          times, values, /*vertex_count=*/1, /*apply_rove=*/true);
  Require(schedule.times.size() == 3,
          "3 distinct keys must all survive (no static duplicates)");
  Require(schedule.static_keys_removed == 0,
          "no static duplicates expected for distinct shapes");
  Require(AlmostEqual(schedule.total_travel, 8.0, 1e-7),
          "total_travel must sum to 5 + 3 = 8");
  Require(AlmostEqual(schedule.max_segment_travel, 5.0, 1e-7),
          "max_segment_travel must be the longest segment (5)");
  // Middle key retimed to travel proportion 5/8 = 0.625.
  Require(AlmostEqual(schedule.times[1], 0.625, 1e-6),
          "middle key must retime to travel-proportional position 5/8");
  Require(AlmostEqual(schedule.times.front(), 0.0) &&
              AlmostEqual(schedule.times.back(), 1.0),
          "endpoints must remain at original times under apply_rove=true");
  Require(schedule.rove_applied,
          "rove_applied must flag true after retiming");
  Require(schedule.max_time_shift_sec > 0.0,
          "max_time_shift_sec must be positive when retiming occurred");
}

void TestRoveScheduleApplyRoveFalseSkipsRetiming() {
  // Same input as the previous test, but apply_rove=false. The
  // helper must still detect static duplicates and populate travel
  // diagnostics, but must NOT shift any interior times.
  const std::vector<double> times = {0.0, 0.5, 1.0};
  const std::vector<std::vector<double>> values = {
      VertexAt(0.0, 0.0),
      VertexAt(3.0, 4.0),
      VertexAt(6.0, 4.0),
  };
  const bbsolver::ShapeMotionRoveSchedule schedule =
      bbsolver::BuildShapeMotionRoveScheduleFromValues(
          times, values, /*vertex_count=*/1, /*apply_rove=*/false);
  Require(schedule.times.size() == 3,
          "apply_rove=false: all distinct keys still survive");
  Require(AlmostEqual(schedule.times[1], 0.5, 1e-9),
          "apply_rove=false: interior time must remain at original");
  Require(!schedule.rove_applied,
          "apply_rove=false: rove_applied must remain false");
  Require(AlmostEqual(schedule.max_time_shift_sec, 0.0, 1e-9),
          "apply_rove=false: max_time_shift_sec must be 0");
  // Travel diagnostics still populated.
  Require(AlmostEqual(schedule.total_travel, 8.0, 1e-7),
          "apply_rove=false: total_travel diagnostic still computed");
}

}  // namespace

int main() {
  TestRoveScheduleHandlesEmptyInput();
  TestRoveScheduleSinglePointReturnsAsIs();
  TestRoveScheduleRemovesInteriorStaticDuplicates();
  TestRoveScheduleAlwaysPreservesEndpoints();
  TestRoveScheduleAppliesTravelProportionalRetiming();
  TestRoveScheduleApplyRoveFalseSkipsRetiming();
  std::cout << "[PASS] test_motion_smooth_rove_schedule\n";
  return 0;
}
