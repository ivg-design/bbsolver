#pragma once

#include "bbsolver/path/frame_fit/path_frame_fit_types.hpp"  // IWYU pragma: export

#include <limits>
#include <vector>

namespace bbsolver {

// Fit one AE shape_flat frame:
//   [closed_flag, vertex_count, x, y, in_x, in_y, out_x, out_y,...]
// The returned shape preserves the closed flag and keeps the source seam as
// fitted vertex 0 for stable per-frame downstream processing.
PathFrameFitResult FitShapeFlatFrame(const std::vector<double>& shape_flat,
                                     const PathFrameFitOptions& options);

PathFrameFitResult FitShapeFlatFrame(const std::vector<double>& shape_flat,
                                     double outline_tolerance);

// Extract required source-vertex feature anchors as normalized outline
// fractions in seam order. These are geometric wrist/corner turns that should
// be included in any caller-supplied stable fraction layout. Zero tangents are
// only a cue when the source vertex also has a real turn; redundant collinear
// zero-tangent points are not returned.
std::vector<PathFeatureAnchor> ExtractShapeFlatFeatureAnchors(
    const std::vector<double>& shape_flat,
    const PathFrameFitOptions& options = {});

// Build a canonical fraction layout for a sequence of frames without copying a
// single frame's slot positions. Required feature anchors are extracted from
// every frame, clustered by outline fraction, and inserted first. Remaining
// slots are distributed by repeatedly splitting the largest gaps between the
// seam/endpoints and clustered feature anchors. If the clustered feature count
// alone exceeds target_count, ok is false so callers can raise K or fall back.
PathFeatureFractionLayoutResult BuildShapeFlatFeatureFractionLayout(
    const std::vector<std::vector<double>>& shape_flat_frames,
    int target_count,
    const PathFrameFitOptions& options = {});

// Build a bounded ladder of fixed-topology replacement target vertex counts.
// Callers should try these in order after computing their initial target. Every
// returned value is < source_min_vertices, so a passing candidate remains a
// vertex-count reduction for every source frame. Example:
//   initial_target=22, source_min=28 -> {22, 24, 26, 27}
std::vector<int> BuildShapeFlatReplacementTargetLadder(
    int initial_target_vertices,
    int source_min_vertices,
    const PathReplacementTargetLadderOptions& options = {});

std::vector<int> BuildShapeFlatReplacementRetryTargetLadder(
    int fitted_vertices,
    int source_min_vertices,
    int configured_min_vertices,
    int configured_max_vertices);

// Fit one frame using a caller-supplied stable outline layout. Fractions are
// normalized arc-length positions in source seam order: [0, 1) for closed
// paths, [0, 1] for open paths. The returned vertex order matches the supplied
// fraction order and closed/open is preserved. This API does not add/remove
// vertices to rescue an over-tolerance layout; callers should pick a richer
// fraction set if result.applied is false with "fit exceeds tolerance". Stable
// feature-layout slots near a frame-local required feature anchor snap to that
// source vertex for zero/locked-ish corner treatment while result.outline_fractions
// remains the supplied canonical layout.
PathFrameFitResult FitShapeFlatFrameAtFractions(
    const std::vector<double>& shape_flat,
    const std::vector<double>& outline_fractions,
    const PathFrameFitOptions& options);

// Bounded per-frame geometric refinement for a fixed-fraction fit. The input
// should be the result of FitShapeFlatFrameAtFractions. Non-feature landmarks
// may move locally and tangents are recomputed against the original dense
// outline; hard feature/source anchors remain locked and keep zero-ish corner
// tangents. This is deliberately a per-frame optimizer, not a sequence-wide
// combinatorial search.
PathFrameGeometryRefineResult RefineShapeFlatFrameGeometry(
    const std::vector<double>& shape_flat,
    const PathFrameFitResult& fixed_fraction_fit,
    const PathFrameFitOptions& fit_options = {},
    const PathFrameGeometryRefineOptions& refine_options = {});

// Greedily densify a canonical fraction layout for a sequence of frames by
// inserting midpoint slots between existing fractions. Each accepted insertion
// is replay-tested with FitShapeFlatFrameAtFractions on every supplied frame,
// so returned fractions remain strictly seam-ordered and reusable. The helper
// is intentionally bounded by PathFractionExpansionOptions; callers should set
// max_fraction_count from their replacement/key-count budget.
PathFractionExpansionResult ExpandShapeFlatOutlineFractions(
    const std::vector<std::vector<double>>& shape_flat_frames,
    const std::vector<double>& seed_fractions,
    const PathFrameFitOptions& fit_options,
    const PathFractionExpansionOptions& expansion_options = {});

// Symmetric outline distance between two shape_flat frames, measured after
// densifying cubic segments into polylines.
double ShapeFlatFrameOutlineError(const std::vector<double>& source,
                                  const std::vector<double>& fitted,
                                  const PathFrameFitOptions& options = {},
                                  double cutoff_error = std::numeric_limits<double>::infinity());

// Pre-densify a shape_flat frame for repeated outline-error checks. The error
// helper below uses the same directed Hausdorff metric as
// ShapeFlatFrameOutlineError, but avoids re-decoding/re-densifying the cached
// side for every candidate.
ShapeFlatOutlinePolyline BuildShapeFlatOutlinePolyline(
    const std::vector<double>& shape_flat,
    const PathFrameFitOptions& options = {});

double ShapeFlatFrameOutlineErrorFromPolylines(
    const ShapeFlatOutlinePolyline& source,
    const ShapeFlatOutlinePolyline& fitted,
    double cutoff_error = std::numeric_limits<double>::infinity());

// For self-overlapping closed paths, extract the visible filled-region
// boundary from a densified outline. Simple non-overlapping paths are returned
// as not applied. The result is a zero-tangent shape_flat intended to be passed
// back through FitShapeFlatFrame/FitShapeFlatFrameAtFractions for simplification
// and stable topology.
VisibleShapeFlatOutlineResult ExtractVisibleShapeFlatOutline(
    const std::vector<double>& shape_flat,
    const PathFrameFitOptions& options = {});

}  // namespace bbsolver
