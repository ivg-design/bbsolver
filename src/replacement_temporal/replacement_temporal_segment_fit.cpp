#include "bbsolver/replacement_temporal/replacement_temporal_segment_fit.hpp"

#include "bbsolver/domain.hpp"
#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"
#include "bbsolver/replacement_temporal/replacement_temporal_keys.hpp"
#include "bbsolver/replacement_temporal/replacement_temporal_relaxed_fit.hpp"

#include <chrono>
#include <cstddef>
#include <ratio>
#include <string>
#include <utility>

namespace bbsolver {
namespace replacement_temporal {

SegmentFitResult InfeasibleSegment(std::string reason) {
  SegmentFitResult result;
  result.feasible = false;
  result.interp = InterpType::Linear;
  result.reason = std::move(reason);
  result.ease_out_at_i = NeutralShapeEase();
  result.ease_in_at_j = NeutralShapeEase();
  return result;
}

SegmentFitResult FitReplacementSegment(
    int i,
    int j,
    const PropertySamples& reduced,
    const PropertySamples& original,
    const SolverConfig& config,
    const ShapeMorphProgressBandOptions& band_options,
    bool allow_relaxed_endpoint_fit) {
  if (!config.allow_linear && !config.allow_bezier) {
    return InfeasibleSegment("replacement_temporal_no_interpolation_enabled");
  }
  if (i < 0 || j <= i ||
      j >= static_cast<int>(reduced.samples.size()) ||
      j >= static_cast<int>(original.samples.size())) {
    return InfeasibleSegment("replacement_temporal_invalid_segment");
  }

  SegmentFitResult result;
  const auto oracle_start = std::chrono::steady_clock::now();
  const ShapeMorphProgressBandResult oracle =
      EvaluateShapeFlatMorphProgressBands(
          original,
          i,
          j,
          reduced.samples[static_cast<std::size_t>(i)].v,
          reduced.samples[static_cast<std::size_t>(j)].v,
          band_options);
  const auto oracle_end = std::chrono::steady_clock::now();

  result.fit_replacement_oracle_calls = 1;
  result.fit_replacement_oracle_evaluations = oracle.evaluations;
  result.fit_replacement_oracle_wall_ms =
      std::chrono::duration<double, std::milli>(
          oracle_end - oracle_start).count();
  result.fit_replacement_outline_wall_ms = oracle.outline_error_wall_ms;
  result.ease_out_at_i = NeutralShapeEase();
  result.ease_in_at_j = NeutralShapeEase();
  result.max_err = oracle.max_best_error;
  result.max_err_screen_px = oracle.max_best_error;
  result.rms_err = oracle.max_best_error;

  if (!oracle.ok) {
    result.feasible = false;
    result.interp = InterpType::Linear;
    result.reason = oracle.reason.empty()
                        ? "replacement_temporal_oracle_failed"
                        : oracle.reason;
    return result;
  }

  if (config.allow_linear && oracle.linear_progress_possible) {
    result.feasible = true;
    result.interp = InterpType::Linear;
    result.max_err = oracle.max_linear_error;
    result.max_err_screen_px = oracle.max_linear_error;
    result.rms_err = oracle.max_linear_error;
    result.reason = "replacement_shape_morph_linear_ok";
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
    result.ease_out_at_i = ShapeEaseForInfluence(
        use_fitted ? oracle.fitted_bezier_out_influence : 33.3);
    result.ease_in_at_j = ShapeEaseForInfluence(
        use_fitted ? oracle.fitted_bezier_in_influence : 33.3);
    result.reason = use_fitted ? "replacement_shape_morph_bezier_fit_ok"
                               : "replacement_shape_morph_bezier_ok";
    return result;
  }

  if (allow_relaxed_endpoint_fit) {
    SegmentFitResult relaxed =
        TryRelaxedReplacementSegment(i, j, reduced, original, config,
                                     band_options, oracle);
    if (relaxed.feasible) {
      AddReplacementFitAttribution(relaxed, result);
      return relaxed;
    }
    AddReplacementFitAttribution(result, relaxed);
  }

  result.feasible = false;
  result.interp = InterpType::Linear;
  result.reason = oracle.monotone_progress_possible
                      ? "infeasible_shape_morph_timing"
                      : "infeasible_shape_morph_chord";
  return result;
}

}  // namespace replacement_temporal
}  // namespace bbsolver
