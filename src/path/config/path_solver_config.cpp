#include "bbsolver/path/config/path_solver_config.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/path/config/path_gap_policy.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"

#include <algorithm>
#include <cmath>

namespace bbsolver {

int PathChildMaxGap(const CompInfo& comp) {
  return InteractivePathMaxGap(comp);
}

SolverConfig PathChildConfig(SolverConfig config) {
  config.tolerance = std::max(config.tolerance * 0.1, 1e-6);
  if (config.tolerance_screen_px > 0.0) {
    config.tolerance_screen_px =
        std::max(config.tolerance_screen_px * 0.1, 1e-6);
  }
  return config;
}

double EffectivePathTolerance(const SolverConfig& config) {
  double tolerance = 0.0;
  if (std::isfinite(config.tolerance)) {
    tolerance = std::max(tolerance, config.tolerance);
  }
  if (std::isfinite(config.tolerance_screen_px)) {
    tolerance = std::max(tolerance, config.tolerance_screen_px);
  }
  return std::max(tolerance, 0.0);
}

PathFrameFitOptions ReplacementFrameFitOptions(const SolverConfig& config) {
  PathFrameFitOptions options;
  // Leave error budget for temporal interpolation. If per-frame fitting spends
  // the entire user tolerance, the final keyframed reduced path can exceed that
  // tolerance against the original source outline.
  options.outline_tolerance = EffectivePathTolerance(config) * 0.5;
  return options;
}

PathFrameFitOptions VisibleOutlineFrameFitOptions(const SolverConfig& config) {
  PathFrameFitOptions options = ReplacementFrameFitOptions(config);
  options.source_vertices_are_semantic_anchors = false;
  return options;
}

PathTemporalValidationOptions ReplacementPathTemporalValidationOptions(
    const SolverConfig& config,
    bool visible_outline_reference) {
  PathTemporalValidationOptions options;
  options.frame_fit_options = ReplacementFrameFitOptions(config);
  options.frame_fit_options.source_vertices_are_semantic_anchors =
      !visible_outline_reference;
  options.frame_fit_options.outline_tolerance = EffectivePathTolerance(config);
  return options;
}

SolverConfig ReplacementTemporalConfig(SolverConfig config,
                                       double frame_outline_error) {
  (void)frame_outline_error;
  // The temporal replacement stage is validated directly against the original
  // source outline in stage 4. Do not subtract the per-frame topology/refine
  // error from this search budget: doing so rejects segments that still pass
  // the final source-outline tolerance.
  config.allow_shape_temporal_bezier = true;
  return config;
}

}  // namespace bbsolver
