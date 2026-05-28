#pragma once

#include <vector>

#include "bbsolver/domain.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_loop_adaptive.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_loop_schedule.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_trajectory_smooth.hpp"

namespace bbsolver {

// the closed-loop branch + source-pose-constraint
// bookkeeping in the shape-flat Motion Smooth orchestrator. The
// branch is responsible for choosing whether to adaptively resample a
// detected closed loop, choosing the key-time schedule
// (interval-travel-roving vs evenly-spaced) when source-fidelity is
// enabled, computing the source-pose-constraint mask, and updating
// the post-resample turn-after diagnostic.
//
// When `closed_loop` is false, the helper returns the trivial schedule
// (schedule_times = key_times; schedule_values chosen based on
// motion_smooth_source_fidelity) plus an all-true
// source_pose_constraint_indices mask when source-fidelity is enabled
// (so the tangent-lock step skips every key under fidelity mode).
struct ClosedLoopAdaptiveResampleResult {
  AdaptiveClosedLoopShapeSamples adaptive_loop;
  SourcePoseIntervalTimeSchedule source_pose_interval_schedule;
  std::vector<double> schedule_times;
  std::vector<std::vector<double>> schedule_values;
  std::vector<bool> source_pose_constraint_indices;
  int loop_subdivisions = 0;
  bool adaptive_loop_resample = false;
  // When `closed_loop` is true, this is the post-resample turn-angle
  // overriding `smooth_result.max_turn_after_deg`. Caller uses
  // `trajectory_turn_after_overridden` to decide whether to take the
  // override or keep the smoothing-stage estimate.
  double trajectory_turn_after_override = 0.0;
  bool trajectory_turn_after_overridden = false;
};

ClosedLoopAdaptiveResampleResult BuildShapeFlatClosedLoopAdaptiveResample(
    const ShapeMotionTrajectorySmoothResult& smooth_result,
    const std::vector<double>& key_times,
    const SolverConfig& config,
    int vertex_count,
    int dims,
    double strength,
    bool closed_loop);

}  // namespace bbsolver
