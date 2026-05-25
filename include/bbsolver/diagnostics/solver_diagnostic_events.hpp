#pragma once

#include "bbsolver/domain.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>

namespace bbsolver {

struct SolverConfig;
struct PropertySamples;
struct PostSolvePathVertexReductionResult;

// Schema version stamped onto every event payload produced by this module.
// Bump when the wire format of any event changes; consumers can pin against
// this constant.
constexpr int kSolverDiagnosticEventSchemaVersion = 1;

struct SolveStartDiagnosticInput {
  std::string request_id;
  std::filesystem::path input_path;
  std::filesystem::path output_path;
  std::size_t property_count = 0;
  double tolerance = 0.0;
  double screen_px = 0.0;
  bool decompose_paths = false;
  bool fit_canonical_paths = false;
  bool fit_replacement_paths = false;
  bool emit_landmark_subpaths = false;
};

struct BridgePrunePhaseDiagnosticInput {
  std::string request_id;
  std::string phase;
  std::size_t property_index = 0;
  std::size_t property_count = 0;
  int target_vertices = 0;
  int removed_index = -1;
  std::size_t candidate_count = 0;
  std::size_t candidates_checked = 0;
  int attempt = 0;
  bool accepted = false;
  bool batch = false;
};

// Event name: "solve_start". Reports solve-level inputs and opt-in flags.
// Pure: formats caller-supplied state only.
nlohmann::json BuildSolveStartEvent(
    const SolveStartDiagnosticInput& input);

// Event name: "parallel_runtime". Reports the resolved parallel-runtime
// configuration for a requested job count, capturing detected hardware
// concurrency, the hard cap, TBB availability, and the operator-visible
// phase string. Pure: reads runtime_env state only; no side effects.
nlohmann::json BuildParallelRuntimeEvent(int requested_jobs);

// Event name: "solve_mode_capabilities". Reports the normalized solve mode,
// route-allowance predicates, and motion-smoothing mode classification.
// Pure: reads `config.solve_optimization_mode` only.
nlohmann::json BuildSolveModeCapabilitiesEvent(const SolverConfig& config);

// Event name: "cancellation_status". Reports whether a cancel file path
// was provided, the normalized path string when present, whether the cancel
// file currently exists on disk, and the documented partial-write exit
// code. Pure: a single filesystem stat via CancelFileExists; no writes.
nlohmann::json BuildCancellationStatusEvent(
    const std::optional<std::filesystem::path>& cancel_file);

// Event name: "solve_cancelled". Reports the cancellation point without
// invoking the partial-write path. Pure: formats caller-supplied state only.
nlohmann::json BuildSolveCancelledEvent(const std::string& request_id,
                                        const std::string& phase,
                                        std::size_t property_idx,
                                        std::size_t properties_completed,
                                        double solve_time_ms);

// Event name: "solve_done". Reports solve-level output totals.
// Pure: formats caller-supplied state only.
nlohmann::json BuildSolveDoneEvent(const std::string& request_id,
                                   std::size_t property_count,
                                   int total_keys,
                                   int total_samples_input,
                                   double solve_time_ms);

// Event name: "post_temporal_bridge_prune_result". Reports the post-solve
// bridge-prune outcome for one property without emitting it. Pure: formats
// caller-supplied state only.
nlohmann::json BuildBridgePruneResultEvent(
    const std::string& request_id,
    const PropertySamples& property_samples,
    std::size_t property_idx,
    std::size_t property_count,
    const PostSolvePathVertexReductionResult& result);

// Event name: "post_temporal_bridge_prune_phase". Reports a bridge-prune
// candidate/progress/acceptance phase summary without emitting it. Pure:
// formats caller-supplied state only.
nlohmann::json BuildBridgePrunePhaseEvent(
    const PropertySamples& property_samples,
    const BridgePrunePhaseDiagnosticInput& input);

}  // namespace bbsolver
