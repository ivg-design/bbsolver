#pragma once

#include "bbsolver/domain.hpp"

#include <string>
#include <vector>

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

namespace bbsolver {

// Options for ValidatePathTemporalCandidate. The embedded PathFrameFitOptions
// controls the dense polyline subdivision used by ShapeFlatFrameOutlineError
// and sets the outline_tolerance threshold for the ok verdict.
struct PathTemporalValidationOptions {
  PathFrameFitOptions frame_fit_options;
};

// Per-call result from ValidatePathTemporalCandidate.
struct PathTemporalValidationResult {
  // True when max_outline_error <= frame_fit_options.outline_tolerance.
  bool ok = false;

  // Maximum symmetric Hausdorff outline error across all checked samples.
  double max_outline_error = 0.0;

  // Time of the worst-error sample (seconds, in the property's time base).
  double worst_t_sec = 0.0;

  // 0-based index of the worst sample in original.samples.
  int worst_sample_idx = -1;

  // Normalized position of the worst sample in [0, 1] over the property's
  // [t_start_sec, t_end_sec] range. Useful for locating the failure region
  // within a segment without converting to frame numbers.
  double worst_t_fraction = 0.0;

  // Number of source samples evaluated.
  int samples_checked = 0;

  // Human-readable summary suitable for appending to PropertyKeys.notes.
  std::string notes;
};

struct ShapeMorphProgressInterval {
  double min_u = 0.0;
  double max_u = 0.0;
  int first_step = 0;
  int last_step = 0;
};

struct ShapeMorphProgressSampleBand {
  int sample_idx = -1;
  double t_sec = 0.0;
  double best_u = 0.0;
  double best_error = 0.0;
  std::vector<ShapeMorphProgressInterval> intervals;
};

struct ShapeMorphProgressBandOptions {
  PathFrameFitOptions frame_fit_options;
  int progress_steps = 16;
  int max_window_samples = 8;
  int max_evaluations = 384;
  // Production callers that only need to accept/reject linear or neutral
  // Bezier timing can leave this false to avoid scanning the full u grid.
  // Diagnostic callers set it true to populate per-sample acceptable u bands
  // and the monotone-progress oracle.
  bool compute_progress_bands = false;
  // Production replacement-path solving can request a bounded AE-compatible
  // Shape Path temporal influence fit. The fit keeps TemporalEase.speed at 0
  // and varies only the outgoing/incoming influence pair.
  bool fit_bezier_influence_pairs = false;
  double min_bezier_influence = 0.1;
  double max_bezier_influence = 100.0;
  int bezier_influence_grid_steps = 5;
  int bezier_influence_refinement_steps = 1;
  int max_bezier_influence_pairs = 8;
};

struct ShapeMorphProgressBandResult {
  bool ok = false;
  // True when compute_progress_bands populated per-sample u bands.
  bool progress_bands_computed = false;
  // True when every source sample has at least one acceptable arbitrary u on
  // the progress grid. This is a shape-chord lower bound, not an AE timing
  // guarantee.
  bool progress_band_possible = false;
  // True when the per-sample acceptable u bands admit a monotone path. This
  // says an arbitrary monotone progress function could traverse the chord.
  bool monotone_band_progress_possible = false;
  // Backward-compatible aggregate: true when either a monotone band path or an
  // enabled AE-compatible ease path is possible.
  bool monotone_progress_possible = false;
  // True when one of the enabled AE-compatible timing families fits: linear,
  // neutral/default Bezier, or fitted influence-pair Bezier.
  bool ae_ease_progress_possible = false;
  bool linear_progress_possible = false;
  bool default_bezier_progress_possible = false;
  bool fitted_bezier_progress_possible = false;
  std::string reason;
  int start_sample_idx = -1;
  int end_sample_idx = -1;
  int samples_checked = 0;
  int progress_steps = 0;
  int evaluations = 0;
  int fitted_bezier_pairs_tried = 0;
  double outline_error_wall_ms = 0.0;
  double fitted_bezier_wall_ms = 0.0;
  double fitted_bezier_out_influence = 33.3;
  double fitted_bezier_in_influence = 33.3;
  double max_best_error = 0.0;
  double max_linear_error = 0.0;
  double max_default_bezier_error = 0.0;
  double max_fitted_bezier_error = 0.0;
  std::vector<ShapeMorphProgressSampleBand> samples;
};

// Bounded oracle for one fixed-topology replacement path interval. It tests
// whether every source frame in [start_sample_idx, end_sample_idx] can be
// represented by a single morph chord A + u * (B - A), where A and B are the
// reduced fixed-topology endpoint shape_flat values. The acceptable u bands are
// found on a discrete progress grid and validated with source-outline distance.
//
// This does not choose production keys. It is intended as the cached segment
// evaluator that replacement-path temporal fitting can call before attempting
// timing optimization. A successful result has reason
// "shape_morph_chord_ok"; a geometric impossibility reports
// "infeasible_shape_morph_chord".
ShapeMorphProgressBandResult EvaluateShapeFlatMorphProgressBands(
    const PropertySamples& original,
    int start_sample_idx,
    int end_sample_idx,
    const std::vector<double>& endpoint_a,
    const std::vector<double>& endpoint_b,
    const ShapeMorphProgressBandOptions& options = {});

// Compact classification for diagnostics. With compute_progress_bands=true,
// this distinguishes:
//   chord_infeasible     -- no arbitrary u can keep every sample in tolerance
//   band_infeasible      -- per-sample u exists, but no monotone u path exists
//   ae_ease_infeasible   -- monotone u exists, but enabled AE timing cannot fit
//   ae_ease_feasible     -- an enabled AE-compatible timing family fits
// Non-ok oracle results return the result.reason when present.
std::string ShapeMorphProgressFeasibilityClass(
    const ShapeMorphProgressBandResult& result,
    double tolerance);

// Evaluate a stage-1 replacement candidate PropertyKeys against the original
// source PropertySamples at every sample time and report the maximum symmetric
// Hausdorff outline distance between the interpolated candidate shape and the
// original source shape.
//
// original  — the source shape_flat PropertySamples the candidate was derived
//             from. Each sample.v must be a valid shape_flat vector
//             (v[1] = n_vertices, size = 2 + 6 * n_vertices). The source vertex
//             count may be higher than the candidate's.
//
// candidate — PropertyKeys whose keys[].v are shape_flat vectors for the
//             reduced-vertex path. Evaluated via EvalKeysAt at each source
//             sample time; the closed flag and vertex count must be consistent
//             across all keys.
//
// At each sample time, the outline error is computed by densifying both the
// source and candidate shapes into polylines and measuring the symmetric
// point-to-segment distance (ShapeFlatFrameOutlineError). This correctly
// handles different vertex counts between source and candidate.
//
// Returns ok=false for empty inputs or malformed shape_flat. Returns ok=true
// when max_outline_error <= options.frame_fit_options.outline_tolerance + 1e-9.
PathTemporalValidationResult ValidatePathTemporalCandidate(
    const PropertySamples& original,
    const PropertyKeys& candidate,
    const PathTemporalValidationOptions& options = {});

}  // namespace bbsolver
