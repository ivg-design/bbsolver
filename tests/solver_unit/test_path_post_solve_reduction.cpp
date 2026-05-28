#include "bbsolver/path/reduction/path_post_solve_reduction.hpp"
#include "bbsolver/domain.hpp"

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

#include "bbsolver/shape/shape_flat_topology.hpp"
#include "bbsolver/path/bridge_prune/path_bridge_prune.hpp"

namespace {

void Require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

std::vector<double> Square() {
  return bbsolver::ShapeFlatFromVertices({
      {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      {100.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      {100.0, 100.0, 0.0, 0.0, 0.0, 0.0},
      {0.0, 100.0, 0.0, 0.0, 0.0, 0.0},
  }, true);
}

bbsolver::PropertySamples ShapeFlatSamples(
    const std::vector<std::vector<double>>& values) {
  bbsolver::PropertySamples samples;
  samples.property.id = "shape";
  samples.property.kind = bbsolver::ValueKind::Custom;
  samples.property.units_label = "shape_flat";
  samples.property.dimensions = values.empty() ? 1: static_cast<int>(values[0].size());
  for (std::size_t i = 0; i < values.size(); ++i) {
    bbsolver::Sample sample;
    sample.t_sec = static_cast<double>(i);
    sample.v = values[i];
    samples.samples.push_back(std::move(sample));
  }
  return samples;
}

bbsolver::PropertyKeys KeysWithValues(
    const std::vector<std::vector<double>>& values,
    bool converged = true) {
  bbsolver::PropertyKeys keys;
  keys.property_id = "shape";
  keys.converged = converged;
  for (std::size_t i = 0; i < values.size(); ++i) {
    bbsolver::Key key;
    key.t_sec = static_cast<double>(i);
    key.v = values[i];
    keys.keys.push_back(std::move(key));
  }
  return keys;
}

void TestNonShapeFlatIsSkipped() {
  bbsolver::PropertySamples samples;
  samples.property.kind = bbsolver::ValueKind::Scalar;
  samples.property.units_label = "px";
  bbsolver::Sample sample;
  sample.t_sec = 0.0;
  sample.v = {1.0};
  samples.samples.push_back(sample);

  const bbsolver::PropertyKeys keys = KeysWithValues({{1.0}, {2.0}});
  const bbsolver::PostSolvePathVertexReductionResult result =
      bbsolver::TryPostSolvePathVertexReduction(
          samples, keys, bbsolver::SolverConfig{}, bbsolver::CompInfo{});

  Require(!result.accepted, "non-shape property must not be accepted");
  Require(!result.attempted, "non-shape property must not be attempted");
  Require(result.notes == "post_solve_vertex_reduction_skipped: non_shape_flat",
          "non-shape skip note must be preserved");
  Require(result.keys.keys.size() == keys.keys.size(),
          "result must carry original keys on skip");
}

void TestNonConvergedKeysAreSkipped() {
  const bbsolver::PropertySamples samples = ShapeFlatSamples({Square(), Square()});
  const bbsolver::PropertyKeys keys = KeysWithValues({Square(), Square()}, false);
  const bbsolver::PostSolvePathVertexReductionResult result =
      bbsolver::TryPostSolvePathVertexReduction(
          samples, keys, bbsolver::SolverConfig{}, bbsolver::CompInfo{});

  Require(!result.accepted, "non-converged keys must not be accepted");
  Require(result.notes == "post_solve_vertex_reduction_skipped: keys_not_converged",
          "non-converged skip note must be preserved");
}

void TestMalformedShapeFlatIsSkipped() {
  const bbsolver::PropertySamples samples = ShapeFlatSamples({{1.0, 4.0}});
  const bbsolver::PropertyKeys keys = KeysWithValues({{1.0, 4.0}, {1.0, 4.0}});
  const bbsolver::PostSolvePathVertexReductionResult result =
      bbsolver::TryPostSolvePathVertexReduction(
          samples, keys, bbsolver::SolverConfig{}, bbsolver::CompInfo{});

  Require(!result.accepted, "malformed shape-flat must not be accepted");
  Require(result.notes == "post_solve_vertex_reduction_skipped: malformed_shape_flat",
          "malformed skip note must be preserved");
}

}  // namespace

int main() {
  TestNonShapeFlatIsSkipped();
  TestNonConvergedKeysAreSkipped();
  TestMalformedShapeFlatIsSkipped();
  return 0;
}
