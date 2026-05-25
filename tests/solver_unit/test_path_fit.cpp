#include "bbsolver/path/fit/path_fit.hpp"
#include "bbsolver/domain.hpp"

#include <cassert>
#include <utility>
#include <vector>
#include <cstddef>

namespace {

std::vector<double> ShapeFlat(bool closed,
                              const std::vector<std::vector<double>>& vertices,
                              const std::vector<bool>& zero_tangent) {
  std::vector<double> out;
  out.push_back(closed ? 1.0 : 0.0);
  out.push_back(static_cast<double>(vertices.size()));
  for (std::size_t i = 0; i < vertices.size(); ++i) {
    out.push_back(vertices[i][0]);
    out.push_back(vertices[i][1]);
    if (zero_tangent[i]) {
      out.push_back(0.0);
      out.push_back(0.0);
      out.push_back(0.0);
      out.push_back(0.0);
    } else {
      out.push_back(-4.0);
      out.push_back(0.0);
      out.push_back(4.0);
      out.push_back(0.0);
    }
  }
  return out;
}

bbsolver::PropertySamples MakeRedundantRectangle(bool all_zero_tangents = false,
                                                 int frame_count = 2) {
  const std::vector<std::vector<double>> base{
      {0.0, 0.0},    {50.0, 0.0},
      {100.0, 0.0},  {100.0, 50.0},
      {100.0, 100.0}, {50.0, 100.0},
      {0.0, 100.0},  {0.0, 50.0},
  };
  std::vector<bool> zero_tangent{
      true, false, true, false, true, false, true, false,
  };
  if (all_zero_tangents) {
    zero_tangent.assign(base.size(), true);
  }

  bbsolver::PropertySamples ps;
  ps.property.id = "unit/path";
  ps.property.kind = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions = 2 + 6 * 8;
  ps.samples_per_frame = 1;

  for (int frame = 0; frame < frame_count; ++frame) {
    std::vector<std::vector<double>> vertices = base;
    for (std::vector<double>& vertex : vertices) {
      vertex[0] += frame * 10.0;
      vertex[1] += frame * 5.0;
    }
    bbsolver::Sample sample;
    sample.t_sec = static_cast<double>(frame);
    sample.v = ShapeFlat(true, vertices, zero_tangent);
    ps.samples.push_back(std::move(sample));
  }
  return ps;
}

void AssertSamePathFitResult(const bbsolver::PathFitResult& a,
                             const bbsolver::PathFitResult& b) {
  assert(a.is_shape_flat == b.is_shape_flat);
  assert(a.stable_topology == b.stable_topology);
  assert(a.applied == b.applied);
  assert(a.closed == b.closed);
  assert(a.source_vertex_count == b.source_vertex_count);
  assert(a.fitted_vertex_count == b.fitted_vertex_count);
  assert(a.locked_vertex_count == b.locked_vertex_count);
  assert(std::abs(a.max_outline_error - b.max_outline_error) < 1e-12);
  assert(a.notes == b.notes);
  assert(a.kept_indices == b.kept_indices);
  assert(a.samples.property.dimensions == b.samples.property.dimensions);
  assert(a.samples.samples.size() == b.samples.samples.size());
  for (std::size_t sample_idx = 0; sample_idx < a.samples.samples.size(); ++sample_idx) {
    assert(std::abs(a.samples.samples[sample_idx].t_sec -
                    b.samples.samples[sample_idx].t_sec) < 1e-12);
    assert(a.samples.samples[sample_idx].v == b.samples.samples[sample_idx].v);
  }
}

double ComponentOrZero(const std::vector<double>& values, std::size_t idx) {
  return idx < values.size() ? values[idx] : 0.0;
}

}  // namespace

int main() {
  bbsolver::SolverConfig cfg;
  cfg.tolerance = 0.01;

  const bbsolver::PropertySamples ps = MakeRedundantRectangle();
  const bbsolver::PathFitResult result = bbsolver::FitCanonicalPathProperty(ps, cfg);

  assert(result.is_shape_flat);
  assert(result.stable_topology);
  assert(result.applied);
  assert(result.source_vertex_count == 8);
  assert(result.fitted_vertex_count == 4);
  assert(result.locked_vertex_count == 4);
  assert(result.max_outline_error <= cfg.tolerance);
  assert(result.samples.property.dimensions == 26);
  assert(result.samples.samples.size() == ps.samples.size());
  assert(result.kept_indices.size() == 4);
  assert(result.kept_indices[0] == 0);
  assert(result.kept_indices[1] == 2);
  assert(result.kept_indices[2] == 4);
  assert(result.kept_indices[3] == 6);

  for (const bbsolver::Sample& sample : result.samples.samples) {
    assert(sample.v.size() == 26);
    assert(std::llround(sample.v[0]) == 1);
    assert(std::llround(sample.v[1]) == 4);
    for (int vertex = 0; vertex < 4; ++vertex) {
      const std::size_t offset = 2 + static_cast<std::size_t>(vertex) * 6;
      assert(std::abs(ComponentOrZero(sample.v, offset + 2)) < 1e-12);
      assert(std::abs(ComponentOrZero(sample.v, offset + 3)) < 1e-12);
      assert(std::abs(ComponentOrZero(sample.v, offset + 4)) < 1e-12);
      assert(std::abs(ComponentOrZero(sample.v, offset + 5)) < 1e-12);
    }
  }

  const bbsolver::PropertySamples all_zero = MakeRedundantRectangle(true);
  const bbsolver::PathFitResult all_zero_result =
      bbsolver::FitCanonicalPathProperty(all_zero, cfg);
  assert(all_zero_result.is_shape_flat);
  assert(all_zero_result.stable_topology);
  assert(all_zero_result.applied);
  assert(all_zero_result.source_vertex_count == 8);
  assert(all_zero_result.fitted_vertex_count == 4);
  assert(all_zero_result.locked_vertex_count == 4);
  assert(all_zero_result.kept_indices.size() == 4);
  assert(all_zero_result.kept_indices[0] == 0);
  assert(all_zero_result.kept_indices[1] == 2);
  assert(all_zero_result.kept_indices[2] == 4);
  assert(all_zero_result.kept_indices[3] == 6);

  bbsolver::SolverConfig serial_cfg;
  serial_cfg.tolerance = 0.01;
  serial_cfg.parallel_jobs = 1;
  bbsolver::SolverConfig parallel_cfg = serial_cfg;
  parallel_cfg.parallel_jobs = 8;
  const bbsolver::PropertySamples many_samples =
      MakeRedundantRectangle(/*all_zero_tangents=*/false, 80);
  const bbsolver::PathFitResult serial_result =
      bbsolver::FitCanonicalPathProperty(many_samples, serial_cfg);
  const bbsolver::PathFitResult parallel_result =
      bbsolver::FitCanonicalPathProperty(many_samples, parallel_cfg);
  assert(serial_result.applied);
  AssertSamePathFitResult(serial_result, parallel_result);

  return 0;
}
