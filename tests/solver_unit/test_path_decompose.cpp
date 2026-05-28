#include "bbsolver/path/decompose/path_decompose.hpp"
#include "bbsolver/domain.hpp"

#include <cassert>
#include <vector>
#include <cstddef>

namespace {

bbsolver::Sample MakeSample(double t, const std::vector<double>& v) {
  bbsolver::Sample sample;
  sample.t_sec = t;
  sample.v = v;
  return sample;
}

std::vector<double> TriangleFlat(double offset) {
  return {
      1.0, 3.0,
      0.0 + offset, 0.0, 0.0, 0.0, 0.5, 0.0,
      10.0 + offset, 0.0, -0.5, 0.0, 0.0, 0.5,
      5.0 + offset, 8.0, 0.0, -0.5, -0.5, 0.0,
  };
}

bbsolver::PropertySamples MakeTrianglePath() {
  bbsolver::PropertySamples ps;
  ps.property.id = "unit/path";
  ps.property.match_name = "ADBE Vector Shape";
  ps.property.display_name = "Path";
  ps.property.layer_path = "Unit/Path";
  ps.property.kind = bbsolver::ValueKind::Custom;
  ps.property.dimensions = 20;
  ps.property.units_label = "shape_flat";
  ps.t_start_sec = 0.0;
  ps.t_end_sec = 2.0;
  ps.samples = {
      MakeSample(0.0, TriangleFlat(0.0)),
      MakeSample(1.0, TriangleFlat(1.0)),
      MakeSample(2.0, TriangleFlat(2.0)),
  };
  return ps;
}

bbsolver::PropertyKeys KeysFromChild(const bbsolver::PathChildSamples& child) {
  bbsolver::PropertyKeys keys;
  keys.property_id = child.samples.property.id;
  keys.converged = true;
  for (const bbsolver::Sample& sample: child.samples.samples) {
    bbsolver::Key key;
    key.t_sec = sample.t_sec;
    key.v = sample.v;
    key.interp_in = bbsolver::InterpType::Linear;
    key.interp_out = bbsolver::InterpType::Linear;
    key.temporal_ease_in = {bbsolver::TemporalEase{}};
    key.temporal_ease_out = {bbsolver::TemporalEase{}};
    keys.keys.push_back(key);
  }
  return keys;
}

bbsolver::Key AnchorKey(double t) {
  bbsolver::Key key;
  key.t_sec = t;
  key.interp_in = bbsolver::InterpType::Linear;
  key.interp_out = bbsolver::InterpType::Linear;
  key.temporal_ease_in = {bbsolver::TemporalEase{}};
  key.temporal_ease_out = {bbsolver::TemporalEase{}};
  return key;
}

}  // namespace

int main() {
  const bbsolver::PropertySamples path = MakeTrianglePath();
  const bbsolver::PathDecomposeResult decomposed = bbsolver::DecomposePathBundle(path);
  assert(decomposed.is_shape_flat);
  assert(decomposed.stable_topology);
  assert(decomposed.closed);
  assert(decomposed.vertex_count == 3);
  assert(decomposed.children.size() == 9);

  for (const bbsolver::PathChildSamples& child: decomposed.children) {
    assert(child.samples.property.kind == bbsolver::ValueKind::TwoD_Spatial);
    assert(child.samples.property.dimensions == 2);
    assert(child.samples.property.is_spatial);
    assert(child.samples.samples.size() == path.samples.size());
  }

  std::vector<bbsolver::PropertyKeys> child_keys;
  child_keys.reserve(decomposed.children.size());
  for (const bbsolver::PathChildSamples& child: decomposed.children) {
    child_keys.push_back(KeysFromChild(child));
  }

  const bbsolver::PropertyKeys reassembled =
      bbsolver::ReassemblePathKeys(
          path.property,
          child_keys,
          {AnchorKey(0.0), AnchorKey(1.0), AnchorKey(2.0)},
          decomposed.closed);
  assert(reassembled.converged);
  assert(reassembled.keys.size() == path.samples.size());
  for (std::size_t i = 0; i < path.samples.size(); ++i) {
    assert(reassembled.keys[i].v.size() == path.samples[i].v.size());
    for (std::size_t d = 0; d < path.samples[i].v.size(); ++d) {
      assert(std::abs(reassembled.keys[i].v[d] - path.samples[i].v[d]) < 1e-12);
    }
  }

  const bbsolver::PropertyKeys sparse =
      bbsolver::ReassemblePathKeys(
          path.property,
          child_keys,
          {AnchorKey(0.0), AnchorKey(2.0)},
          decomposed.closed);
  assert(sparse.converged);
  assert(sparse.keys.size() == 2);
  assert(std::abs(sparse.keys[0].t_sec - 0.0) < 1e-12);
  assert(std::abs(sparse.keys[1].t_sec - 2.0) < 1e-12);

  return 0;
}
