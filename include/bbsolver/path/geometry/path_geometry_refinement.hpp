#pragma once

#include "bbsolver/domain.hpp"

#include <string>
#include <vector>

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

namespace bbsolver {

// Result of the per-frame geometry refinement pass (stage 2).
struct PathGeometryRefinementResult {
  // True unless a source frame is malformed or FitShapeFlatFrameAtFractions
  // signals an internal error. False does not imply tolerance exceeded.
  bool ok = false;
  // True when all frames were refined and refined_samples is populated. False
  // when any frame failed (malformed, target_not_met, or exceeds tolerance).
  bool applied = false;
  // Maximum per-frame outline error across refined frames. 0 when !applied.
  double refined_max_error = 0.0;
  // Number of source frames successfully re-fitted.
  int frames_refined = 0;
  // Human-readable status; starts with "ok" or "failed_*".
  std::string notes;
  // Populated only when applied=true: source frames re-fitted at winning_fractions.
  PropertySamples refined_samples;
};

// Stage-2 geometry refinement: re-fit every source sample using the supplied
// fraction layout and options. This verifies that the winning fraction layout
// from stage-1 can reproduce each original source frame within the given
// tolerance, producing the best-quality per-frame geometry at that topology.
//
// When applied=true, refined_samples replaces the stage-1 samples as input to
// the temporal solver (stage 3); the fitter's tangent optimisation may produce
// slightly better per-frame shapes than the coherence pass. When applied=false,
// callers should keep the stage-1 samples unchanged.
//
// Returns ok=false with a diagnostic note if any source frame is malformed or
// FitShapeFlatFrameAtFractions cannot map the winning fractions.
PathGeometryRefinementResult RefinePathGeometryAtFractions(
    const PropertySamples& original,
    const std::vector<double>& winning_fractions,
    const PathFrameFitOptions& options);

}  // namespace bbsolver
