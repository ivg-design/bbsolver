#include "bbsolver/replacement_temporal/replacement_temporal_anchor_prune_fit.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/path/temporal/path_temporal_validation.hpp"
#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/replacement_temporal/replacement_temporal_forward_span.hpp"
#include "bbsolver/replacement_temporal/replacement_temporal_keys.hpp"
#include "bbsolver/replacement_temporal/replacement_temporal_segment_fit.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>
#include <cstddef>

namespace bbsolver {
namespace replacement_temporal {

bool SampleValuesEqualWithin(const std::vector<double>& a,
                             const std::vector<double>& b,
                             double epsilon) {
  if (a.size() != b.size()) {
    return false;
  }
  for (std::size_t idx = 0; idx < a.size(); ++idx) {
    if (std::abs(a[idx] - b[idx]) > epsilon) {
      return false;
    }
  }
  return true;
}

RelaxedEndpointFit FitAnchoredEndpointChord(
    const PropertySamples& reduced,
    int i,
    int j,
    const std::vector<double>& endpoint_a,
    const std::vector<double>& progress) {
  RelaxedEndpointFit fit;
  if (i < 0 || j <= i ||
      j >= static_cast<int>(reduced.samples.size()) ||
      progress.size() != static_cast<std::size_t>(j - i + 1) ||
      !IsValidShapeFlat(endpoint_a)) {
    return fit;
  }

  const int dims = static_cast<int>(endpoint_a.size());
  const int closed = static_cast<int>(std::llround(endpoint_a[0]));
  const int vertex_count = static_cast<int>(std::llround(endpoint_a[1]));
  fit.endpoint_a = endpoint_a;
  fit.endpoint_b = endpoint_a;

  double denom = 0.0;
  for (double u: progress) {
    denom += u * u;
  }
  if (!(denom > 1e-12)) {
    return fit;
  }

  for (int sample_idx = i; sample_idx <= j; ++sample_idx) {
    const std::vector<double>& v =
        reduced.samples[static_cast<std::size_t>(sample_idx)].v;
    if (static_cast<int>(v.size()) != dims ||
        static_cast<int>(std::llround(v[0])) != closed ||
        static_cast<int>(std::llround(v[1])) != vertex_count) {
      return fit;
    }
  }

  for (int dim = 2; dim < dims; ++dim) {
    double numer = 0.0;
    for (int sample_idx = i; sample_idx <= j; ++sample_idx) {
      const double u = progress[static_cast<std::size_t>(sample_idx - i)];
      const double one_minus_u = 1.0 - u;
      const std::vector<double>& v =
          reduced.samples[static_cast<std::size_t>(sample_idx)].v;
      numer += u * (v[static_cast<std::size_t>(dim)] -
                    one_minus_u * endpoint_a[static_cast<std::size_t>(dim)]);
    }
    const double b = numer / denom;
    if (!std::isfinite(b)) {
      return fit;
    }
    fit.endpoint_b[static_cast<std::size_t>(dim)] = b;
  }

  fit.ok = true;
  return fit;
}

SegmentFitResult FitAnchoredFallbackPruneSegmentWithEase(
    int i,
    int j,
    const std::vector<double>& endpoint_a,
    const PropertySamples& reduced,
    const PropertySamples& original,
    const ShapeMorphProgressBandOptions& band_options,
    InterpType interp,
    TemporalEase ease_out,
    TemporalEase ease_in,
    const std::string& feasible_reason,
    const std::string& infeasible_reason) {
  SegmentFitResult result = InfeasibleSegment(infeasible_reason);
  const std::vector<double> progress =
      SegmentProgressValues(reduced,
                                i,
                                j,
                                interp == InterpType::Bezier,
                                ease_out,
                                ease_in,
                                band_options);
  const RelaxedEndpointFit fit =
      FitAnchoredEndpointChord(reduced, i, j, endpoint_a, progress);
  if (!fit.ok) {
    return result;
  }
  const RelaxedValidation validation =
      ValidateRelaxedChord(original,
                               i,
                               j,
                               fit.endpoint_a,
                               fit.endpoint_b,
                               progress,
                               band_options);
  if (!validation.ok) {
    result.max_err = validation.max_err;
    result.max_err_screen_px = validation.max_err;
    result.rms_err = validation.rms_err;
    return result;
  }
  result.feasible = true;
  result.interp = interp;
  result.ease_out_at_i = {ease_out};
  result.ease_in_at_j = {ease_in};
  result.key_value_at_i = fit.endpoint_a;
  result.key_value_at_j = fit.endpoint_b;
  result.max_err = validation.max_err;
  result.max_err_screen_px = validation.max_err;
  result.rms_err = validation.rms_err;
  result.iters = static_cast<int>(progress.size());
  result.reason = feasible_reason;
  return result;
}

SegmentFitResult FitAnchoredFallbackLinearPruneSegment(
    int i,
    int j,
    const std::vector<double>& endpoint_a,
    const PropertySamples& reduced,
    const PropertySamples& original,
    const ShapeMorphProgressBandOptions& band_options) {
  const TemporalEase neutral = NeutralShapeEase().front();
  return FitAnchoredFallbackPruneSegmentWithEase(
      i,
      j,
      endpoint_a,
      reduced,
      original,
      band_options,
      InterpType::Linear,
      neutral,
      neutral,
      "exact_anchor_fallback_anchored_linear_prune",
      "infeasible_exact_anchor_fallback_anchored_linear_prune");
}

SegmentFitResult FitAnchoredFallbackBezierPruneSegment(
    int i,
    int j,
    const std::vector<double>& endpoint_a,
    const PropertySamples& reduced,
    const PropertySamples& original,
    const ShapeMorphProgressBandOptions& band_options) {
  if (!band_options.fit_bezier_influence_pairs) {
    return InfeasibleSegment(
        "infeasible_exact_anchor_fallback_anchored_bezier_prune_disabled");
  }
  std::vector<std::pair<double, double>> pairs = {
      {33.3, 33.3},
      {90.0, 10.0},
      {10.0, 90.0},
      {80.0, 20.0},
      {20.0, 80.0},
      {66.0, 33.0},
      {33.0, 66.0},
  };
  const int pair_budget =
      std::min(3,
               std::min(static_cast<int>(pairs.size()),
                        std::max(1, band_options.max_bezier_influence_pairs)));
  SegmentFitResult best = InfeasibleSegment(
      "infeasible_exact_anchor_fallback_anchored_bezier_prune");
  for (int idx = 0; idx < pair_budget; ++idx) {
    const TemporalEase ease_out{0.0, pairs[static_cast<std::size_t>(idx)].first};
    const TemporalEase ease_in{0.0, pairs[static_cast<std::size_t>(idx)].second};
    SegmentFitResult candidate = FitAnchoredFallbackPruneSegmentWithEase(
        i,
        j,
        endpoint_a,
        reduced,
        original,
        band_options,
        InterpType::Bezier,
        ease_out,
        ease_in,
        "exact_anchor_fallback_anchored_bezier_prune",
        "infeasible_exact_anchor_fallback_anchored_bezier_prune");
    if (candidate.feasible &&
        (!best.feasible || candidate.max_err + 1e-12 < best.max_err)) {
      best = std::move(candidate);
    }
  }
  return best;
}

SegmentFitResult FitFallbackLinearPruneSegment(
    int i,
    int j,
    const std::vector<double>& current_anchor_value,
    const PropertySamples& reduced,
    const PropertySamples& original,
    const ShapeMorphProgressBandOptions& band_options) {
  if (SampleValuesEqualWithin(
          current_anchor_value,
          reduced.samples[static_cast<std::size_t>(i)].v,
          1e-7)) {
    SegmentFitResult strict = FitForwardLongestSpanLinearSegment(
        i, j, reduced, original, band_options.frame_fit_options);
    if (strict.feasible) {
      return strict;
    }
  }
  SegmentFitResult anchored_linear = FitAnchoredFallbackLinearPruneSegment(
      i, j, current_anchor_value, reduced, original, band_options);
  if (anchored_linear.feasible) {
    return anchored_linear;
  }
  return FitAnchoredFallbackBezierPruneSegment(
      i, j, current_anchor_value, reduced, original, band_options);
}

}  // namespace replacement_temporal
}  // namespace bbsolver
