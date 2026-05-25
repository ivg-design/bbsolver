#include "bbsolver/replacement_temporal/replacement_temporal_options.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/replacement_temporal/replacement_temporal_solver.hpp"

#include <algorithm>
#include <cmath>

namespace bbsolver {
namespace {

constexpr int kDefaultMaxGapSamples = 6;
constexpr int kMaxOracleSubdivisions = 6;
constexpr int kDefaultForwardLongestSpanMaxGapSamples = 256;
constexpr int kDefaultForwardLongestSpanMaxSegmentChecks = 20000;

}  // namespace

ReplacementTemporalSolverOptions NormalizeReplacementTemporalOptions(
    ReplacementTemporalSolverOptions options,
    const SolverConfig& config) {
  if (options.band_options.max_window_samples <= 0) {
    options.band_options.max_window_samples = kDefaultMaxGapSamples + 1;
  }
  options.band_options.max_window_samples =
      std::max(2, options.band_options.max_window_samples);
  if (options.band_options.progress_steps <= 0) {
    options.band_options.progress_steps = 16;
  }
  options.band_options.progress_steps =
      std::clamp(options.band_options.progress_steps, 2, 24);
  const double cfg_min_influence = std::max(0.1, config.min_influence);
  const double cfg_max_influence =
      std::min(100.0, std::max(cfg_min_influence, config.max_influence));
  options.band_options.min_bezier_influence =
      std::clamp(options.band_options.min_bezier_influence,
                 cfg_min_influence,
                 cfg_max_influence);
  options.band_options.max_bezier_influence =
      std::clamp(options.band_options.max_bezier_influence,
                 options.band_options.min_bezier_influence,
                 cfg_max_influence);
  options.band_options.bezier_influence_grid_steps =
      std::clamp(options.band_options.bezier_influence_grid_steps, 2, 7);
  options.band_options.bezier_influence_refinement_steps =
      std::clamp(options.band_options.bezier_influence_refinement_steps, 0, 2);
  options.band_options.max_bezier_influence_pairs =
      std::clamp(options.band_options.max_bezier_influence_pairs, 0, 48);
  options.band_options.fit_bezier_influence_pairs =
      config.allow_bezier &&
      config.allow_shape_temporal_bezier &&
      options.band_options.max_bezier_influence_pairs > 0;
  const bool scan_progress_grid = options.band_options.compute_progress_bands;
  const int required_evaluations =
      options.band_options.max_window_samples *
      (2 +
       (scan_progress_grid ? options.band_options.progress_steps + 1 : 0) +
       (options.band_options.fit_bezier_influence_pairs
            ? options.band_options.max_bezier_influence_pairs
            : 0));
  if (options.band_options.max_evaluations <= 0) {
    options.band_options.max_evaluations = required_evaluations;
  } else {
    options.band_options.max_evaluations =
        std::max(options.band_options.max_evaluations, required_evaluations);
  }
  options.band_options.frame_fit_options.max_subdivisions_per_segment =
      std::max(1, std::min(options.band_options.frame_fit_options.max_subdivisions_per_segment,
                           kMaxOracleSubdivisions));
  // Production DP only needs the linear/default-Bezier predicates. Full bands
  // are still available to direct diagnostic callers of
  // EvaluateShapeFlatMorphProgressBands.
  options.band_options.compute_progress_bands = false;
  options.allow_relaxed_endpoint_fit =
      options.allow_relaxed_endpoint_fit &&
      (config.allow_linear || config.allow_bezier);
  if (options.max_gap_samples <= 0) {
    options.max_gap_samples = kDefaultMaxGapSamples;
  }
  options.max_gap_samples =
      std::max(1, std::min(options.max_gap_samples,
                           options.band_options.max_window_samples - 1));
  options.allow_multimode_anchor_union =
      options.allow_multimode_anchor_union && config.allow_linear;
  options.multimode_max_regions =
      std::clamp(options.multimode_max_regions, 1, 16);
  if (options.multimode_max_gap_samples <= 0) {
    options.multimode_max_gap_samples =
        std::max(options.max_gap_samples * 3, options.max_gap_samples);
  }
  options.multimode_max_gap_samples =
      std::clamp(options.multimode_max_gap_samples, 1, 96);
  if (!(options.multimode_region_tolerance > 0.0)) {
    options.multimode_region_tolerance =
        std::max(options.band_options.frame_fit_options.outline_tolerance, 0.0);
  }
  if (options.multimode_max_region_segment_checks < 0) {
    options.multimode_max_region_segment_checks = 0;
  }
  if (options.multimode_max_validation_samples < 0) {
    options.multimode_max_validation_samples = 0;
  }
  if (options.multimode_max_validation_work_units < 0) {
    options.multimode_max_validation_work_units = 0;
  }
  if (!std::isfinite(options.multimode_max_candidate_key_ratio) ||
      options.multimode_max_candidate_key_ratio < 0.0) {
    options.multimode_max_candidate_key_ratio = 0.0;
  }
  options.multimode_max_candidate_key_ratio =
      std::min(options.multimode_max_candidate_key_ratio, 1.0);
  if (!std::isfinite(options.multimode_fast_accept_key_ratio) ||
      options.multimode_fast_accept_key_ratio < 0.0) {
    options.multimode_fast_accept_key_ratio = 0.0;
  }
  options.multimode_fast_accept_key_ratio =
      std::min(options.multimode_fast_accept_key_ratio, 1.0);
  options.forward_longest_span_min_vertex_count =
      std::max(0, options.forward_longest_span_min_vertex_count);
  options.forward_longest_span_min_samples =
      std::max(2, options.forward_longest_span_min_samples);
  if (options.forward_longest_span_max_gap_samples <= 0) {
    options.forward_longest_span_max_gap_samples =
        kDefaultForwardLongestSpanMaxGapSamples;
  }
  options.forward_longest_span_max_gap_samples =
      std::clamp(options.forward_longest_span_max_gap_samples, 1, 1024);
  if (options.forward_longest_span_max_segment_checks <= 0) {
    options.forward_longest_span_max_segment_checks =
        kDefaultForwardLongestSpanMaxSegmentChecks;
  }
  return options;
}

}  // namespace bbsolver
