#include "bbsolver/path/config/path_gap_policy.hpp"
#include "bbsolver/domain.hpp"

#include "env_test_support.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    std::abort();
  }
}

// W5: ScopedEnv consolidated into solver/tests/solver_unit/env_test_support.hpp
// so the Windows MSVC build uses the _putenv_s / _dupenv_s variant
// instead of the POSIX-only setenv/unsetenv.
using bbsolver::test_support::ScopedEnv;

constexpr const char* kPrimaryGap = "BBSOLVER_PATH_SPECIFIC_MAX_GAP";

bbsolver::CompInfo CompWithFps(double fps) {
  bbsolver::CompInfo comp;
  comp.fps = fps;
  return comp;
}

bbsolver::SolverConfig DefaultConfig() {
  bbsolver::SolverConfig config;
  config.path_specific_max_gap = 0;
  return config;
}

void TestInteractivePathMaxGapFpsBehavior() {
  Require(bbsolver::InteractivePathMaxGap(CompWithFps(0.0)) == 6,
          "zero fps must clamp through one fps and minimum six samples");
  Require(bbsolver::InteractivePathMaxGap(CompWithFps(12.0)) == 6,
          "12 fps must still use the six-sample floor");
  Require(bbsolver::InteractivePathMaxGap(CompWithFps(21.0)) == 7,
          "21 fps must round one-third second to seven samples");
  Require(bbsolver::InteractivePathMaxGap(CompWithFps(24.0)) == 8,
          "24 fps must yield one-third second rounded to eight samples");
  Require(bbsolver::InteractivePathMaxGap(CompWithFps(60.0)) == 20,
          "60 fps must yield one-third second rounded to 20 samples");
}

void TestPathSpecificMaxGapConfigOverride() {
  ScopedEnv primary(kPrimaryGap);
  primary.Set("17");
  bbsolver::SolverConfig config = DefaultConfig();
  config.path_specific_max_gap = 13;
  Require(bbsolver::PathSpecificMaxGap(CompWithFps(24.0), config) == 13,
          "positive config path_specific_max_gap must override env");
}

void TestPathSpecificMaxGapEnvOverride() {
  ScopedEnv primary(kPrimaryGap);
  primary.Set("11");
  Require(bbsolver::PathSpecificMaxGap(CompWithFps(24.0), DefaultConfig()) == 11,
          "path gap env must override the default clamp");
}

void TestPathSpecificMaxGapDefaultClamp() {
  ScopedEnv primary(kPrimaryGap);
  primary.Clear();
  Require(bbsolver::PathSpecificMaxGap(CompWithFps(12.0), DefaultConfig()) == 6,
          "default path-specific gap must keep the six-sample floor");
  Require(bbsolver::PathSpecificMaxGap(CompWithFps(24.0), DefaultConfig()) == 8,
          "default path-specific gap must allow eight at 24 fps");
  Require(bbsolver::PathSpecificMaxGap(CompWithFps(60.0), DefaultConfig()) == 8,
          "default path-specific gap must clamp to eight for high fps");
}

void TestPathSpecificMaxGapPrimaryZeroFallsThrough() {
  ScopedEnv primary(kPrimaryGap);
  primary.Set("0");
  Require(bbsolver::PathSpecificMaxGap(CompWithFps(24.0), DefaultConfig()) == 8,
          "zero primary env must fall through to the default clamp");
}

}  // namespace

int main() {
  TestInteractivePathMaxGapFpsBehavior();
  TestPathSpecificMaxGapConfigOverride();
  TestPathSpecificMaxGapEnvOverride();
  TestPathSpecificMaxGapDefaultClamp();
  TestPathSpecificMaxGapPrimaryZeroFallsThrough();
  std::cout << "[PASS] test_path_gap_policy\n";
  return 0;
}
