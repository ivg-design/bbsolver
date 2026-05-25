#pragma once

#include "bbsolver/dp/dp_placer.hpp"

#include <string>

namespace bbsolver {

struct DPPlacementInstrumentation {
  int candidate_slots = 0;
  int unreachable_candidates = 0;
  int incompatible_candidates = 0;
  int final_anchor_candidate_slots = 0;
  double fit_wall_ms = 0.0;
  double reduction_wall_ms = 0.0;
  double final_anchor_fit_wall_ms = 0.0;
  double final_anchor_reduction_wall_ms = 0.0;
  int fit_segment_hold_attempts = 0;
  int fit_segment_linear_attempts = 0;
  int fit_segment_hold_units_evaluated = 0;
  int fit_segment_linear_units_evaluated = 0;
  int fit_segment_hold_fail_fast_exits = 0;
  int fit_segment_linear_fail_fast_exits = 0;
  int fit_shape_temporal_attempts = 0;
  int fit_shape_temporal_gate_rejections = 0;
  int fit_shape_temporal_outline_evaluations = 0;
  double fit_segment_hold_wall_ms = 0.0;
  double fit_segment_linear_wall_ms = 0.0;
  double fit_segment_hold_shape_outline_wall_ms = 0.0;
  double fit_segment_linear_shape_outline_wall_ms = 0.0;
  double fit_shape_temporal_ceres_wall_ms = 0.0;
  double fit_shape_temporal_outline_wall_ms = 0.0;
  double fit_shape_temporal_total_wall_ms = 0.0;
  int fit_replacement_oracle_calls = 0;
  int fit_replacement_oracle_evaluations = 0;
  int fit_replacement_relaxed_attempts = 0;
  int fit_replacement_relaxed_validations = 0;
  double fit_replacement_oracle_wall_ms = 0.0;
  double fit_replacement_outline_wall_ms = 0.0;
  double fit_replacement_relaxed_wall_ms = 0.0;
};

void AddSegmentFitAttribution(DPPlacementInstrumentation& instrumentation,
                              const SegmentFitResult& result);

void EmitPlacementProgress(
    const PlacementProgressFn& progress_fn,
    std::string stage,
    int step_index,
    int step_total,
    int sample_index,
    int samples,
    int segments_tried,
    int segments_feasible,
    const DPPlacementInstrumentation* instrumentation = nullptr);

}  // namespace bbsolver
