#pragma once

#include <string>

namespace bbsolver {

// Hard cap on parallel jobs surfaced to TBB. The progress JSON exposes
// this value verbatim through the `parallel_config` event.
constexpr int kParallelJobsHardCap = 64;

// True when the env var matches one of {"1","true","TRUE","yes","YES"}.
// Unset or any other value (including empty / "0" / "no") yields false.
bool EnvFlagEnabled(const char* name);

// True if either `primary` or `legacy` flag is enabled.
bool EnvFlagEnabledEither(const char* primary, const char* legacy);

// Parse the env var as a positive base-10 integer. Returns 0 when the
// variable is unset, empty, malformed, non-positive, or exactly zero.
// Values above INT_MAX are clamped to INT_MAX.
int EnvPositiveInt(const char* name);

// Returns `EnvPositiveInt(primary)` when positive, otherwise falls back
// to `EnvPositiveInt(legacy)`.
int EnvPositiveIntEither(const char* primary, const char* legacy);

// True when TBB was linked into this build. Governs whether
// ResolveParallelJobs can return multi-job results.
bool TbbRuntimeAvailable();

// std::thread::hardware_concurrency() clamped into [1, kParallelJobsHardCap].
// A reported value of 0 is treated as 1.
int DetectedParallelJobs();

// Resolve a requested parallel job count against the runtime cap.
// - Negative input throws std::runtime_error.
// - When TBB is unavailable, always returns 1.
// - When `requested_jobs == 0`, returns DetectedParallelJobs().
// - Otherwise returns clamp(requested_jobs, 1, DetectedParallelJobs()).
int ResolveParallelJobs(int requested_jobs);

// Compose the `parallel_config` phase string surfaced via progress JSON.
std::string ParallelRuntimePhase(int requested_jobs, int resolved_jobs);

}  // namespace bbsolver
