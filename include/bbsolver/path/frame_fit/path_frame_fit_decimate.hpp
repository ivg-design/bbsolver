#pragma once

// Decimation pipeline for the path frame fitter. PFF12 lifts the
// SimplifyDensePolyline + EnsureMinimumKept entry points out of
// path_frame_fit.cpp so the new FitShapeFlatFrame module
// (path_frame_fit_main.cpp) can call them across translation units. All
// supporting helpers (CubicMarkKept, BuildForwardSourceVertexCandidates,
// SimplifyCandidateIntervalDp, SimplifySourceVertexIntervals) move with
// the entry points and stay private to the new TU.
//
// Pure layout helper: no DiagnosticsWriter, no progress, no operator
// state. Failure is surfaced via an empty `kept` vector. Diagnostics
// ownership: caller-owned (the main fitter surfaces decimation outcomes
// through PathFrameFitResult fields).

#include <vector>

#include "bbsolver/path/dense/path_dense_polyline.hpp"

namespace bbsolver {
namespace pff_decimate {

// Decimate a dense polyline down to a minimum set of indices that still
// approximates the source within `tolerance`. `required_source_vertices`
// marks sharp anchors that MUST stay; `tangent_locked_source_vertices`
// marks vertices whose tangents must be zero in the rebuilt shape.
// Returns the kept dense indices in source-seam order.
std::vector<int> SimplifyDensePolyline(
    const std::vector<DensePoint>& dense,
    const std::vector<int>& source_to_dense,
    const std::vector<bool>& required_source_vertices,
    const std::vector<bool>& tangent_locked_source_vertices,
    bool closed,
    double tolerance);

// Guarantee a minimum kept-vertex count (3 for closed, 2 for open) by
// greedily pulling in the dense index with the largest residual error
// against the current kept set. Idempotent if `kept` is already large
// enough.
std::vector<int> EnsureMinimumKept(const std::vector<DensePoint>& dense,
                                   std::vector<int> kept,
                                   bool closed);

}  // namespace pff_decimate
}  // namespace bbsolver
