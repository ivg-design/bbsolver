#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/fit/segment_fitter.hpp"

#include <cassert>
#include <string>
#include <vector>
#include <cstddef>

namespace {

bbsolver::PropertySamples MakeSeparatedDim(const std::string& suffix,
                                           const std::vector<double>& values,
                                           const bbsolver::CompInfo& comp) {
  bbsolver::PropertySamples ps;
  ps.property.id = "unit/Position/sep/" + suffix;
  ps.property.match_name = "ADBE Position_" + suffix;
  ps.property.display_name = suffix + " Position";
  ps.property.layer_path = "Unit/Layer/Transform/Position/" + suffix;
  ps.property.kind = bbsolver::ValueKind::Scalar;
  ps.property.dimensions = 1;
  ps.property.is_spatial = false;
  ps.property.is_separated = true;
  ps.property.units_label = "px";
  ps.t_start_sec = 0.0;
  ps.t_end_sec = values.empty() ? 0.0: static_cast<double>(values.size() - 1) / comp.fps;
  ps.samples_per_frame = 1;
  ps.samples.reserve(values.size());
  for (std::size_t i = 0; i < values.size(); ++i) {
    bbsolver::Sample sample;
    sample.t_sec = static_cast<double>(i) / comp.fps;
    sample.v = {values[i]};
    ps.samples.push_back(sample);
  }
  return ps;
}

bbsolver::SampleBundle MakeSeparatedPositionBundle() {
  bbsolver::SampleBundle bundle;
  bundle.request_id = "unit/separated-position";
  bundle.comp.fps = 24.0;
  bundle.comp.duration_sec = 2.0;
  bundle.comp.width = 1920;
  bundle.comp.height = 1080;
  bundle.config.tolerance = 0.5;
  bundle.config.max_iters_per_segment = 200;

  std::vector<double> x;
  std::vector<double> y;
  std::vector<double> z;
  for (int frame = 0; frame <= 48; ++frame) {
    const double t = static_cast<double>(frame) / bundle.comp.fps;
    x.push_back(120.0 * t);
    y.push_back(180.0 * t - 70.0 * t * t);
    z.push_back(30.0 + 35.0 * t + 18.0 * t * t);
  }

  bundle.properties.push_back(MakeSeparatedDim("x", x, bundle.comp));
  bundle.properties.push_back(MakeSeparatedDim("y", y, bundle.comp));
  bundle.properties.push_back(MakeSeparatedDim("z", z, bundle.comp));
  return bundle;
}

}  // namespace

int main() {
  const bbsolver::SampleBundle bundle = MakeSeparatedPositionBundle();
  assert(bundle.properties.size() == 3);

  for (const bbsolver::PropertySamples& ps: bundle.properties) {
    assert(ps.property.is_separated);
    assert(ps.property.dimensions == 1);
    assert(!ps.property.is_spatial);

    const bbsolver::PropertyKeys keys =
        bbsolver::SolveProperty(ps, bundle.config, bundle.comp, bbsolver::FitSegment);
    assert(keys.converged);
    assert(keys.keys.size() <= 6);
    assert(keys.max_err <= bundle.config.tolerance);
    for (const bbsolver::Key& key: keys.keys) {
      assert(key.v.size() == 1);
      assert(key.temporal_ease_in.size() == 1);
      assert(key.temporal_ease_out.size() == 1);
    }
  }

  return 0;
}
