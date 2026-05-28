#include "bbsolver/motion_smooth/motion_smooth_shape_flat.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <utility>
#include <vector>
#include <cstddef>

#include "bbsolver/motion_smooth/motion_smooth_sample_points.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_flat_closed_loop.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_flat_key_emission.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_flat_notes.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_flat_topology_gate.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_loop_adaptive.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_loop_schedule.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_quality.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_source_key_schedule.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_rove_schedule.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_tangent_lock.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_trajectory_smooth.hpp"
#include "bbsolver/samples/raw_frame_keys.hpp"

namespace bbsolver {

PropertyKeys MotionSmoothShapeFlatTrajectoryKeys(
    const PropertySamples& property_samples,
    const SolverConfig& config) {
  MotionSmoothShapeFlatTopologyGateResult gate =
      ValidateMotionSmoothShapeFlatTopology(property_samples);
  if (!gate.ok) {
    return std::move(gate.fallback_keys);
  }
  const int vertex_count = gate.vertex_count;
  const int dims = gate.dims;
  std::vector<double> key_times = std::move(gate.key_times);

  PropertyKeys keys;
  keys.property_id = property_samples.property.id;
  keys.converged = true;
  keys.max_err = 0.0;
  keys.max_err_screen_px = 0.0;

  const double requested_strength =
      config.motion_smooth_tolerance > 0.0
          ? config.motion_smooth_tolerance
: (config.tolerance_screen_px > 0.0
                ? config.tolerance_screen_px
: config.tolerance);
  const double strength = std::max(1.0, requested_strength);
  const std::vector<std::vector<double>> raw =
      MotionSmoothRawPoints(property_samples, dims);
  const ShapeMotionSourceKeySchedule source_key_schedule =
      BuildShapeMotionSourceKeySchedule(
          property_samples, key_times, raw, dims, strength);
  key_times = source_key_schedule.times;
  std::vector<double> source_fidelity_times;
  if (config.motion_smooth_source_fidelity) {
    source_fidelity_times.reserve(property_samples.samples.size());
    for (const Sample& sample: property_samples.samples) {
      source_fidelity_times.push_back(sample.t_sec);
    }
  }
  const ShapeMotionTrajectorySmoothResult smooth_result =
      BuildShapeMotionTrajectorySmoothValues(
          property_samples,
          key_times,
          raw,
          vertex_count,
          dims,
          strength,
          config.motion_smooth_source_fidelity
              ? &source_fidelity_times
: nullptr,
          config.motion_smooth_source_fidelity
              ? &raw
: nullptr);
  const double loop_close_distance =
      (!smooth_result.original_values.empty())
          ? ShapeFlatControlDistance(smooth_result.original_values.front(),
                                     smooth_result.original_values.back(),
                                     vertex_count)
: 0.0;
  const bool closed_loop =
      smooth_result.original_values.size() >= 4 &&
      loop_close_distance <= std::max(1e-6, strength * 0.01);
  const ShapeMotionQualityMetrics motion_quality_before =
      ShapeMotionQuality(smooth_result.original_values,
                         vertex_count,
                         &key_times);
  double trajectory_turn_before =
      motion_quality_before.valid ? motion_quality_before.max_turn_deg
: smooth_result.max_turn_before_deg;
  ClosedLoopAdaptiveResampleResult resample =
      BuildShapeFlatClosedLoopAdaptiveResample(
          smooth_result, key_times, config, vertex_count, dims, strength,
          closed_loop);
  AdaptiveClosedLoopShapeSamples& adaptive_loop = resample.adaptive_loop;
  SourcePoseIntervalTimeSchedule& source_pose_interval_schedule =
      resample.source_pose_interval_schedule;
  std::vector<double>& schedule_times = resample.schedule_times;
  std::vector<std::vector<double>>& schedule_values = resample.schedule_values;
  std::vector<bool>& source_pose_constraint_indices =
      resample.source_pose_constraint_indices;
  const int loop_subdivisions = resample.loop_subdivisions;
  const bool adaptive_loop_resample = resample.adaptive_loop_resample;
  double trajectory_turn_after = resample.trajectory_turn_after_overridden
      ? resample.trajectory_turn_after_override
: smooth_result.max_turn_after_deg;
  int source_pose_constraint_key_count = 0;
  for (bool constrained: source_pose_constraint_indices) {
    if (constrained) {
      ++source_pose_constraint_key_count;
    }
  }
  const ShapeTangentLockStats tangent_lock =
      config.motion_smooth_source_fidelity
          ? LockShapeFlatRotationalTangentsExcept(
                &schedule_values, source_pose_constraint_indices)
: LockShapeFlatRotationalTangents(&schedule_values);
  const bool use_ease =
      config.motion_smooth_use_ease && config.allow_bezier;
  const ShapeMotionRoveSchedule rove_schedule =
      BuildShapeMotionRoveScheduleFromValues(
          schedule_times,
          schedule_values,
          vertex_count,
          !config.motion_smooth_source_fidelity);
  if (rove_schedule.times.size() < 2 ||
      rove_schedule.values.size() != rove_schedule.times.size()) {
    return ShapeFlatFrameKeyFallback(
        property_samples,
        "solve_mode_motion_smooth_skipped: invalid_rove_schedule");
  }
  keys.keys = EmitMotionSmoothShapeFlatKeysFromRoveSchedule(
      rove_schedule, property_samples, config, dims, use_ease);
  const ShapeMotionQualityMetrics motion_quality_after =
      ShapeMotionQuality(rove_schedule.values,
                         vertex_count,
                         &rove_schedule.times);
  if (motion_quality_after.valid) {
    trajectory_turn_after = motion_quality_after.max_turn_deg;
  }

  keys.max_err = smooth_result.max_shape_displacement;
  keys.max_err_screen_px = smooth_result.max_shape_displacement;
  const MotionSmoothShapeFlatNotesInputs notes_inputs = {
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
      static_cast<int>(keys.keys.size()),
      source_pose_constraint_key_count,
      strength,
      trajectory_turn_before,
      trajectory_turn_after,
      loop_close_distance,
      loop_subdivisions,
      closed_loop,
      adaptive_loop_resample,
      use_ease,
  };
  keys.notes = BuildMotionSmoothShapeFlatNotes(notes_inputs);

  SegmentReport segment;
  segment.start_idx = 0;
  segment.end_idx = static_cast<int>(property_samples.samples.size()) - 1;
  segment.max_err = smooth_result.max_shape_displacement;
  segment.max_err_screen_px = smooth_result.max_shape_displacement;
  segment.rms_err = 0.0;
  segment.iters = static_cast<int>(keys.keys.size());
  segment.reason = "motion_smooth_shape_trajectory_filter";
  keys.segments.push_back(std::move(segment));
  return keys;
}

}  // namespace bbsolver
