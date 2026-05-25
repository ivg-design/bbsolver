#include "bbsolver/routing/solve_mode_policy.hpp"
#include "bbsolver/domain.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    std::abort();
  }
}

bbsolver::SolverConfig ConfigForMode(const std::string& mode) {
  bbsolver::SolverConfig config;
  config.solve_optimization_mode = mode;
  return config;
}

void TestNormalizeDefaultsToFull() {
  Require(bbsolver::NormalizeSolveOptimizationMode("") == "full",
          "empty solve mode must normalize to full");
  Require(bbsolver::NormalizeSolveOptimizationMode("default") == "full",
          "default solve mode must normalize to full");
  Require(bbsolver::NormalizeSolveOptimizationMode("auto") == "full",
          "auto solve mode must normalize to full");
}

void TestNormalizeCaseAndHyphen() {
  Require(bbsolver::NormalizeSolveOptimizationMode("MOTION-SMOOTH") ==
              "motion_smooth",
          "uppercase hyphenated motion-smooth must normalize");
  Require(bbsolver::NormalizeSolveOptimizationMode("MOTION-PATH-SMOOTH") ==
              "motion_path_smooth",
          "uppercase hyphenated motion-path-smooth must normalize");
  Require(bbsolver::NormalizeSolveOptimizationMode("VERTEX-ONLY") ==
              "vertex_only",
          "uppercase hyphenated vertex-only must normalize");
}

void TestInvalidModeThrowsExactMessage() {
  try {
    (void)bbsolver::NormalizeSolveOptimizationMode("invalid-mode");
  } catch (const std::runtime_error& exc) {
    Require(std::string(exc.what()) ==
                "Invalid solve_optimization_mode 'invalid_mode' "
                "(expected full, temporal_only, vertex_only, motion_smooth, "
                "or motion_path_smooth)",
            "invalid mode exception wording changed");
    return;
  }
  Require(false, "invalid solve mode must throw");
}

void TestPredicateMatrixFull() {
  const bbsolver::SolverConfig config = ConfigForMode("full");
  Require(bbsolver::SolveModeAllowsTemporal(config),
          "full mode must allow temporal");
  Require(bbsolver::SolveModeAllowsVertex(config),
          "full mode must allow vertex");
  Require(bbsolver::SolveModeAllowsSpatialTopology(config),
          "full mode must allow spatial topology");
  Require(!bbsolver::SolveModeIsMotionSmooth(config),
          "full mode must not be motion smooth");
  Require(!bbsolver::SolveModeIsMotionPathSmooth(config),
          "full mode must not be motion path smooth");
  Require(!bbsolver::SolveModeUsesMotionSmoothing(config),
          "full mode must not use motion smoothing");
}

void TestPredicateMatrixTemporalOnly() {
  const bbsolver::SolverConfig config = ConfigForMode("temporal_only");
  Require(bbsolver::SolveModeAllowsTemporal(config),
          "temporal_only must allow temporal");
  Require(!bbsolver::SolveModeAllowsVertex(config),
          "temporal_only must not allow vertex");
  Require(!bbsolver::SolveModeAllowsSpatialTopology(config),
          "temporal_only must not allow spatial topology");
  Require(!bbsolver::SolveModeIsMotionSmooth(config),
          "temporal_only must not be motion smooth");
  Require(!bbsolver::SolveModeIsMotionPathSmooth(config),
          "temporal_only must not be motion path smooth");
  Require(!bbsolver::SolveModeUsesMotionSmoothing(config),
          "temporal_only must not use motion smoothing");
}

void TestPredicateMatrixVertexOnly() {
  const bbsolver::SolverConfig config = ConfigForMode("vertex_only");
  Require(!bbsolver::SolveModeAllowsTemporal(config),
          "vertex_only must not allow temporal");
  Require(bbsolver::SolveModeAllowsVertex(config),
          "vertex_only must allow vertex");
  Require(!bbsolver::SolveModeAllowsSpatialTopology(config),
          "vertex_only must not allow spatial topology");
  Require(!bbsolver::SolveModeIsMotionSmooth(config),
          "vertex_only must not be motion smooth");
  Require(!bbsolver::SolveModeIsMotionPathSmooth(config),
          "vertex_only must not be motion path smooth");
  Require(!bbsolver::SolveModeUsesMotionSmoothing(config),
          "vertex_only must not use motion smoothing");
}

void TestPredicateMatrixMotionSmooth() {
  const bbsolver::SolverConfig config = ConfigForMode("motion_smooth");
  Require(!bbsolver::SolveModeAllowsTemporal(config),
          "motion_smooth must not allow temporal");
  Require(!bbsolver::SolveModeAllowsVertex(config),
          "motion_smooth must not allow vertex");
  Require(!bbsolver::SolveModeAllowsSpatialTopology(config),
          "motion_smooth must not allow spatial topology");
  Require(bbsolver::SolveModeIsMotionSmooth(config),
          "motion_smooth must report motion smooth");
  Require(!bbsolver::SolveModeIsMotionPathSmooth(config),
          "motion_smooth must not report motion path smooth");
  Require(bbsolver::SolveModeUsesMotionSmoothing(config),
          "motion_smooth must use motion smoothing");
}

void TestPredicateMatrixMotionPathSmooth() {
  const bbsolver::SolverConfig config = ConfigForMode("motion_path_smooth");
  Require(!bbsolver::SolveModeAllowsTemporal(config),
          "motion_path_smooth must not allow temporal");
  Require(!bbsolver::SolveModeAllowsVertex(config),
          "motion_path_smooth must not allow vertex");
  Require(!bbsolver::SolveModeAllowsSpatialTopology(config),
          "motion_path_smooth must not allow spatial topology");
  Require(!bbsolver::SolveModeIsMotionSmooth(config),
          "motion_path_smooth must not report legacy motion smooth");
  Require(bbsolver::SolveModeIsMotionPathSmooth(config),
          "motion_path_smooth must report motion path smooth");
  Require(bbsolver::SolveModeUsesMotionSmoothing(config),
          "motion_path_smooth must use motion smoothing");
}

}  // namespace

int main() {
  TestNormalizeDefaultsToFull();
  TestNormalizeCaseAndHyphen();
  TestInvalidModeThrowsExactMessage();
  TestPredicateMatrixFull();
  TestPredicateMatrixTemporalOnly();
  TestPredicateMatrixVertexOnly();
  TestPredicateMatrixMotionSmooth();
  TestPredicateMatrixMotionPathSmooth();
  std::cout << "[PASS] test_solve_mode_policy\n";
  return 0;
}
