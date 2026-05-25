#include "bbsolver/solve/solve_lifecycle_reporting.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/app/cli_options.hpp"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string>
#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>

#include "bbsolver/io/io_json.hpp"
#include "bbsolver/progress/progress.hpp"
#include "bbsolver/runtime/runtime_env.hpp"
#include "bbsolver/progress/solve_cancellation.hpp"
#include "bbsolver/diagnostics/solver_diagnostic_events.hpp"
#include "bbsolver/diagnostics/solver_diagnostics.hpp"
#include "bbsolver/solve/solver_observability.hpp"

namespace bbsolver {

nlohmann::json BuildSolveStartProgressEvent(const SampleBundle& samples) {
  return {
      {"event", "solve_start"},
      {"phase", "Preparing solver input"},
      {"progress", 0.02},
      {"request_id", samples.request_id},
      {"properties", samples.properties.size()},
      {"solve_optimization_mode", samples.config.solve_optimization_mode},
  };
}

nlohmann::json BuildParallelConfigProgressEvent(const SolveOptions& options,
                                                int resolved_parallel_jobs) {
  return {
      {"event", "parallel_config"},
      {"phase", ParallelRuntimePhase(options.jobs, resolved_parallel_jobs)},
      {"progress", 0.025},
      {"parallel_jobs_requested", options.jobs},
      {"parallel_jobs_resolved", resolved_parallel_jobs},
      {"parallel_jobs_detected", DetectedParallelJobs()},
      {"parallel_jobs_hard_cap", kParallelJobsHardCap},
      {"tbb_available", TbbRuntimeAvailable()},
  };
}

nlohmann::json BuildSolveDoneProgressEvent(const KeyBundle& keys) {
  return {
      {"event", "done"},
      {"phase", "Solver finished"},
      {"progress", 1.0},
      {"properties", keys.property_results.size()},
      {"total_keys", keys.total_keys},
      {"solve_time_ms", keys.solve_time_ms},
  };
}

void EmitSolveStartLifecycle(const SampleBundle& samples,
                             const SolveOptions& options,
                             const std::filesystem::path& input_path,
                             const std::filesystem::path& output_path,
                             int resolved_parallel_jobs,
                             const ProgressWriter& progress,
                             const DiagnosticsWriter& diagnostics) {
  progress.Emit(BuildSolveStartProgressEvent(samples));
  progress.Emit(BuildParallelConfigProgressEvent(options, resolved_parallel_jobs));

  SolveStartDiagnosticInput solve_start_diagnostic;
  solve_start_diagnostic.request_id = samples.request_id;
  solve_start_diagnostic.input_path = input_path;
  solve_start_diagnostic.output_path = output_path;
  solve_start_diagnostic.property_count = samples.properties.size();
  solve_start_diagnostic.tolerance = samples.config.tolerance;
  solve_start_diagnostic.screen_px = samples.config.tolerance_screen_px;
  solve_start_diagnostic.decompose_paths = options.decompose_paths;
  solve_start_diagnostic.fit_canonical_paths = options.fit_canonical_paths;
  solve_start_diagnostic.fit_replacement_paths = options.fit_replacement_paths;
  solve_start_diagnostic.emit_landmark_subpaths =
      options.emit_landmark_subpaths;
  diagnostics.Emit(BuildSolveStartEvent(solve_start_diagnostic));

  nlohmann::json parallel_runtime_event =
      BuildParallelRuntimeEvent(options.jobs);
  parallel_runtime_event["request_id"] = samples.request_id;
  diagnostics.Emit(parallel_runtime_event);

  nlohmann::json capabilities_event =
      BuildSolveModeCapabilitiesEvent(samples.config);
  capabilities_event["request_id"] = samples.request_id;
  diagnostics.Emit(capabilities_event);

  nlohmann::json cancellation_event =
      BuildCancellationStatusEvent(options.cancel_file);
  cancellation_event["request_id"] = samples.request_id;
  diagnostics.Emit(cancellation_event);
}

void EmitSolveDoneLifecycle(const std::string& request_id,
                            const KeyBundle& keys,
                            const ProgressWriter& progress,
                            const DiagnosticsWriter& diagnostics) {
  progress.Emit(BuildSolveDoneProgressEvent(keys));
  diagnostics.Emit(BuildSolveDoneEvent(request_id,
                                       keys.property_results.size(),
                                       keys.total_keys,
                                       keys.total_samples_input,
                                       keys.solve_time_ms));
}

void EmitSolveCancelledLifecycle(const std::string& request_id,
                                 const char* phase,
                                 std::size_t property_idx,
                                 const KeyBundle& keys,
                                 double solve_time_ms,
                                 const DiagnosticsWriter& diagnostics) {
  diagnostics.Emit(BuildSolveCancelledEvent(request_id,
                                            phase,
                                            property_idx,
                                            keys.property_results.size(),
                                            solve_time_ms));
}

int WriteCancelledSolvePartial(
    const std::filesystem::path& output_path,
    const std::string& request_id,
    const char* phase,
    std::size_t property_idx,
    KeyBundle& keys,
    std::chrono::steady_clock::time_point start,
    const DiagnosticsWriter& diagnostics) {
  EmitSolveCancelledLifecycle(request_id,
                              phase,
                              property_idx,
                              keys,
                              MillisecondsSince(start),
                              diagnostics);
  return WriteCancelledPartial(output_path, keys, start);
}

int WriteCompletedSolveOutput(
    const std::filesystem::path& output_path,
    const std::string& request_id,
    KeyBundle& keys,
    std::chrono::steady_clock::time_point start,
    const ProgressWriter& progress,
    const DiagnosticsWriter& diagnostics) {
  keys.solve_time_ms = MillisecondsSince(start);
  WriteKeyBundleJson(output_path, keys);
  EmitSolveDoneLifecycle(request_id, keys, progress, diagnostics);
  return 0;
}

}  // namespace bbsolver
