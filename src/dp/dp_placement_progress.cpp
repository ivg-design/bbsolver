#include "bbsolver/dp/dp_placement_progress.hpp"
#include "bbsolver/dp/dp_placer.hpp"

#include <utility>
#include <string>

namespace bbsolver {

void AddSegmentFitAttribution(DPPlacementInstrumentation& instrumentation,
                              const SegmentFitResult& result) {
  instrumentation.fit_segment_hold_attempts +=
      result.fit_segment_hold_attempts;
  instrumentation.fit_segment_linear_attempts +=
      result.fit_segment_linear_attempts;
  instrumentation.fit_segment_hold_units_evaluated +=
      result.fit_segment_hold_units_evaluated;
  instrumentation.fit_segment_linear_units_evaluated +=
      result.fit_segment_linear_units_evaluated;
  instrumentation.fit_segment_hold_fail_fast_exits +=
      result.fit_segment_hold_fail_fast_exits;
  instrumentation.fit_segment_linear_fail_fast_exits +=
      result.fit_segment_linear_fail_fast_exits;
  instrumentation.fit_shape_temporal_attempts +=
      result.fit_shape_temporal_attempts;
  instrumentation.fit_shape_temporal_gate_rejections +=
      result.fit_shape_temporal_gate_rejections;
  instrumentation.fit_shape_temporal_outline_evaluations +=
      result.fit_shape_temporal_outline_evaluations;
  instrumentation.fit_segment_hold_wall_ms +=
      result.fit_segment_hold_wall_ms;
  instrumentation.fit_segment_linear_wall_ms +=
      result.fit_segment_linear_wall_ms;
  instrumentation.fit_segment_hold_shape_outline_wall_ms +=
      result.fit_segment_hold_shape_outline_wall_ms;
  instrumentation.fit_segment_linear_shape_outline_wall_ms +=
      result.fit_segment_linear_shape_outline_wall_ms;
  instrumentation.fit_shape_temporal_ceres_wall_ms +=
      result.fit_shape_temporal_ceres_wall_ms;
  instrumentation.fit_shape_temporal_outline_wall_ms +=
      result.fit_shape_temporal_outline_wall_ms;
  instrumentation.fit_shape_temporal_total_wall_ms +=
      result.fit_shape_temporal_total_wall_ms;
  instrumentation.fit_replacement_oracle_calls +=
      result.fit_replacement_oracle_calls;
  instrumentation.fit_replacement_oracle_evaluations +=
      result.fit_replacement_oracle_evaluations;
  instrumentation.fit_replacement_relaxed_attempts +=
      result.fit_replacement_relaxed_attempts;
  instrumentation.fit_replacement_relaxed_validations +=
      result.fit_replacement_relaxed_validations;
  instrumentation.fit_replacement_oracle_wall_ms +=
      result.fit_replacement_oracle_wall_ms;
  instrumentation.fit_replacement_outline_wall_ms +=
      result.fit_replacement_outline_wall_ms;
  instrumentation.fit_replacement_relaxed_wall_ms +=
      result.fit_replacement_relaxed_wall_ms;
}

void EmitPlacementProgress(
    const PlacementProgressFn& progress_fn,
    std::string stage,
    int step_index,
    int step_total,
    int sample_index,
    int samples,
    int segments_tried,
    int segments_feasible,
    const DPPlacementInstrumentation* instrumentation) {
  if (!progress_fn) {
    return;
  }
  PlacementProgress progress;
  progress.stage = std::move(stage);
  progress.step_index = step_index;
  progress.step_total = step_total;
  progress.sample_index = sample_index;
  progress.samples = samples;
  progress.segments_tried = segments_tried;
  progress.segments_feasible = segments_feasible;
  if (instrumentation != nullptr) {
    progress.dp_candidate_slots = instrumentation->candidate_slots;
    progress.dp_unreachable_candidates =
        instrumentation->unreachable_candidates;
    progress.dp_incompatible_candidates =
        instrumentation->incompatible_candidates;
    progress.dp_final_anchor_candidate_slots =
        instrumentation->final_anchor_candidate_slots;
    progress.dp_fit_wall_ms = instrumentation->fit_wall_ms;
    progress.dp_reduction_wall_ms = instrumentation->reduction_wall_ms;
    progress.dp_final_anchor_fit_wall_ms =
        instrumentation->final_anchor_fit_wall_ms;
    progress.dp_final_anchor_reduction_wall_ms =
        instrumentation->final_anchor_reduction_wall_ms;
    progress.fit_segment_hold_attempts =
        instrumentation->fit_segment_hold_attempts;
    progress.fit_segment_linear_attempts =
        instrumentation->fit_segment_linear_attempts;
    progress.fit_segment_hold_units_evaluated =
        instrumentation->fit_segment_hold_units_evaluated;
    progress.fit_segment_linear_units_evaluated =
        instrumentation->fit_segment_linear_units_evaluated;
    progress.fit_segment_hold_fail_fast_exits =
        instrumentation->fit_segment_hold_fail_fast_exits;
    progress.fit_segment_linear_fail_fast_exits =
        instrumentation->fit_segment_linear_fail_fast_exits;
    progress.fit_shape_temporal_attempts =
        instrumentation->fit_shape_temporal_attempts;
    progress.fit_shape_temporal_gate_rejections =
        instrumentation->fit_shape_temporal_gate_rejections;
    progress.fit_shape_temporal_outline_evaluations =
        instrumentation->fit_shape_temporal_outline_evaluations;
    progress.fit_segment_hold_wall_ms =
        instrumentation->fit_segment_hold_wall_ms;
    progress.fit_segment_linear_wall_ms =
        instrumentation->fit_segment_linear_wall_ms;
    progress.fit_segment_hold_shape_outline_wall_ms =
        instrumentation->fit_segment_hold_shape_outline_wall_ms;
    progress.fit_segment_linear_shape_outline_wall_ms =
        instrumentation->fit_segment_linear_shape_outline_wall_ms;
    progress.fit_shape_temporal_ceres_wall_ms =
        instrumentation->fit_shape_temporal_ceres_wall_ms;
    progress.fit_shape_temporal_outline_wall_ms =
        instrumentation->fit_shape_temporal_outline_wall_ms;
    progress.fit_shape_temporal_total_wall_ms =
        instrumentation->fit_shape_temporal_total_wall_ms;
    progress.fit_replacement_oracle_calls =
        instrumentation->fit_replacement_oracle_calls;
    progress.fit_replacement_oracle_evaluations =
        instrumentation->fit_replacement_oracle_evaluations;
    progress.fit_replacement_relaxed_attempts =
        instrumentation->fit_replacement_relaxed_attempts;
    progress.fit_replacement_relaxed_validations =
        instrumentation->fit_replacement_relaxed_validations;
    progress.fit_replacement_oracle_wall_ms =
        instrumentation->fit_replacement_oracle_wall_ms;
    progress.fit_replacement_outline_wall_ms =
        instrumentation->fit_replacement_outline_wall_ms;
    progress.fit_replacement_relaxed_wall_ms =
        instrumentation->fit_replacement_relaxed_wall_ms;
  }
  progress_fn(progress);
}

}  // namespace bbsolver
