#include "bbsolver/fit/segment_fit_policy.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <utility>
#include <cstddef>
#include <string>

#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/metrics/error_metrics.hpp"
#include "bbsolver/fit/segment_fit_samples.hpp"

namespace bbsolver::segment_fit {

bool Passes(const ErrorReport& report, const SolverConfig& cfg) {
  const bool property_ok = report.max_err <= cfg.tolerance * kAcceptanceHeadroom;
  const double screen_tolerance =
      cfg.tolerance_screen_px > 0.0 ? cfg.tolerance_screen_px: cfg.tolerance;
  const bool screen_ok =
      (cfg.tolerance_screen_px <= 0.0 && cfg.weight_screen <= 0.0) ||
      report.max_err_screen_px <= screen_tolerance;
  return property_ok && screen_ok;
}

double ShapeTemporalBezierGateRatio(const SolverConfig& cfg) {
  if (std::isfinite(cfg.shape_temporal_bezier_attempt_threshold_ratio) &&
      cfg.shape_temporal_bezier_attempt_threshold_ratio >= 0.0) {
    return cfg.shape_temporal_bezier_attempt_threshold_ratio;
  }
  const char* value = std::getenv("BBSOLVER_BEZIER_GATE_RATIO");
  if (value != nullptr && *value != '\0') {
    char* end = nullptr;
    const double parsed = std::strtod(value, &end);
    if (end != value && std::isfinite(parsed) && parsed >= 0.0) {
      return parsed;
    }
  }
  return -1.0;
}

bool ShapeTemporalBezierGateAllows(const SegmentFitResult& linear_miss,
                                   const SolverConfig& cfg) {
  const double gate_ratio = ShapeTemporalBezierGateRatio(cfg);
  if (gate_ratio < 0.0) {
    return true;
  }
  if (!std::isfinite(linear_miss.max_err)) {
    return false;
  }
  const double threshold = std::max(0.0, cfg.tolerance) * gate_ratio;
  return linear_miss.max_err <= threshold;
}

double AcceptancePropertyCeiling(const SolverConfig& cfg) {
  return cfg.tolerance * kAcceptanceHeadroom;
}

double AcceptanceScreenCeiling(const SolverConfig& cfg) {
  if (cfg.tolerance_screen_px <= 0.0 && cfg.weight_screen <= 0.0) {
    return std::numeric_limits<double>::infinity();
  }
  return cfg.tolerance_screen_px > 0.0 ? cfg.tolerance_screen_px: cfg.tolerance;
}

double LinearFailFastPropertyCeiling(const PropertySamples& ps,
                                     const SolverConfig& cfg) {
  double ceiling = AcceptancePropertyCeiling(cfg);
  if (cfg.allow_bezier && cfg.allow_shape_temporal_bezier &&
      IsShapeFlatPath(ps)) {
    const double gate_ratio = ShapeTemporalBezierGateRatio(cfg);
    if (gate_ratio >= 0.0) {
      ceiling = std::max(ceiling, std::max(0.0, cfg.tolerance) * gate_ratio);
    }
  }
  return ceiling;
}

double LinearFailFastScreenCeiling(const PropertySamples& ps,
                                   const SolverConfig& cfg) {
  if (cfg.allow_bezier && cfg.allow_shape_temporal_bezier &&
      IsShapeFlatPath(ps) && ShapeTemporalBezierGateRatio(cfg) >= 0.0) {
    return std::numeric_limits<double>::infinity();
  }
  return AcceptanceScreenCeiling(cfg);
}

double ScreenScaleForDim(const PropertySamples& ps, int dim) {
  if (!ps.layer_xform_at_start || dim > 1) {
    return 0.0;
  }
  const auto& scale = ps.layer_xform_at_start->scale;
  if (scale.empty()) {
    return 1.0;
  }
  if (dim == 1 && scale.size() >= 2) {
    return std::abs(ComponentOrZero(scale, 1) / 100.0);
  }
  return std::abs(ComponentOrZero(scale, 0) / 100.0);
}

double ResidualWeightForDim(const PropertySamples& ps,
                            const SolverConfig& cfg,
                            int dim) {
  double weight = std::max(cfg.weight_pos, 1e-12);
  if (cfg.weight_screen > 0.0) {
    const double screen_scale = ScreenScaleForDim(ps, dim);
    weight += cfg.weight_screen * screen_scale * screen_scale;
  }
  return std::sqrt(weight);
}

void AddFitAttribution(SegmentFitResult& dst, const SegmentFitResult& src) {
  dst.fit_segment_hold_attempts += src.fit_segment_hold_attempts;
  dst.fit_segment_linear_attempts += src.fit_segment_linear_attempts;
  dst.fit_segment_hold_units_evaluated +=
      src.fit_segment_hold_units_evaluated;
  dst.fit_segment_linear_units_evaluated +=
      src.fit_segment_linear_units_evaluated;
  dst.fit_segment_hold_fail_fast_exits +=
      src.fit_segment_hold_fail_fast_exits;
  dst.fit_segment_linear_fail_fast_exits +=
      src.fit_segment_linear_fail_fast_exits;
  dst.fit_shape_temporal_attempts += src.fit_shape_temporal_attempts;
  dst.fit_shape_temporal_gate_rejections +=
      src.fit_shape_temporal_gate_rejections;
  dst.fit_shape_temporal_outline_evaluations +=
      src.fit_shape_temporal_outline_evaluations;
  dst.fit_segment_hold_wall_ms += src.fit_segment_hold_wall_ms;
  dst.fit_segment_linear_wall_ms += src.fit_segment_linear_wall_ms;
  dst.fit_segment_hold_shape_outline_wall_ms +=
      src.fit_segment_hold_shape_outline_wall_ms;
  dst.fit_segment_linear_shape_outline_wall_ms +=
      src.fit_segment_linear_shape_outline_wall_ms;
  dst.fit_shape_temporal_ceres_wall_ms += src.fit_shape_temporal_ceres_wall_ms;
  dst.fit_shape_temporal_outline_wall_ms +=
      src.fit_shape_temporal_outline_wall_ms;
  dst.fit_shape_temporal_total_wall_ms += src.fit_shape_temporal_total_wall_ms;
}

SegmentFitResult WithFitAttribution(SegmentFitResult result,
                                    const SegmentFitResult& attribution) {
  AddFitAttribution(result, attribution);
  return result;
}

void CopyError(SegmentFitResult& result, const ErrorReport& report) {
  result.max_err = report.max_err;
  result.max_err_screen_px = report.max_err_screen_px;
  result.rms_err = report.rms_err;
}

void AddOrdinaryHoldAttribution(SegmentFitResult& attribution,
                                const ErrorReport& report) {
  attribution.fit_segment_hold_units_evaluated += report.units_evaluated;
  attribution.fit_segment_hold_fail_fast_exits +=
      report.fail_fast_exit ? 1: 0;
  attribution.fit_segment_hold_shape_outline_wall_ms +=
      report.shape_outline_wall_ms;
}

void AddOrdinaryLinearAttribution(SegmentFitResult& attribution,
                                  const ErrorReport& report) {
  attribution.fit_segment_linear_units_evaluated += report.units_evaluated;
  attribution.fit_segment_linear_fail_fast_exits +=
      report.fail_fast_exit ? 1: 0;
  attribution.fit_segment_linear_shape_outline_wall_ms +=
      report.shape_outline_wall_ms;
}

SegmentFitResult ResultFromReport(InterpType interp,
                                  const ErrorReport& report,
                                  const PropertySamples& ps,
                                  std::string reason) {
  SegmentFitResult result;
  result.feasible = true;
  result.interp = interp;
  result.ease_out_at_i = DefaultEases(TemporalChannels(ps));
  result.ease_in_at_j = DefaultEases(TemporalChannels(ps));
  if (ps.property.is_spatial) {
    result.spatial_out_at_i.assign(static_cast<std::size_t>(Dimensions(ps)),
                                   0.0);
    result.spatial_in_at_j.assign(static_cast<std::size_t>(Dimensions(ps)),
                                  0.0);
  }
  result.reason = std::move(reason);
  CopyError(result, report);
  return result;
}

}  // namespace bbsolver::segment_fit
