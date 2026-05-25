#include "bbsolver/routing/property_route_solver.hpp"
#include "bbsolver/domain.hpp"

#include <stdexcept>
#include <string>
#include <utility>

#include "bbsolver/app/cli_options.hpp"
#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/motion_smooth/motion_smooth_dispatch.hpp"
#include "bbsolver/motion_smooth/motion_smooth_reduction_gate.hpp"
#include "bbsolver/path/decompose/path_decomposed_solver.hpp"
#include "bbsolver/solve/plain_property_solver.hpp"
#include "bbsolver/progress/progress.hpp"
#include "bbsolver/routing/property_classification.hpp"
#include "bbsolver/routing/property_solver_routing.hpp"
#include "bbsolver/samples/raw_frame_keys.hpp"
#include "bbsolver/replacement_temporal/replacement_temporal_solve_options.hpp"
#include "bbsolver/replacement_temporal/replacement_temporal_solver.hpp"

namespace bbsolver {
namespace {

void RequireRouteRequest(const PropertyRouteSolveRequest& request) {
  if (request.original_property_samples == nullptr ||
      request.property_samples == nullptr ||
      request.temporal_source_samples == nullptr ||
      request.temporal_property_samples == nullptr ||
      request.config == nullptr ||
      request.temporal_config == nullptr ||
      request.comp == nullptr ||
      request.options == nullptr ||
      request.progress == nullptr ||
      request.near_optimal_fast_path == nullptr) {
    throw std::invalid_argument("property route solve request is incomplete");
  }
}

}  // namespace

PropertyKeys SolvePropertyRoute(const PropertyRouteSolveRequest& request) {
  RequireRouteRequest(request);

  const PropertySamples& original_property_samples =
      *request.original_property_samples;
  const PropertySamples& property_samples = *request.property_samples;
  const PropertySamples& temporal_source_samples =
      *request.temporal_source_samples;
  const PropertySamples& temporal_property_samples =
      *request.temporal_property_samples;
  const SolverConfig& config = *request.config;
  const SolverConfig& temporal_config = *request.temporal_config;
  const CompInfo& comp = *request.comp;
  const SolveOptions& options = *request.options;
  const ProgressWriter& progress = *request.progress;
  ShapeFlatNearOptimalResult& near_optimal_fast_path =
      *request.near_optimal_fast_path;

  switch (request.route) {
    case PropertySolveRoute::PreserveSourceKeys: {
      PropertyKeys property_keys = std::move(near_optimal_fast_path.keys);
      progress.Emit({
          {"event", "optimization_diagnostic"},
          {"phase", "Shape path already near optimal; use Motion Smooth for "
                    "trajectory smoothing on " +
                        ProgressPropertyLabel(original_property_samples)},
          {"progress", SolveProgressForPropertyStage(
                           request.property_idx,
                           request.property_count,
                           0.42)},
          {"id", original_property_samples.property.id},
          {"display_name", ProgressPropertyLabel(original_property_samples)},
          {"i", request.property_idx},
          {"n", request.property_count},
          {"source_key_count", near_optimal_fast_path.source_key_count},
          {"source_vertices", near_optimal_fast_path.source_vertices},
          {"source_samples", near_optimal_fast_path.source_samples},
          {"notes", near_optimal_fast_path.note},
      });
      return property_keys;
    }
    case PropertySolveRoute::MotionSmooth:
      return MotionSmoothKeys(temporal_property_samples, config, comp, options);
    case PropertySolveRoute::FrameKeyFallback:
      if (IsShapeFlatPath(temporal_property_samples)) {
        return ShapeFlatFrameKeyFallback(
            temporal_property_samples,
            "solve_mode_vertex_only; temporal_optimization_skipped=true");
      }
      return RawFrameKeyFallback(
          temporal_property_samples,
          "solve_mode_vertex_only; temporal_optimization_skipped=true");
    case PropertySolveRoute::ReplacementShapeFlatTemporal: {
      auto replacement_temporal_progress =
          [&](const PlacementProgress& placement) {
            progress.Emit(PlacementProgressEvent(
                "replacement_temporal_solve_progress",
                request.canonical_path_applied
                    ? "Solving canonical fitted path temporal placement"
                    : "Solving replacement temporal placement",
                request.property_idx,
                request.property_count,
                0.40,
                0.70,
                property_samples,
                placement));
          };
      return SolveReplacementShapeFlatTemporal(
          temporal_source_samples,
          temporal_property_samples,
          temporal_config,
          comp,
          ReplacementTemporalOptions(
              temporal_config,
              request.replacement_temporal_max_gap,
              options,
              replacement_temporal_progress));
    }
    case PropertySolveRoute::PathDecomposed:
      return SolvePathDecomposedProperty(
          temporal_property_samples,
          temporal_config,
          comp,
          options,
          progress,
          request.property_idx,
          request.property_count);
    case PropertySolveRoute::PlainTemporal: {
      auto plain_temporal_progress = [&](const PlacementProgress& placement) {
        progress.Emit(PlacementProgressEvent(
            "temporal_solve_progress",
            "Solving temporal placement",
            request.property_idx,
            request.property_count,
            0.40,
            0.70,
            temporal_property_samples,
            placement));
      };
      return SolvePlainProperty(
          temporal_property_samples,
          temporal_config,
          comp,
          options,
          request.replacement_temporal_max_gap,
          plain_temporal_progress);
    }
  }
  throw std::invalid_argument("unknown property solve route");
}

}  // namespace bbsolver
