#include "bbsolver/solve/solve_property_temporal_prelude.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <cstddef>

#include "bbsolver/motion_smooth/motion_smooth_reduction_gate.hpp"
#include "bbsolver/path/config/path_solver_config.hpp"
#include "bbsolver/progress/progress.hpp"
#include "bbsolver/routing/property_classification.hpp"
#include "bbsolver/routing/property_solver_routing.hpp"
#include "bbsolver/solve/solver_reporting.hpp"
#include "bbsolver/routing/solve_mode_policy.hpp"
#include "bbsolver/solve/static_key_cleanup.hpp"

namespace bbsolver {

PropertyTemporalPreludeState PreparePropertyTemporalPrelude(
    const PropertyTemporalPreludeRequest& request) {
  PropertyTemporalPreludeState state;
  state.temporal_source_samples = *request.original_property_samples;
  state.temporal_property_samples = *request.property_samples;
  state.temporal_config = *request.config;

  const SolverConfig& config = *request.config;
  const CompInfo& comp = *request.comp;
  const ProgressWriter& progress = *request.progress;
  ShapeFlatNearOptimalResult& near_optimal_fast_path =
      *request.near_optimal_fast_path;
  std::string& path_fit_note = *request.path_fit_note;

  const int final_static_boundary_sample =
      FindFinalStaticSuffixStartSample(
          request.original_property_samples->samples);
  if (!SolveModeUsesMotionSmoothing(config) &&
      final_static_boundary_sample >= 0) {
    const double final_static_boundary_t =
        request.original_property_samples
            ->samples[static_cast<std::size_t>(final_static_boundary_sample)]
            .t_sec;
    const int source_trimmed =
        TrimSamplesAfterTime(state.temporal_source_samples,
                             final_static_boundary_t);
    const int property_trimmed =
        TrimSamplesAfterTime(state.temporal_property_samples,
                             final_static_boundary_t);
    if (source_trimmed > 0 || property_trimmed > 0) {
      state.final_static_trim_note =
          "final_static_suffix_trim_for_solve=true"
          "; final_static_boundary_sample=" +
          std::to_string(final_static_boundary_sample) +
          "; final_static_boundary_frame=" +
          std::to_string(static_cast<int>(
              std::llround(final_static_boundary_t * comp.fps))) +
          "; final_static_source_samples_trimmed=" +
          std::to_string(source_trimmed) +
          "; final_static_solver_samples_trimmed=" +
          std::to_string(property_trimmed);
    }
  }

  state.motion_smooth_enabled = SolveModeUsesMotionSmoothing(config);
  state.temporal_optimization_enabled = SolveModeAllowsTemporal(config);
  const std::string property_start_phase =
      near_optimal_fast_path.applied
          ? "Preserving source shape keys for " +
                ProgressPropertyLabel(state.temporal_property_samples)
          : state.motion_smooth_enabled
          ? (SolveModeIsMotionPathSmooth(config)
                 ? "Smoothing motion path for "
                 : "Normalizing smooth motion for ") +
                ProgressPropertyLabel(state.temporal_property_samples)
          : (!state.temporal_optimization_enabled
                 ? "Preparing vertex-only keys for " +
                       ProgressPropertyLabel(state.temporal_property_samples)
                 : "Solving temporal keys for " +
                       ProgressPropertyLabel(state.temporal_property_samples));
  progress.Emit({
      {"event", "property_start"},
      {"phase", property_start_phase},
      {"progress", SolveProgressForPropertyStage(
                       request.property_idx, request.property_count, 0.40)},
      {"id", state.temporal_property_samples.property.id},
      {"display_name", ProgressPropertyLabel(state.temporal_property_samples)},
      {"i", request.property_idx},
      {"n", request.property_count},
      {"samples", state.temporal_property_samples.samples.size()},
  });

  const double effective_path_tolerance = EffectivePathTolerance(config);
  const bool canonical_path_temporal_allowed =
      state.temporal_optimization_enabled &&
      request.canonical_path_applied &&
      effective_path_tolerance >= 2.0;
  state.path_temporal_reduced_by_fit =
      state.temporal_optimization_enabled &&
      (request.replacement_path_applied || request.canonical_path_applied) &&
      (!request.canonical_path_applied || canonical_path_temporal_allowed) &&
      IsShapeFlatPath(*request.property_samples);
  if (request.canonical_path_applied && !canonical_path_temporal_allowed) {
    const std::string gate_note =
        "canonical_fit_path_temporal_skipped: effective_tolerance=" +
        std::to_string(effective_path_tolerance) +
        "; threshold=2.000000; plain_temporal_fallback_used";
    AppendJoinedNote(path_fit_note, gate_note);
  }
  if (state.path_temporal_reduced_by_fit) {
    state.replacement_temporal_max_gap =
        std::max(6, std::min(8, PathChildMaxGap(comp)));
    const std::string gap_note =
        std::string(request.canonical_path_applied
                        ? "canonical_fit_temporal_max_gap_samples="
                        : "replacement_temporal_max_gap_samples=") +
        std::to_string(state.replacement_temporal_max_gap);
    AppendJoinedNote(path_fit_note, gap_note);
  }
  if (state.path_temporal_reduced_by_fit) {
    state.temporal_config =
        ReplacementTemporalConfig(config, request.replacement_frame_fit_error);
    const std::string budget_note =
        std::string(request.canonical_path_applied
                        ? "canonical_fit_temporal_tolerance="
                        : "replacement_temporal_tolerance=") +
        std::to_string(state.temporal_config.tolerance) +
        (request.canonical_path_applied
             ? "; canonical_fit_frame_outline_error="
             : "; replacement_frame_outline_error=") +
        std::to_string(request.replacement_frame_fit_error) +
        (request.canonical_path_applied
             ? "; canonical_fit_shape_temporal_bezier=true"
             : "; replacement_shape_temporal_bezier=true");
    AppendJoinedNote(path_fit_note, budget_note);
  }

  PropertySolveRouteInput route_input;
  route_input.preserve_source_keys = near_optimal_fast_path.applied;
  route_input.motion_smooth_enabled = state.motion_smooth_enabled;
  route_input.temporal_optimization_enabled =
      state.temporal_optimization_enabled;
  route_input.path_temporal_reduced_by_fit =
      state.path_temporal_reduced_by_fit;
  route_input.decompose_paths = request.decompose_paths;
  route_input.decompose_candidate_is_shape_flat =
      IsShapeFlatPath(*request.property_samples);
  state.property_solve_route = ChoosePropertySolveRoute(route_input);

  return state;
}

}  // namespace bbsolver
