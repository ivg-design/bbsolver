#pragma once

// Shared candidate-fit struct used by the main fitter (BuildBestCandidate)
// and the per-frame Refine extractor (BuildRefinedCandidate). PFF10a moves
// this struct + the BuildBestCandidate forward declaration out of
// path_frame_fit.cpp's anonymous namespace so PFF10b can call into
// BuildBestCandidate from a separate translation unit.
//
// Pure data + a single forward declaration. The function body stays in
// path_frame_fit.cpp; only its linkage changes from internal (anonymous
// namespace) to external `bbsolver::pff_fitter::`.

#include <limits>
#include <vector>

#include "bbsolver/path/dense/path_dense_polyline.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

namespace bbsolver {
namespace pff_fitter {

// One candidate fitted-frame result: the encoded shape_flat plus its
// directed-Hausdorff source-outline error and a tag indicating whether the
// candidate was tangent-fitted via Catmull-Rom rather than the constrained
// per-segment cubic solver.
struct Candidate {
  std::vector<double> flat;
  double error = std::numeric_limits<double>::infinity();
  bool catmull = false;
};

// Build the lowest-error Candidate for the given dense polyline + kept
// landmark indices, trying zero-tangent, Catmull-Rom tangent (if
// `options.use_catmull_tangents`), and per-segment cubic-fit variants and
// returning whichever has the smallest outline error against the source
// dense polyline. The body lives in path_frame_fit.cpp; PFF10a promotes
// only the linkage so PFF10b's path_frame_geometry_refine.cpp can call it.
Candidate BuildBestCandidate(const std::vector<DensePoint>& dense,
                             const std::vector<int>& kept,
                             const std::vector<bool>& sharp_source_vertices,
                             bool closed,
                             const PathFrameFitOptions& options);

}  // namespace pff_fitter
}  // namespace bbsolver
