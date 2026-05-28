//  focused test: the -extracted key-emission helper.
//
// EmitMotionSmoothShapeFlatKeysFromRoveSchedule materializes the
// PropertyKeys::keys vector from a ShapeMotionRoveSchedule. The
// orchestration is small but the contract is load-bearing:
//
//   * First key's `interp_in` is always Linear (the outer edge).
//   * Last key's `interp_out` is always Linear (the outer edge).
//   * Interior interp choice (`interp_in` for ki>0, `interp_out` for
//     ki+1<size) is Bezier iff `use_ease=true`, else Linear.
//   * `temporal_continuous` and (initially) `temporal_auto_bezier`
//     mirror `use_ease`. After ApplyMotionSmoothBezierEase runs (only
//     when `use_ease=true`), `temporal_auto_bezier` is flipped to
//     `false` for keys that participated in a segment pair.
//   * Times and values round-trip from the rove schedule into the
//     emitted Key vector unchanged.
//
//  already locks the early-return guards of
// ApplyMotionSmoothBezierEase in isolation; this file locks the
// integration — that the helper actually invokes the ease curve when
// `use_ease=true` (verified by observing the ease arrays mutate).

#include "bbsolver/motion_smooth/motion_smooth_shape_flat_key_emission.hpp"

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "bbsolver/domain.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_rove_schedule.hpp"

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

bbsolver::PropertySamples MakeProperty(int dim_count) {
  bbsolver::PropertySamples ps;
  ps.property.dimensions = dim_count;
  bbsolver::Sample anchor;
  anchor.t_sec = 0.0;
  anchor.v.assign(static_cast<std::size_t>(dim_count), 0.0);
  ps.samples.push_back(std::move(anchor));
  return ps;
}

bbsolver::ShapeMotionRoveSchedule MakeRove(
    const std::vector<double>& times,
    const std::vector<std::vector<double>>& values) {
  bbsolver::ShapeMotionRoveSchedule rove;
  rove.times = times;
  rove.values = values;
  return rove;
}

bbsolver::SolverConfig EaseEnabledConfig() {
  bbsolver::SolverConfig config;
  config.motion_smooth_use_ease = true;
  config.allow_bezier = true;
  config.motion_smooth_bezier_x1 = 0.25;
  config.motion_smooth_bezier_y1 = 0.50;
  config.motion_smooth_bezier_x2 = 0.75;
  config.motion_smooth_bezier_y2 = 0.50;
  config.min_influence = 0.01;
  config.max_influence = 100.0;
  return config;
}

void TestEmitFromEmptyScheduleReturnsEmptyKeys() {
  // The orchestrator's caller has already short-circuited empty
  // schedules via the `rove_schedule.times.size() < 2` gate, but the
  // helper itself must still handle the boundary gracefully.
  const bbsolver::PropertySamples property = MakeProperty(2);
  const bbsolver::SolverConfig config;
  const bbsolver::ShapeMotionRoveSchedule empty_rove;
  const std::vector<bbsolver::Key> keys =
      bbsolver::EmitMotionSmoothShapeFlatKeysFromRoveSchedule(
          empty_rove, property, config, 2, /*use_ease=*/false);
  Require(keys.empty(), "empty rove schedule must yield empty key vector");
}

void TestEmitSingleKeyEdgesAreBothLinear() {
  // With a single key, the same key is BOTH first and last, so both
  // `interp_in` and `interp_out` must pin to Linear regardless of
  // `use_ease`.
  const bbsolver::PropertySamples property = MakeProperty(2);
  const bbsolver::SolverConfig config = EaseEnabledConfig();
  const bbsolver::ShapeMotionRoveSchedule rove =
      MakeRove({0.5}, {{1.0, 2.0}});
  const std::vector<bbsolver::Key> keys =
      bbsolver::EmitMotionSmoothShapeFlatKeysFromRoveSchedule(
          rove, property, config, 2, /*use_ease=*/true);
  Require(keys.size() == 1,
          "single-key rove must yield single-key output");
  Require(keys[0].interp_in == bbsolver::InterpType::Linear,
          "single-key first edge interp_in must be Linear");
  Require(keys[0].interp_out == bbsolver::InterpType::Linear,
          "single-key last edge interp_out must be Linear");
}

void TestEmitMultiKeyWithoutEaseUsesLinearEverywhere() {
  // With `use_ease=false`, every interior interp must be Linear
  // (matching the outer-edge Linear pins). `temporal_continuous` must
  // be false, `temporal_auto_bezier` must be false.
  const bbsolver::PropertySamples property = MakeProperty(2);
  const bbsolver::SolverConfig config = EaseEnabledConfig();
  const bbsolver::ShapeMotionRoveSchedule rove = MakeRove(
      {0.0, 0.5, 1.0},
      {{0.0, 0.0}, {1.0, 1.0}, {2.0, 0.0}});
  const std::vector<bbsolver::Key> keys =
      bbsolver::EmitMotionSmoothShapeFlatKeysFromRoveSchedule(
          rove, property, config, 2, /*use_ease=*/false);
  Require(keys.size() == 3, "3-key rove must yield 3-key output");
  for (std::size_t i = 0; i < keys.size(); ++i) {
    Require(keys[i].interp_in == bbsolver::InterpType::Linear,
            "use_ease=false: every interp_in must be Linear");
    Require(keys[i].interp_out == bbsolver::InterpType::Linear,
            "use_ease=false: every interp_out must be Linear");
    Require(!keys[i].temporal_continuous,
            "use_ease=false: temporal_continuous must be false");
    Require(!keys[i].temporal_auto_bezier,
            "use_ease=false: temporal_auto_bezier must be false");
  }
}

void TestEmitMultiKeyWithEaseUsesBezierInterior() {
  // With `use_ease=true`, interior interps must be Bezier; outer
  // edges must remain Linear. `temporal_continuous` must be true on
  // every key.
  const bbsolver::PropertySamples property = MakeProperty(2);
  const bbsolver::SolverConfig config = EaseEnabledConfig();
  const bbsolver::ShapeMotionRoveSchedule rove = MakeRove(
      {0.0, 0.5, 1.0},
      {{0.0, 0.0}, {1.0, 1.0}, {2.0, 0.0}});
  const std::vector<bbsolver::Key> keys =
      bbsolver::EmitMotionSmoothShapeFlatKeysFromRoveSchedule(
          rove, property, config, 2, /*use_ease=*/true);
  Require(keys.size() == 3, "3-key rove must yield 3-key output");
  // Outer edges pinned to Linear regardless of use_ease.
  Require(keys.front().interp_in == bbsolver::InterpType::Linear,
          "first key interp_in must be Linear (outer edge)");
  Require(keys.back().interp_out == bbsolver::InterpType::Linear,
          "last key interp_out must be Linear (outer edge)");
  // Interior edges are Bezier under use_ease=true.
  Require(keys.front().interp_out == bbsolver::InterpType::Bezier,
          "first key interp_out (interior side) must be Bezier under use_ease=true");
  Require(keys[1].interp_in == bbsolver::InterpType::Bezier,
          "interior key interp_in must be Bezier under use_ease=true");
  Require(keys[1].interp_out == bbsolver::InterpType::Bezier,
          "interior key interp_out must be Bezier under use_ease=true");
  Require(keys.back().interp_in == bbsolver::InterpType::Bezier,
          "last key interp_in (interior side) must be Bezier under use_ease=true");
  for (const bbsolver::Key& key: keys) {
    Require(key.temporal_continuous,
            "use_ease=true: temporal_continuous must be true on every key");
  }
}

void TestEmitTimesAndValuesRoundTripFromRoveSchedule() {
  // The helper must not transform times or values — those flow
  // directly into Key.t_sec and Key.v unchanged.
  const bbsolver::PropertySamples property = MakeProperty(2);
  const bbsolver::SolverConfig config;
  const std::vector<double> source_times = {0.1, 0.5, 0.9};
  const std::vector<std::vector<double>> source_values = {
      {3.14, 2.71}, {1.41, 1.73}, {0.57, 0.69}};
  const bbsolver::ShapeMotionRoveSchedule rove =
      MakeRove(source_times, source_values);
  const std::vector<bbsolver::Key> keys =
      bbsolver::EmitMotionSmoothShapeFlatKeysFromRoveSchedule(
          rove, property, config, 2, /*use_ease=*/false);
  Require(keys.size() == 3, "round-trip output must match input size");
  for (std::size_t i = 0; i < keys.size(); ++i) {
    Require(AlmostEqual(keys[i].t_sec, source_times[i]),
            "Key.t_sec must round-trip from rove_schedule.times[i]");
    Require(keys[i].v == source_values[i],
            "Key.v must round-trip from rove_schedule.values[i]");
  }
}

void TestEmitWithEaseInvokesApplyMotionSmoothBezierEase() {
  // When `use_ease=true` the helper must call
  // ApplyMotionSmoothBezierEase, which mutates the temporal_ease_out /
  // temporal_ease_in speed fields away from the DefaultEasesForProperty
  // baseline. Compare the speed of the first key's ease_out under
  // use_ease=true vs use_ease=false to confirm the ease curve ran.
  const bbsolver::PropertySamples property = MakeProperty(2);
  const bbsolver::SolverConfig config = EaseEnabledConfig();
  // Use non-zero positions so avg_speed = distance/dt is non-zero,
  // which makes the ease speed non-zero too.
  const bbsolver::ShapeMotionRoveSchedule rove = MakeRove(
      {0.0, 1.0},
      {{0.0, 0.0}, {3.0, 4.0}});  // distance = 5, dt = 1, avg_speed = 5

  const std::vector<bbsolver::Key> keys_no_ease =
      bbsolver::EmitMotionSmoothShapeFlatKeysFromRoveSchedule(
          rove, property, config, 2, /*use_ease=*/false);
  const std::vector<bbsolver::Key> keys_with_ease =
      bbsolver::EmitMotionSmoothShapeFlatKeysFromRoveSchedule(
          rove, property, config, 2, /*use_ease=*/true);
  Require(!keys_no_ease.front().temporal_ease_out.empty(),
          "ease_out vector must be sized under either use_ease");
  Require(!keys_with_ease.front().temporal_ease_out.empty(),
          "ease_out vector must be sized under either use_ease");
  // Under use_ease=false the speed/influence stay at the defaults
  // from DefaultEasesForProperty (typically 0.0 / 33.333... or
  // similar). Under use_ease=true ApplyMotionSmoothBezierEase
  // recomputes them. The discriminator is that the two values
  // differ; we don't bind the absolute speed (that depends on the
  // bezier formula) — only that ease ran.
  const double speed_no_ease =
      keys_no_ease.front().temporal_ease_out.front().speed;
  const double speed_with_ease =
      keys_with_ease.front().temporal_ease_out.front().speed;
  Require(!AlmostEqual(speed_no_ease, speed_with_ease, 1e-9),
          "ApplyMotionSmoothBezierEase must mutate first-key ease_out.speed "
          "under use_ease=true compared to use_ease=false");
}

}  // namespace

int main() {
  TestEmitFromEmptyScheduleReturnsEmptyKeys();
  TestEmitSingleKeyEdgesAreBothLinear();
  TestEmitMultiKeyWithoutEaseUsesLinearEverywhere();
  TestEmitMultiKeyWithEaseUsesBezierInterior();
  TestEmitTimesAndValuesRoundTripFromRoveSchedule();
  TestEmitWithEaseInvokesApplyMotionSmoothBezierEase();
  std::cout << "[PASS] test_motion_smooth_shape_flat_key_emission\n";
  return 0;
}
