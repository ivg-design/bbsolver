#pragma once

#include <string>

#include "bbsolver/domain.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_loop_adaptive.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_loop_schedule.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_quality.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_rove_schedule.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_source_key_schedule.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_tangent_lock.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_trajectory_smooth.hpp"

namespace bbsolver {

// the notes string composed by the shape-flat Motion
// Smooth orchestrator carries every diagnostic counter downstream
// consumers (panel, diagnostics analyzers, JSON parsers) parse. The
// composition was ~110 LOC of pure data → string concatenation inside
// MotionSmoothShapeFlatTrajectoryKeys; lifting it here makes the
// orchestrator a thinner control-flow function and makes the notes
// format testable as its own surface.
//
// Inputs are passed by const-reference to the existing result bundles
// produced by the sub-modules plus the scalar counters that
// the orchestrator computes locally. The result struct is the literal
// notes string with no normalization — semicolon-delimited tokens in
// the exact order the original code emitted them.
struct MotionSmoothShapeFlatNotesInputs {
  const ShapeMotionSourceKeySchedule& source_key_schedule;
  const ShapeMotionTrajectorySmoothResult& smooth_result;
  const ShapeMotionRoveSchedule& rove_schedule;
  const ShapeTangentLockStats& tangent_lock;
  const SourcePoseIntervalTimeSchedule& source_pose_interval_schedule;
  const AdaptiveClosedLoopShapeSamples& adaptive_loop;
  const ShapeMotionQualityMetrics& motion_quality_before;
  const ShapeMotionQualityMetrics& motion_quality_after;
  const SolverConfig& config;
  const PropertySamples& property_samples;
  int vertex_count;
  int output_key_count;
  int source_pose_constraint_key_count;
  double strength;
  double trajectory_turn_before_deg;
  double trajectory_turn_after_deg;
  double loop_close_distance;
  int loop_subdivisions;
  bool closed_loop;
  bool adaptive_closed_loop_resample;
  bool use_ease;
};

std::string BuildMotionSmoothShapeFlatNotes(
    const MotionSmoothShapeFlatNotesInputs& inputs);

}  // namespace bbsolver
