// bbsolver dynamic-programming key placer.
// Implementation lives in dp_placer.cpp. SegmentFitFn supplies the
// fitting/error math used for each candidate span.

#pragma once

#include "bbsolver/domain.hpp"

#include <functional>
#include <string>
#include <vector>

namespace bbsolver {

// ---- SegmentFitResult --------------------------------------------------------
// One (i -> j) segment's solution. The segment endpoints sit on sample indices i and j
// of the parent PropertySamples. Spatial/temporal handles are *relative* to v_i and v_j
// per AE convention.
//
// Convention:
//   ease_out_at_i = temporal ease used as OUT-ease on the key placed at sample i
//   ease_in_at_j  = temporal ease used as IN-ease  on the key placed at sample j
// Same for spatial tangents.
struct SegmentFitResult {
    bool        feasible          = false;
    InterpType  interp            = InterpType::Bezier;

    std::vector<TemporalEase> ease_out_at_i;  // size = dims_temporal (1 if non-separated)
    std::vector<TemporalEase> ease_in_at_j;
    std::vector<double>       spatial_out_at_i; // size = dims (empty if non-spatial)
    std::vector<double>       spatial_in_at_j;
    // Optional key values for fits whose best segment endpoint values differ
    // from the sampled values at i/j. Empty means "use ps.samples[i/j].v".
    std::vector<double>       key_value_at_i;
    std::vector<double>       key_value_at_j;

    double      max_err           = 0.0;  // L∞ in property units
    double      max_err_screen_px = 0.0;
    double      rms_err           = 0.0;
    int         iters             = 0;
    int         fit_segment_hold_attempts = 0;
    int         fit_segment_linear_attempts = 0;
    int         fit_segment_hold_units_evaluated = 0;
    int         fit_segment_linear_units_evaluated = 0;
    int         fit_segment_hold_fail_fast_exits = 0;
    int         fit_segment_linear_fail_fast_exits = 0;
    int         fit_shape_temporal_attempts = 0;
    int         fit_shape_temporal_gate_rejections = 0;
    int         fit_shape_temporal_outline_evaluations = 0;
    double      fit_segment_hold_wall_ms = 0.0;
    double      fit_segment_linear_wall_ms = 0.0;
    double      fit_segment_hold_shape_outline_wall_ms = 0.0;
    double      fit_segment_linear_shape_outline_wall_ms = 0.0;
    double      fit_shape_temporal_ceres_wall_ms = 0.0;
    double      fit_shape_temporal_outline_wall_ms = 0.0;
    double      fit_shape_temporal_total_wall_ms = 0.0;
    int         fit_replacement_oracle_calls = 0;
    int         fit_replacement_oracle_evaluations = 0;
    int         fit_replacement_relaxed_attempts = 0;
    int         fit_replacement_relaxed_validations = 0;
    double      fit_replacement_oracle_wall_ms = 0.0;
    double      fit_replacement_outline_wall_ms = 0.0;
    double      fit_replacement_relaxed_wall_ms = 0.0;
    std::string reason;                    // "bezier_ok" | "linear" | "hold" | "infeasible_*"
};

// fit(i, j, ps, cfg, comp) — round-2 implementation in segment_fitter.cpp.
// MUST be thread-safe (DP runs fits in parallel via TBB).
using SegmentFitFn = std::function<SegmentFitResult(
    int i, int j,
    const PropertySamples& ps,
    const SolverConfig& cfg,
    const CompInfo& comp)>;
using CancelFn = std::function<bool()>;

struct PlacementProgress {
    std::string stage;
    int step_index = 0;
    int step_total = 0;
    int sample_index = -1;
    int samples = 0;
    int segments_tried = 0;
    int segments_feasible = 0;
    int dp_candidate_slots = 0;
    int dp_unreachable_candidates = 0;
    int dp_incompatible_candidates = 0;
    int dp_final_anchor_candidate_slots = 0;
    double dp_fit_wall_ms = 0.0;
    double dp_reduction_wall_ms = 0.0;
    double dp_final_anchor_fit_wall_ms = 0.0;
    double dp_final_anchor_reduction_wall_ms = 0.0;
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

using PlacementProgressFn = std::function<void(const PlacementProgress&)>;

// ---- Placement result --------------------------------------------------------
struct DPPlacement {
    std::vector<int> sample_indices;            // anchor sample indices, sorted; size = K
    std::vector<SegmentFitResult> segments;     // size = K-1; segments[s] joins indices[s]->indices[s+1]
    bool converged = false;
    int total_segments_tried   = 0;
    int total_segments_feasible = 0;
    double max_err             = 0.0;
    double max_err_screen_px   = 0.0;
    std::string notes;
};

// ---- Public API --------------------------------------------------------------

// Minimum-K placement.
//
//   dp[j] = min_{i where fit(i, j) feasible} (dp[i] + 1)
//   dp[0] = 0
//
// max_gap_samples caps the search window for fit(i, j) to keep complexity bounded.
//   0 (default) => auto: round(2.0 * comp.fps) samples (i.e. 2-second cap).
// Always guarantees first sample (idx 0) and last sample (idx N-1) are anchors.
DPPlacement RunDPPlacement(
    const PropertySamples& ps,
    const SolverConfig& cfg,
    const CompInfo& comp,
    SegmentFitFn fit_fn,
    int max_gap_samples = 0,
    CancelFn cancel_fn = {},
    PlacementProgressFn progress_fn = {});

// Convenience: full single-property pipeline -> populated PropertyKeys.
// Handles assembling Key list from the segment fits (joining ease_in/out at shared anchors,
// stitching spatial tangents likewise), then writes SegmentReport rows.
PropertyKeys SolveProperty(
    const PropertySamples& ps,
    const SolverConfig& cfg,
    const CompInfo& comp,
    SegmentFitFn fit_fn,
    CancelFn cancel_fn = {},
    int max_gap_samples = 0,
    PlacementProgressFn progress_fn = {});

// Parallel fan-out across properties (TBB). fit_factory produces a thread-safe fit fn
// per property (allowing per-property caching / dimension-aware closures).
std::vector<PropertyKeys> SolveAll(
    const std::vector<PropertySamples>& properties,
    const SolverConfig& cfg,
    const CompInfo& comp,
    const std::function<SegmentFitFn(const PropertySamples&)>& fit_factory);

} // namespace bbsolver
