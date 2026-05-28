#include "bbsolver/solve/solve_path_preparation.hpp"
#include "bbsolver/domain.hpp"

#include <chrono>
#include <string>
#include <utility>

#include "bbsolver/motion_smooth/motion_smooth_reduction_gate.hpp"
#include "bbsolver/path/fit/path_fit.hpp"
#include "bbsolver/path/config/path_solver_config.hpp"
#include "bbsolver/path/geometry/path_geometry_refinement.hpp"
#include "bbsolver/path/geometry/path_visible_outline_prepass.hpp"
#include "bbsolver/path/replacement/path_replacement_solver.hpp"
#include "bbsolver/progress/progress.hpp"
#include "bbsolver/routing/property_classification.hpp"
#include "bbsolver/solve/solver_observability.hpp"
#include "bbsolver/solve/solver_reporting.hpp"
#include "bbsolver/routing/solve_mode_policy.hpp"

namespace bbsolver {

PathSolvePreparationState PreparePathSolveInputs(
    const PathSolvePreparationRequest& request) {
  PathSolvePreparationState state;
  state.original_property_samples = *request.source_property;
  state.property_samples = state.original_property_samples;
  const SolverConfig& config = *request.config;
  const CompInfo& comp = *request.comp;
  const ProgressWriter& progress = *request.progress;

  progress.Emit(PropertyProgressEvent(
      "property_prepare",
      "Preparing " + ProgressPropertyLabel(state.original_property_samples),
      request.property_idx,
      request.property_count,
      0.02,
      state.original_property_samples));

  state.near_optimal_fast_path =
      TryShapeFlatAlreadyNearOptimalFastPath(
          state.original_property_samples, config, comp);
  const bool vertex_only_outline_mode =
      SolveModeAllowsVertex(config) &&
      !SolveModeAllowsTemporal(config) &&
      config.allow_path_spatial_fit;
  const bool replacement_outline_mode =
      SolveModeAllowsSpatialTopology(config) &&
      config.allow_path_replacement_fit;
  if (!state.near_optimal_fast_path.applied &&
      (replacement_outline_mode || vertex_only_outline_mode) &&
      IsShapeFlatPath(state.original_property_samples)) {
    progress.Emit(PropertyProgressEvent(
        "visible_outline_prepass_start",
        "Checking visible filled outline for " +
            ProgressPropertyLabel(state.original_property_samples),
        request.property_idx,
        request.property_count,
        0.06,
        state.original_property_samples));
    const auto visible_outline_start = std::chrono::steady_clock::now();
    VisibleOutlinePrepassResult visible_outline =
        TryVisibleOutlinePrepass(state.original_property_samples, config);
    const double visible_outline_ms = MillisecondsSince(visible_outline_start);
    if (visible_outline.applied) {
      state.original_property_samples = std::move(visible_outline.samples);
      state.property_samples = state.original_property_samples;
      state.visible_outline_reference = true;
      state.path_fit_note = visible_outline.notes;
      progress.Emit({
          {"event", "visible_outline_prepass"},
          {"phase", "Visible filled outline extracted for " +
                        ProgressPropertyLabel(state.original_property_samples)},
          {"progress", SolveProgressForPropertyStage(
                           request.property_idx, request.property_count, 0.09)},
          {"id", state.original_property_samples.property.id},
          {"display_name",
           ProgressPropertyLabel(state.original_property_samples)},
          {"i", request.property_idx},
          {"n", request.property_count},
          {"source_min_vertices", visible_outline.source_min_vertices},
          {"source_max_vertices", visible_outline.source_max_vertices},
          {"outline_min_vertices", visible_outline.outline_min_vertices},
          {"outline_max_vertices", visible_outline.outline_max_vertices},
          {"fitted_vertices", visible_outline.fitted_vertices},
          {"max_outline_error", visible_outline.max_outline_error},
          {"ms", visible_outline_ms},
      });
    } else if (!visible_outline.notes.empty()) {
      state.path_fit_note = visible_outline.notes;
      progress.Emit({
          {"event", "visible_outline_prepass_skipped"},
          {"phase", "Visible filled outline not used for " +
                        ProgressPropertyLabel(state.original_property_samples)},
          {"progress", SolveProgressForPropertyStage(
                           request.property_idx, request.property_count, 0.09)},
          {"id", state.original_property_samples.property.id},
          {"display_name",
           ProgressPropertyLabel(state.original_property_samples)},
          {"i", request.property_idx},
          {"n", request.property_count},
          {"notes", visible_outline.notes},
          {"ms", visible_outline_ms},
      });
    }
  }

  if (!state.near_optimal_fast_path.applied &&
      SolveModeAllowsSpatialTopology(config) &&
      config.allow_path_replacement_fit &&
      IsShapeFlatPath(state.original_property_samples)) {
    progress.Emit(PropertyProgressEvent(
        "path_replacement_fit_start",
        "Fitting replacement path topology for " +
            ProgressPropertyLabel(state.original_property_samples),
        request.property_idx,
        request.property_count,
        0.10,
        state.original_property_samples));
    const auto replacement_fit_start = std::chrono::steady_clock::now();
    ReplacementPathFitResult fit =
        FitReplacementPathProperty(
            state.original_property_samples,
            config,
            &progress,
            request.property_idx,
            request.property_count,
            !state.visible_outline_reference);
    const double replacement_fit_ms = MillisecondsSince(replacement_fit_start);
    state.path_fit_note = fit.notes;
    if (fit.applied) {
      state.property_samples = std::move(fit.samples);
      state.replacement_path_applied = true;
      state.replacement_fitted_vertices = fit.fitted_vertices;
      state.replacement_original_max_vertices = fit.source_max_vertices;
      state.replacement_source_min_vertices = fit.source_min_vertices;
      state.replacement_estimated_candidate_keys = fit.estimated_candidate_keys;
      state.replacement_estimated_original_keys = fit.estimated_original_keys;
      state.replacement_winning_fractions = std::move(fit.winning_fractions);
      state.replacement_topology_fit_error = fit.max_outline_error;
      state.replacement_frame_fit_error = fit.max_outline_error;
      progress.Emit({
          {"event", "path_replacement_fit"},
          {"phase", "Replacement topology fitted for " +
                        ProgressPropertyLabel(state.property_samples)},
          {"progress", SolveProgressForPropertyStage(
                           request.property_idx, request.property_count, 0.22)},
          {"id", state.property_samples.property.id},
          {"display_name", ProgressPropertyLabel(state.property_samples)},
          {"i", request.property_idx},
          {"n", request.property_count},
          {"source_min_vertices", fit.source_min_vertices},
          {"source_max_vertices", fit.source_max_vertices},
          {"auto_min_vertices", fit.auto_min_vertices},
          {"auto_max_vertices", fit.auto_max_vertices},
          {"fitted_vertices", fit.fitted_vertices},
          {"max_outline_error", fit.max_outline_error},
          {"ms", replacement_fit_ms},
      });
    } else {
      progress.Emit(PropertyProgressEvent(
          "path_replacement_fit_skipped",
          "Replacement topology not accepted for " +
              ProgressPropertyLabel(state.original_property_samples),
          request.property_idx,
          request.property_count,
          0.22,
          state.original_property_samples));
    }
  }

  if (!state.near_optimal_fast_path.applied &&
      !state.replacement_path_applied &&
      SolveModeAllowsSpatialTopology(config) &&
      config.allow_path_spatial_fit &&
      IsShapeFlatPath(state.original_property_samples)) {
    progress.Emit(PropertyProgressEvent(
        "path_fit_start",
        "Fitting canonical path topology for " +
            ProgressPropertyLabel(state.original_property_samples),
        request.property_idx,
        request.property_count,
        0.12,
        state.original_property_samples));
    PathFitResult fit =
        FitCanonicalPathProperty(state.original_property_samples, config);
    AppendJoinedNote(state.path_fit_note, fit.notes);
    if (fit.applied) {
      state.property_samples = std::move(fit.samples);
      state.canonical_path_applied = true;
      state.replacement_frame_fit_error = fit.max_outline_error;
      state.replacement_fitted_vertices = fit.fitted_vertex_count;
      state.replacement_original_max_vertices = fit.source_vertex_count;
      progress.Emit({
          {"event", "path_fit"},
          {"phase", "Canonical path topology fitted for " +
                        ProgressPropertyLabel(state.property_samples)},
          {"progress", SolveProgressForPropertyStage(
                           request.property_idx, request.property_count, 0.22)},
          {"id", state.property_samples.property.id},
          {"display_name", ProgressPropertyLabel(state.property_samples)},
          {"i", request.property_idx},
          {"n", request.property_count},
          {"source_vertices", fit.source_vertex_count},
          {"fitted_vertices", fit.fitted_vertex_count},
          {"locked_vertices", fit.locked_vertex_count},
          {"max_outline_error", fit.max_outline_error},
      });
    } else {
      progress.Emit(PropertyProgressEvent(
          "path_fit_skipped",
          "Canonical path topology not accepted for " +
              ProgressPropertyLabel(state.original_property_samples),
          request.property_idx,
          request.property_count,
          0.22,
          state.original_property_samples));
    }
  }

  if (state.replacement_path_applied &&
      !state.replacement_winning_fractions.empty()) {
    progress.Emit(PropertyProgressEvent(
        "path_geometry_refine_start",
        "Refining fitted path geometry for " +
            ProgressPropertyLabel(state.original_property_samples),
        request.property_idx,
        request.property_count,
        0.28,
        state.original_property_samples));
    const PathGeometryRefinementResult refinement =
        RefinePathGeometryAtFractions(
            state.original_property_samples,
            state.replacement_winning_fractions,
            state.visible_outline_reference
                ? VisibleOutlineFrameFitOptions(config)
: ReplacementFrameFitOptions(config));
    if (refinement.applied) {
      state.property_samples = std::move(refinement.refined_samples);
      state.replacement_frame_fit_error = refinement.refined_max_error;
    }
    const std::string stage2_note =
        "topology_fit_error=" +
        std::to_string(state.replacement_topology_fit_error) +
        "; refined_fit_error=" + std::to_string(refinement.refined_max_error) +
        "; stage2_refinement=" + refinement.notes;
    AppendJoinedNote(state.path_fit_note, stage2_note);
    progress.Emit(PropertyProgressEvent(
        "path_geometry_refine_done",
        "Finished path geometry refinement for " +
            ProgressPropertyLabel(state.property_samples),
        request.property_idx,
        request.property_count,
        0.34,
        state.property_samples));
  }

  return state;
}

}  // namespace bbsolver
