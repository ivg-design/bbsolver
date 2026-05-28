#include "bbsolver/runtime/runtime_env.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>

namespace bbsolver {

bool EnvFlagEnabled(const char* name) {
  const char* value = std::getenv(name);
  if (value == nullptr) {
    return false;
  }
  const std::string s = value;
  return s == "1" || s == "true" || s == "TRUE" || s == "yes" || s == "YES";
}

bool EnvFlagEnabledEither(const char* primary, const char* legacy) {
  return EnvFlagEnabled(primary) || EnvFlagEnabled(legacy);
}

int EnvPositiveInt(const char* name) {
  const char* value = std::getenv(name);
  if (value == nullptr || *value == '\0') {
    return 0;
  }
  char* end = nullptr;
  errno = 0;
  const long long parsed = std::strtoll(value, &end, 10);
  if (end == value || parsed <= 0) {
    return 0;
  }
  if (errno == ERANGE) {
    return std::numeric_limits<int>::max();
  }
  if (errno != 0) {
    return 0;
  }
  return static_cast<int>(std::min<long long>(
      parsed, static_cast<long long>(std::numeric_limits<int>::max())));
}

int EnvPositiveIntEither(const char* primary, const char* legacy) {
  const int value = EnvPositiveInt(primary);
  return value > 0 ? value: EnvPositiveInt(legacy);
}

bool TbbRuntimeAvailable() {
#ifdef BBSOLVER_HAVE_TBB
  return true;
#else
  return false;
#endif
}

int DetectedParallelJobs() {
  const unsigned int detected = std::thread::hardware_concurrency();
  const int jobs = detected == 0 ? 1: static_cast<int>(detected);
  return std::clamp(jobs, 1, kParallelJobsHardCap);
}

int ResolveParallelJobs(int requested_jobs) {
  if (requested_jobs < 0) {
    throw std::runtime_error("--jobs must be zero or a positive integer");
  }
  if (!TbbRuntimeAvailable()) {
    return 1;
  }
  const int detected = DetectedParallelJobs();
  if (requested_jobs == 0) {
    return detected;
  }
  return std::clamp(requested_jobs, 1, detected);
}

std::string ParallelRuntimePhase(int requested_jobs, int resolved_jobs) {
  std::string phase = "Parallel runtime: ";
  phase += std::to_string(resolved_jobs);
  phase += resolved_jobs == 1 ? " job": " jobs";
  phase += requested_jobs == 0 ? " (auto": " (requested";
  if (TbbRuntimeAvailable()) {
    phase += ", TBB";
    if (requested_jobs > resolved_jobs && requested_jobs > 0) {
      phase += ", capped";
    }
  } else {
    phase += ", TBB unavailable, serial fallback";
  }
  phase += ")";
  return phase;
}

}  // namespace bbsolver
