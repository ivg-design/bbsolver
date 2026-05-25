// MS26 focused test: the MS22-extracted shape-flat notes helper.
//
// The orchestrator's notes string is the canonical surface that
// downstream consumers (panel, diagnostics analyzers, JSON parsers)
// parse for Motion Smooth shape-flat output. The MS22 extraction
// (motion_smooth_shape_flat_notes.{hpp,cpp}) made it testable as its
// own function. This file exercises three contracts the helper must
// honour:
//
//   1. The opening token `solve_mode_motion_smooth` always appears,
//      followed by the three always-on flags (rove_time,
//      trajectory_filter, stable_topology, source_key_times).
//   2. The closed-loop tokens (`closed_loop_resample=true`,
//      `loop_subdivisions=`, etc.) appear iff `closed_loop == true`.
//   3. `source_error_not_constrained=true` toggles on with
//      `motion_smooth_source_fidelity == false`, and false otherwise.

#include "bbsolver/motion_smooth/motion_smooth_shape_flat_notes.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "bbsolver/domain.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_loop_adaptive.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_loop_schedule.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_quality.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_rove_schedule.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_source_key_schedule.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_tangent_lock.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_trajectory_smooth.hpp"

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    std::abort();
  }
}

bool Contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

// Construct the default input bundle. Individual tests adjust a few
// fields; the bulk of the struct is shared boilerplate.
struct Fixture {
  bbsolver::ShapeMotionSourceKeySchedule source_key_schedule;
  bbsolver::ShapeMotionTrajectorySmoothResult smooth_result;
  bbsolver::ShapeMotionRoveSchedule rove_schedule;
  bbsolver::ShapeTangentLockStats tangent_lock;
  bbsolver::SourcePoseIntervalTimeSchedule source_pose_interval_schedule;
  bbsolver::AdaptiveClosedLoopShapeSamples adaptive_loop;
  bbsolver::ShapeMotionQualityMetrics motion_quality_before;
  bbsolver::ShapeMotionQualityMetrics motion_quality_after;
  bbsolver::SolverConfig config;
  bbsolver::PropertySamples property_samples;

  bbsolver::MotionSmoothShapeFlatNotesInputs Build(
      int vertex_count,
      int output_key_count,
      int source_pose_constraint_key_count,
      double strength,
      double turn_before,
      double turn_after,
      double loop_close_distance,
      int loop_subdivisions,
      bool closed_loop,
      bool adaptive_loop_resample,
      bool use_ease) {
    return bbsolver::MotionSmoothShapeFlatNotesInputs{
        source_key_schedule,
        smooth_result,
        rove_schedule,
        tangent_lock,
        source_pose_interval_schedule,
        adaptive_loop,
        motion_quality_before,
        motion_quality_after,
        config,
        property_samples,
        vertex_count,
        output_key_count,
        source_pose_constraint_key_count,
        strength,
        turn_before,
        turn_after,
        loop_close_distance,
        loop_subdivisions,
        closed_loop,
        adaptive_loop_resample,
        use_ease,
    };
  }
};

void TestNotesOpensWithModeTokenAndAlwaysOnFlags() {
  Fixture fixture;
  fixture.property_samples.samples.resize(10);
  const bbsolver::MotionSmoothShapeFlatNotesInputs in = fixture.Build(
      /*vertex_count=*/4, /*output_key_count=*/8,
      /*source_pose_constraint_key_count=*/0, /*strength=*/1.0,
      /*turn_before=*/0.0, /*turn_after=*/0.0,
      /*loop_close_distance=*/0.0, /*loop_subdivisions=*/0,
      /*closed_loop=*/false, /*adaptive_loop_resample=*/false,
      /*use_ease=*/false);
  const std::string notes = bbsolver::BuildMotionSmoothShapeFlatNotes(in);
  Require(notes.rfind("solve_mode_motion_smooth", 0) == 0,
          "notes must open with `solve_mode_motion_smooth`");
  Require(Contains(notes, "motion_smooth_shape_rove_time=true"),
          "notes must carry the rove_time on-flag");
  Require(Contains(notes, "motion_smooth_shape_trajectory_filter=true"),
          "notes must carry the trajectory_filter on-flag");
  Require(Contains(notes, "motion_smooth_stable_topology=true"),
          "notes must carry the stable_topology on-flag");
  Require(Contains(notes, "motion_smooth_source_key_times=true"),
          "notes must carry the source_key_times on-flag");
}

void TestClosedLoopTokensAppearWhenClosedLoopTrue() {
  Fixture fixture;
  fixture.property_samples.samples.resize(10);
  fixture.adaptive_loop.refinement_passes = 3;
  fixture.adaptive_loop.splits = 7;
  fixture.adaptive_loop.max_keys = 20;
  fixture.adaptive_loop.budget_hit = true;
  fixture.adaptive_loop.target_turn_deg = 30.0;
  fixture.adaptive_loop.chord_error_tolerance = 0.5;
  const bbsolver::MotionSmoothShapeFlatNotesInputs in = fixture.Build(
      /*vertex_count=*/4, /*output_key_count=*/8,
      /*source_pose_constraint_key_count=*/0, /*strength=*/1.0,
      /*turn_before=*/0.0, /*turn_after=*/0.0,
      /*loop_close_distance=*/0.001, /*loop_subdivisions=*/2,
      /*closed_loop=*/true, /*adaptive_loop_resample=*/true,
      /*use_ease=*/false);
  const std::string notes = bbsolver::BuildMotionSmoothShapeFlatNotes(in);
  Require(Contains(notes, "motion_smooth_closed_loop=true"),
          "closed-loop notes must publish motion_smooth_closed_loop=true");
  Require(Contains(notes, "closed_loop_resample=true"),
          "closed-loop notes must publish closed_loop_resample=true");
  Require(Contains(notes, "adaptive_closed_loop_resample=true"),
          "closed-loop notes must publish adaptive_closed_loop_resample=true");
  Require(Contains(notes, "loop_refinement_passes=3"),
          "closed-loop notes must echo adaptive_loop.refinement_passes");
  Require(Contains(notes, "loop_refinement_budget_hit=true"),
          "closed-loop notes must echo adaptive_loop.budget_hit");
}

void TestClosedLoopTokensAbsentWhenClosedLoopFalse() {
  Fixture fixture;
  fixture.property_samples.samples.resize(10);
  const bbsolver::MotionSmoothShapeFlatNotesInputs in = fixture.Build(
      /*vertex_count=*/4, /*output_key_count=*/8,
      /*source_pose_constraint_key_count=*/0, /*strength=*/1.0,
      /*turn_before=*/0.0, /*turn_after=*/0.0,
      /*loop_close_distance=*/0.5, /*loop_subdivisions=*/0,
      /*closed_loop=*/false, /*adaptive_loop_resample=*/false,
      /*use_ease=*/false);
  const std::string notes = bbsolver::BuildMotionSmoothShapeFlatNotes(in);
  Require(Contains(notes, "motion_smooth_closed_loop=false"),
          "open-loop notes must publish motion_smooth_closed_loop=false");
  Require(!Contains(notes, "closed_loop_resample=true"),
          "open-loop notes must NOT publish closed_loop_resample=true");
  Require(!Contains(notes, "loop_refinement_passes="),
          "open-loop notes must omit loop_refinement_passes");
  Require(!Contains(notes, "loop_target_turn_deg="),
          "open-loop notes must omit loop_target_turn_deg");
}

void TestSourceFidelityFlipsConstraintTokens() {
  Fixture fidelity_off;
  fidelity_off.property_samples.samples.resize(10);
  fidelity_off.config.motion_smooth_source_fidelity = false;
  const std::string notes_off = bbsolver::BuildMotionSmoothShapeFlatNotes(
      fidelity_off.Build(4, 8, 0, 1.0, 0.0, 0.0, 0.5, 0, false, false, false));
  Require(Contains(notes_off, "motion_smooth_source_fidelity=false"),
          "fidelity-off notes must publish motion_smooth_source_fidelity=false");
  Require(Contains(notes_off, "source_error_not_constrained=true"),
          "fidelity-off notes must publish source_error_not_constrained=true");
  Require(Contains(notes_off, "key_schedule=source_keys_roved"),
          "fidelity-off notes must publish key_schedule=source_keys_roved");

  Fixture fidelity_on;
  fidelity_on.property_samples.samples.resize(10);
  fidelity_on.config.motion_smooth_source_fidelity = true;
  fidelity_on.source_key_schedule.simplified_count = 5;
  const std::string notes_on = bbsolver::BuildMotionSmoothShapeFlatNotes(
      fidelity_on.Build(4, 8, 0, 1.0, 0.0, 0.0, 0.5, 0, false, false, false));
  Require(Contains(notes_on, "motion_smooth_source_fidelity=true"),
          "fidelity-on notes must publish motion_smooth_source_fidelity=true");
  Require(Contains(notes_on, "source_error_not_constrained=false"),
          "fidelity-on notes must publish source_error_not_constrained=false");
  Require(Contains(notes_on, "key_schedule=source_key_times_spline"),
          "fidelity-on notes must publish key_schedule=source_key_times_spline");
  Require(Contains(notes_on, "source_fidelity_samples=5"),
          "fidelity-on notes must echo simplified_count via source_fidelity_samples");
}

}  // namespace

int main() {
  TestNotesOpensWithModeTokenAndAlwaysOnFlags();
  TestClosedLoopTokensAppearWhenClosedLoopTrue();
  TestClosedLoopTokensAbsentWhenClosedLoopFalse();
  TestSourceFidelityFlipsConstraintTokens();
  std::cout << "[PASS] test_motion_smooth_shape_flat_notes\n";
  return 0;
}
