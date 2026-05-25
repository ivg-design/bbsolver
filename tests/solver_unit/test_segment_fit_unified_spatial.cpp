#include "bbsolver/fit/segment_fit_unified_spatial.hpp"
#include "bbsolver/domain.hpp"

#include <cassert>
#include <vector>
#include <cstddef>
#include <utility>

namespace {

bbsolver::PropertySamples MakeSpatialProperty() {
  bbsolver::PropertySamples ps;
  ps.property.dimensions = 2;
  ps.property.is_spatial = true;
  ps.property.kind = bbsolver::ValueKind::TwoD_Spatial;
  const std::vector<std::vector<double>> values{{0.0, 0.0}, {5.0, 0.0}, {10.0, 0.0}};
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
    const std::vector<double> p0{0.0, 0.0};
    const std::vector<double> p1{10.0, 0.0};
    const std::vector<double> out{0.0, 0.0};
    const std::vector<double> in{0.0, 0.0};
    assert(sf::EvalPathPoint(-1.0, p0, out, p1, in) == p0);
    assert(sf::EvalPathPoint(2.0, p0, out, p1, in) == p1);
    assert(std::abs(sf::VectorDistanceSquared({1.0, 2.0}, {4.0}) - 13.0) < 1e-12);
  }

  {
    const sf::PathProjectionLut lut =
        sf::BuildPathProjectionLut({0.0, 0.0}, {0.0, 0.0}, {10.0, 0.0}, {0.0, 0.0});
    assert(lut.points.size() == 513);
    assert(lut.length.size() == 513);
    assert(std::abs(lut.length.back() - 10.0) < 1e-9);
  }

  {
    const bbsolver::PropertySamples ps = MakeSpatialProperty();
    const std::vector<std::pair<double, double>> targets =
        sf::ProjectSegmentSamplesToPathDistances(ps, 0, 2, {0.0, 0.0}, {0.0, 0.0});
    assert(targets.size() == 3);
    assert(targets.front().second == 0.0);
    assert(std::abs(targets.back().second - 10.0) < 1e-9);
    assert(targets[1].second > targets[0].second);
  }

  {
    assert(sf::EaseHandleDistance({10.0, 50.0}, 2.0, 8.0) == 8.0);
    assert(sf::EaseHandleDistance({-10.0, 50.0}, 2.0, 8.0) == 0.0);
    assert(sf::SpeedFromHandleDistance(8.0, 50.0, 2.0) == 8.0);
    assert(sf::SpeedFromHandleDistance(8.0, 50.0, 0.0) == 0.0);
  }

  return 0;
}
