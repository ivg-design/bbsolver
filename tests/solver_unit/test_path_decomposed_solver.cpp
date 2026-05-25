#include "bbsolver/path/decompose/path_decomposed_solver.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/app/cli_options.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "bbsolver/progress/progress.hpp"

namespace {

bbsolver::PropertySamples EmptyScalarProperty() {
  bbsolver::PropertySamples property;
  property.property.id = "unit/decomposed/plain";
  property.property.dimensions = 1;
  property.property.kind = bbsolver::ValueKind::Scalar;
  return property;
}

void TestNonShapeFlatUsesPlainSolverFallback() {
  const bbsolver::ProgressWriter progress(-1);
  const bbsolver::PropertyKeys keys = bbsolver::SolvePathDecomposedProperty(
      EmptyScalarProperty(),
      bbsolver::SolverConfig{},
      bbsolver::CompInfo{},
      bbsolver::SolveOptions{},
      progress,
      0,
      1);

  assert(keys.property_id == "unit/decomposed/plain");
  assert(!keys.converged);
  assert(keys.keys.empty());
  assert(keys.notes == "no samples");
}

void TestCancelFileShortCircuitsThroughPlainSolver() {
  const std::filesystem::path cancel_path =
      std::filesystem::temp_directory_path() /
      "bb_path_decomposed_solver_cancel.flag";
  std::filesystem::remove(cancel_path);
  {
    std::ofstream touch(cancel_path);
    touch << "cancel";
  }

  bbsolver::SolveOptions options;
  options.cancel_file = cancel_path;
  const bbsolver::ProgressWriter progress(-1);
  const bbsolver::PropertyKeys keys = bbsolver::SolvePathDecomposedProperty(
      EmptyScalarProperty(),
      bbsolver::SolverConfig{},
      bbsolver::CompInfo{},
      options,
      progress,
      0,
      1);
  std::filesystem::remove(cancel_path);

  assert(keys.property_id == "unit/decomposed/plain");
  assert(!keys.converged);
  assert(keys.keys.empty());
  assert(keys.notes == "cancelled");
}

}  // namespace

int main() {
  TestNonShapeFlatUsesPlainSolverFallback();
  TestCancelFileShortCircuitsThroughPlainSolver();
  std::cout << "[PASS] test_path_decomposed_solver\n";
  return 0;
}
