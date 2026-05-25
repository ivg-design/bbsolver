#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"

namespace bbsolver {

struct ReplacementTemporalSolverOptions {
  ShapeMorphProgressBandOptions band_options;
  int max_gap_samples = 6;
  bool allow_relaxed_endpoint_fit = true;
  bool allow_multimode_anchor_union = true;
  int multimode_max_regions = 4;
  int multimode_max_gap_samples = 24;
  double multimode_region_tolerance = 0.0;
  int multimode_max_region_segment_checks = 20000;
  int multimode_max_validation_samples = 180;
  int multimode_max_validation_work_units = 80000;
  double multimode_max_candidate_key_ratio = 0.60;
  // When the validated anchor-union prototype is already materially smaller
  // than raw per-frame keys, return it before the more expensive single-progress
  // DP search. 0 disables the fast path.
  double multimode_fast_accept_key_ratio = 0.60;
  // Experimental dominant-topology reducer. It greedily selects the farthest
  // exact-linear Shape Path span that validates against the source outline, and
  // only replaces the normal DP/multimode result when it uses fewer keys. This
  // remains opt-in while the production topology/regime path is unresolved.
  bool allow_forward_longest_span = false;
  int forward_longest_span_min_vertex_count = 52;
  int forward_longest_span_min_samples = 24;
  int forward_longest_span_max_gap_samples = 256;
  int forward_longest_span_max_segment_checks = 20000;
  CancelFn cancel_fn = {};
  PlacementProgressFn placement_progress_fn = {};
};

// Solve a fixed-topology reduced shape_flat stream against the original source
// shape_flat samples. Segment feasibility is decided by
// EvaluateShapeFlatMorphProgressBands(original, i, j, reduced[i], reduced[j]).
//
// Accepts Linear segments when the morph chord has a linear progress path. When
// config.allow_shape_temporal_bezier is true, it also tries AE-compatible Shape
// Path temporal Bezier timing: TemporalEase.speed stays 0 and a bounded
// outgoing/incoming influence-pair search chooses the segment timing. If the
// sampled endpoint chord fails, the solver may fit ordinary Shape Path key
// values for the segment endpoints and validate that relaxed chord against the
// original source outline before accepting it.
PropertyKeys SolveReplacementShapeFlatTemporal(
    const PropertySamples& original,
    const PropertySamples& reduced,
    const SolverConfig& config,
    const CompInfo& comp,
    const ReplacementTemporalSolverOptions& options = {});

}  // namespace bbsolver
