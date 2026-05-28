#include "bbsolver/path/geometry/path_geometry_refinement.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/path/temporal/path_temporal_progress.hpp"

namespace bbsolver {

PathGeometryRefinementResult RefinePathGeometryAtFractions(
    const PropertySamples& original,
    const std::vector<double>& winning_fractions,
    const PathFrameFitOptions& options) {
  PathGeometryRefinementResult result;

  if (original.samples.empty()) {
    result.notes = "no_source_samples";
    return result;
  }
  if (winning_fractions.empty()) {
    result.notes = "no_winning_fractions";
    return result;
  }

  result.ok = true;
  PropertySamples refined = original;
  refined.property.dimensions = 2 + 6 * static_cast<int>(winning_fractions.size());
  refined.samples.clear();
  refined.samples.reserve(original.samples.size());
  double max_error = 0.0;
  const int expected_count = static_cast<int>(winning_fractions.size());
  // Computed once; used to accept frames where max_outline_error == 0 even when
  // fit.applied=false (source vertex count == expected_count).
  const double refine_tolerance = std::max(options.outline_tolerance, 0.0);

  for (const Sample& src: original.samples) {
    if (!PathTemporalShapeFlatIsValid(src.v)) {
      result.notes =
          "failed_malformed_source_at_t=" + std::to_string(src.t_sec);
      result.ok = false;
      return result;
    }

    const PathFrameFitResult fit =
        FitShapeFlatFrameAtFractions(src.v, winning_fractions, options);

    // Structural check: ok, correct output vertex count, target fraction count
    // met. Note: fit.applied may be false when source_vertex_count ==
    // expected_count (a variable-topology frame already at the target vertex
    // count). That is not a failure — the tolerance check below guards quality.
    if (!fit.ok || !fit.target_met || fit.fitted_vertex_count != expected_count) {
      result.notes =
          "failed_at_t=" + std::to_string(src.t_sec) +
          (fit.warning.empty() ? std::string{}: "; " + fit.warning);
      result.ok = false;
      return result;
    }
    // Separate tolerance check: catches both the "exceeds tolerance" case
    // (fit.applied=false because error > tolerance) and any regression where
    // a structurally valid fit still deviates more than the caller allows.
    if (fit.max_outline_error > refine_tolerance + 1e-9) {
      result.notes = "failed_at_t=" + std::to_string(src.t_sec) +
                     "; exceeds_tolerance; max_outline_error=" +
                     std::to_string(fit.max_outline_error);
      result.ok = false;
      return result;
    }

    max_error = std::max(max_error, fit.max_outline_error);

    Sample s;
    s.t_sec = src.t_sec;
    s.v     = fit.fitted;
    refined.samples.push_back(std::move(s));
    ++result.frames_refined;
  }

  result.refined_max_error = max_error;
  result.refined_samples   = std::move(refined);
  result.applied           = true;
  result.notes =
      "ok; frames=" + std::to_string(result.frames_refined) +
      "; refined_max_error=" + std::to_string(result.refined_max_error);
  return result;
}

}  // namespace bbsolver
