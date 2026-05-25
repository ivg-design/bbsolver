#include "bbsolver/motion_smooth/motion_smooth_shape_flat_notes.hpp"

#include <string>

#include "bbsolver/domain.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_quality.hpp"

namespace bbsolver {

std::string BuildMotionSmoothShapeFlatNotes(
    const MotionSmoothShapeFlatNotesInputs& in) {
  return
      std::string("solve_mode_motion_smooth") +
      "; motion_smooth_shape_rove_time=true" +
      "; motion_smooth_shape_trajectory_filter=true" +
      "; motion_smooth_stable_topology=true" +
      "; motion_smooth_source_key_times=true" +
      "; source_key_count=" +
      std::to_string(in.source_key_schedule.simplified_count) +
      "; source_key_count_raw=" +
      std::to_string(in.source_key_schedule.raw_count) +
      "; source_key_simplified_count=" +
      std::to_string(in.source_key_schedule.simplified_count) +
      "; redundant_source_keys_removed=" +
      std::to_string(in.source_key_schedule.redundant_removed) +
      "; source_key_anchor_simplification=" +
      std::string(in.source_key_schedule.simplification_enabled ? "true"
                                                                : "false") +
      "; source_key_simplify_tolerance=" +
      std::to_string(in.source_key_schedule.simplify_tolerance) +
      "; motion_smooth_source_fidelity=" +
      std::string(in.config.motion_smooth_source_fidelity ? "true" : "false") +
      "; source_fidelity_samples=" +
      std::to_string(in.config.motion_smooth_source_fidelity
                         ? in.source_key_schedule.simplified_count
                         : 0) +
      "; source_fidelity_mode=" +
      std::string(in.config.motion_smooth_source_fidelity
                      ? "source_key_pose_constraints"
                      : "off") +
      "; source_pose_constraints=" +
      std::to_string(in.config.motion_smooth_source_fidelity
                         ? in.source_key_schedule.simplified_count
                         : 0) +
      "; source_pose_constraint_keys=" +
      std::to_string(in.source_pose_constraint_key_count) +
      "; source_pose_interval_rove=" +
      std::string(in.source_pose_interval_schedule.applied ? "true"
                                                           : "false") +
      "; source_pose_interval_rove_max_time_shift_sec=" +
      std::to_string(in.source_pose_interval_schedule.max_time_shift_sec) +
      "; input_samples=" + std::to_string(in.property_samples.samples.size()) +
      "; output_keys=" + std::to_string(in.output_key_count) +
      "; source_vertices=" + std::to_string(in.vertex_count) +
      "; smoothing_strength=" + std::to_string(in.strength) +
      "; smoothing_passes=" +
      std::to_string(in.smooth_result.smoothing_passes) +
      "; smoothing_blend=" +
      std::to_string(in.smooth_result.smoothing_blend) +
      "; max_smoothing_displacement=" +
      std::to_string(in.smooth_result.max_shape_displacement) +
      "; max_control_smoothing_displacement=" +
      std::to_string(in.smooth_result.max_control_displacement) +
      "; smoothing_displacement_limit=" +
      std::to_string(in.smooth_result.displacement_limit) +
      "; trajectory_turn_before_deg=" +
      std::to_string(in.trajectory_turn_before_deg) +
      "; trajectory_turn_after_deg=" +
      std::to_string(in.trajectory_turn_after_deg) +
      "; motion_smooth_closed_loop=" +
      std::string(in.closed_loop ? "true" : "false") +
      "; loop_close_distance=" +
      std::to_string(in.loop_close_distance) +
      (in.closed_loop
           ? std::string("; closed_loop_resample=true") +
                 "; adaptive_closed_loop_resample=" +
                 (in.adaptive_closed_loop_resample ? "true" : "false") +
                 "; loop_subdivisions=" +
                 std::to_string(in.loop_subdivisions) +
                 "; loop_refinement_passes=" +
                 std::to_string(in.adaptive_loop.refinement_passes) +
                 "; loop_refinement_splits=" +
                 std::to_string(in.adaptive_loop.splits) +
                 "; loop_refinement_budget_hit=" +
                 (in.adaptive_loop.budget_hit ? "true" : "false") +
                 "; loop_max_keys=" +
                 std::to_string(in.adaptive_loop.max_keys) +
                 "; loop_target_turn_deg=" +
                 std::to_string(in.adaptive_loop.target_turn_deg) +
                 "; loop_chord_error_tolerance=" +
                 std::to_string(in.adaptive_loop.chord_error_tolerance)
           : "") +
      "; " + ShapeMotionQualityNote(in.motion_quality_before,
                                    "motion_quality_before") +
      "; " + ShapeMotionQualityNote(in.motion_quality_after,
                                    "motion_quality_after") +
      "; shape_tangent_lock=true" +
      "; shape_tangent_pairs_locked=" +
      std::to_string(in.tangent_lock.pairs_locked) +
      "; shape_tangent_lock_skipped_source_pose_constraints=" +
      std::to_string(in.source_pose_constraint_key_count) +
      "; shape_tangent_lock_max_deviation_before_deg=" +
      std::to_string(in.tangent_lock.max_deviation_before_deg) +
      "; key_schedule=" +
      std::string(in.config.motion_smooth_source_fidelity
                      ? "source_key_times_spline"
                      : "source_keys_roved") +
      "; rove_applied=" +
      std::string(in.rove_schedule.rove_applied ? "true" : "false") +
      "; rove_total_travel=" +
      std::to_string(in.rove_schedule.total_travel) +
      "; rove_max_segment_travel=" +
      std::to_string(in.rove_schedule.max_segment_travel) +
      "; rove_max_control_step=" +
      std::to_string(in.rove_schedule.max_control_step) +
      "; rove_max_time_shift_sec=" +
      std::to_string(in.rove_schedule.max_time_shift_sec) +
      "; static_source_keys_removed=" +
      std::to_string(in.rove_schedule.static_keys_removed) +
      "; motion_smooth_ease=" + (in.use_ease ? "on" : "off") +
      "; motion_smooth_bezier=" +
      std::to_string(in.config.motion_smooth_bezier_x1) + "," +
      std::to_string(in.config.motion_smooth_bezier_y1) + "," +
      std::to_string(in.config.motion_smooth_bezier_x2) + "," +
      std::to_string(in.config.motion_smooth_bezier_y2) +
      "; source_error_not_constrained=" +
      std::string(in.config.motion_smooth_source_fidelity ? "false" : "true");
}

}  // namespace bbsolver
