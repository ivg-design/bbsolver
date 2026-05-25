#include "bbsolver/fit/segment_fitter.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/metrics/ae_curve.hpp"
#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/metrics/unified_spatial.hpp"
#include "bbsolver/verify/verifier.hpp"
#include "env_test_support.hpp"
#include "bbsolver/metrics/error_metrics.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>
#include <cstddef>
#include <utility>

namespace {

constexpr double kPi = 3.14159265358979323846;

bbsolver::PropertySamples MakeProperty(const std::vector<std::vector<double>>& values,
                                       double fps,
                                       int dimensions,
                                       bool spatial = false,
                                       bool separated = false) {
  bbsolver::PropertySamples ps;
  ps.property.id = "unit/property";
  ps.property.dimensions = dimensions;
  ps.property.is_spatial = spatial;
  ps.property.is_separated = separated;
  if (spatial) {
    ps.property.kind = dimensions == 3 ? bbsolver::ValueKind::ThreeD_Spatial
                                       : bbsolver::ValueKind::TwoD_Spatial;
  } else if (dimensions == 2) {
    ps.property.kind = bbsolver::ValueKind::TwoD;
  } else if (dimensions == 3) {
    ps.property.kind = bbsolver::ValueKind::ThreeD;
  } else {
    ps.property.kind = bbsolver::ValueKind::Scalar;
  }
  ps.t_start_sec = 0.0;
  ps.t_end_sec = (values.empty() || fps <= 0.0) ? 0.0 : (static_cast<double>(values.size() - 1) / fps);
  ps.samples_per_frame = 1;
  for (std::size_t i = 0; i < values.size(); ++i) {
    bbsolver::Sample sample;
    sample.t_sec = static_cast<double>(i) / fps;
    sample.v = values[i];
    ps.samples.push_back(sample);
  }
  return ps;
}

bbsolver::PropertySamples MakeShapeFlatProperty(const std::vector<std::vector<double>>& values,
                                                double fps,
                                                int dimensions) {
  bbsolver::PropertySamples ps = MakeProperty(values, fps, dimensions);
  ps.property.kind = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  return ps;
}

bbsolver::SolverConfig Config(double tolerance) {
  bbsolver::SolverConfig cfg;
  cfg.tolerance = tolerance;
  cfg.max_iters_per_segment = 200;
  return cfg;
}

bbsolver::CompInfo Comp(double fps) {
  bbsolver::CompInfo comp;
  comp.fps = fps;
  return comp;
}

std::vector<double> ShapeFlatRect(double x, double y, double w, double h) {
  const std::vector<std::pair<double, double>> points = {
      {x, y},
      {x + w, y},
      {x + w, y + h},
      {x, y + h},
  };
  std::vector<double> flat;
  flat.reserve(2 + points.size() * 6);
  flat.push_back(1.0);
  flat.push_back(static_cast<double>(points.size()));
  for (const auto& point : points) {
    flat.push_back(point.first);
    flat.push_back(point.second);
    flat.push_back(0.0);
    flat.push_back(0.0);
    flat.push_back(0.0);
    flat.push_back(0.0);
  }
  return flat;
}

std::vector<double> LerpFlat(const std::vector<double>& a,
                             const std::vector<double>& b,
                             double u) {
  std::vector<double> out(a.size(), 0.0);
  for (std::size_t i = 0; i < out.size(); ++i) {
    out[i] = a[i] + (b[i] - a[i]) * u;
  }
  return out;
}

bbsolver::Key ShapeKey(double t,
                       const std::vector<double>& value,
                       bbsolver::InterpType interp,
                       bbsolver::TemporalEase ease_in,
                       bbsolver::TemporalEase ease_out) {
  bbsolver::Key key;
  key.t_sec = t;
  key.v = value;
  key.interp_in = interp;
  key.interp_out = interp;
  key.temporal_ease_in = {ease_in};
  key.temporal_ease_out = {ease_out};
  return key;
}

// W5: EnvVarGuard consolidated into solver/tests/solver_unit/env_test_support.hpp
// as ScopedEnv (the `(name, value)` two-arg ctor matches the previous
// EnvVarGuard shape; `nullptr` value means "capture + unset"). The
// portable helper uses _putenv_s / _dupenv_s on MSVC and setenv /
// unsetenv on POSIX.
using EnvVarGuard = bbsolver::test_support::ScopedEnv;

}  // namespace

int main() {
  {
    const bbsolver::PropertySamples ps = MakeProperty({{3.0}, {3.0}, {3.0}}, 24.0, 1);
    const bbsolver::SegmentFitResult fit = bbsolver::FitSegment(0, 2, ps, Config(1e-9), Comp(24.0));
    assert(fit.feasible);
    assert(fit.interp == bbsolver::InterpType::Hold);
    assert(fit.fit_segment_hold_attempts == 1);
    assert(fit.fit_segment_linear_attempts == 0);
    assert(fit.fit_segment_hold_units_evaluated == 3);
    assert(fit.fit_segment_hold_fail_fast_exits == 0);
    assert(fit.fit_segment_hold_shape_outline_wall_ms == 0.0);
    assert(fit.fit_segment_hold_wall_ms >= 0.0);
  }

  {
    const bbsolver::PropertySamples ps = MakeProperty({{0.0}, {5.0}, {10.0}}, 24.0, 1);
    const bbsolver::SegmentFitResult fit = bbsolver::FitSegment(0, 2, ps, Config(1e-9), Comp(24.0));
    assert(fit.feasible);
    assert(fit.interp == bbsolver::InterpType::Linear);
    assert(fit.fit_segment_hold_attempts == 1);
    assert(fit.fit_segment_linear_attempts == 1);
    assert(fit.fit_segment_hold_units_evaluated > 0);
    assert(fit.fit_segment_linear_units_evaluated == 3);
    assert(fit.fit_segment_linear_fail_fast_exits == 0);
    assert(fit.fit_segment_linear_shape_outline_wall_ms == 0.0);
    assert(fit.fit_segment_linear_wall_ms >= 0.0);
  }

  {
    bbsolver::PropertySamples ps = MakeProperty({{0.0}, {10.0}, {0.0}}, 24.0, 1);
    bbsolver::SolverConfig cfg = Config(1.0);
    cfg.allow_bezier = false;
    const bbsolver::SegmentFitResult fit = bbsolver::FitSegment(0, 2, ps, cfg, Comp(24.0));
    assert(!fit.feasible);
    assert(fit.reason == "infeasible_hold");
    assert(fit.fit_segment_hold_attempts == 1);
    assert(fit.fit_segment_linear_attempts == 1);
    assert(fit.fit_segment_hold_units_evaluated > 0);
    assert(fit.fit_segment_linear_units_evaluated > 0);
    assert(fit.fit_segment_hold_fail_fast_exits == 1);
    assert(fit.fit_segment_linear_fail_fast_exits == 1);
  }

  {
    bbsolver::PropertySamples ps =
        MakeProperty({{0.0}, {2.0}, {100.0}, {0.0}}, 24.0, 1);
    bbsolver::SolverConfig cfg = Config(1.0);
    cfg.allow_linear = false;
    cfg.allow_bezier = false;
    const bbsolver::SegmentFitResult fit = bbsolver::FitSegment(0, 3, ps, cfg, Comp(24.0));
    assert(!fit.feasible);
    assert(fit.reason == "infeasible_hold");
    assert(fit.max_err > cfg.tolerance);
    assert(fit.max_err < 100.0);
    assert(fit.fit_segment_hold_attempts == 1);
    assert(fit.fit_segment_linear_attempts == 0);
  }

  {
    const std::vector<double> flat0 = ShapeFlatRect(0.0, 0.0, 100.0, 40.0);
    const std::vector<double> flat_mid =
        ShapeFlatRect(80.0, -35.0, 80.0, 120.0);
    const std::vector<double> flat1 = ShapeFlatRect(10.0, 5.0, 100.0, 40.0);
    const bbsolver::PropertySamples ps =
        MakeShapeFlatProperty({flat0, flat_mid, flat1},
                              24.0,
                              static_cast<int>(flat0.size()));
    bbsolver::SolverConfig cfg = Config(1e-4);
    cfg.allow_bezier = false;
    const bbsolver::SegmentFitResult fit =
        bbsolver::FitSegment(0, 2, ps, cfg, Comp(24.0));
    assert(!fit.feasible);
    assert(fit.fit_segment_hold_attempts == 1);
    assert(fit.fit_segment_linear_attempts == 1);
    assert(fit.fit_segment_hold_units_evaluated > 0);
    assert(fit.fit_segment_linear_units_evaluated > 0);
    assert(fit.fit_segment_hold_fail_fast_exits == 1);
    assert(fit.fit_segment_linear_fail_fast_exits == 1);
    assert(fit.fit_segment_hold_shape_outline_wall_ms >= 0.0);
    assert(fit.fit_segment_linear_shape_outline_wall_ms >= 0.0);
  }

  {
    std::vector<std::vector<double>> values;
    for (int i = 0; i <= 12; ++i) {
      const double t = static_cast<double>(i) / 12.0;
      values.push_back({10.0 * (3.0 * t * t - 2.0 * t * t * t)});
    }
    const bbsolver::PropertySamples ps = MakeProperty(values, 12.0, 1);
    const bbsolver::SegmentFitResult fit = bbsolver::FitSegment(0, 12, ps, Config(1e-6), Comp(12.0));
    assert(fit.feasible);
    assert(fit.interp == bbsolver::InterpType::Bezier);
    assert(fit.max_err < 1e-6);
  }

  {
    std::vector<std::vector<double>> values;
    for (int i = 0; i <= 16; ++i) {
      const double t = static_cast<double>(i) / 16.0;
      values.push_back({std::sin(0.5 * kPi * t)});
    }
    const bbsolver::PropertySamples ps = MakeProperty(values, 16.0, 1);
    const bbsolver::SegmentFitResult fit = bbsolver::FitSegment(0, 16, ps, Config(0.01), Comp(16.0));
    assert(fit.feasible);
    assert(fit.interp == bbsolver::InterpType::Bezier);
  }

  {
    const bbsolver::PropertySamples ps = MakeProperty(
        {{1.0, 0.0}, {std::sqrt(0.5), std::sqrt(0.5)}, {0.0, 1.0}},
        24.0,
        2,
        true);
    bbsolver::SolverConfig cfg = Config(0.05);
    cfg.max_iters_per_segment = 80;
    const bbsolver::SegmentFitResult fit = bbsolver::FitSegment(0, 2, ps, cfg, Comp(24.0));
    assert(fit.feasible);
    assert(fit.interp == bbsolver::InterpType::Bezier);
    assert(fit.ease_out_at_i.size() == 1);
    assert(fit.ease_in_at_j.size() == 1);
    assert(fit.spatial_out_at_i.size() == 2);
    assert(fit.spatial_in_at_j.size() == 2);

    const bbsolver::PropertyKeys keys =
        bbsolver::SolveProperty(ps, cfg, Comp(24.0), bbsolver::FitSegment);
    assert(keys.converged);
    for (const bbsolver::Key& key : keys.keys) {
      assert(key.temporal_ease_in.size() == 1);
      assert(key.temporal_ease_out.size() == 1);
    }
  }

  {
    const bbsolver::TemporalEase ease_a_out{0.0, 100.0 / 3.0};
    const bbsolver::TemporalEase ease_a_in{0.0, 100.0 / 3.0};
    const bbsolver::TemporalEase ease_b_out{8.0, 20.0};
    const bbsolver::TemporalEase ease_b_in{-3.0, 70.0};
    std::vector<std::vector<double>> values;
    for (int i = 0; i <= 12; ++i) {
      const double t = static_cast<double>(i) / 12.0;
      values.push_back({
          bbsolver::EvalTemporalBezier(t, 0.0, 0.0, ease_a_out, 1.0, 10.0, ease_a_in),
          bbsolver::EvalTemporalBezier(t, 0.0, -4.0, ease_b_out, 1.0, 9.0, ease_b_in),
      });
    }
    const bbsolver::PropertySamples ps = MakeProperty(values, 12.0, 2, false, true);
    const bbsolver::SegmentFitResult fit = bbsolver::FitSegment(0, 12, ps, Config(1e-4), Comp(12.0));
    assert(fit.feasible);
    assert(fit.interp == bbsolver::InterpType::Bezier);
    assert(fit.ease_out_at_i.size() == 2);
    assert(fit.ease_in_at_j.size() == 2);
    assert(std::abs(fit.ease_out_at_i[0].speed - fit.ease_out_at_i[1].speed) > 1e-3);
  }

  {
    const std::vector<double> p0{0.0, 0.0};
    const std::vector<double> p1{120.0, 20.0};
    const std::vector<double> out_tan{20.0, 80.0};
    const std::vector<double> in_tan{-45.0, 60.0};
    const bbsolver::TemporalEase out_ease{260.0, 65.0};
    const bbsolver::TemporalEase in_ease{90.0, 35.0};
    std::vector<std::vector<double>> values;
    for (int i = 0; i <= 18; ++i) {
      const double t = static_cast<double>(i) / 18.0;
      values.push_back(bbsolver::EvalUnifiedSpatialBezier(
          t, 0.0, p0, out_ease, out_tan, 1.0, p1, in_ease, in_tan));
    }
    const bbsolver::PropertySamples ps = MakeProperty(values, 18.0, 2, true);
    bbsolver::SolverConfig cfg = Config(0.75);
    cfg.max_iters_per_segment = 160;
    const bbsolver::SegmentFitResult fit = bbsolver::FitSegment(0, 18, ps, cfg, Comp(18.0));
    assert(fit.feasible);
    assert(fit.interp == bbsolver::InterpType::Bezier);
    assert(fit.ease_out_at_i.size() == 1);
    assert(fit.ease_in_at_j.size() == 1);
    assert(fit.ease_out_at_i[0].speed > 1.0);
    assert(fit.max_err <= cfg.tolerance);
  }

  {
    const std::vector<double> flat0{
        1.0, 2.0,
        0.0, 0.0, 0.0, 0.0, 8.0, 0.0,
        100.0, 0.0, -8.0, 0.0, 0.0, 0.0,
    };
    const std::vector<double> flat1{
        1.0, 2.0,
        20.0, 40.0, -2.0, 6.0, 11.0, -3.0,
        120.0, 60.0, -12.0, 4.0, 5.0, -7.0,
    };
    const bbsolver::TemporalEase out_ease{0.0, 72.0};
    const bbsolver::TemporalEase in_ease{0.0, 68.0};
    std::vector<std::vector<double>> values;
    for (int i = 0; i <= 24; ++i) {
      const double t = static_cast<double>(i) / 24.0;
      const double u = bbsolver::EvalTemporalBezier(t, 0.0, 0.0, out_ease, 1.0, 1.0, in_ease);
      std::vector<double> v(flat0.size(), 0.0);
      for (std::size_t d = 0; d < flat0.size(); ++d) {
        v[d] = flat0[d] + (flat1[d] - flat0[d]) * u;
      }
      values.push_back(std::move(v));
    }
    const bbsolver::PropertySamples ps =
        MakeShapeFlatProperty(values, 24.0, static_cast<int>(flat0.size()));
    bbsolver::SolverConfig cfg = Config(1e-4);
    cfg.max_iters_per_segment = 160;
    bbsolver::SegmentFitResult default_fit = bbsolver::FitSegment(0, 24, ps, cfg, Comp(24.0));
    assert(!default_fit.feasible);

    cfg.allow_shape_temporal_bezier = true;
    {
      EnvVarGuard no_gate("BBSOLVER_BEZIER_GATE_RATIO", nullptr);
      const bbsolver::SegmentFitResult fit =
          bbsolver::FitSegment(0, 24, ps, cfg, Comp(24.0));
      assert(fit.feasible);
      assert(fit.interp == bbsolver::InterpType::Bezier);
      assert(fit.max_err <= cfg.tolerance);
      assert(fit.fit_segment_hold_attempts == 1);
      assert(fit.fit_segment_linear_attempts == 1);
      assert(fit.fit_segment_hold_units_evaluated > 0);
      assert(fit.fit_segment_linear_units_evaluated > 0);
      assert(fit.fit_segment_hold_fail_fast_exits >= 0);
      assert(fit.fit_segment_linear_fail_fast_exits >= 0);
      assert(fit.fit_segment_hold_shape_outline_wall_ms >= 0.0);
      assert(fit.fit_segment_linear_shape_outline_wall_ms >= 0.0);
      assert(fit.fit_shape_temporal_attempts == 1);
      assert(fit.fit_shape_temporal_gate_rejections == 0);
      assert(fit.fit_shape_temporal_outline_evaluations == 25);
      assert(fit.fit_shape_temporal_ceres_wall_ms >= 0.0);
      assert(fit.fit_shape_temporal_outline_wall_ms >= 0.0);
      assert(fit.fit_shape_temporal_total_wall_ms >= fit.fit_shape_temporal_outline_wall_ms);
    }

    cfg.shape_temporal_bezier_attempt_threshold_ratio = 1.5;
    {
      EnvVarGuard config_gate("BBSOLVER_BEZIER_GATE_RATIO", nullptr);
      const bbsolver::SegmentFitResult gated_fit =
          bbsolver::FitSegment(0, 24, ps, cfg, Comp(24.0));
      assert(!gated_fit.feasible);
      assert(gated_fit.reason == "infeasible_shape_temporal_bezier_gate");
      assert(gated_fit.max_err > cfg.tolerance);
      assert(gated_fit.max_err >
             cfg.tolerance * cfg.shape_temporal_bezier_attempt_threshold_ratio);
      assert(gated_fit.fit_shape_temporal_attempts == 0);
      assert(gated_fit.fit_shape_temporal_gate_rejections == 1);
    }

    {
      EnvVarGuard stale_env("BBSOLVER_BEZIER_GATE_RATIO", "1000000");
      const bbsolver::SegmentFitResult gated_fit =
          bbsolver::FitSegment(0, 24, ps, cfg, Comp(24.0));
      assert(!gated_fit.feasible);
      assert(gated_fit.reason == "infeasible_shape_temporal_bezier_gate");
      assert(gated_fit.max_err > cfg.tolerance);
    }

    {
      cfg.shape_temporal_bezier_attempt_threshold_ratio = -1.0;
      EnvVarGuard wide_gate("BBSOLVER_BEZIER_GATE_RATIO", "1000000");
      const bbsolver::SegmentFitResult fit =
          bbsolver::FitSegment(0, 24, ps, cfg, Comp(24.0));
      assert(fit.feasible);
      assert(fit.interp == bbsolver::InterpType::Bezier);
      assert(fit.ease_out_at_i.size() == 1);
      assert(fit.ease_in_at_j.size() == 1);
      assert(std::abs(fit.ease_out_at_i[0].speed) < 1e-12);
      assert(std::abs(fit.ease_in_at_j[0].speed) < 1e-12);
      assert(fit.max_err <= cfg.tolerance);

      const bbsolver::PropertyKeys keys =
          bbsolver::SolveProperty(ps, cfg, Comp(24.0), bbsolver::FitSegment);
      assert(keys.converged);
      assert(keys.keys.size() == 2);
      assert(keys.segments.size() == 1);
      assert(keys.segments[0].reason == "shape_temporal_bezier_ok");
    }
  }

  {
    const std::vector<double> flat0 = ShapeFlatRect(0.0, 0.0, 100.0, 40.0);
    const std::vector<double> flat1 = ShapeFlatRect(20.0, 10.0, 140.0, 60.0);
    const bbsolver::TemporalEase out_ease{0.75, 72.0};
    const bbsolver::TemporalEase in_ease{0.05, 22.0};

    std::vector<std::vector<double>> values;
    for (int i = 0; i <= 24; ++i) {
      const double t = static_cast<double>(i) / 24.0;
      const double u = bbsolver::EvalTemporalBezier(
          t, 0.0, 0.0, out_ease, 1.0, 1.0, in_ease);
      values.push_back(LerpFlat(flat0, flat1, u));
    }

    const bbsolver::PropertySamples ps =
        MakeShapeFlatProperty(values, 24.0, static_cast<int>(flat0.size()));
    bbsolver::SolverConfig cfg = Config(1e-4);
    cfg.max_iters_per_segment = 160;
    cfg.allow_shape_temporal_bezier = true;

    std::vector<bbsolver::Key> bad_progress_keys;
    bad_progress_keys.push_back(ShapeKey(
        0.0, flat0, bbsolver::InterpType::Bezier, {0.0, 33.3}, out_ease));
    bad_progress_keys.push_back(ShapeKey(
        1.0, flat1, bbsolver::InterpType::Bezier, in_ease, {0.0, 33.3}));
    const bbsolver::ErrorReport bad_report =
        bbsolver::ValidateKeys(ps, bad_progress_keys, cfg, Comp(24.0));
    assert(bad_report.max_err > cfg.tolerance);

    const bbsolver::SegmentFitResult fit =
        bbsolver::FitSegment(0, 24, ps, cfg, Comp(24.0));
    assert(!fit.feasible);
    assert(fit.max_err > cfg.tolerance);
  }

  return 0;
}
