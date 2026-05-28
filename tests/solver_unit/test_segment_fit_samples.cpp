#include "bbsolver/domain.hpp"

#include "bbsolver/fit/segment_fit_samples.hpp"
#include "bbsolver/dp/dp_placer.hpp"
#include "env_test_support.hpp"
#include "bbsolver/metrics/error_metrics.hpp"

#include <cassert>
#include <cstdlib>
#include <string>
#include <vector>
#include <cstddef>

namespace {

bbsolver::PropertySamples MakeProperty(const std::vector<std::vector<double>>& values,
                                       int dimensions,
                                       bool spatial = false,
                                       bool separated = false) {
  bbsolver::PropertySamples ps;
  ps.property.dimensions = dimensions;
  ps.property.is_spatial = spatial;
  ps.property.is_separated = separated;
  ps.property.kind = spatial ? bbsolver::ValueKind::TwoD_Spatial
: bbsolver::ValueKind::Scalar;
  for (std::size_t i = 0; i < values.size(); ++i) {
    bbsolver::Sample sample;
    sample.t_sec = static_cast<double>(i) * 0.5;
    sample.v = values[i];
    ps.samples.push_back(sample);
  }
  return ps;
}

bbsolver::SolverConfig Config() {
  bbsolver::SolverConfig cfg;
  cfg.tolerance = 1.0;
  cfg.min_influence = 10.0;
  cfg.max_influence = 80.0;
  return cfg;
}

// W5: EnvVarGuard consolidated into solver/tests/solver_unit/env_test_support.hpp.
// The two-arg ctor `(name, value)` matches the previous EnvVarGuard
// shape (nullptr value means capture + unset).
using EnvVarGuard = bbsolver::test_support::ScopedEnv;

}  // namespace

int main() {
  namespace sf = bbsolver::segment_fit;

  {
    bbsolver::PropertySamples ps =
        MakeProperty({{1.0, 2.0}, {4.0}, {7.0, 8.0}}, 0);
    assert(sf::Dimensions(ps) == 1);
    ps.property.dimensions = 2;
    assert(sf::Dimensions(ps) == 2);
    assert(sf::TemporalChannels(ps) == 1);
    ps.property.is_separated = true;
    assert(sf::TemporalChannels(ps) == 2);
    assert(sf::ComponentOrZero(ps.samples[1].v, 1) == 0.0);
    assert(sf::SampleValue(ps, -1, 0) == 0.0);
    assert(sf::SampleValue(ps, 0, 1) == 2.0);
    const std::vector<double> sample = sf::SampleVector(ps, 1);
    assert(sample.size() == 2);
    assert(sample[0] == 4.0);
    assert(sample[1] == 0.0);
    assert(sf::SampleTime(ps, 2) == 1.0);
    assert(sf::SampleTime(ps, 99) == 0.0);
  }

  {
    const bbsolver::PropertySamples ps =
        MakeProperty({{0.0, 0.0}, {3.0, 4.0}, {6.0, 4.0}}, 2, true);
    assert(std::abs(sf::EstimateSlope(ps, 0, 1, 0) - 6.0) < 1e-12);
    assert(std::abs(sf::SampleDistance(ps, 0, 1) - 5.0) < 1e-12);
    assert(std::abs(sf::EndpointSpatialSpeed(ps, 0, 2, true) - 10.0) < 1e-12);
    assert(std::abs(sf::EndpointSlopeIn(ps, 0, 2, 1) - 0.0) < 1e-12);
  }

  {
    bbsolver::SolverConfig cfg = Config();
    assert(sf::ClampInfluence(5.0, cfg) == 10.0);
    assert(sf::ClampInfluence(90.0, cfg) == 80.0);
    assert(sf::ClampInfluence(55.0, cfg) == 55.0);
    assert(sf::ClampInfluence(NAN, cfg) == sf::kDefaultInfluence);
    const std::vector<bbsolver::TemporalEase> eases = sf::DefaultEases(0);
    assert(eases.size() == 1);
    assert(eases[0].influence == sf::kDefaultInfluence);
  }

  {
    bbsolver::ErrorReport report;
    bbsolver::SolverConfig cfg = Config();
    report.max_err = 0.98;
    report.max_err_screen_px = 2.0;
    assert(sf::Passes(report, cfg));
    report.max_err = 0.991;
    assert(!sf::Passes(report, cfg));
    report.max_err = 0.1;
    cfg.tolerance_screen_px = 0.5;
    report.max_err_screen_px = 0.6;
    assert(!sf::Passes(report, cfg));
  }

  {
    EnvVarGuard primary("BBSOLVER_BEZIER_GATE_RATIO", "2.5");
    bbsolver::SolverConfig cfg = Config();
    cfg.shape_temporal_bezier_attempt_threshold_ratio = -1.0;
    assert(sf::ShapeTemporalBezierGateRatio(cfg) == 2.5);
    cfg.shape_temporal_bezier_attempt_threshold_ratio = 1.25;
    assert(sf::ShapeTemporalBezierGateRatio(cfg) == 1.25);

    bbsolver::SegmentFitResult miss;
    miss.max_err = 2.0;
    assert(sf::ShapeTemporalBezierGateAllows(miss, cfg));
    miss.max_err = 2.1;
    assert(!sf::ShapeTemporalBezierGateAllows(miss, cfg));
  }

  {
    bbsolver::PropertySamples ps = MakeProperty({{0.0}, {1.0}}, 1);
    bbsolver::SolverConfig cfg = Config();
    cfg.allow_bezier = true;
    cfg.allow_shape_temporal_bezier = true;
    ps.property.kind = bbsolver::ValueKind::Custom;
    ps.property.units_label = "shape_flat";
    cfg.shape_temporal_bezier_attempt_threshold_ratio = 3.0;
    assert(sf::LinearFailFastPropertyCeiling(ps, cfg) == 3.0);
    assert(std::isinf(sf::LinearFailFastScreenCeiling(ps, cfg)));
  }

  return 0;
}
