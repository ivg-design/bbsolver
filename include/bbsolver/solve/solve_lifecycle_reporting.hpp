#pragma once

#include "bbsolver/domain.hpp"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string>

#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>

#include "bbsolver/app/cli_options.hpp"
#include "bbsolver/diagnostics/solver_diagnostics.hpp"
#include "bbsolver/progress/progress.hpp"

namespace bbsolver {

nlohmann::json BuildSolveStartProgressEvent(const SampleBundle& samples);

nlohmann::json BuildParallelConfigProgressEvent(const SolveOptions& options,
                                                int resolved_parallel_jobs);

nlohmann::json BuildSolveDoneProgressEvent(const KeyBundle& keys);

void EmitSolveStartLifecycle(const SampleBundle& samples,
                             const SolveOptions& options,
                             const std::filesystem::path& input_path,
                             const std::filesystem::path& output_path,
                             int resolved_parallel_jobs,
                             const ProgressWriter& progress,
                             const DiagnosticsWriter& diagnostics);

void EmitSolveDoneLifecycle(const std::string& request_id,
                            const KeyBundle& keys,
                            const ProgressWriter& progress,
                            const DiagnosticsWriter& diagnostics);

void EmitSolveCancelledLifecycle(const std::string& request_id,
                                 const char* phase,
                                 std::size_t property_idx,
                                 const KeyBundle& keys,
                                 double solve_time_ms,
                                 const DiagnosticsWriter& diagnostics);

int WriteCancelledSolvePartial(
    const std::filesystem::path& output_path,
    const std::string& request_id,
    const char* phase,
    std::size_t property_idx,
    KeyBundle& keys,
    std::chrono::steady_clock::time_point start,
    const DiagnosticsWriter& diagnostics);

int WriteCompletedSolveOutput(
    const std::filesystem::path& output_path,
    const std::string& request_id,
    KeyBundle& keys,
    std::chrono::steady_clock::time_point start,
    const ProgressWriter& progress,
    const DiagnosticsWriter& diagnostics);

}  // namespace bbsolver
