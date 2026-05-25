#include "bbsolver/solve/solve_command.hpp"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <iostream>
#include <ratio>
#include <string>
#include <utility>

#include "bbsolver/app/cli_options.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/motion_smooth/motion_smooth_reduction_gate.hpp"
#include "bbsolver/path/bridge_prune/path_bridge_prune.hpp"
#include "bbsolver/path/replacement/path_replacement_post_temporal.hpp"
#include "bbsolver/path/reduction/path_vertex_reduction.hpp"
#include "bbsolver/progress/progress.hpp"
#include "bbsolver/routing/property_classification.hpp"
#include "bbsolver/routing/property_route_solver.hpp"
#include "bbsolver/shape/sharp_corner_policy.hpp"
#include "bbsolver/progress/solve_cancellation.hpp"
#include "bbsolver/solve/solve_command_config.hpp"
#include "bbsolver/solve/solve_lifecycle_reporting.hpp"
#include "bbsolver/runtime/solve_parallel_runtime_scope.hpp"
#include "bbsolver/solve/solve_path_preparation.hpp"
#include "bbsolver/solve/solve_property_completion.hpp"
#include "bbsolver/solve/solve_property_temporal_prelude.hpp"
#include "bbsolver/solve/solve_property_temporal_result.hpp"
#include "bbsolver/diagnostics/solver_diagnostics.hpp"
#include "bbsolver/solve/solver_reporting.hpp"

namespace bbsolver {

using bbsolver::BridgePruneLocalProgress;            // NOLINT(misc-unused-using-decls)
using bbsolver::BridgePruneProgressChunkSize;        // NOLINT(misc-unused-using-decls)
using bbsolver::PersistentShapeFlatSharpCornerIndicesByVertexCount;  // NOLINT(misc-unused-using-decls)
using bbsolver::PostTemporalBridgePrunePassBudget;   // NOLINT(misc-unused-using-decls)
using bbsolver::ShapeFlatKeyIndexIsProtectedCorner;  // NOLINT(misc-unused-using-decls)
using bbsolver::TryPostTemporalBridgePrune;          // NOLINT(misc-unused-using-decls)

int RunSolve(int argc, char** argv) {
  if (argc < 4) {
    PrintUsage(std::cerr);
    return 2;
  }

  SolveCommandConfig command = ParseSolveCommandConfig(argc, argv);
  const auto start = std::chrono::steady_clock::now();
  LoadSolveCommandSamples(command);
  const std::filesystem::path& input_path = command.input_path;
  const std::filesystem::path& output_path = command.output_path;
  SolveOptions& options = command.options;
  auto& samples = command.samples;
  auto& keys = command.keys;
  const int resolved_parallel_jobs = command.resolved_parallel_jobs;

  const ProgressWriter progress(options.progress_fd);
  const DiagnosticsWriter diagnostics =
      options.diagnostics_file.has_value()
          ? DiagnosticsWriter::ToFile(*options.diagnostics_file)
          : DiagnosticsWriter();
  const SolveParallelRuntimeScope parallel_runtime_scope(
      resolved_parallel_jobs);
  EmitSolveStartLifecycle(samples,
                          options,
                          input_path,
                          output_path,
                          resolved_parallel_jobs,
                          progress,
                          diagnostics);
  const auto write_cancelled = [&](const char* phase,
                                   std::size_t property_idx) {
    return WriteCancelledSolvePartial(output_path,
                                      samples.request_id,
                                      phase,
                                      property_idx,
                                      keys,
                                      start,
                                      diagnostics);
  };

  for (std::size_t property_idx = 0; property_idx < samples.properties.size(); ++property_idx) {
    if (CancelFileExists(options.cancel_file)) {
      return write_cancelled("property_loop", property_idx);
    }

    PathSolvePreparationRequest path_prep_request;
    path_prep_request.source_property = &samples.properties[property_idx];
    path_prep_request.config = &samples.config;
    path_prep_request.comp = &samples.comp;
    path_prep_request.progress = &progress;
    path_prep_request.property_idx = property_idx;
    path_prep_request.property_count = samples.properties.size();
    PathSolvePreparationState path_prep =
        PreparePathSolveInputs(path_prep_request);

    auto& original_property_samples = path_prep.original_property_samples;
    auto& property_samples = path_prep.property_samples;
    bool& visible_outline_reference = path_prep.visible_outline_reference;
    std::string& path_fit_note = path_prep.path_fit_note;
    auto& near_optimal_fast_path = path_prep.near_optimal_fast_path;
    bool& replacement_path_applied = path_prep.replacement_path_applied;
    bool& canonical_path_applied = path_prep.canonical_path_applied;
    bool& replacement_output_accepted = path_prep.replacement_output_accepted;
    bool& replacement_fast_vertex_preference_accepted =
        path_prep.replacement_fast_vertex_preference_accepted;
    int& replacement_fitted_vertices = path_prep.replacement_fitted_vertices;
    int& replacement_original_max_vertices =
        path_prep.replacement_original_max_vertices;
    int& replacement_source_min_vertices =
        path_prep.replacement_source_min_vertices;
    int& replacement_estimated_candidate_keys =
        path_prep.replacement_estimated_candidate_keys;
    int& replacement_estimated_original_keys =
        path_prep.replacement_estimated_original_keys;
    double& replacement_frame_fit_error =
        path_prep.replacement_frame_fit_error;
    WarnUnifiedSpatialPropertyIfNeeded(property_samples, std::cerr);

    const auto prop_start = std::chrono::steady_clock::now();
    PropertyTemporalPreludeRequest temporal_prelude_request;
    temporal_prelude_request.original_property_samples =
        &original_property_samples;
    temporal_prelude_request.property_samples = &property_samples;
    temporal_prelude_request.config = &samples.config;
    temporal_prelude_request.comp = &samples.comp;
    temporal_prelude_request.progress = &progress;
    temporal_prelude_request.near_optimal_fast_path = &near_optimal_fast_path;
    temporal_prelude_request.path_fit_note = &path_fit_note;
    temporal_prelude_request.property_idx = property_idx;
    temporal_prelude_request.property_count = samples.properties.size();
    temporal_prelude_request.replacement_path_applied =
        replacement_path_applied;
    temporal_prelude_request.canonical_path_applied = canonical_path_applied;
    temporal_prelude_request.decompose_paths = options.decompose_paths;
    temporal_prelude_request.replacement_frame_fit_error =
        replacement_frame_fit_error;
    PropertyTemporalPreludeState temporal_prelude =
        PreparePropertyTemporalPrelude(temporal_prelude_request);

    auto& temporal_source_samples =
        temporal_prelude.temporal_source_samples;
    auto& temporal_property_samples =
        temporal_prelude.temporal_property_samples;
    const std::string& final_static_trim_note =
        temporal_prelude.final_static_trim_note;
    const bool motion_smooth_enabled =
        temporal_prelude.motion_smooth_enabled;
    const bool temporal_optimization_enabled =
        temporal_prelude.temporal_optimization_enabled;
    const int replacement_temporal_max_gap =
        temporal_prelude.replacement_temporal_max_gap;
    const auto& temporal_config = temporal_prelude.temporal_config;
    const auto property_solve_route =
        temporal_prelude.property_solve_route;

    PropertyRouteSolveRequest route_solve_request;
    route_solve_request.route = property_solve_route;
    route_solve_request.original_property_samples = &original_property_samples;
    route_solve_request.property_samples = &property_samples;
    route_solve_request.temporal_source_samples = &temporal_source_samples;
    route_solve_request.temporal_property_samples = &temporal_property_samples;
    route_solve_request.config = &samples.config;
    route_solve_request.temporal_config = &temporal_config;
    route_solve_request.comp = &samples.comp;
    route_solve_request.options = &options;
    route_solve_request.progress = &progress;
    route_solve_request.near_optimal_fast_path = &near_optimal_fast_path;
    route_solve_request.property_idx = property_idx;
    route_solve_request.property_count = samples.properties.size();
    route_solve_request.replacement_temporal_max_gap =
        replacement_temporal_max_gap;
    route_solve_request.canonical_path_applied = canonical_path_applied;
    PropertyKeys property_keys = SolvePropertyRoute(route_solve_request);
    ApplyFinalStaticTrimNote(property_keys, final_static_trim_note);
    if (!motion_smooth_enabled &&
        temporal_optimization_enabled &&
        IsShapeFlatPath(temporal_property_samples)) {
      ShapeMotionReductionGateResult smoothness_gate =
          GateShapeMotionQualityRegression(
              temporal_property_samples, property_keys, samples.config);
      if (smoothness_gate.attempted) {
        if (smoothness_gate.rejected) {
          const std::string candidate_note = property_keys.notes;
          property_keys = std::move(smoothness_gate.preserved_keys);
          property_keys.notes = candidate_note.empty()
              ? smoothness_gate.note
              : candidate_note + "; " + smoothness_gate.note;
        } else {
          AppendSolverNote(property_keys, smoothness_gate.note);
        }
      }
    }
    const auto prop_end = std::chrono::steady_clock::now();
    const double prop_ms =
        std::chrono::duration<double, std::milli>(prop_end - prop_start)
            .count();
    PropertyTemporalSolveResultRequest temporal_result_request;
    temporal_result_request.property_samples = &property_samples;
    temporal_result_request.property_keys = &property_keys;
    temporal_result_request.progress = &progress;
    temporal_result_request.cancel_fn =
        [&]() { return CancelFileExists(options.cancel_file); };
    temporal_result_request.property_idx = property_idx;
    temporal_result_request.property_count = samples.properties.size();
    temporal_result_request.prop_ms = prop_ms;
    const PropertyTemporalSolveResult temporal_result =
        ReportPropertyTemporalSolveResult(temporal_result_request);
    if (temporal_result.cancelled) {
      return write_cancelled(temporal_result.cancel_phase.c_str(),
                             property_idx);
    }
    if (replacement_path_applied) {
      PostTemporalReplacementRequest replacement_request;
      replacement_request.original_property_samples =
          &original_property_samples;
      replacement_request.config = &samples.config;
      replacement_request.comp = &samples.comp;
      replacement_request.options = &options;
      replacement_request.progress = &progress;
      replacement_request.cancel_fn =
          [&]() { return CancelFileExists(options.cancel_file); };
      replacement_request.property_idx = property_idx;
      replacement_request.property_count = samples.properties.size();
      replacement_request.visible_outline_reference = visible_outline_reference;
      replacement_request.replacement_output_accepted =
          replacement_output_accepted;
      replacement_request.replacement_fast_vertex_preference_accepted =
          replacement_fast_vertex_preference_accepted;
      replacement_request.replacement_fitted_vertices =
          replacement_fitted_vertices;
      replacement_request.replacement_original_max_vertices =
          replacement_original_max_vertices;
      replacement_request.replacement_source_min_vertices =
          replacement_source_min_vertices;
      replacement_request.replacement_estimated_candidate_keys =
          replacement_estimated_candidate_keys;
      replacement_request.replacement_estimated_original_keys =
          replacement_estimated_original_keys;
      const PostTemporalReplacementResult replacement_result =
          ProcessPostTemporalReplacement(
              replacement_request, &property_keys, &property_samples,
              &path_fit_note);
      if (replacement_result.cancelled) {
        return write_cancelled(replacement_result.cancel_phase.c_str(),
                               property_idx);
      }
      replacement_fitted_vertices =
          replacement_result.replacement_fitted_vertices;
      replacement_output_accepted =
          replacement_result.replacement_output_accepted;
      replacement_fast_vertex_preference_accepted =
          replacement_result.replacement_fast_vertex_preference_accepted;
    }
    PropertyCompletionRequest completion_request;
    completion_request.original_property_samples = &original_property_samples;
    completion_request.property_samples = &property_samples;
    completion_request.property_keys = &property_keys;
    completion_request.keys = &keys;
    completion_request.config = &samples.config;
    completion_request.comp = &samples.comp;
    completion_request.progress = &progress;
    completion_request.path_fit_note = &path_fit_note;
    completion_request.cancel_fn =
        [&]() { return CancelFileExists(options.cancel_file); };
    completion_request.property_idx = property_idx;
    completion_request.property_count = samples.properties.size();
    completion_request.temporal_optimization_enabled =
        temporal_optimization_enabled;
    completion_request.near_optimal_fast_path_applied =
        near_optimal_fast_path.applied;
    completion_request.decompose_paths = options.decompose_paths;
    completion_request.visible_outline_reference = visible_outline_reference;
    completion_request.replacement_output_accepted =
        replacement_output_accepted;
    completion_request.replacement_fast_vertex_preference_accepted =
        replacement_fast_vertex_preference_accepted;
    completion_request.emit_landmark_subpaths =
        options.emit_landmark_subpaths;
    completion_request.prop_ms = prop_ms;
    const PropertyCompletionResult completion_result =
        CompleteSolvedProperty(completion_request);
    if (completion_result.cancelled) {
      return write_cancelled(completion_result.cancel_phase.c_str(),
                             property_idx);
    }
  }

  return WriteCompletedSolveOutput(output_path,
                                   samples.request_id,
                                   keys,
                                   start,
                                   progress,
                                   diagnostics);
}

}  // namespace bbsolver
