#include "bbsolver/fit/segment_fitter.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <vector>

#include "bbsolver/metrics/ae_curve.hpp"
#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/metrics/error_metrics.hpp"
#include "bbsolver/fit/segment_fit_ceres.hpp"
#include "bbsolver/fit/segment_fit_policy.hpp"
#include "bbsolver/fit/segment_fit_samples.hpp"
#include "bbsolver/fit/segment_fit_shape_temporal.hpp"

namespace bbsolver {

using segment_fit::AcceptancePropertyCeiling;
using segment_fit::AcceptanceScreenCeiling;
using segment_fit::AddOrdinaryHoldAttribution;
using segment_fit::AddOrdinaryLinearAttribution;
using segment_fit::ComponentOrZero;
using segment_fit::Dimensions;
using segment_fit::ElapsedMs;
using segment_fit::IsShapeFlatPath;
using segment_fit::LinearFailFastPropertyCeiling;
using segment_fit::LinearFailFastScreenCeiling;
using segment_fit::Passes;
using segment_fit::ResultFromReport;
using segment_fit::SampleTime;
using segment_fit::SampleVector;
using segment_fit::ShapeTemporalBezierGateAllows;
using segment_fit::ShapeTemporalBezierGateRatio;
using segment_fit::TryCeresBezier;
using segment_fit::TryHermiteBezier;
using segment_fit::TryShapeFlatTemporalBezier;
using segment_fit::UseFastUnifiedSpatialOnly;
using segment_fit::WithFitAttribution;
using segment_fit::kMaxCeresDimensions;

SegmentFitResult FitSegment(int i,
                            int j,
                            const PropertySamples& ps,
                            const SolverConfig& cfg,
                            const CompInfo& comp) {
  SegmentFitResult infeasible;
  SegmentFitResult attribution;
  infeasible.reason = "infeasible_empty";
  if (ps.samples.empty()) {
    return infeasible;
  }

  i = std::clamp(i, 0, static_cast<int>(ps.samples.size()) - 1);
  j = std::clamp(j, i, static_cast<int>(ps.samples.size()) - 1);
  if (j <= i) {
    const ErrorReport report;
    return WithFitAttribution(ResultFromReport(InterpType::Hold, report, ps, "hold"),
                              attribution);
  }

  const std::vector<double> v0 = SampleVector(ps, i);
  SegmentFitResult linear_miss;
  bool have_linear_miss = false;
  if (cfg.allow_hold) {
    attribution.fit_segment_hold_attempts += 1;
    const auto hold_start = std::chrono::steady_clock::now();
    const ErrorReport report = ComputeError(
        ps,
        i,
        j,
        [v0](double) { return v0; },
        cfg,
        comp,
        ps.layer_xform_at_start ? &*ps.layer_xform_at_start : nullptr,
        AcceptancePropertyCeiling(cfg),
        AcceptanceScreenCeiling(cfg),
        true);
    AddOrdinaryHoldAttribution(attribution, report);
    attribution.fit_segment_hold_wall_ms +=
        ElapsedMs(hold_start, std::chrono::steady_clock::now());
    if (Passes(report, cfg)) {
      return WithFitAttribution(
          ResultFromReport(InterpType::Hold, report, ps, "hold"),
          attribution);
    }
    infeasible =
        ResultFromReport(InterpType::Hold, report, ps, "infeasible_hold");
    infeasible.feasible = false;
  }

  if (cfg.allow_linear) {
    attribution.fit_segment_linear_attempts += 1;
    const std::vector<double> v1 = SampleVector(ps, j);
    const double t0 = SampleTime(ps, i);
    const double t1 = SampleTime(ps, j);
    const auto linear_start = std::chrono::steady_clock::now();
    const ErrorReport report = ComputeError(
        ps,
        i,
        j,
        [t0, t1, v0, v1](double t) {
          std::vector<double> out(v0.size(), 0.0);
          for (std::size_t d = 0; d < out.size(); ++d) {
            out[d] = EvalLinear(t, t0, ComponentOrZero(v0, d), t1, ComponentOrZero(v1, d));
          }
          return out;
        },
        cfg,
        comp,
        ps.layer_xform_at_start ? &*ps.layer_xform_at_start : nullptr,
        LinearFailFastPropertyCeiling(ps, cfg),
        LinearFailFastScreenCeiling(ps, cfg),
        true);
    AddOrdinaryLinearAttribution(attribution, report);
    attribution.fit_segment_linear_wall_ms +=
        ElapsedMs(linear_start, std::chrono::steady_clock::now());
    if (Passes(report, cfg)) {
      return WithFitAttribution(
          ResultFromReport(InterpType::Linear, report, ps, "linear"),
          attribution);
    }
    linear_miss =
        ResultFromReport(InterpType::Linear, report, ps, "infeasible_linear");
    linear_miss.feasible = false;
    have_linear_miss = true;
    if (infeasible.max_err == 0.0 || report.max_err < infeasible.max_err) {
      infeasible = linear_miss;
    }
  }

  if (cfg.allow_bezier) {
    if (cfg.allow_shape_temporal_bezier && IsShapeFlatPath(ps)) {
      if (ShapeTemporalBezierGateRatio(cfg) >= 0.0) {
        if (!have_linear_miss || !ShapeTemporalBezierGateAllows(linear_miss, cfg)) {
          SegmentFitResult gated = have_linear_miss ? linear_miss : infeasible;
          attribution.fit_shape_temporal_gate_rejections += 1;
          gated.feasible = false;
          gated.reason = "infeasible_shape_temporal_bezier_gate";
          return WithFitAttribution(gated, attribution);
        }
      }
      SegmentFitResult shape_bezier = TryShapeFlatTemporalBezier(i, j, ps, cfg, comp);
      if (shape_bezier.feasible) {
        return WithFitAttribution(shape_bezier, attribution);
      }
      if (infeasible.max_err == 0.0 || shape_bezier.max_err < infeasible.max_err) {
        infeasible = shape_bezier;
      }
    }

    if (Dimensions(ps) > kMaxCeresDimensions) {
      // Path properties serialize hundreds of vertex/tangent scalars into one
      // flat property. Round-3 deliberately keeps those on the cheap
      // Hold/Linear path: Ceres Bezier fitting each scalar would be slow and
      // would not preserve path topology semantics yet.
      infeasible.reason = "infeasible_highdim_linear_only";
      return WithFitAttribution(infeasible, attribution);
    }

    SegmentFitResult hermite = TryHermiteBezier(i, j, ps, cfg, comp);
    if (hermite.feasible) {
      return WithFitAttribution(hermite, attribution);
    }
    if (UseFastUnifiedSpatialOnly(ps)) {
      return WithFitAttribution(hermite, attribution);
    }
    SegmentFitResult ceres = TryCeresBezier(i, j, ps, cfg, comp, hermite);
    if (ceres.feasible) {
      return WithFitAttribution(ceres, attribution);
    }
    if (infeasible.max_err == 0.0 || ceres.max_err < infeasible.max_err) {
      infeasible = ceres;
    }
  }

  return WithFitAttribution(infeasible, attribution);
}

}  // namespace bbsolver
