#include "bbsolver/fit/segment_fit_bezier.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/metrics/ae_curve.hpp"

#include <cassert>
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
    sample.t_sec = static_cast<double>(i);
    sample.v = values[i];
    ps.samples.push_back(sample);
  }
  return ps;
}

}  // namespace

int main() {
  namespace sf = bbsolver::segment_fit;

  {
    const bbsolver::PropertySamples ps =
        MakeProperty({{0.0}, {5.0}, {10.0}}, 1);
    const bbsolver::SolverConfig cfg;
    const std::vector<bbsolver::TemporalEase> out =
        sf::HermiteEase(ps, 0, 2, true, cfg);
    const std::vector<bbsolver::TemporalEase> in =
        sf::HermiteEase(ps, 0, 2, false, cfg);
    assert(out.size() == 1);
    assert(in.size() == 1);
    assert(std::abs(out[0].speed - 5.0) < 1e-12);
    assert(std::abs(in[0].speed - 5.0) < 1e-12);
  }

  {
    const std::vector<bbsolver::TemporalEase> eases{{1.0, 10.0}, {2.0, 20.0}};
    assert(sf::EaseForDim({}, 4).influence == 33.3);
    assert(sf::EaseForDim({{7.0, 40.0}}, 5).speed == 7.0);
    assert(sf::EaseForDim(eases, -1).speed == 1.0);
    assert(sf::EaseForDim(eases, 9).speed == 2.0);
  }

  {
    const bbsolver::PropertySamples ps =
        MakeProperty({{0.0, 0.0}, {6.0, 12.0}}, 2, true);
    const bbsolver::SolverConfig cfg;
    const std::vector<double> out =
        sf::HermiteSpatialTangents(ps, 0, 1, true, cfg);
    const std::vector<double> in =
        sf::HermiteSpatialTangents(ps, 0, 1, false, cfg);
    assert(out.size() == 2);
    assert(in.size() == 2);
    assert(out[0] > 0.0);
    assert(out[1] > 0.0);
    assert(in[0] < 0.0);
    assert(in[1] < 0.0);
  }

  {
    const bbsolver::PropertySamples ps =
        MakeProperty({{0.0}, {10.0}}, 1);
    const bbsolver::TemporalEase ease{0.0, 33.3};
    const std::vector<double> value =
        sf::ReconstructBezier(ps, 0, 1, {ease}, {ease}, {}, {}, 0.5);
    const double expected =
        bbsolver::EvalTemporalBezier(0.5, 0.0, 0.0, ease, 1.0, 10.0, ease);
    assert(value.size() == 1);
    assert(std::abs(value[0] - expected) < 1e-12);
  }

  return 0;
}
