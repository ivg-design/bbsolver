#include "bbsolver/path/multimode/path_multimode_landmark_segment_fit.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/path/multimode/path_multimode_geometry.hpp"
#include "bbsolver/path/multimode/path_multimode_region_candidate.hpp"
#include "bbsolver/path/multimode/path_multimode_temporal.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <vector>
#include <cstddef>

namespace bbsolver {
namespace path_multimode {
namespace {

struct RelaxedEndpointFit {
  bool ok = false;
  std::vector<double> endpoint_a;
  std::vector<double> endpoint_b;
};

struct RelaxedValidation {
  bool ok = false;
  double max_err = 0.0;
  double rms_err = 0.0;
};

RelaxedEndpointFit FitRelaxedEndpointChord(
    const PropertySamples& region_samples,
    int i,
    int j,
    const std::vector<double>& progress) {
  RelaxedEndpointFit fit;
  if (i < 0 || j <= i ||
      j >= static_cast<int>(region_samples.samples.size()) ||
      progress.size() != static_cast<std::size_t>(j - i + 1)) {
    return fit;
  }
  const std::vector<double>& first =
      region_samples.samples[static_cast<std::size_t>(i)].v;
  const std::vector<double>& last =
      region_samples.samples[static_cast<std::size_t>(j)].v;
  if (!SameShapeFlatTopology(first, last)) {
    return fit;
  }
  const std::size_t dim = first.size();
  double s00 = 0.0;
  double s01 = 0.0;
  double s11 = 0.0;
  std::vector<double> r0(dim, 0.0);
  std::vector<double> r1(dim, 0.0);
  for (int sample_idx = i; sample_idx <= j; ++sample_idx) {
    const std::vector<double>& sample =
        region_samples.samples[static_cast<std::size_t>(sample_idx)].v;
    if (!SameShapeFlatTopology(first, sample)) {
      return fit;
    }
    const double u = std::clamp(progress[static_cast<std::size_t>(sample_idx - i)],
                                0.0, 1.0);
    const double w0 = 1.0 - u;
    const double w1 = u;
    s00 += w0 * w0;
    s01 += w0 * w1;
    s11 += w1 * w1;
    for (std::size_t component = 2; component < dim; ++component) {
      r0[component] += w0 * sample[component];
      r1[component] += w1 * sample[component];
    }
  }

  const double det = s00 * s11 - s01 * s01;
  if (std::abs(det) < 1e-12) {
    return fit;
  }
  fit.endpoint_a = first;
  fit.endpoint_b = last;
  fit.endpoint_a[0] = first[0];
  fit.endpoint_a[1] = first[1];
  fit.endpoint_b[0] = first[0];
  fit.endpoint_b[1] = first[1];
  for (std::size_t component = 2; component < dim; ++component) {
    fit.endpoint_a[component] =
        (r0[component] * s11 - r1[component] * s01) / det;
    fit.endpoint_b[component] =
        (s00 * r1[component] - s01 * r0[component]) / det;
  }
  fit.ok = SameShapeFlatTopology(fit.endpoint_a, fit.endpoint_b);
  return fit;
}

RelaxedValidation ValidateRelaxedRegionChord(
    const PropertySamples& region_samples,
    int i,
    int j,
    const std::vector<double>& endpoint_a,
    const std::vector<double>& endpoint_b,
    const std::vector<double>& progress,
    const ShapeMorphProgressBandOptions& options) {
  RelaxedValidation validation;
  if (i < 0 || j <= i ||
      j >= static_cast<int>(region_samples.samples.size()) ||
      progress.size() != static_cast<std::size_t>(j - i + 1) ||
      !SameShapeFlatTopology(endpoint_a, endpoint_b)) {
    return validation;
  }

  const double tolerance =
      std::max(options.frame_fit_options.outline_tolerance, 0.0);
  double sum_sq = 0.0;
  int samples_checked = 0;
  for (int sample_idx = i; sample_idx <= j; ++sample_idx) {
    const double u =
        progress[static_cast<std::size_t>(sample_idx - i)];
    const std::vector<double> predicted =
        LinearInterpolateShapeFlat(endpoint_a, endpoint_b, u);
    if (predicted.empty()) {
      return {};
    }
    const double err = ShapeFlatFrameOutlineError(
        region_samples.samples[static_cast<std::size_t>(sample_idx)].v,
        predicted,
        options.frame_fit_options);
    validation.max_err = std::max(validation.max_err, err);
    sum_sq += err * err;
    ++samples_checked;
    if (err > tolerance + 1e-9) {
      validation.ok = false;
      validation.rms_err =
          std::sqrt(sum_sq / static_cast<double>(samples_checked));
      return validation;
    }
  }
  validation.ok = samples_checked == j - i + 1;
  validation.rms_err = samples_checked > 0
                           ? std::sqrt(sum_sq /
                                       static_cast<double>(samples_checked))
: 0.0;
  return validation;
}

SegmentFitResult RelaxedLandmarkResult(InterpType interp,
                                       std::string reason,
                                       TemporalEase ease_out,
                                       TemporalEase ease_in,
                                       const RelaxedEndpointFit& fit,
                                       const RelaxedValidation& validation) {
  SegmentFitResult result;
  result.feasible = true;
  result.interp = interp;
  result.reason = std::move(reason);
  result.ease_out_at_i = {ease_out};
  result.ease_in_at_j = {ease_in};
  result.key_value_at_i = fit.endpoint_a;
  result.key_value_at_j = fit.endpoint_b;
  result.max_err = validation.max_err;
  result.max_err_screen_px = validation.max_err;
  result.rms_err = validation.rms_err;
  return result;
}

SegmentFitResult TryRelaxedLandmarkRegionSegment(
    int i,
    int j,
    const PropertySamples& region_samples,
    const SolverConfig& config,
    const ShapeMorphProgressBandOptions& band_options,
    const ShapeMorphProgressBandResult& strict_oracle) {
  if (config.allow_linear) {
    const TemporalEase neutral{0.0, 33.3};
    const std::vector<double> progress =
        SegmentProgressValues(region_samples, i, j, false, neutral, neutral,
                              band_options);
    const RelaxedEndpointFit fit =
        FitRelaxedEndpointChord(region_samples, i, j, progress);
    if (fit.ok) {
      const RelaxedValidation validation =
          ValidateRelaxedRegionChord(region_samples, i, j,
                                     fit.endpoint_a, fit.endpoint_b,
                                     progress, band_options);
      if (validation.ok) {
        return RelaxedLandmarkResult(
            InterpType::Linear,
            "landmark_subpath_temporal_relaxed_linear_ok",
            neutral,
            neutral,
            fit,
            validation);
      }
    }
  }

  if (config.allow_bezier && config.allow_shape_temporal_bezier) {
    const TemporalEase neutral{0.0, 33.3};
    const std::vector<double> default_progress =
        SegmentProgressValues(region_samples, i, j, true, neutral, neutral,
                              band_options);
    const RelaxedEndpointFit default_fit =
        FitRelaxedEndpointChord(region_samples, i, j, default_progress);
    if (default_fit.ok) {
      const RelaxedValidation validation =
          ValidateRelaxedRegionChord(region_samples, i, j,
                                     default_fit.endpoint_a,
                                     default_fit.endpoint_b,
                                     default_progress, band_options);
      if (validation.ok) {
        return RelaxedLandmarkResult(
            InterpType::Bezier,
            "landmark_subpath_temporal_relaxed_bezier_ok",
            neutral,
            neutral,
            default_fit,
            validation);
      }
    }

    const auto try_relaxed_bezier_pair =
        [&](const LandmarkInfluencePair& pair,
            const std::string& reason) -> SegmentFitResult {
      const TemporalEase ease_out{0.0, pair.out_influence};
      const TemporalEase ease_in{0.0, pair.in_influence};
      const std::vector<double> fitted_progress =
          SegmentProgressValues(region_samples, i, j, true, ease_out, ease_in,
                                band_options);
      const RelaxedEndpointFit fitted_fit =
          FitRelaxedEndpointChord(region_samples, i, j, fitted_progress);
      if (fitted_fit.ok) {
        const RelaxedValidation validation =
            ValidateRelaxedRegionChord(region_samples, i, j,
                                       fitted_fit.endpoint_a,
                                       fitted_fit.endpoint_b,
                                       fitted_progress, band_options);
        if (validation.ok) {
          return RelaxedLandmarkResult(
              InterpType::Bezier,
              reason,
              ease_out,
              ease_in,
              fitted_fit,
              validation);
        }
      }
      SegmentFitResult failed;
      failed.reason = "landmark_subpath_temporal_relaxed_bezier_pair_failed";
      return failed;
    };

    LandmarkInfluencePair strict_pair;
    bool has_strict_pair =
        strict_oracle.fitted_bezier_pairs_tried > 0 &&
        std::isfinite(strict_oracle.max_fitted_bezier_error);
    if (has_strict_pair) {
      strict_pair.out_influence = strict_oracle.fitted_bezier_out_influence;
      strict_pair.in_influence = strict_oracle.fitted_bezier_in_influence;
      const SegmentFitResult strict_relaxed = try_relaxed_bezier_pair(
          strict_pair, "landmark_subpath_temporal_relaxed_bezier_fit_ok");
      if (strict_relaxed.feasible) {
        return strict_relaxed;
      }
    }

    if (CanRunExtendedRelaxedBezierSearch(region_samples, i, j)) {
      const std::vector<LandmarkInfluencePair> pairs =
          BuildLandmarkInfluencePairs(band_options, strict_oracle);
      SegmentFitResult best_relaxed_bezier;
      RelaxedValidation best_validation;
      best_validation.max_err = std::numeric_limits<double>::infinity();
      bool found_relaxed_bezier = false;
      for (const LandmarkInfluencePair& pair: pairs) {
        if ((std::abs(pair.out_influence - 33.3) <= 1e-6 &&
             std::abs(pair.in_influence - 33.3) <= 1e-6) ||
            (has_strict_pair && SameInfluencePair(pair, strict_pair))) {
          continue;
        }
        SegmentFitResult candidate = try_relaxed_bezier_pair(
            pair, "landmark_subpath_temporal_relaxed_bezier_search_ok");
        if (candidate.feasible &&
            (!found_relaxed_bezier ||
             candidate.max_err + 1e-12 < best_validation.max_err)) {
          best_validation.ok = true;
          best_validation.max_err = candidate.max_err;
          best_relaxed_bezier = std::move(candidate);
          found_relaxed_bezier = true;
        }
      }
      if (found_relaxed_bezier) {
        return best_relaxed_bezier;
      }
    }
  }

  SegmentFitResult result;
  result.reason = "landmark_subpath_temporal_relaxed_failed";
  return result;
}

}  // namespace

SegmentFitResult FitLandmarkRegionShapeSegment(
    int i,
    int j,
    const PropertySamples& region_samples,
    const SolverConfig& config,
    const ShapeMorphProgressBandOptions& band_options,
    bool allow_relaxed_endpoints) {
  SegmentFitResult result;
  result.feasible = false;
  result.interp = InterpType::Linear;
  if (i < 0 || j <= i ||
      j >= static_cast<int>(region_samples.samples.size())) {
    result.reason = "landmark_subpath_temporal_invalid_segment";
    return result;
  }

  const ShapeMorphProgressBandResult oracle =
      EvaluateShapeFlatMorphProgressBands(
          region_samples,
          i,
          j,
          region_samples.samples[static_cast<std::size_t>(i)].v,
          region_samples.samples[static_cast<std::size_t>(j)].v,
          band_options);
  result.max_err = oracle.max_best_error;
  result.max_err_screen_px = oracle.max_best_error;
  result.rms_err = oracle.max_best_error;
  if (!oracle.ok) {
    result.reason = oracle.reason.empty()
                        ? "landmark_subpath_temporal_oracle_failed"
: oracle.reason;
    return result;
  }

  if (config.allow_linear && oracle.linear_progress_possible) {
    result.feasible = true;
    result.interp = InterpType::Linear;
    result.max_err = oracle.max_linear_error;
    result.max_err_screen_px = oracle.max_linear_error;
    result.rms_err = oracle.max_linear_error;
    result.reason = "landmark_subpath_temporal_linear_ok";
    return result;
  }

  if (config.allow_bezier && config.allow_shape_temporal_bezier &&
      (oracle.default_bezier_progress_possible ||
       oracle.fitted_bezier_progress_possible)) {
    const bool use_fitted =
        oracle.fitted_bezier_progress_possible &&
        (!oracle.default_bezier_progress_possible ||
         oracle.max_fitted_bezier_error + 1e-12 <
             oracle.max_default_bezier_error);
    result.feasible = true;
    result.interp = InterpType::Bezier;
    result.max_err = use_fitted ? oracle.max_fitted_bezier_error
: oracle.max_default_bezier_error;
    result.max_err_screen_px = result.max_err;
    result.rms_err = result.max_err;
    result.ease_out_at_i =
        ShapeEase(use_fitted ? oracle.fitted_bezier_out_influence: 33.3);
    result.ease_in_at_j =
        ShapeEase(use_fitted ? oracle.fitted_bezier_in_influence: 33.3);
    result.reason = use_fitted ? "landmark_subpath_temporal_bezier_fit_ok"
: "landmark_subpath_temporal_bezier_ok";
    return result;
  }

  if (allow_relaxed_endpoints) {
    const SegmentFitResult relaxed =
        TryRelaxedLandmarkRegionSegment(i, j, region_samples, config,
                                        band_options, oracle);
    if (relaxed.feasible) {
      return relaxed;
    }
  }

  result.reason = oracle.monotone_progress_possible
                      ? "landmark_subpath_temporal_infeasible_timing"
: "landmark_subpath_temporal_infeasible_chord";
  return result;
}

}  // namespace path_multimode
}  // namespace bbsolver
