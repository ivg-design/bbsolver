#include "bbsolver/fit/segment_fit_policy.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/dp/dp_placer.hpp"
#include "env_test_support.hpp"
#include "bbsolver/metrics/error_metrics.hpp"

#include <cassert>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

bbsolver::PropertySamples MakeProperty(int dimensions,
                                       bool spatial = false,
                                       bool separated = false) {
  bbsolver::PropertySamples ps;
  ps.property.dimensions = dimensions;
  ps.property.is_spatial = spatial;
  ps.property.is_separated = separated;
  ps.property.kind = spatial ? bbsolver::ValueKind::TwoD_Spatial
: bbsolver::ValueKind::Scalar;
  return ps;
}

bbsolver::SolverConfig Config() {
  bbsolver::SolverConfig cfg;
  cfg.tolerance = 2.0;
  cfg.tolerance_screen_px = 0.0;
  cfg.weight_pos = 4.0;
  cfg.weight_screen = 0.0;
  cfg.shape_temporal_bezier_attempt_threshold_ratio = -1.0;
  return cfg;
}

// W5: EnvVarGuard consolidated into solver/tests/solver_unit/env_test_support.hpp.
// The two-arg ctor `(name, value)` matches the previous EnvVarGuard
// shape (nullptr value means capture + unset).
using EnvVarGuard = bbsolver::test_support::ScopedEnv;

void SetAllAttributionFields(bbsolver::SegmentFitResult& result, int base) {
  result.fit_segment_hold_attempts = base + 1;
  result.fit_segment_linear_attempts = base + 2;
  result.fit_segment_hold_units_evaluated = base + 3;
  result.fit_segment_linear_units_evaluated = base + 4;
  result.fit_segment_hold_fail_fast_exits = base + 5;
  result.fit_segment_linear_fail_fast_exits = base + 6;
  result.fit_shape_temporal_attempts = base + 7;
  result.fit_shape_temporal_gate_rejections = base + 8;
  result.fit_shape_temporal_outline_evaluations = base + 9;
  result.fit_segment_hold_wall_ms = base + 10.0;
  result.fit_segment_linear_wall_ms = base + 11.0;
  result.fit_segment_hold_shape_outline_wall_ms = base + 12.0;
  result.fit_segment_linear_shape_outline_wall_ms = base + 13.0;
  result.fit_shape_temporal_ceres_wall_ms = base + 14.0;
  result.fit_shape_temporal_outline_wall_ms = base + 15.0;
  result.fit_shape_temporal_total_wall_ms = base + 16.0;
}

}  // namespace

int main() {
  namespace sf = bbsolver::segment_fit;

  {
    bbsolver::SolverConfig cfg = Config();
    assert(std::abs(sf::AcceptancePropertyCeiling(cfg) - 1.98) < 1e-12);
    assert(std::isinf(sf::AcceptanceScreenCeiling(cfg)));
    cfg.weight_screen = 1.0;
    assert(sf::AcceptanceScreenCeiling(cfg) == cfg.tolerance);
    cfg.tolerance_screen_px = 3.0;
    assert(sf::AcceptanceScreenCeiling(cfg) == 3.0);
  }

  {
    bbsolver::PropertySamples ps = MakeProperty(2, true);
    assert(sf::ScreenScaleForDim(ps, 0) == 0.0);
    ps.layer_xform_at_start = bbsolver::LayerXform{};
    assert(sf::ScreenScaleForDim(ps, 0) == 1.0);
    ps.layer_xform_at_start->scale = {200.0, -50.0};
    assert(sf::ScreenScaleForDim(ps, 0) == 2.0);
    assert(sf::ScreenScaleForDim(ps, 1) == 0.5);
    assert(sf::ScreenScaleForDim(ps, 2) == 0.0);

    bbsolver::SolverConfig cfg = Config();
    cfg.weight_pos = 4.0;
    assert(sf::ResidualWeightForDim(ps, cfg, 0) == 2.0);
    cfg.weight_screen = 3.0;
    assert(std::abs(sf::ResidualWeightForDim(ps, cfg, 0) - 4.0) < 1e-12);
  }

  {
    EnvVarGuard no_env("BBSOLVER_BEZIER_GATE_RATIO", nullptr);
    bbsolver::SolverConfig cfg = Config();
    assert(sf::ShapeTemporalBezierGateRatio(cfg) == -1.0);
  }

  {
    EnvVarGuard primary("BBSOLVER_BEZIER_GATE_RATIO", "not-a-number");
    bbsolver::SolverConfig cfg = Config();
    assert(sf::ShapeTemporalBezierGateRatio(cfg) == -1.0);
  }

  {
    bbsolver::SegmentFitResult dst;
    bbsolver::SegmentFitResult src;
    SetAllAttributionFields(dst, 10);
    SetAllAttributionFields(src, 100);
    sf::AddFitAttribution(dst, src);
    assert(dst.fit_segment_hold_attempts == 112);
    assert(dst.fit_segment_linear_attempts == 114);
    assert(dst.fit_segment_hold_units_evaluated == 116);
    assert(dst.fit_segment_linear_units_evaluated == 118);
    assert(dst.fit_segment_hold_fail_fast_exits == 120);
    assert(dst.fit_segment_linear_fail_fast_exits == 122);
    assert(dst.fit_shape_temporal_attempts == 124);
    assert(dst.fit_shape_temporal_gate_rejections == 126);
    assert(dst.fit_shape_temporal_outline_evaluations == 128);
    assert(dst.fit_segment_hold_wall_ms == 130.0);
    assert(dst.fit_segment_linear_wall_ms == 132.0);
    assert(dst.fit_segment_hold_shape_outline_wall_ms == 134.0);
    assert(dst.fit_segment_linear_shape_outline_wall_ms == 136.0);
    assert(dst.fit_shape_temporal_ceres_wall_ms == 138.0);
    assert(dst.fit_shape_temporal_outline_wall_ms == 140.0);
    assert(dst.fit_shape_temporal_total_wall_ms == 142.0);
  }

  {
    bbsolver::SegmentFitResult result;
    bbsolver::ErrorReport report;
    report.max_err = 1.0;
    report.max_err_screen_px = 2.0;
    report.rms_err = 3.0;
    sf::CopyError(result, report);
    assert(result.max_err == 1.0);
    assert(result.max_err_screen_px == 2.0);
    assert(result.rms_err == 3.0);

    report.units_evaluated = 7;
    report.fail_fast_exit = true;
    report.shape_outline_wall_ms = 1.25;
    sf::AddOrdinaryHoldAttribution(result, report);
    sf::AddOrdinaryLinearAttribution(result, report);
    assert(result.fit_segment_hold_units_evaluated == 7);
    assert(result.fit_segment_hold_fail_fast_exits == 1);
    assert(result.fit_segment_hold_shape_outline_wall_ms == 1.25);
    assert(result.fit_segment_linear_units_evaluated == 7);
    assert(result.fit_segment_linear_fail_fast_exits == 1);
    assert(result.fit_segment_linear_shape_outline_wall_ms == 1.25);
  }

  {
    bbsolver::PropertySamples ps = MakeProperty(3, true, true);
    bbsolver::ErrorReport report;
    report.max_err = 0.5;
    report.max_err_screen_px = 0.25;
    report.rms_err = 0.125;
    bbsolver::SegmentFitResult result = sf::ResultFromReport(
        bbsolver::InterpType::Linear, report, ps, "linear");
    assert(result.feasible);
    assert(result.interp == bbsolver::InterpType::Linear);
    assert(result.reason == "linear");
    assert(result.ease_out_at_i.size() == 3);
    assert(result.ease_in_at_j.size() == 3);
    assert(result.spatial_out_at_i == std::vector<double>({0.0, 0.0, 0.0}));
    assert(result.spatial_in_at_j == std::vector<double>({0.0, 0.0, 0.0}));
    assert(result.max_err == report.max_err);
  }

  {
    bbsolver::SegmentFitResult result;
    result.reason = "candidate";
    bbsolver::SegmentFitResult attribution;
    attribution.fit_segment_hold_attempts = 2;
    const bbsolver::SegmentFitResult combined =
        sf::WithFitAttribution(result, attribution);
    assert(combined.reason == "candidate");
    assert(combined.fit_segment_hold_attempts == 2);
  }

  return 0;
}
