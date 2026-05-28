#include "bbsolver/fit/segment_fit_ceres.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/dp/dp_placer.hpp"

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
    sample.t_sec = static_cast<double>(i) / 24.0;
    sample.v = values[i];
    ps.samples.push_back(sample);
  }
  return ps;
}

bbsolver::SolverConfig Config(double tolerance = 1e-6) {
  bbsolver::SolverConfig cfg;
  cfg.tolerance = tolerance;
  cfg.max_iters_per_segment = 80;
  return cfg;
}

bbsolver::CompInfo Comp() {
  bbsolver::CompInfo comp;
  comp.fps = 24.0;
  return comp;
}

bbsolver::SegmentFitResult Seed(bool spatial = false) {
  bbsolver::SegmentFitResult seed;
  seed.feasible = false;
  seed.interp = bbsolver::InterpType::Bezier;
  seed.ease_out_at_i = {{2.0, 0.0}, {4.0, 150.0}};
  seed.ease_in_at_j = {{6.0, 0.0}, {8.0, 150.0}};
  if (spatial) {
    seed.spatial_out_at_i = {3.0, 4.0};
    seed.spatial_in_at_j = {-5.0, -6.0};
  }
  return seed;
}

}  // namespace

int main() {
  namespace sf = bbsolver::segment_fit;

  {
    sf::DimCeresResult result;
    assert(result.ease_out.speed == 0.0);
    assert(result.ease_out.influence == 33.3);
    assert(result.ease_in.speed == 0.0);
    assert(result.ease_in.influence == 33.3);
    assert(result.spatial_out == 0.0);
    assert(result.spatial_in == 0.0);
    assert(result.iters == 0);
  }

  {
    const bbsolver::PropertySamples ps =
        MakeProperty({{0.0, 10.0}, {12.0, 18.0}}, 2, false, true);
    bbsolver::SolverConfig cfg = Config();
    cfg.min_influence = 10.0;
    cfg.max_influence = 90.0;
    const sf::DimCeresResult dim0 =
        sf::RunSingleDimCeres(0, 0, 1, ps, cfg, Seed());
    assert(dim0.iters == 0);
    assert(dim0.ease_out.speed == 2.0);
    assert(dim0.ease_in.speed == 6.0);
    assert(dim0.ease_out.influence == 10.0);
    assert(dim0.ease_in.influence == 90.0);
  }

  {
    const bbsolver::PropertySamples ps =
        MakeProperty({{0.0, 10.0}, {6.0, 14.0}, {12.0, 18.0}},
                     2,
                     false,
                     true);
    bbsolver::SegmentFitResult result =
        sf::TrySeparatedCeresBezier(0, 2, ps, Config(), Comp(), Seed());
    assert(result.ease_out_at_i.size() == 2);
    assert(result.ease_in_at_j.size() == 2);
    assert(result.spatial_out_at_i.empty());
    assert(result.spatial_in_at_j.empty());
    assert(result.reason == "bezier_ok" ||
           result.reason == "infeasible_bezier_ceres");
  }

  {
    const bbsolver::PropertySamples ps =
        MakeProperty({{0.0, 0.0}, {3.0, 4.0}, {6.0, 8.0}}, 2, true);
    bbsolver::SegmentFitResult hermite =
        sf::TryHermiteBezier(0, 2, ps, Config(0.25), Comp());
    assert(hermite.interp == bbsolver::InterpType::Bezier);
    assert(hermite.ease_out_at_i.size() == 1);
    assert(hermite.ease_in_at_j.size() == 1);
    assert(hermite.spatial_out_at_i.size() == 2);
    assert(hermite.spatial_in_at_j.size() == 2);
    assert(hermite.reason == "unified_spatial_speed_ok" ||
           hermite.reason == "infeasible_unified_spatial_speed");
  }

  {
    const bbsolver::PropertySamples ps =
        MakeProperty({{0.0}, {2.5}, {5.0}}, 1);
    bbsolver::SegmentFitResult seed;
    seed.ease_out_at_i = {{0.0, 33.3}};
    seed.ease_in_at_j = {{0.0, 33.3}};
    bbsolver::SegmentFitResult result =
        sf::TryCeresBezier(0, 2, ps, Config(1e-5), Comp(), seed);
    assert(result.ease_out_at_i.size() == 1);
    assert(result.ease_in_at_j.size() == 1);
    assert(result.reason == "bezier_ok" ||
           result.reason == "infeasible_bezier_ceres");
    assert(std::isfinite(result.max_err));
  }

  return 0;
}
