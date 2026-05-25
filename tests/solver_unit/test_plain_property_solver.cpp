#include "bbsolver/solve/plain_property_solver.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/app/cli_options.hpp"

#include <cassert>
#include <fstream>
#include <filesystem>
#include <iostream>

namespace {

bbsolver::PropertySamples EmptyScalarProperty() {
  bbsolver::PropertySamples property;
  property.property.id = "unit/plain";
  property.property.dimensions = 1;
  property.property.kind = bbsolver::ValueKind::Scalar;
  return property;
}

void TestEmptyPropertyForwardsNoSamplesResult() {
  const bbsolver::PropertyKeys keys = bbsolver::SolvePlainProperty(
      EmptyScalarProperty(),
      bbsolver::SolverConfig{},
      bbsolver::CompInfo{},
      bbsolver::SolveOptions{});

  assert(keys.property_id == "unit/plain");
  assert(!keys.converged);
  assert(keys.keys.empty());
  assert(keys.notes == "no samples");
}

void TestCancelFileShortCircuitsBeforeNoSamples() {
  const std::filesystem::path cancel_path =
      std::filesystem::temp_directory_path() /
      "bb_plain_property_solver_cancel.flag";
  std::filesystem::remove(cancel_path);
  {
    std::ofstream touch(cancel_path);
    touch << "cancel";
  }

  bbsolver::SolveOptions options;
  options.cancel_file = cancel_path;
  const bbsolver::PropertyKeys keys = bbsolver::SolvePlainProperty(
      EmptyScalarProperty(),
      bbsolver::SolverConfig{},
      bbsolver::CompInfo{},
      options);
  std::filesystem::remove(cancel_path);

  assert(keys.property_id == "unit/plain");
  assert(!keys.converged);
  assert(keys.keys.empty());
  assert(keys.notes == "cancelled");
}

}  // namespace

int main() {
  TestEmptyPropertyForwardsNoSamplesResult();
  TestCancelFileShortCircuitsBeforeNoSamples();
  std::cout << "[PASS] test_plain_property_solver\n";
  return 0;
}
