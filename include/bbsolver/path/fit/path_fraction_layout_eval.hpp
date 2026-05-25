#pragma once

// Internal solver header that exposes the canonical-layout evaluator used by
// the outline-fraction expansion and feature-layout subsystems. The struct
// and function previously lived in path_frame_fit.cpp's anonymous namespace;
// they now have external linkage so separately-translated modules can call into
// the same evaluator without duplicating the dense-polyline pipeline that
// drives it.
//
// Behavior: EvaluateFractionLayout replays a candidate outline-fraction
// layout against every supplied source frame via FitShapeFlatFrameAtFractions
// and reports the worst per-frame outline error plus, for the worst frame,
// the source-outline fraction nearest to the directed-Hausdorff witness.
// This is pure-math evaluation: no DiagnosticsWriter, no progress events, no
// cancellation. Acceptance failures are conveyed through the FractionLayoutEvaluation
// result's warning string and ok flag exactly as before.

#include <limits>
#include <string>
#include <vector>

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

namespace bbsolver {

struct FractionLayoutEvaluation {
  bool ok = false;
  std::string warning;
  double max_error = std::numeric_limits<double>::infinity();
  double worst_fraction = 0.0;
  bool has_worst_fraction = false;
};

// Replay-test every supplied shape_flat frame at the candidate outline
// fractions using FitShapeFlatFrameAtFractions and return the worst-case
// max_outline_error across all frames, the source-outline fraction nearest
// the directed-Hausdorff witness, and an ok/warning verdict. Returns ok=false
// with a non-empty warning when the layout is unreplayable for any frame.
FractionLayoutEvaluation EvaluateFractionLayout(
    const std::vector<std::vector<double>>& shape_flat_frames,
    const std::vector<double>& fractions,
    const PathFrameFitOptions& fit_options);

}  // namespace bbsolver
