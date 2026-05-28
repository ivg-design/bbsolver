#include "bbsolver/runtime/runtime_env.hpp"
#include "env_test_support.hpp"

#include <climits>
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

// W5: ScopedEnv now lives in solver/tests/solver_unit/env_test_support.hpp so
// the Windows MSVC build picks up the _putenv_s / _dupenv_s variant
// instead of the POSIX-only setenv/unsetenv.
using bbsolver::test_support::ScopedEnv;

constexpr const char* kFlagName = "BBSOLVER_TEST_FLAG_RUNTIME_ENV";
constexpr const char* kPrimaryFlag = "BBSOLVER_TEST_FLAG_PRIMARY";
constexpr const char* kLegacyFlag = "BBSOLVER_TEST_FLAG_LEGACY";
constexpr const char* kIntName = "BBSOLVER_TEST_INT_RUNTIME_ENV";
constexpr const char* kPrimaryInt = "BBSOLVER_TEST_INT_PRIMARY";
constexpr const char* kLegacyInt = "BBSOLVER_TEST_INT_LEGACY";

void TestEnvFlagEnabledUnsetIsFalse() {
  ScopedEnv guard(kFlagName);
  Require(!bbsolver::EnvFlagEnabled(kFlagName),
          "unset env var must read as disabled");
}

void TestEnvFlagEnabledRecognizesTrueLiterals() {
  ScopedEnv guard(kFlagName);
  const char* truthy[] = {"1", "true", "TRUE", "yes", "YES"};
  for (const char* literal: truthy) {
    guard.Set(literal);
    Require(bbsolver::EnvFlagEnabled(kFlagName),
            std::string("literal '") + literal + "' must read as enabled");
  }
}

void TestEnvFlagEnabledRejectsOtherStrings() {
  ScopedEnv guard(kFlagName);
  const char* falsy[] = {"0", "false", "no", "", "True", "Yes", "off"};
  for (const char* literal: falsy) {
    guard.Set(literal);
    Require(!bbsolver::EnvFlagEnabled(kFlagName),
            std::string("literal '") + literal +
                "' must NOT read as enabled");
  }
}

void TestEnvFlagEnabledEitherPrefersPrimary() {
  ScopedEnv primary(kPrimaryFlag);
  ScopedEnv legacy(kLegacyFlag);
  primary.Set("1");
  legacy.Clear();
  Require(bbsolver::EnvFlagEnabledEither(kPrimaryFlag, kLegacyFlag),
          "primary on must enable the either-flag");
}

void TestEnvFlagEnabledEitherFallsBackToLegacy() {
  ScopedEnv primary(kPrimaryFlag);
  ScopedEnv legacy(kLegacyFlag);
  primary.Clear();
  legacy.Set("yes");
  Require(bbsolver::EnvFlagEnabledEither(kPrimaryFlag, kLegacyFlag),
          "legacy on with primary unset must enable the either-flag");
}

void TestEnvFlagEnabledEitherBothOffIsFalse() {
  ScopedEnv primary(kPrimaryFlag);
  ScopedEnv legacy(kLegacyFlag);
  primary.Clear();
  legacy.Clear();
  Require(!bbsolver::EnvFlagEnabledEither(kPrimaryFlag, kLegacyFlag),
          "both unset must yield false");
}

void TestEnvPositiveIntReturnsZeroForUnsetOrEmpty() {
  ScopedEnv guard(kIntName);
  Require(bbsolver::EnvPositiveInt(kIntName) == 0,
          "unset env must yield 0");
  guard.Set("");
  Require(bbsolver::EnvPositiveInt(kIntName) == 0,
          "empty value must yield 0");
}

void TestEnvPositiveIntRejectsNonPositive() {
  ScopedEnv guard(kIntName);
  guard.Set("0");
  Require(bbsolver::EnvPositiveInt(kIntName) == 0, "literal 0 must yield 0");
  guard.Set("-3");
  Require(bbsolver::EnvPositiveInt(kIntName) == 0, "negative must yield 0");
  guard.Set("not-a-number");
  Require(bbsolver::EnvPositiveInt(kIntName) == 0,
          "non-numeric must yield 0");
}

void TestEnvPositiveIntParsesPositive() {
  ScopedEnv guard(kIntName);
  guard.Set("42");
  Require(bbsolver::EnvPositiveInt(kIntName) == 42,
          "positive value must round-trip");
}

void TestEnvPositiveIntClampsAtIntMax() {
  ScopedEnv guard(kIntName);
  // 999999999999999999 is well above INT_MAX on every supported platform.
  guard.Set("999999999999999999");
  Require(bbsolver::EnvPositiveInt(kIntName) == INT_MAX,
          "out-of-range value must clamp to INT_MAX");
}

void TestEnvPositiveIntEitherShortCircuits() {
  ScopedEnv primary(kPrimaryInt);
  ScopedEnv legacy(kLegacyInt);
  primary.Set("12");
  legacy.Set("99");
  Require(bbsolver::EnvPositiveIntEither(kPrimaryInt, kLegacyInt) == 12,
          "positive primary must short-circuit before legacy");
}

void TestEnvPositiveIntEitherFallsBack() {
  ScopedEnv primary(kPrimaryInt);
  ScopedEnv legacy(kLegacyInt);
  primary.Clear();
  legacy.Set("99");
  Require(bbsolver::EnvPositiveIntEither(kPrimaryInt, kLegacyInt) == 99,
          "missing primary must fall back to legacy");
}

void TestDetectedParallelJobsWithinCap() {
  const int detected = bbsolver::DetectedParallelJobs();
  Require(detected >= 1, "DetectedParallelJobs must never fall below 1");
  Require(detected <= bbsolver::kParallelJobsHardCap,
          "DetectedParallelJobs must respect the hard cap");
}

void TestResolveParallelJobsNegativeThrows() {
  bool threw = false;
  try {
    (void)bbsolver::ResolveParallelJobs(-1);
  } catch (const std::runtime_error&) {
    threw = true;
  }
  Require(threw, "negative jobs must throw runtime_error");
}

void TestResolveParallelJobsBehaviorMatchesRuntime() {
  const int detected = bbsolver::DetectedParallelJobs();
  if (bbsolver::TbbRuntimeAvailable()) {
    Require(bbsolver::ResolveParallelJobs(0) == detected,
            "auto under TBB must return detected count");
    Require(bbsolver::ResolveParallelJobs(1) == 1,
            "request=1 under TBB must return 1");
    Require(bbsolver::ResolveParallelJobs(detected) == detected,
            "request==detected under TBB must equal detected");
    Require(bbsolver::ResolveParallelJobs(detected + 1000) == detected,
            "request beyond detected under TBB must clamp to detected");
  } else {
    Require(bbsolver::ResolveParallelJobs(0) == 1,
            "auto without TBB must serialize");
    Require(bbsolver::ResolveParallelJobs(4) == 1,
            "request without TBB must serialize");
  }
}

void TestParallelRuntimePhaseAutoTbb() {
  if (!bbsolver::TbbRuntimeAvailable()) {
    return;
  }
  Require(bbsolver::ParallelRuntimePhase(0, 4) ==
              "Parallel runtime: 4 jobs (auto, TBB)",
          "auto + TBB phrasing must match the progress contract");
}

void TestParallelRuntimePhaseRequestedTbb() {
  if (!bbsolver::TbbRuntimeAvailable()) {
    return;
  }
  Require(bbsolver::ParallelRuntimePhase(4, 4) ==
              "Parallel runtime: 4 jobs (requested, TBB)",
          "requested + TBB phrasing must match the progress contract");
}

void TestParallelRuntimePhaseRequestedCapped() {
  if (!bbsolver::TbbRuntimeAvailable()) {
    return;
  }
  Require(bbsolver::ParallelRuntimePhase(99, 4) ==
              "Parallel runtime: 4 jobs (requested, TBB, capped)",
          "capped request must annotate with ', capped'");
}

void TestParallelRuntimePhaseSingularJob() {
  if (!bbsolver::TbbRuntimeAvailable()) {
    return;
  }
  Require(bbsolver::ParallelRuntimePhase(1, 1) ==
              "Parallel runtime: 1 job (requested, TBB)",
          "single-job phrasing must say 'job' (not 'jobs')");
}

void TestParallelRuntimePhaseFallbackWithoutTbb() {
  if (bbsolver::TbbRuntimeAvailable()) {
    return;
  }
  Require(bbsolver::ParallelRuntimePhase(0, 1) ==
              "Parallel runtime: 1 job (auto, TBB unavailable, "
              "serial fallback)",
          "no-TBB phrasing must report serial fallback");
}

}  // namespace

int main() {
  TestEnvFlagEnabledUnsetIsFalse();
  TestEnvFlagEnabledRecognizesTrueLiterals();
  TestEnvFlagEnabledRejectsOtherStrings();
  TestEnvFlagEnabledEitherPrefersPrimary();
  TestEnvFlagEnabledEitherFallsBackToLegacy();
  TestEnvFlagEnabledEitherBothOffIsFalse();
  TestEnvPositiveIntReturnsZeroForUnsetOrEmpty();
  TestEnvPositiveIntRejectsNonPositive();
  TestEnvPositiveIntParsesPositive();
  TestEnvPositiveIntClampsAtIntMax();
  TestEnvPositiveIntEitherShortCircuits();
  TestEnvPositiveIntEitherFallsBack();
  TestDetectedParallelJobsWithinCap();
  TestResolveParallelJobsNegativeThrows();
  TestResolveParallelJobsBehaviorMatchesRuntime();
  TestParallelRuntimePhaseAutoTbb();
  TestParallelRuntimePhaseRequestedTbb();
  TestParallelRuntimePhaseRequestedCapped();
  TestParallelRuntimePhaseSingularJob();
  TestParallelRuntimePhaseFallbackWithoutTbb();
  std::cout << "[PASS] test_runtime_env\n";
  return 0;
}
