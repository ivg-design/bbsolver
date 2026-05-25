#include "bbsolver/fit/segment_fit_shape_temporal.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/metrics/error_metrics.hpp"

#include <cassert>
#include <vector>
#include <cstddef>

namespace {

std::vector<double> Flat(double x0, double y0, double x1, double y1) {
  return {1.0, 2.0, x0, y0, 0.0, 0.0, 0.0, 0.0,
          x1, y1, 0.0, 0.0, 0.0, 0.0};
}

bbsolver::PropertySamples MakeShapeProperty() {
  bbsolver::PropertySamples ps;
  ps.property.kind = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions = 14;
  const std::vector<std::vector<double>> values{
      Flat(0.0, 0.0, 10.0, 0.0),
      Flat(5.0, 10.0, 15.0, 10.0),
      Flat(10.0, 20.0, 20.0, 20.0),
  };
  for (std::size_t i = 0; i < values.size(); ++i) {
    bbsolver::Sample sample;
    sample.t_sec = static_cast<double>(i) * 0.5;
    sample.v = values[i];
    ps.samples.push_back(sample);
  }
  return ps;
}

}  // namespace

int main() {
  namespace sf = bbsolver::segment_fit;

  {
    const bbsolver::PropertySamples ps = MakeShapeProperty();
    assert(std::abs(sf::ShapeFlatProjectedProgress(ps, 0, 2, 1) - 0.5) < 1e-12);
  }

  {
    const bbsolver::PropertySamples ps = MakeShapeProperty();
    const bbsolver::TemporalEase ease{0.0, 33.3};
    const std::vector<double> value =
        sf::ReconstructShapeFlatTemporalBezier(ps, 0, 2, ease, ease, 0.5);
    assert(value.size() == 14);
    assert(std::abs(value[2] - 5.0) < 1e-9);
    assert(std::abs(value[3] - 10.0) < 1e-9);
  }

  {
    const bbsolver::PropertySamples ps = MakeShapeProperty();
    const bbsolver::TemporalEase ease{0.0, 33.3};
    const std::vector<double> value =
        sf::ReconstructShapeFlatKeyBezier(ps, 0, 2, ease, ease, 0.5);
    assert(value.size() == 14);
    assert(std::abs(value[2] - 5.0) < 1e-9);
  }

  {
    const bbsolver::PropertySamples ps = MakeShapeProperty();
    bbsolver::SolverConfig cfg;
    cfg.tolerance = 1e-6;
    int evaluations = 0;
    double outline_ms = 0.0;
    const bbsolver::ErrorReport report = sf::ComputeShapeFlatOutlineError(
        ps,
        0,
        2,
        [&](double t) {
          if (t < 0.25) {
            return ps.samples[0].v;
          }
          if (t > 0.75) {
            return ps.samples[2].v;
          }
          return ps.samples[1].v;
        },
        cfg,
        &evaluations,
        &outline_ms);
    assert(evaluations == 3);
    assert(outline_ms >= 0.0);
    assert(report.max_err == 0.0);
    assert(report.max_err_screen_px == 0.0);
  }

  {
    const bbsolver::PropertySamples ps = MakeShapeProperty();
    bbsolver::SolverConfig cfg;
    cfg.tolerance = 1e-6;
    cfg.allow_shape_temporal_bezier = true;
    const bbsolver::CompInfo comp;
    const bbsolver::SegmentFitResult fit =
        sf::TryShapeFlatTemporalBezier(0, 2, ps, cfg, comp);
    assert(fit.fit_shape_temporal_attempts == 1);
    assert(fit.fit_shape_temporal_outline_evaluations == 3);
    assert(fit.reason == "shape_temporal_bezier_ok" ||
           fit.reason == "infeasible_shape_temporal_bezier");
  }

  return 0;
}
