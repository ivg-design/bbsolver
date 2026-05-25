// MS15 focused test: ApplyMotionSmoothBezierEase early-return guards.
//
// The existing test_motion_smooth_solver.cpp::TestBezierEaseApplication
// exercises the happy path: 2 keys, use_ease=true, valid bezier
// control points → keys are mutated. After MS12 promoted the ease
// application to its own TU (motion_smooth_bezier_ease.cpp), the
// three early-return guards (null pointer, < 2 keys,
// !motion_smooth_use_ease) became a behavioural contract that future
// edits could quietly invert. This test locks all three guards.

#include "bbsolver/motion_smooth/motion_smooth_bezier_ease.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "bbsolver/domain.hpp"

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    std::abort();
  }
}

bbsolver::PropertySamples BaseProperty() {
  bbsolver::PropertySamples ps;
  ps.property.dimensions = 2;
  ps.property.is_spatial = true;
  return ps;
}

bbsolver::SolverConfig ValidEaseConfig() {
  bbsolver::SolverConfig config;
  config.motion_smooth_use_ease = true;
  config.allow_bezier = true;
  config.motion_smooth_bezier_x1 = 0.25;
  config.motion_smooth_bezier_y1 = 0.5;
  config.motion_smooth_bezier_x2 = 0.75;
  config.motion_smooth_bezier_y2 = 0.5;
  config.min_influence = 0.01;
  config.max_influence = 100.0;
  return config;
}

void TestNullKeysPointerIsNoOp() {
  const bbsolver::PropertySamples property = BaseProperty();
  const bbsolver::SolverConfig config = ValidEaseConfig();
  // Passing nullptr must not crash and must not write through the
  // pointer. There is nothing to observe other than "did not segfault";
  // the act of returning here is the assertion.
  bbsolver::ApplyMotionSmoothBezierEase(property, config, 2, nullptr);
}

void TestSingleKeyIsNoOp() {
  // With < 2 keys there is no segment to ease, so the function must
  // return without mutating the (single) key's interp/ease state.
  const bbsolver::PropertySamples property = BaseProperty();
  const bbsolver::SolverConfig config = ValidEaseConfig();
  std::vector<bbsolver::Key> keys(1);
  keys[0].t_sec = 0.0;
  keys[0].v = {0.0, 0.0};
  keys[0].interp_in = bbsolver::InterpType::Linear;
  keys[0].interp_out = bbsolver::InterpType::Linear;
  keys[0].temporal_continuous = false;
  bbsolver::ApplyMotionSmoothBezierEase(property, config, 2, &keys);
  Require(keys.size() == 1,
          "single-key bundle must not grow");
  Require(keys[0].interp_in == bbsolver::InterpType::Linear &&
              keys[0].interp_out == bbsolver::InterpType::Linear,
          "single-key bundle must not have its interpolation rewritten");
  Require(!keys[0].temporal_continuous,
          "single-key bundle must not have temporal_continuous flipped");
}

void TestEaseDisabledIsNoOp() {
  // With motion_smooth_use_ease=false the function must return
  // without rewriting the keys, even when 2+ keys are present and
  // bezier control points are valid.
  const bbsolver::PropertySamples property = BaseProperty();
  bbsolver::SolverConfig config = ValidEaseConfig();
  config.motion_smooth_use_ease = false;
  std::vector<bbsolver::Key> keys(2);
  keys[0].t_sec = 0.0;
  keys[0].v = {0.0, 0.0};
  keys[0].interp_in = bbsolver::InterpType::Linear;
  keys[0].interp_out = bbsolver::InterpType::Linear;
  keys[0].temporal_continuous = false;
  keys[1].t_sec = 1.0;
  keys[1].v = {3.0, 4.0};
  keys[1].interp_in = bbsolver::InterpType::Linear;
  keys[1].interp_out = bbsolver::InterpType::Linear;
  keys[1].temporal_continuous = false;
  bbsolver::ApplyMotionSmoothBezierEase(property, config, 2, &keys);
  Require(keys[0].interp_in == bbsolver::InterpType::Linear &&
              keys[0].interp_out == bbsolver::InterpType::Linear,
          "ease-disabled: first key interp must stay Linear");
  Require(keys[1].interp_in == bbsolver::InterpType::Linear &&
              keys[1].interp_out == bbsolver::InterpType::Linear,
          "ease-disabled: last key interp must stay Linear");
  Require(!keys[0].temporal_continuous && !keys[1].temporal_continuous,
          "ease-disabled: temporal_continuous must not be flipped on");
}

void TestInfluenceClampHonorsConfigRange() {
  // The MS12 docs entry promises the influence values are clamped to
  // `[config.min_influence, config.max_influence]`. Drive the raw
  // computed influence well outside both endpoints and verify the
  // clamp kicks in. With x1=0.25 → raw out_influence = 25; setting
  // config.max_influence=10 must clamp out_influence to 10.
  const bbsolver::PropertySamples property = BaseProperty();
  bbsolver::SolverConfig config = ValidEaseConfig();
  config.min_influence = 50.0;   // floor above raw 25
  config.max_influence = 100.0;  // ceiling well above raw
  std::vector<bbsolver::Key> keys(2);
  keys[0].t_sec = 0.0;
  keys[0].v = {0.0, 0.0};
  keys[1].t_sec = 1.0;
  keys[1].v = {3.0, 4.0};
  bbsolver::ApplyMotionSmoothBezierEase(property, config, 2, &keys);
  Require(!keys[0].temporal_ease_out.empty(),
          "outgoing ease must be sized after application");
  Require(keys[0].temporal_ease_out[0].influence >= config.min_influence,
          "outgoing influence must be clamped up to min_influence");

  config.min_influence = 0.01;
  config.max_influence = 5.0;  // ceiling well below raw 25
  bbsolver::ApplyMotionSmoothBezierEase(property, config, 2, &keys);
  Require(!keys[0].temporal_ease_out.empty(),
          "outgoing ease must remain sized after second application");
  Require(keys[0].temporal_ease_out[0].influence <= config.max_influence,
          "outgoing influence must be clamped down to max_influence");
}

}  // namespace

int main() {
  TestNullKeysPointerIsNoOp();
  TestSingleKeyIsNoOp();
  TestEaseDisabledIsNoOp();
  TestInfluenceClampHonorsConfigRange();
  std::cout << "[PASS] test_motion_smooth_bezier_ease_guards\n";
  return 0;
}
