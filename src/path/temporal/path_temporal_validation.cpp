#include "bbsolver/path/temporal/path_temporal_validation.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <ratio>
#include <string>
#include <utility>
#include <vector>
#include <cstddef>

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/verify/verifier.hpp"
#include "bbsolver/path/temporal/path_temporal_band_helpers.hpp"
#include "bbsolver/path/temporal/path_temporal_influence.hpp"
#include "bbsolver/path/temporal/path_temporal_progress.hpp"

namespace bbsolver {

ShapeMorphProgressBandResult EvaluateShapeFlatMorphProgressBands(
    const PropertySamples& original,
    int start_sample_idx,
    int end_sample_idx,
    const std::vector<double>& endpoint_a,
    const std::vector<double>& endpoint_b,
    const ShapeMorphProgressBandOptions& options) {
  ShapeMorphProgressBandResult result;
  result.progress_bands_computed = options.compute_progress_bands;
  result.start_sample_idx = start_sample_idx;
  result.end_sample_idx = end_sample_idx;
  result.progress_steps = options.progress_steps;
  result.max_best_error = 0.0;
  result.max_linear_error = 0.0;
  result.max_default_bezier_error = 0.0;
  result.max_fitted_bezier_error =
      options.fit_bezier_influence_pairs
          ? std::numeric_limits<double>::infinity()
          : 0.0;

  if (original.samples.empty()) {
    result.reason = "no_source_samples";
    return result;
  }
  if (start_sample_idx < 0 ||
      end_sample_idx < start_sample_idx ||
      end_sample_idx >= static_cast<int>(original.samples.size())) {
    result.reason = "invalid_sample_window";
    return result;
  }
  const int sample_count = end_sample_idx - start_sample_idx + 1;
  if (options.max_window_samples > 0 && sample_count > options.max_window_samples) {
    result.reason = "shape_morph_chord_window_too_large";
    return result;
  }
  if (options.progress_steps < 2) {
    result.reason = "shape_morph_chord_bad_progress_grid";
    return result;
  }
  const bool scan_progress_grid = options.compute_progress_bands;
  const int influence_pair_budget =
      options.fit_bezier_influence_pairs
          ? std::max(0, options.max_bezier_influence_pairs)
          : 0;
  const int evaluations_per_sample =
      2 + (scan_progress_grid ? options.progress_steps + 1 : 0) +
      influence_pair_budget;
  const int evaluation_count = sample_count * evaluations_per_sample;
  if (options.max_evaluations > 0 && evaluation_count > options.max_evaluations) {
    result.reason = "shape_morph_chord_evaluation_budget_exceeded";
    return result;
  }
  if (!PathTemporalShapeFlatIsValid(endpoint_a) ||
      !PathTemporalShapeFlatIsValid(endpoint_b) ||
      endpoint_a.size() != endpoint_b.size() ||
      std::llround(endpoint_a[0]) != std::llround(endpoint_b[0]) ||
      std::llround(endpoint_a[1]) != std::llround(endpoint_b[1])) {
    result.reason = "malformed_shape_morph_endpoints";
    return result;
  }

  std::vector<std::vector<double>> candidate_grid;
  std::vector<ShapeFlatOutlinePolyline> candidate_grid_outlines;
  candidate_grid.reserve(static_cast<std::size_t>(options.progress_steps + 1));
  candidate_grid_outlines.reserve(static_cast<std::size_t>(options.progress_steps + 1));
  for (int step = 0; step <= options.progress_steps; ++step) {
    candidate_grid.push_back(
        LerpShapeFlatChord(endpoint_a,
                           endpoint_b,
                           static_cast<double>(step) /
                               static_cast<double>(options.progress_steps)));
    candidate_grid_outlines.push_back(
        BuildShapeFlatOutlinePolyline(candidate_grid.back(),
                                      options.frame_fit_options));
  }

  std::vector<ShapeFlatOutlinePolyline> source_outlines;
  source_outlines.reserve(static_cast<std::size_t>(sample_count));
  for (int sample_idx = start_sample_idx; sample_idx <= end_sample_idx; ++sample_idx) {
    const Sample& sample = original.samples[static_cast<std::size_t>(sample_idx)];
    if (!PathTemporalShapeFlatIsValid(sample.v)) {
      result.reason = "malformed_source_at_t=" + std::to_string(sample.t_sec);
      return result;
    }
    ShapeFlatOutlinePolyline source_outline =
        BuildShapeFlatOutlinePolyline(sample.v, options.frame_fit_options);
    if (!source_outline.ok) {
      result.reason = "malformed_source_at_t=" + std::to_string(sample.t_sec);
      return result;
    }
    source_outlines.push_back(std::move(source_outline));
  }

  std::vector<std::vector<bool>> accepted_rows;
  if (options.compute_progress_bands) {
    accepted_rows.reserve(static_cast<std::size_t>(sample_count));
  }
  result.samples.reserve(static_cast<std::size_t>(sample_count));
  const double tolerance = std::max(options.frame_fit_options.outline_tolerance, 0.0);
  const double t0 = original.samples[static_cast<std::size_t>(start_sample_idx)].t_sec;
  const double t1 = original.samples[static_cast<std::size_t>(end_sample_idx)].t_sec;

  const auto outline_error = [&](const ShapeFlatOutlinePolyline& source_outline,
                                 const ShapeFlatOutlinePolyline& candidate_outline) {
    const auto outline_start = std::chrono::steady_clock::now();
    const double err = ShapeFlatFrameOutlineErrorFromPolylines(
        source_outline,
        candidate_outline);
    const auto outline_end = std::chrono::steady_clock::now();
    result.outline_error_wall_ms +=
        std::chrono::duration<double, std::milli>(
            outline_end - outline_start).count();
    return err;
  };

  for (int sample_idx = start_sample_idx; sample_idx <= end_sample_idx; ++sample_idx) {
    const Sample& sample = original.samples[static_cast<std::size_t>(sample_idx)];
    ShapeMorphProgressSampleBand sample_band;
    sample_band.sample_idx = sample_idx;
    sample_band.t_sec = sample.t_sec;
    sample_band.best_error = std::numeric_limits<double>::infinity();

    const ShapeFlatOutlinePolyline& source_outline =
        source_outlines[static_cast<std::size_t>(sample_idx - start_sample_idx)];

    const double alpha = (t1 > t0) ? (sample.t_sec - t0) / (t1 - t0) : 0.0;
    const int linear_step = ProgressStepForLinear(alpha, options.progress_steps);
    const int bezier_step =
        ProgressStepForDefaultBezier(alpha, options.progress_steps);
    const double linear_err = outline_error(
        source_outline,
        candidate_grid_outlines[static_cast<std::size_t>(linear_step)]);
    ++result.evaluations;
    const double bezier_err = outline_error(
        source_outline,
        candidate_grid_outlines[static_cast<std::size_t>(bezier_step)]);
    ++result.evaluations;
    result.max_linear_error = std::max(result.max_linear_error, linear_err);
    result.max_default_bezier_error =
        std::max(result.max_default_bezier_error, bezier_err);

    if (scan_progress_grid) {
      std::vector<bool> accepted(static_cast<std::size_t>(options.progress_steps + 1), false);
      for (int step = 0; step <= options.progress_steps; ++step) {
        const double err = outline_error(
            source_outline,
            candidate_grid_outlines[static_cast<std::size_t>(step)]);
        ++result.evaluations;
        if (err < sample_band.best_error) {
          sample_band.best_error = err;
          sample_band.best_u = static_cast<double>(step) /
                               static_cast<double>(options.progress_steps);
        }
        accepted[static_cast<std::size_t>(step)] = err <= tolerance + 1e-9;
      }

      if (sample_idx == start_sample_idx) {
        for (int step = 1; step <= options.progress_steps; ++step) {
          accepted[static_cast<std::size_t>(step)] = false;
        }
      } else if (sample_idx == end_sample_idx) {
        for (int step = 0; step < options.progress_steps; ++step) {
          accepted[static_cast<std::size_t>(step)] = false;
        }
      }

      result.max_best_error = std::max(result.max_best_error, sample_band.best_error);
      if (options.compute_progress_bands) {
        sample_band.intervals =
            BuildShapeMorphProgressIntervals(accepted, options.progress_steps);
        accepted_rows.push_back(std::move(accepted));
      }
    } else {
      sample_band.best_error = std::min(linear_err, bezier_err);
      sample_band.best_u =
          linear_err <= bezier_err
              ? static_cast<double>(linear_step) / static_cast<double>(options.progress_steps)
              : static_cast<double>(bezier_step) / static_cast<double>(options.progress_steps);
      result.max_best_error = std::max(result.max_best_error, sample_band.best_error);
    }
    result.samples.push_back(std::move(sample_band));
  }

  result.samples_checked = sample_count;
  result.linear_progress_possible = result.max_linear_error <= tolerance + 1e-9;
  result.default_bezier_progress_possible =
      result.max_default_bezier_error <= tolerance + 1e-9;

  if (options.fit_bezier_influence_pairs &&
      influence_pair_budget > 0 &&
      !result.linear_progress_possible &&
      !result.default_bezier_progress_possible) {
    const std::vector<ShapeTemporalInfluencePair> candidates =
        BuildInitialShapeTemporalInfluenceCandidates(options);

    double best_error = std::numeric_limits<double>::infinity();
    ShapeTemporalInfluencePair best_pair;
    std::vector<ShapeTemporalInfluencePair> tried;
    tried.reserve(static_cast<std::size_t>(influence_pair_budget));

    const auto try_pair = [&](ShapeTemporalInfluencePair pair) {
      if (result.fitted_bezier_pairs_tried >= influence_pair_budget) {
        return;
      }
      for (const ShapeTemporalInfluencePair& seen : tried) {
        if (ShapeTemporalInfluencesAlmostSame(seen.out_influence,
                                              pair.out_influence) &&
            ShapeTemporalInfluencesAlmostSame(seen.in_influence,
                                              pair.in_influence)) {
          return;
        }
      }
      if (options.max_evaluations > 0 &&
          result.evaluations + sample_count > options.max_evaluations) {
        return;
      }
      tried.push_back(pair);
      ++result.fitted_bezier_pairs_tried;
      const auto pair_start = std::chrono::steady_clock::now();
      const double err = EvaluateShapeTemporalInfluencePairMaxError(
          original,
          start_sample_idx,
          end_sample_idx,
          source_outlines,
          endpoint_a,
          endpoint_b,
          options,
          pair,
          best_error,
          &result.evaluations,
          &result.outline_error_wall_ms);
      const auto pair_end = std::chrono::steady_clock::now();
      result.fitted_bezier_wall_ms +=
          std::chrono::duration<double, std::milli>(
              pair_end - pair_start).count();
      if (err < best_error) {
        best_error = err;
        best_pair = pair;
      }
    };

    const int reserved_refine_pairs =
        options.bezier_influence_refinement_steps > 0
            ? std::min(8, influence_pair_budget / 2)
            : 0;
    const int coarse_pair_budget =
        std::max(1, influence_pair_budget - reserved_refine_pairs);
    std::size_t next_candidate_idx = 0;
    for (; next_candidate_idx < candidates.size(); ++next_candidate_idx) {
      try_pair(candidates[next_candidate_idx]);
      if (result.fitted_bezier_pairs_tried >= coarse_pair_budget) {
        ++next_candidate_idx;
        break;
      }
    }

    double refine_step =
        (ClampShapeTemporalInfluencePercent(options.max_bezier_influence,
                                            options.min_bezier_influence,
                                            options.max_bezier_influence) -
         ClampShapeTemporalInfluencePercent(options.min_bezier_influence,
                                            options.min_bezier_influence,
                                            options.max_bezier_influence)) /
        static_cast<double>(std::max(2, options.bezier_influence_grid_steps) - 1);
    for (int refine = 0;
         refine < options.bezier_influence_refinement_steps &&
         result.fitted_bezier_pairs_tried < influence_pair_budget &&
         std::isfinite(best_error);
         ++refine) {
      refine_step *= 0.5;
      for (double out_delta : std::array<double, 3>{-refine_step, 0.0, refine_step}) {
        for (double in_delta : std::array<double, 3>{-refine_step, 0.0, refine_step}) {
          if (out_delta == 0.0 && in_delta == 0.0) {
            continue;
          }
          try_pair({best_pair.out_influence + out_delta,
                    best_pair.in_influence + in_delta});
        }
      }
    }
    for (; next_candidate_idx < candidates.size() &&
           result.fitted_bezier_pairs_tried < influence_pair_budget;
         ++next_candidate_idx) {
      try_pair(candidates[next_candidate_idx]);
    }

    result.max_fitted_bezier_error = best_error;
    if (std::isfinite(best_error)) {
      result.fitted_bezier_out_influence = best_pair.out_influence;
      result.fitted_bezier_in_influence = best_pair.in_influence;
      result.fitted_bezier_progress_possible = best_error <= tolerance + 1e-9;
      result.max_best_error =
          result.max_best_error > 0.0
              ? std::min(result.max_best_error, best_error)
              : best_error;
    }
  }

  result.progress_band_possible =
      options.compute_progress_bands &&
      result.max_best_error <= tolerance + 1e-9;
  result.monotone_band_progress_possible =
      options.compute_progress_bands &&
      result.progress_band_possible &&
      ShapeMorphHasMonotoneProgressPath(accepted_rows);
  result.ae_ease_progress_possible =
      result.linear_progress_possible ||
      result.default_bezier_progress_possible ||
      result.fitted_bezier_progress_possible;
  result.monotone_progress_possible =
      options.compute_progress_bands
          ? (result.monotone_band_progress_possible ||
             result.ae_ease_progress_possible)
          : result.ae_ease_progress_possible;
  result.ok = true;
  const std::string feasibility =
      ShapeMorphProgressFeasibilityClass(result, tolerance);
  result.reason =
      (feasibility == "chord_infeasible")
          ? "infeasible_shape_morph_chord"
          : (feasibility == "band_infeasible")
                ? "infeasible_shape_morph_band"
                : "shape_morph_chord_ok";
  return result;
}

std::string ShapeMorphProgressFeasibilityClass(
    const ShapeMorphProgressBandResult& result,
    double tolerance) {
  if (!result.ok) {
    return result.reason.empty() ? "oracle_failed" : result.reason;
  }
  const double tol = std::max(tolerance, 0.0);
  if (result.progress_bands_computed) {
    if (!result.progress_band_possible ||
        result.max_best_error > tol + 1e-9) {
      return "chord_infeasible";
    }
    if (!result.monotone_band_progress_possible &&
        !result.ae_ease_progress_possible) {
      return "band_infeasible";
    }
  } else if (result.max_best_error > tol + 1e-9) {
    return "chord_infeasible";
  }
  if (!result.ae_ease_progress_possible) {
    return "ae_ease_infeasible";
  }
  return "ae_ease_feasible";
}

PathTemporalValidationResult ValidatePathTemporalCandidate(
    const PropertySamples& original,
    const PropertyKeys& candidate,
    const PathTemporalValidationOptions& options) {
  PathTemporalValidationResult result;

  if (original.samples.empty()) {
    result.notes = "no_source_samples";
    return result;
  }
  if (candidate.keys.empty()) {
    result.notes = "no_candidate_keys";
    return result;
  }

  const double t_start = original.t_start_sec;
  const double t_range = original.t_end_sec - original.t_start_sec;

  double max_err   = 0.0;
  int    worst_idx = 0;

  const int n_samples = static_cast<int>(original.samples.size());
  for (int i = 0; i < n_samples; ++i) {
    const Sample& src = original.samples[static_cast<std::size_t>(i)];

    if (!PathTemporalShapeFlatIsValid(src.v)) {
      result.notes =
          "malformed_source_at_t=" + std::to_string(src.t_sec);
      return result;
    }

    const std::vector<double> cand_v =
        EvalKeysAt(candidate.keys, src.t_sec);

    if (!PathTemporalShapeFlatIsValid(cand_v)) {
      result.notes =
          "malformed_candidate_eval_at_t=" + std::to_string(src.t_sec);
      return result;
    }

    // ShapeFlatFrameOutlineError densifies both shapes to polylines and
    // computes symmetric Hausdorff distance. Different vertex counts between
    // source and candidate are handled correctly.
    const double err =
        ShapeFlatFrameOutlineError(src.v, cand_v, options.frame_fit_options);

    if (err > max_err) {
      max_err   = err;
      worst_idx = i;
    }
  }

  const Sample& worst =
      original.samples[static_cast<std::size_t>(worst_idx)];

  result.max_outline_error = max_err;
  result.worst_sample_idx  = worst_idx;
  result.worst_t_sec       = worst.t_sec;
  result.worst_t_fraction  = (t_range > 1e-12)
                                 ? (worst.t_sec - t_start) / t_range
                                 : 0.0;
  result.samples_checked   = n_samples;
  result.ok =
      max_err <= options.frame_fit_options.outline_tolerance + 1e-9;

  result.notes =
      (result.ok ? "ok" : "exceeds_tolerance") +
      std::string("; max_outline_error=") + std::to_string(max_err) +
      "; worst_t=" + std::to_string(result.worst_t_sec) +
      "; worst_t_fraction=" + std::to_string(result.worst_t_fraction) +
      "; samples_checked=" + std::to_string(result.samples_checked);

  return result;
}

}  // namespace bbsolver
