#include "bbsolver/solve/solve_property_post_processing.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <utility>

#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/path/bridge_prune/path_bridge_prune.hpp"
#include "bbsolver/path/reduction/path_post_solve_reduction.hpp"
#include "bbsolver/progress/progress.hpp"
#include "bbsolver/routing/property_classification.hpp"
#include "bbsolver/routing/solve_mode_policy.hpp"
#include "bbsolver/solve/solver_observability.hpp"
#include "bbsolver/solve/solver_reporting.hpp"
#include "bbsolver/solve/static_key_cleanup.hpp"
#include "bbsolver/temporal/refit/temporal_refit.hpp"
#include "bbsolver/temporal/refit/temporal_refit_gate.hpp"

namespace bbsolver {

PropertyPostSolveProcessingResult ProcessSolvedPropertyPostSolve(
    const PropertyPostSolveProcessingRequest& request) {
  const PropertySamples& original_property_samples =
      *request.original_property_samples;
  PropertySamples& property_samples = *request.property_samples;
  PropertyKeys& property_keys = *request.property_keys;
  const SolverConfig& config = *request.config;
  const CompInfo& comp = *request.comp;
  const ProgressWriter& progress = *request.progress;
  std::string& path_fit_note = *request.path_fit_note;
  const auto cancelled = [&]() {
    return request.cancel_fn && request.cancel_fn();
  };

  const bool allow_post_solve_vertex_reduction =
      SolveModeAllowsVertex(config) &&
      !request.near_optimal_fast_path_applied &&
      !request.decompose_paths &&
      IsShapeFlatPath(original_property_samples) &&
      property_keys.converged &&
      !request.replacement_fast_vertex_preference_accepted &&
      (!request.replacement_output_accepted ||
       config.path_replacement_prefer_vertices);
  if (allow_post_solve_vertex_reduction) {
    progress.Emit(PropertyProgressEvent(
        "post_solve_vertex_reduction_start",
        "Trying post-solve path vertex reduction for " +
            ProgressPropertyLabel(original_property_samples),
        request.property_idx,
        request.property_count,
        0.81,
        original_property_samples));
    PostSolvePathVertexReductionResult post_reduction =
        TryPostSolvePathVertexReduction(
            original_property_samples, property_keys, config, comp, &progress,
            request.property_idx, request.property_count, request.cancel_fn,
            !request.visible_outline_reference);
    if (post_reduction.notes == "cancelled" || cancelled()) {
      return {true, "post_solve_vertex_reduction"};
    }
    if (post_reduction.accepted) {
      property_keys = std::move(post_reduction.keys);
      property_samples = original_property_samples;
      AppendSolverNote(property_keys, post_reduction.notes);
    } else if (post_reduction.attempted) {
      AppendSolverNote(property_keys, post_reduction.notes);
    }
    progress.Emit({
        {"event", "post_solve_vertex_reduction_done"},
        {"phase", std::string("Post-solve path vertex reduction ") +
                      (post_reduction.accepted ? "accepted for "
: "rejected for ") +
                      ProgressPropertyLabel(original_property_samples)},
        {"progress", SolveProgressForPropertyStage(
                         request.property_idx, request.property_count, 0.90)},
        {"id", original_property_samples.property.id},
        {"display_name", ProgressPropertyLabel(original_property_samples)},
        {"i", request.property_idx},
        {"n", request.property_count},
        {"accepted", post_reduction.accepted},
        {"source_vertices", post_reduction.source_vertices},
        {"fitted_vertices", post_reduction.fitted_vertices},
        {"max_outline_error", post_reduction.max_outline_error},
    });
  }

  if (!path_fit_note.empty()) {
    AppendJoinedNote(property_keys.notes, path_fit_note);
  }

  const StaticKeyRunCollapseResult static_collapse =
      CollapseRedundantStaticKeyRuns(
          original_property_samples, config, comp, property_keys);
  if (static_collapse.attempted) {
    progress.Emit({
        {"event", "static_key_run_collapse"},
        {"phase", std::string("Static key cleanup ") +
                      (static_collapse.accepted ? "accepted for "
: "rejected for ") +
                      ProgressPropertyLabel(original_property_samples)},
        {"progress", SolveProgressForPropertyStage(
                         request.property_idx, request.property_count, 0.91)},
        {"id", original_property_samples.property.id},
        {"display_name", ProgressPropertyLabel(original_property_samples)},
        {"i", request.property_idx},
        {"n", request.property_count},
        {"accepted", static_collapse.accepted},
        {"runs_collapsed", static_collapse.runs_collapsed},
        {"keys_removed", static_collapse.keys_removed},
        {"max_err", static_collapse.max_err},
        {"max_err_screen_px", static_collapse.max_err_screen_px},
    });
  }

  const FinalStaticBoundaryAnchorResult final_static_anchor =
      AnchorFinalStaticBoundary(
          original_property_samples, config, comp, property_keys);
  if (final_static_anchor.attempted) {
    progress.Emit({
        {"event", "final_static_boundary_anchor"},
        {"phase", std::string("Final static boundary anchor ") +
                      (final_static_anchor.accepted ? "accepted for "
: "rejected for ") +
                      ProgressPropertyLabel(original_property_samples)},
        {"progress", SolveProgressForPropertyStage(
                         request.property_idx, request.property_count, 0.915)},
        {"id", original_property_samples.property.id},
        {"display_name", ProgressPropertyLabel(original_property_samples)},
        {"i", request.property_idx},
        {"n", request.property_count},
        {"accepted", final_static_anchor.accepted},
        {"boundary_sample", final_static_anchor.boundary_sample},
        {"boundary_frame", static_cast<int>(std::llround(
                               final_static_anchor.boundary_t_sec *
                               comp.fps))},
        {"suffix_samples", final_static_anchor.suffix_samples},
        {"tail_keys_removed", final_static_anchor.tail_keys_removed},
        {"max_err", final_static_anchor.max_err},
        {"max_err_screen_px", final_static_anchor.max_err_screen_px},
    });
  }

  if (!request.near_optimal_fast_path_applied &&
      PipelineAllowsTemporalRefit(original_property_samples, property_keys,
                                  config)) {
    TemporalRefitOptions refit_options;
    refit_options.cancel_fn = request.cancel_fn;
    refit_options.max_gap_samples = config.path_specific_max_gap;
    refit_options.progress_fn =
        [&](const PlacementProgress& refit_progress) {
          const double ratio =
              refit_progress.step_total > 0
                  ? static_cast<double>(refit_progress.step_index) /
                        static_cast<double>(refit_progress.step_total)
: 0.0;
          progress.Emit({
              {"event", refit_progress.stage},
              {"phase", "Temporal refit for " +
                            ProgressPropertyLabel(original_property_samples)},
              {"progress", SolveProgressForPropertyStage(
                               request.property_idx,
                               request.property_count,
                               0.916 + 0.035 *
                                           std::clamp(ratio, 0.0, 1.0))},
              {"id", original_property_samples.property.id},
              {"display_name", ProgressPropertyLabel(original_property_samples)},
              {"i", request.property_idx},
              {"n", request.property_count},
              {"step", refit_progress.step_index},
              {"steps", refit_progress.step_total},
              {"sample_index", refit_progress.sample_index},
              {"samples", refit_progress.samples},
              {"segments_tried", refit_progress.segments_tried},
              {"segments_feasible", refit_progress.segments_feasible},
          });
        };
    TemporalRefitResult temporal_refit =
        TryTemporalRefitKeyReduction(
            original_property_samples, property_keys, config, comp,
            refit_options);
    if (temporal_refit.rejection_reason == "cancelled" || cancelled()) {
      return {true, "temporal_refit"};
    }
    if (temporal_refit.attempted) {
      if (temporal_refit.accepted) {
        property_keys = std::move(temporal_refit.keys);
      }
      AppendSolverNote(property_keys, temporal_refit.notes);
      progress.Emit({
          {"event", "temporal_refit_done"},
          {"phase", std::string("Temporal refit ") +
                        (temporal_refit.accepted ? "accepted for "
: "rejected for ") +
                        ProgressPropertyLabel(original_property_samples)},
          {"progress", SolveProgressForPropertyStage(
                           request.property_idx, request.property_count, 0.952)},
          {"id", original_property_samples.property.id},
          {"display_name", ProgressPropertyLabel(original_property_samples)},
          {"i", request.property_idx},
          {"n", request.property_count},
          {"accepted", temporal_refit.accepted},
          {"input_keys", temporal_refit.input_key_count},
          {"output_keys", temporal_refit.output_key_count},
          {"max_err", temporal_refit.max_err},
          {"max_err_screen_px", temporal_refit.max_err_screen_px},
          {"reason", temporal_refit.rejection_reason},
      });
    }
  }

  const std::string accuracy_gate_note =
      request.temporal_optimization_enabled
          ? AccuracyGateOptimizationNote(
                original_property_samples, config, property_keys)
: "";
  if (!accuracy_gate_note.empty()) {
    AppendSolverNote(property_keys, accuracy_gate_note);
    progress.Emit({
        {"event", "optimization_diagnostic"},
        {"phase", "Optimization limited by current accuracy gate for " +
                      ProgressPropertyLabel(original_property_samples)},
        {"progress", SolveProgressForPropertyStage(
                         request.property_idx, request.property_count, 0.72)},
        {"id", original_property_samples.property.id},
        {"display_name", ProgressPropertyLabel(original_property_samples)},
        {"i", request.property_idx},
        {"n", request.property_count},
        {"output_keys", property_keys.keys.size()},
        {"source_samples", original_property_samples.samples.size()},
        {"tolerance", config.tolerance},
        {"screen_px", config.tolerance_screen_px},
    });
  }

  if (config.verbose) {
    std::cout << PropertyName(property_samples)
              << ": K=" << property_keys.keys.size()
              << " max_err=" << property_keys.max_err
              << " max_err_screen_px=" << property_keys.max_err_screen_px
              << " solve_time_ms=" << request.prop_ms << '\n';
  }

  return {};
}

}  // namespace bbsolver
