#include "bbsolver/replacement_temporal/replacement_temporal_relaxed_fit.hpp"

#include "bbsolver/domain.hpp"
#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"
#include "bbsolver/replacement_temporal/replacement_temporal_keys.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <ratio>
#include <string>
#include <utility>
#include <vector>

namespace bbsolver {
namespace replacement_temporal {
namespace {

double ClampAeInfluence(double influence) {
  if (!std::isfinite(influence)) {
    return 33.3;
  }
  return std::clamp(influence, 0.1, 100.0);
}

double CubicScalar(double t, double p0, double p1, double p2, double p3) {
  const double omt = 1.0 - t;
  return omt * omt * omt * p0 +
         3.0 * omt * omt * t * p1 +
         3.0 * omt * t * t * p2 +
         t * t * t * p3;
}

double ClampInfluence(double influence,
                      const ShapeMorphProgressBandOptions& options) {
  if (!std::isfinite(influence)) {
    return 33.3;
  }
  const double lo = std::max(0.1, std::min(options.min_bezier_influence,
                                           options.max_bezier_influence));
  const double hi = std::min(100.0, std::max(options.min_bezier_influence,
                                             options.max_bezier_influence));
  return std::clamp(influence, lo, std::max(lo, hi));
}

double ShapeTemporalBezierProgress(
    double alpha,
    TemporalEase out_ease,
    TemporalEase in_ease,
    const ShapeMorphProgressBandOptions& options) {
  const double out_influence =
      ClampInfluence(out_ease.influence, options) / 100.0;
  const double in_influence =
      ClampInfluence(in_ease.influence, options) / 100.0;
  const double x1 = out_influence;
  const double x2 = 1.0 - in_influence;
  double lo = 0.0;
  double hi = 1.0;
  for (int iter = 0; iter < 40; ++iter) {
    const double mid = (lo + hi) * 0.5;
    const double x = CubicScalar(mid, 0.0, x1, x2, 1.0);
    if (x < alpha) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  return CubicScalar((lo + hi) * 0.5, 0.0, 0.0, 1.0, 1.0);
}

RelaxedEndpointFit FitRelaxedEndpointChord(
    const PropertySamples& reduced,
    int i,
    int j,
    const std::vector<double>& progress) {
  RelaxedEndpointFit fit;
  if (i < 0 || j <= i ||
      j >= static_cast<int>(reduced.samples.size()) ||
      progress.size() != static_cast<std::size_t>(j - i + 1)) {
    return fit;
  }

  const std::vector<double>& sample_a =
      reduced.samples[static_cast<std::size_t>(i)].v;
  const std::vector<double>& sample_b =
      reduced.samples[static_cast<std::size_t>(j)].v;
  if (!IsValidShapeFlat(sample_a) || !IsValidShapeFlat(sample_b) ||
      sample_a.size() != sample_b.size() ||
      std::llround(sample_a[0]) != std::llround(sample_b[0]) ||
      std::llround(sample_a[1]) != std::llround(sample_b[1])) {
    return fit;
  }

  fit.endpoint_a = sample_a;
  fit.endpoint_b = sample_b;
  const int dims = static_cast<int>(sample_a.size());

  double s00 = 0.0;
  double s01 = 0.0;
  double s11 = 0.0;
  for (double u: progress) {
    const double one_minus_u = 1.0 - u;
    s00 += one_minus_u * one_minus_u;
    s01 += one_minus_u * u;
    s11 += u * u;
  }
  const double det = s00 * s11 - s01 * s01;
  if (!(std::abs(det) > 1e-12)) {
    return fit;
  }

  for (int dim = 2; dim < dims; ++dim) {
    double y0 = 0.0;
    double y1 = 0.0;
    for (int sample_idx = i; sample_idx <= j; ++sample_idx) {
      const std::vector<double>& v =
          reduced.samples[static_cast<std::size_t>(sample_idx)].v;
      if (static_cast<int>(v.size()) != dims ||
          std::llround(v[0]) != std::llround(sample_a[0]) ||
          std::llround(v[1]) != std::llround(sample_a[1])) {
        fit.ok = false;
        return fit;
      }
      const double u = progress[static_cast<std::size_t>(sample_idx - i)];
      const double one_minus_u = 1.0 - u;
      y0 += one_minus_u * v[static_cast<std::size_t>(dim)];
      y1 += u * v[static_cast<std::size_t>(dim)];
    }

    const double a = (y0 * s11 - y1 * s01) / det;
    const double b = (s00 * y1 - s01 * y0) / det;
    if (!std::isfinite(a) || !std::isfinite(b)) {
      fit.ok = false;
      return fit;
    }
    fit.endpoint_a[static_cast<std::size_t>(dim)] = a;
    fit.endpoint_b[static_cast<std::size_t>(dim)] = b;
  }

  fit.ok = true;
  return fit;
}

SegmentFitResult RelaxedResult(InterpType interp,
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

SegmentFitResult InfeasibleRelaxedSegment(std::string reason) {
  SegmentFitResult result;
  result.feasible = false;
  result.interp = InterpType::Linear;
  result.reason = std::move(reason);
  result.ease_out_at_i = NeutralShapeEase();
  result.ease_in_at_j = NeutralShapeEase();
  return result;
}

}  // namespace

std::vector<TemporalEase> ShapeEaseForInfluence(double influence) {
  return {TemporalEase{0.0, ClampAeInfluence(influence)}};
}

void AddReplacementFitAttribution(SegmentFitResult& dst,
                                  const SegmentFitResult& src) {
  dst.fit_replacement_oracle_calls += src.fit_replacement_oracle_calls;
  dst.fit_replacement_oracle_evaluations +=
      src.fit_replacement_oracle_evaluations;
  dst.fit_replacement_relaxed_attempts +=
      src.fit_replacement_relaxed_attempts;
  dst.fit_replacement_relaxed_validations +=
      src.fit_replacement_relaxed_validations;
  dst.fit_replacement_oracle_wall_ms +=
      src.fit_replacement_oracle_wall_ms;
  dst.fit_replacement_outline_wall_ms +=
      src.fit_replacement_outline_wall_ms;
  dst.fit_replacement_relaxed_wall_ms +=
      src.fit_replacement_relaxed_wall_ms;
}

std::vector<double> SegmentProgressValues(
    const PropertySamples& ps,
    int i,
    int j,
    bool bezier,
    TemporalEase ease_out,
    TemporalEase ease_in,
    const ShapeMorphProgressBandOptions& options) {
  std::vector<double> progress;
  progress.reserve(static_cast<std::size_t>(j - i + 1));
  const double t0 = ps.samples[static_cast<std::size_t>(i)].t_sec;
  const double t1 = ps.samples[static_cast<std::size_t>(j)].t_sec;
  for (int sample_idx = i; sample_idx <= j; ++sample_idx) {
    const double t = ps.samples[static_cast<std::size_t>(sample_idx)].t_sec;
    const double alpha =
        (t1 > t0) ? std::clamp((t - t0) / (t1 - t0), 0.0, 1.0): 0.0;
    progress.push_back(bezier
                           ? ShapeTemporalBezierProgress(alpha, ease_out,
                                                          ease_in, options)
: alpha);
  }
  return progress;
}

RelaxedValidation ValidateRelaxedChord(
    const PropertySamples& original,
    int i,
    int j,
    const std::vector<double>& endpoint_a,
    const std::vector<double>& endpoint_b,
    const std::vector<double>& progress,
    const ShapeMorphProgressBandOptions& options) {
  RelaxedValidation validation;
  if (!IsValidShapeFlat(endpoint_a) || !IsValidShapeFlat(endpoint_b) ||
      endpoint_a.size() != endpoint_b.size() ||
      progress.size() != static_cast<std::size_t>(j - i + 1)) {
    return validation;
  }

  const double tolerance =
      std::max(options.frame_fit_options.outline_tolerance, 0.0);
  double sum_sq = 0.0;
  int count = 0;
  for (int sample_idx = i; sample_idx <= j; ++sample_idx) {
    const Sample& source =
        original.samples[static_cast<std::size_t>(sample_idx)];
    if (!IsValidShapeFlat(source.v)) {
      return validation;
    }
    const std::vector<double> candidate =
        LerpShapeFlatChord(endpoint_a,
                           endpoint_b,
                           progress[static_cast<std::size_t>(sample_idx - i)]);
    const auto outline_start = std::chrono::steady_clock::now();
    const double err = ShapeFlatFrameOutlineError(
        source.v, candidate, options.frame_fit_options);
    const auto outline_end = std::chrono::steady_clock::now();
    validation.outline_wall_ms +=
        std::chrono::duration<double, std::milli>(
            outline_end - outline_start).count();
    ++validation.outline_checks;
    validation.max_err = std::max(validation.max_err, err);
    sum_sq += err * err;
    ++count;
  }
  validation.rms_err =
      count > 0 ? std::sqrt(sum_sq / static_cast<double>(count)): 0.0;
  validation.ok = validation.max_err <= tolerance + 1e-9;
  return validation;
}

SegmentFitResult TryRelaxedReplacementSegment(
    int i,
    int j,
    const PropertySamples& reduced,
    const PropertySamples& original,
    const SolverConfig& config,
    const ShapeMorphProgressBandOptions& band_options,
    const ShapeMorphProgressBandResult& strict_oracle) {
  SegmentFitResult metrics;
  if (config.allow_linear) {
    ++metrics.fit_replacement_relaxed_attempts;
    const auto attempt_start = std::chrono::steady_clock::now();
    const TemporalEase neutral = NeutralShapeEase().front();
    const std::vector<double> progress =
        SegmentProgressValues(reduced, i, j, false, neutral, neutral,
                              band_options);
    const RelaxedEndpointFit fit =
        FitRelaxedEndpointChord(reduced, i, j, progress);
    if (fit.ok) {
      ++metrics.fit_replacement_relaxed_validations;
      const RelaxedValidation validation =
          ValidateRelaxedChord(original, i, j, fit.endpoint_a, fit.endpoint_b,
                               progress, band_options);
      metrics.fit_replacement_outline_wall_ms += validation.outline_wall_ms;
      if (validation.ok) {
        metrics.fit_replacement_relaxed_wall_ms +=
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - attempt_start)
.count();
        SegmentFitResult result =
            RelaxedResult(InterpType::Linear,
                          "replacement_shape_morph_relaxed_linear_ok",
                          neutral,
                          neutral,
                          fit,
                          validation);
        AddReplacementFitAttribution(result, metrics);
        return result;
      }
    }
    metrics.fit_replacement_relaxed_wall_ms +=
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - attempt_start)
.count();
  }

  if (config.allow_bezier && config.allow_shape_temporal_bezier) {
    ++metrics.fit_replacement_relaxed_attempts;
    const auto default_attempt_start = std::chrono::steady_clock::now();
    const TemporalEase neutral = NeutralShapeEase().front();
    const std::vector<double> default_progress =
        SegmentProgressValues(reduced, i, j, true, neutral, neutral,
                              band_options);
    const RelaxedEndpointFit default_fit =
        FitRelaxedEndpointChord(reduced, i, j, default_progress);
    if (default_fit.ok) {
      ++metrics.fit_replacement_relaxed_validations;
      const RelaxedValidation validation =
          ValidateRelaxedChord(original, i, j,
                               default_fit.endpoint_a,
                               default_fit.endpoint_b,
                               default_progress,
                               band_options);
      metrics.fit_replacement_outline_wall_ms += validation.outline_wall_ms;
      if (validation.ok) {
        metrics.fit_replacement_relaxed_wall_ms +=
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - default_attempt_start)
.count();
        SegmentFitResult result =
            RelaxedResult(InterpType::Bezier,
                          "replacement_shape_morph_relaxed_bezier_ok",
                          neutral,
                          neutral,
                          default_fit,
                          validation);
        AddReplacementFitAttribution(result, metrics);
        return result;
      }
    }
    metrics.fit_replacement_relaxed_wall_ms +=
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - default_attempt_start)
.count();

    if (strict_oracle.fitted_bezier_pairs_tried > 0 &&
        std::isfinite(strict_oracle.max_fitted_bezier_error)) {
      ++metrics.fit_replacement_relaxed_attempts;
      const auto fitted_attempt_start = std::chrono::steady_clock::now();
      const TemporalEase ease_out{0.0,
                                  strict_oracle.fitted_bezier_out_influence};
      const TemporalEase ease_in{0.0,
                                 strict_oracle.fitted_bezier_in_influence};
      const std::vector<double> fitted_progress =
          SegmentProgressValues(reduced, i, j, true, ease_out, ease_in,
                                band_options);
      const RelaxedEndpointFit fitted_fit =
          FitRelaxedEndpointChord(reduced, i, j, fitted_progress);
      if (fitted_fit.ok) {
        ++metrics.fit_replacement_relaxed_validations;
        const RelaxedValidation validation =
            ValidateRelaxedChord(original, i, j,
                                 fitted_fit.endpoint_a,
                                 fitted_fit.endpoint_b,
                                 fitted_progress,
                                 band_options);
        metrics.fit_replacement_outline_wall_ms += validation.outline_wall_ms;
        if (validation.ok) {
          metrics.fit_replacement_relaxed_wall_ms +=
              std::chrono::duration<double, std::milli>(
                  std::chrono::steady_clock::now() - fitted_attempt_start)
.count();
          SegmentFitResult result =
              RelaxedResult(InterpType::Bezier,
                            "replacement_shape_morph_relaxed_bezier_fit_ok",
                            ease_out,
                            ease_in,
                            fitted_fit,
                            validation);
          AddReplacementFitAttribution(result, metrics);
          return result;
        }
      }
      metrics.fit_replacement_relaxed_wall_ms +=
          std::chrono::duration<double, std::milli>(
              std::chrono::steady_clock::now() - fitted_attempt_start)
.count();
    }
  }

  SegmentFitResult result =
      InfeasibleRelaxedSegment("infeasible_shape_morph_relaxed_chord");
  AddReplacementFitAttribution(result, metrics);
  return result;
}

}  // namespace replacement_temporal
}  // namespace bbsolver
