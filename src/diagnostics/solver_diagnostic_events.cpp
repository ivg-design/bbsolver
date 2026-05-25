#include "bbsolver/diagnostics/solver_diagnostic_events.hpp"
#include "bbsolver/domain.hpp"

#include <string>
#include <cstddef>
#include <filesystem>
#include <optional>
#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>

#include "bbsolver/path/bridge_prune/path_bridge_prune.hpp"
#include "bbsolver/runtime/runtime_env.hpp"
#include "bbsolver/progress/solve_cancellation.hpp"
#include "bbsolver/routing/solve_mode_policy.hpp"

namespace bbsolver {

namespace {

// Exit code documented by WriteCancelledPartial for partial-write outcomes.
// Mirrored here so the cancellation event surfaces the same contract value
// without invoking the write path (which would have filesystem side effects).
constexpr int kCancelledPartialExitCode = 5;

std::string DiagnosticPropertyName(const PropertySamples& ps) {
  if (!ps.property.display_name.empty()) {
    return ps.property.display_name;
  }
  if (!ps.property.match_name.empty()) {
    return ps.property.match_name;
  }
  if (!ps.property.id.empty()) {
    return ps.property.id;
  }
  return "<unnamed>";
}

}  // namespace

nlohmann::json BuildSolveStartEvent(
    const SolveStartDiagnosticInput& input) {
  return {
      {"event", "solve_start"},
      {"schema_version", kSolverDiagnosticEventSchemaVersion},
      {"request_id", input.request_id},
      {"input", input.input_path.string()},
      {"output", input.output_path.string()},
      {"properties", input.property_count},
      {"tolerance", input.tolerance},
      {"screen_px", input.screen_px},
      {"decompose_paths", input.decompose_paths},
      {"fit_canonical_paths", input.fit_canonical_paths},
      {"fit_replacement_paths", input.fit_replacement_paths},
      {"emit_landmark_subpaths", input.emit_landmark_subpaths},
  };
}

nlohmann::json BuildParallelRuntimeEvent(int requested_jobs) {
  nlohmann::json out = {
      {"event", "parallel_runtime"},
      {"schema_version", kSolverDiagnosticEventSchemaVersion},
      {"requested_jobs", requested_jobs},
      {"detected_jobs", DetectedParallelJobs()},
      {"hard_cap", kParallelJobsHardCap},
      {"tbb_available", TbbRuntimeAvailable()},
  };
  // ResolveParallelJobs rejects negative input by throwing; surface that as
  // an explicit error field rather than letting the exception escape the
  // pure event builder.
  if (requested_jobs < 0) {
    out["resolved_jobs"] = nullptr;
    out["phase"] = nullptr;
    out["error"] = "requested_jobs_negative";
    return out;
  }
  const int resolved_jobs = ResolveParallelJobs(requested_jobs);
  out["resolved_jobs"] = resolved_jobs;
  out["phase"] = ParallelRuntimePhase(requested_jobs, resolved_jobs);
  return out;
}

nlohmann::json BuildSolveModeCapabilitiesEvent(const SolverConfig& config) {
  return {
      {"event", "solve_mode_capabilities"},
      {"schema_version", kSolverDiagnosticEventSchemaVersion},
      {"mode", NormalizeSolveOptimizationMode(config.solve_optimization_mode)},
      {"allows_temporal", SolveModeAllowsTemporal(config)},
      {"allows_vertex", SolveModeAllowsVertex(config)},
      {"allows_spatial_topology", SolveModeAllowsSpatialTopology(config)},
      {"is_motion_smooth", SolveModeIsMotionSmooth(config)},
      {"is_motion_path_smooth", SolveModeIsMotionPathSmooth(config)},
      {"uses_motion_smoothing", SolveModeUsesMotionSmoothing(config)},
  };
}

nlohmann::json BuildCancellationStatusEvent(
    const std::optional<std::filesystem::path>& cancel_file) {
  nlohmann::json out = {
      {"event", "cancellation_status"},
      {"schema_version", kSolverDiagnosticEventSchemaVersion},
      {"cancel_file_set", cancel_file.has_value()},
      {"cancel_file_exists", CancelFileExists(cancel_file)},
      {"partial_write_exit_code", kCancelledPartialExitCode},
  };
  out["cancel_file_path"] =
      cancel_file.has_value() ? cancel_file->string() : std::string();
  return out;
}

nlohmann::json BuildSolveCancelledEvent(const std::string& request_id,
                                        const std::string& phase,
                                        std::size_t property_idx,
                                        std::size_t properties_completed,
                                        double solve_time_ms) {
  return {
      {"event", "solve_cancelled"},
      {"schema_version", kSolverDiagnosticEventSchemaVersion},
      {"request_id", request_id},
      {"phase", phase},
      {"property_index", property_idx},
      {"properties_completed", properties_completed},
      {"solve_time_ms", solve_time_ms},
      {"partial_write_exit_code", kCancelledPartialExitCode},
  };
}

nlohmann::json BuildSolveDoneEvent(const std::string& request_id,
                                   std::size_t property_count,
                                   int total_keys,
                                   int total_samples_input,
                                   double solve_time_ms) {
  return {
      {"event", "solve_done"},
      {"schema_version", kSolverDiagnosticEventSchemaVersion},
      {"request_id", request_id},
      {"properties", property_count},
      {"total_keys", total_keys},
      {"total_samples_input", total_samples_input},
      {"solve_time_ms", solve_time_ms},
  };
}

nlohmann::json BuildBridgePruneResultEvent(
    const std::string& request_id,
    const PropertySamples& property_samples,
    std::size_t property_idx,
    std::size_t property_count,
    const PostSolvePathVertexReductionResult& result) {
  return {
      {"event", "post_temporal_bridge_prune_result"},
      {"schema_version", kSolverDiagnosticEventSchemaVersion},
      {"request_id", request_id},
      {"property_id", property_samples.property.id},
      {"property_name", DiagnosticPropertyName(property_samples)},
      {"property_index", property_idx},
      {"property_count", property_count},
      {"accepted", result.accepted},
      {"attempted", result.attempted},
      {"source_vertices", result.source_vertices},
      {"fitted_vertices", result.fitted_vertices},
      {"max_outline_error", result.max_outline_error},
      {"notes", result.notes},
  };
}

nlohmann::json BuildBridgePrunePhaseEvent(
    const PropertySamples& property_samples,
    const BridgePrunePhaseDiagnosticInput& input) {
  return {
      {"event", "post_temporal_bridge_prune_phase"},
      {"schema_version", kSolverDiagnosticEventSchemaVersion},
      {"request_id", input.request_id},
      {"property_id", property_samples.property.id},
      {"property_name", DiagnosticPropertyName(property_samples)},
      {"property_index", input.property_index},
      {"property_count", input.property_count},
      {"phase", input.phase},
      {"target_vertices", input.target_vertices},
      {"removed_index", input.removed_index},
      {"candidate_count", input.candidate_count},
      {"candidates_checked", input.candidates_checked},
      {"attempt", input.attempt},
      {"accepted", input.accepted},
      {"batch", input.batch},
  };
}

}  // namespace bbsolver
