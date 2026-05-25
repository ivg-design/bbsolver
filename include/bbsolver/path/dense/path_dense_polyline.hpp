#pragma once

// Shared densified-polyline + outline-polyline utilities used across the
// path_frame_fit family. The dense side carries `DensePoint` (a point plus
// its source-vertex index, -1 for synthesized subdivision points) and the
// pipeline that converts a shape_flat array into a polyline-with-provenance.
// The outline side carries the symmetric-Hausdorff helpers that operate on
// `ShapeFlatOutlinePoint` (declared in the public `path_frame_fit.hpp`).
//
// Pure leaf: no DiagnosticsWriter, no progress events, no acceptance state.
// These helpers previously lived in path_frame_fit.cpp's anonymous namespace
// and had 11 / 10 / 3 / 2 call sites respectively across the file. PFF5
// promotes them so downstream extractions (outline-error trio, refine,
// canonical layout) can be moved into their own modules without duplicating
// the pipeline.
//
// Usage from a .cpp:
//   namespace bbsolver { namespace { using namespace pff_dense; ... } }
// keeps existing unqualified call sites compiling unchanged. The outline
// overloads (`Distance` and `PointSegmentDistance` on `ShapeFlatOutlinePoint`)
// coexist with the `pff_geom::Point` overloads via standard C++ overload
// resolution because the argument types are distinct.

#include <cstddef>
#include <limits>
#include <vector>

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit_geometry.hpp"

namespace bbsolver {

struct DensePoint {
  pff_geom::Point p;
  int source_vertex_index = -1;
};

namespace pff_dense {

// Adaptive subdivision count for a cubic Bezier segment based on its control
// polyline length and the caller's outline tolerance. Honors
// PathFrameFitOptions::max_subdivisions_per_segment as a hard ceiling
// (clamped into [8, 256]).
int SegmentSubdivisions(pff_geom::Point p0,
                        pff_geom::Point p1,
                        pff_geom::Point p2,
                        pff_geom::Point p3,
                        const PathFrameFitOptions& options);

// Append a dense point, deduplicating if it coincides (within 1e-9) with the
// trailing point. If the trailing point has no source-vertex index but the
// incoming one does, the trailing point inherits it.
void PushDensePoint(std::vector<DensePoint>& points, DensePoint point);

// Densify a shape_flat array into a polyline-with-provenance. Each Bezier
// segment is subdivided per SegmentSubdivisions; source vertices keep their
// index, synthesized subdivision points use -1. When `source_to_dense` is
// non-null it is sized to the source vertex count and populated with the
// dense index that corresponds to each source vertex.
std::vector<DensePoint> ShapeFlatToDensePolyline(const std::vector<double>& flat,
                                                 const PathFrameFitOptions& options,
                                                 std::vector<int>* source_to_dense);

// Strip dense provenance and return only the points.
std::vector<pff_geom::Point> DenseToPoints(const std::vector<DensePoint>& dense);

// Bridge dense provenance to the outline-error API. Returns the same xy as
// the dense input, just typed as ShapeFlatOutlinePoint.
ShapeFlatOutlinePoint ToOutlinePoint(pff_geom::Point p);
std::vector<ShapeFlatOutlinePoint> DenseToOutlinePoints(
    const std::vector<DensePoint>& dense);

// Outline-typed overloads for the directed-Hausdorff helpers. Same algorithm
// as the dense Point versions, just typed for the public outline polyline.
double Distance(ShapeFlatOutlinePoint a, ShapeFlatOutlinePoint b);
double PointSegmentDistance(ShapeFlatOutlinePoint p,
                            ShapeFlatOutlinePoint a,
                            ShapeFlatOutlinePoint b);

// Directed Hausdorff distance from a_points to b_points. b_points is treated
// as a polyline (closed=true wraps the last segment back to b_points[0]).
// Returns infinity for empty input. The `cutoff_error` short-circuits the
// scan once max_err exceeds it; default is +infinity (no cutoff).
double DirectedOutlinePolylineDistance(
    const std::vector<ShapeFlatOutlinePoint>& a_points,
    const std::vector<ShapeFlatOutlinePoint>& b_points,
    bool closed,
    double cutoff_error = std::numeric_limits<double>::infinity());

// Same as DirectedOutlinePolylineDistance but typed for `pff_geom::Point` and
// optionally reports the worst point's index in `a_points` via `worst_index`.
double DirectedPolylineDistance(const std::vector<pff_geom::Point>& a_points,
                                const std::vector<pff_geom::Point>& b_points,
                                bool closed,
                                int* worst_index = nullptr);

// Sum of segment lengths along the dense polyline. Closed paths include the
// wraparound segment back to dense[0].
double DensePerimeter(const std::vector<DensePoint>& dense, bool closed);

// Cumulative arc length at each dense index (closed paths wrap the final
// segment but DO NOT append the closing arc to the result).
std::vector<double> DenseArcPositions(const std::vector<DensePoint>& dense, bool closed);

// Normalized outline fraction at the given dense index. Returns 0 for
// out-of-range indices or when total_length is non-positive.
double DenseFractionAtIndex(const std::vector<DensePoint>& dense,
                            const std::vector<double>& arcs,
                            bool closed,
                            int dense_index,
                            double total_length);

// Project an arbitrary point onto the dense polyline and return the
// nearest normalized outline fraction. Degenerate (zero-length) segments
// are skipped.
double ProjectPointToDenseFraction(const std::vector<DensePoint>& dense,
                                   const std::vector<double>& arcs,
                                   bool closed,
                                   pff_geom::Point p,
                                   double total_length);

}  // namespace pff_dense
}  // namespace bbsolver
