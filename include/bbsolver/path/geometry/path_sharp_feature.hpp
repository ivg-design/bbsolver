#pragma once

// Sharp-feature classifier for shape_flat source vertices. Distinguishes
// geometric corners (turn angle past kSharpTurnRadians) from zero-tangent
// semantic cues (small turn but explicit zero in/out tangents) and from
// tangent-locked vertices (turn past kHardTangentLockTurnRadians).
//
// Pure leaf: no DiagnosticsWriter, no progress events, no acceptance state.
// Failure modes (out-of-range vertex, degenerate triangle, non-semantic-anchor
// options) are surfaced through the SharpFeatureDecision struct fields
// (`required=false`, `zero_tangent_cue=false`, `turn_radians=0.0` or infinity).
// Diagnostics ownership: **caller-owned** — the panel side / per-frame
// validator decides whether a sharp/locked classification deserves a status
// note; this module never emits one itself.
//
// Shared by the per-frame feature-anchor extractor and the main fitter's
// BuildFlat call site so corner thresholds have one source of truth.

#include <vector>

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit_geometry.hpp"

namespace bbsolver {
namespace pff_sharp {

// Geometric corner cutoff (radians). About 29 degrees; preserves wrist-like
// bends as sharp anchors during reduction.
constexpr double kSharpTurnRadians = 0.50;
// Tangent-locked cutoff (radians). About 54 degrees; vertices past this are
// locked to zero in/out tangents in the rebuilt shape_flat.
constexpr double kHardTangentLockTurnRadians = 0.95;
// Zero-tangent semantic-anchor cutoff (radians). Small turns that occur at
// vertices with explicit zero in/out tangents still register as sharp.
constexpr double kSemanticSharpTurnRadians = 0.25;
// Tangent-length tolerance for the zero-tangent classifier (unit-space).
constexpr double kZeroTangentEpsilon = 1e-6;

// Classifier verdict for a single source vertex.
struct SharpFeatureDecision {
  bool required = false;        // geometric corner OR zero-tangent cue
  double turn_radians = 0.0;    // 0 on degenerate; infinity for open endpoints
  bool zero_tangent_cue = false;
};

// Interior turn angle (in radians, in [0, π]) at the given source vertex.
// Closed paths wrap; open paths return infinity for the two endpoint slots.
// Returns infinity when adjacent edges are degenerate (length below 1e-9).
double TurnAngleAtSourceVertex(const std::vector<double>& flat,
                               int vertex_index,
                               const pff_geom::DecodedShape& decoded);

// True iff the source vertex has both in and out tangent magnitudes below
// kZeroTangentEpsilon. Out-of-range vertices return false.
bool HasZeroTangentsAtSourceVertex(const std::vector<double>& flat,
                                   int vertex_index,
                                   const pff_geom::DecodedShape& decoded);

// Classify one source vertex. When PathFrameFitOptions::source_vertices_are_semantic_anchors
// is false, returns a default (non-required, no cue) decision regardless of
// geometry — visible-outline mode owns this opt-out.
SharpFeatureDecision DetectSharpFeatureAtSourceVertex(
    const std::vector<double>& flat,
    int vertex_index,
    const pff_geom::DecodedShape& decoded,
    const PathFrameFitOptions& options);

// Per-vertex booleans marking source vertices that the fitter should keep as
// hard corners during reduction. Open-path endpoints are always marked.
std::vector<bool> DetectSharpSourceVertices(
    const std::vector<double>& flat,
    const pff_geom::DecodedShape& decoded,
    const PathFrameFitOptions& options);

// Per-vertex booleans marking source vertices whose in/out tangents must be
// driven to exact zero in the rebuilt shape_flat (zero-tangent cue OR
// turn_radians >= kHardTangentLockTurnRadians). Open-path endpoints are
// always marked.
std::vector<bool> DetectTangentLockedSourceVertices(
    const std::vector<double>& flat,
    const pff_geom::DecodedShape& decoded,
    const PathFrameFitOptions& options);

}  // namespace pff_sharp
}  // namespace bbsolver
