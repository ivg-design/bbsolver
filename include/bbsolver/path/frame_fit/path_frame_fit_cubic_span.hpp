#pragma once

// Cubic-span fit support struct + a forward declaration for the public
// per-segment cubic fitter. PFF12 promotes only the linkage of
// `CubicSpanFit` + `FitDenseSpanCubic` so the new decimation module
// (path_frame_fit_decimate.cpp) can call into the cubic-span machinery
// while its body stays in path_frame_fit.cpp alongside its peer cubic
// helpers (ScoreDenseSpanCubic, SolveUnconstrainedDenseSpanCubic, etc.).
//
// Pure layout helper: no DiagnosticsWriter, no progress, no operator state.
// Diagnostics ownership: caller-owned (the candidate-build and decimation
// callers each decide how to surface a cubic fit's `max_error`).

#include <vector>

#include "bbsolver/path/dense/path_dense_polyline.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit_geometry.hpp"

namespace bbsolver {
namespace pff_cubic_span {

// Result of one cubic Bezier fit over a dense span: the inner control points,
// the worst-case point-to-curve error in source units, and the dense index
// most responsible for the worst residual (used by the decimation pipeline
// to choose where to split).
struct CubicSpanFit {
  pff_geom::Point p1;
  pff_geom::Point p2;
  double max_error = 0.0;
  int worst_dense_index = -1;
};

// Fit the lowest-error cubic Bezier across the dense polyline span between
// `begin` and `end` (both inclusive). Honors `sharp_source_vertices` (zero
// tangents at sharp anchors) and `closed` for wraparound. Body lives in
// path_frame_fit.cpp; this header just exposes the linkage so the
// decimation pipeline can call it.
CubicSpanFit FitDenseSpanCubic(const std::vector<DensePoint>& dense,
                               int begin,
                               int end,
                               const std::vector<bool>& sharp_source_vertices,
                               bool closed);

}  // namespace pff_cubic_span
}  // namespace bbsolver
