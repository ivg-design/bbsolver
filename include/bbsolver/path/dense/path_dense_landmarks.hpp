#pragma once

// Arc-length sampling + landmark insertion on a densified shape_flat
// polyline. PFF10a promotes the previously-anonymous helpers out of
// path_frame_fit.cpp so the new Refine module (PFF10b) and the existing
// `FitShapeFlatFrameAtFractions` can share one implementation rather than
// duplicating the arc-length sampling logic.
//
// Pure leaf: no DiagnosticsWriter, no progress events, no acceptance state.
// `BuildDenseWithFractionLandmarks` reports failure through a boolean return
// value; `SampleDenseAtArc` always returns a sentinel SampledArcPoint
// (`dense.source_vertex_index = -1`, `arc = 0` or `total_length`) for empty
// or degenerate inputs. Diagnostics ownership: **caller-owned**.

#include <vector>

#include "bbsolver/path/dense/path_dense_polyline.hpp"

namespace bbsolver {
namespace pff_landmarks {

// Internal helper: one point along the arc-length-sorted merge of source
// dense vertices and caller-supplied landmark fractions. `request_index` is
// -1 for source-dense points, else the index of the caller-supplied fraction
// the point was sampled from.
struct ArcLengthPoint {
  double arc = 0.0;
  DensePoint dense;
  int request_index = -1;
};

// Internal helper: arc-length parameterized sample of the dense polyline.
struct SampledArcPoint {
  double arc = 0.0;
  DensePoint dense;
};

// Sample the dense polyline at a given arc-length offset. Closed paths wrap
// at total_length; open paths clamp to [0, total_length]. Degenerate
// (zero-length) segments are skipped. Returns a SampledArcPoint with
// `dense.source_vertex_index = -1` when the sample lands strictly between
// two source vertices.
SampledArcPoint SampleDenseAtArc(const std::vector<DensePoint>& dense,
                                 bool closed,
                                 double arc,
                                 double total_length);

// Build a new dense polyline that includes both the original source dense
// vertices and a caller-supplied list of normalized outline fractions, with
// `*kept` written to the strictly-increasing indices of the inserted
// landmarks within the resulting `*combined` polyline. Returns false on
// empty input, degenerate perimeter, or non-monotone landmark ordering.
bool BuildDenseWithFractionLandmarks(const std::vector<DensePoint>& dense,
                                     const std::vector<double>& fractions,
                                     bool closed,
                                     std::vector<DensePoint>* combined,
                                     std::vector<int>* kept);

}  // namespace pff_landmarks
}  // namespace bbsolver
