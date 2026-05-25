#pragma once

// Per-frame feature-anchor extraction + canonical-fraction snap helper.
// `ExtractShapeFlatFeatureAnchors` is the public surface (declared in
// path_frame_fit.hpp); PFF8 hosts its implementation here. The internal
// `pff_anchor::SnapFractionsToFrameFeatureAnchors` was anonymous-namespace
// in path_frame_fit.cpp; PFF8 promotes it to external linkage so the new
// module can own the body while the original two call sites in
// path_frame_fit.cpp continue to reach it via `using namespace pff_anchor;`.
//
// `FeatureClusterRadiusForCount` lives here because both SnapFractions and
// the future canonical-layout cluster builder (PFF9) need it; it's the only
// cluster-radius helper that crosses the anchor/cluster boundary, so we
// host it on the anchor side (the lower-level module in the dependency
// graph) to avoid a circular include.
//
// Pure leaf: no DiagnosticsWriter, no progress, no acceptance state.
// Failure modes are surfaced through empty result vectors or via the
// PathFeatureAnchor struct fields (`source_vertex_index = -1` etc.).
// Diagnostics ownership: **caller-owned** — the canonical-layout builder
// and the per-frame fitter each decide whether to surface an anchor /
// snap verdict as a result note.

#include <algorithm>
#include <vector>

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

namespace bbsolver {
namespace pff_anchor {

// Cluster radius in normalized outline-fraction units, scaled by target
// vertex count. Promoted from path_frame_fit.cpp's anonymous namespace in
// PFF8 because both the per-frame snap helper and the canonical-layout
// cluster builder need to agree on it.
//
// target_count <= 0 returns the empty-frame default 0.02; the positive case
// produces 0.5 / target_count clamped into [0.015, 0.05].
inline double FeatureClusterRadiusForCount(int target_count) {
  if (target_count <= 0) {
    return 0.02;
  }
  return std::clamp(0.5 / static_cast<double>(target_count), 0.015, 0.05);
}

// Snap a canonical outline-fraction layout to the per-frame feature anchors
// extracted from `shape_flat`. Closed paths cycle; open paths leave the
// endpoint slots untouched. Returns the normalized snapped layout or the
// input unchanged when no anchors apply / normalization would collapse the
// layout. The snap radius derives from FeatureClusterRadiusForCount above.
std::vector<double> SnapFractionsToFrameFeatureAnchors(
    const std::vector<double>& canonical_fractions,
    const std::vector<double>& shape_flat,
    const PathFrameFitOptions& options,
    bool closed);

}  // namespace pff_anchor
}  // namespace bbsolver
