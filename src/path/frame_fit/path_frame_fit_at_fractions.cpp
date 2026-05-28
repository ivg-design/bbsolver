// Implements bbsolver::FitShapeFlatFrameAtFractions (declared in
// path_frame_fit.hpp). The fixed-fraction frame fitter depends on
// BuildDenseWithFractionLandmarks, BuildBestCandidate, Candidate, the
// dense-polyline pipeline, the sharp-feature classifier, and anchor snapping.
// Behavior is stable: same warning strings, same target-vertex/tolerance
// branches, same NormalizeOutlineFractions guard.
//
// Diagnostics decision: **none / pure layout**. Acceptance-style helper that
// returns a PathFrameFitResult with status strings in `warning`. No
// DiagnosticsWriter, no progress, no cancellation, no operator state.
// Diagnostics ownership: caller-owned.

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

#include "bbsolver/path/dense/path_dense_landmarks.hpp"
#include "bbsolver/path/dense/path_dense_polyline.hpp"
#include "bbsolver/path/geometry/path_feature_anchor.hpp"
#include "bbsolver/path/geometry/path_fraction_helpers.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit_candidate.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit_geometry.hpp"
#include "bbsolver/path/geometry/path_sharp_feature.hpp"

namespace bbsolver {

PathFrameFitResult FitShapeFlatFrameAtFractions(
    const std::vector<double>& shape_flat,
    const std::vector<double>& outline_fractions,
    const PathFrameFitOptions& options) {
  using pff_geom::DecodeShapeFlat;
  using pff_geom::DecodedShape;

  PathFrameFitResult result;
  result.fitted = shape_flat;
  result.target_vertex_count = static_cast<int>(outline_fractions.size());

  const DecodedShape decoded = DecodeShapeFlat(shape_flat);
  if (!decoded.ok) {
    result.warning = "malformed shape_flat frame";
    return result;
  }
  result.ok = true;
  result.closed = decoded.closed;
  result.source_vertex_count = decoded.vertex_count;
  result.fitted_vertex_count = decoded.vertex_count;
  result.target_met = false;

  std::vector<double> normalized_fractions;
  if (!pff_fractions::NormalizeOutlineFractions(
          outline_fractions, decoded.closed, &normalized_fractions)) {
    result.warning = "invalid outline fractions";
    return result;
  }

  const int min_vertices = decoded.closed ? 3: 2;
  if (static_cast<int>(normalized_fractions.size()) < min_vertices) {
    result.warning = "insufficient outline fractions";
    return result;
  }
  result.outline_fractions = normalized_fractions;
  const std::vector<double> landmark_fractions =
      pff_anchor::SnapFractionsToFrameFeatureAnchors(
          normalized_fractions, shape_flat, options, decoded.closed);

  std::vector<int> source_to_dense;
  const std::vector<DensePoint> source_dense =
      pff_dense::ShapeFlatToDensePolyline(shape_flat, options, &source_to_dense);
  if (static_cast<int>(source_dense.size()) <= min_vertices) {
    result.warning = "shape_flat frame has insufficient outline samples";
    return result;
  }

  std::vector<DensePoint> combined_dense;
  std::vector<int> kept;
  if (!pff_landmarks::BuildDenseWithFractionLandmarks(
          source_dense, landmark_fractions, decoded.closed,
          &combined_dense, &kept)) {
    result.warning = "outline fractions could not be mapped";
    return result;
  }

  const std::vector<bool> tangent_locked_source_vertices =
      pff_sharp::DetectTangentLockedSourceVertices(shape_flat, decoded, options);
  pff_fitter::Candidate candidate = pff_fitter::BuildBestCandidate(
      combined_dense, kept, tangent_locked_source_vertices, decoded.closed, options);
  candidate.error =
      ShapeFlatFrameOutlineError(shape_flat, candidate.flat, options);

  result.fitted = std::move(candidate.flat);
  const DecodedShape fitted_decoded = DecodeShapeFlat(result.fitted);
  result.fitted_vertex_count =
      fitted_decoded.ok ? fitted_decoded.vertex_count
: static_cast<int>(normalized_fractions.size());
  result.target_met =
      fitted_decoded.ok &&
      result.fitted_vertex_count == static_cast<int>(normalized_fractions.size());
  result.max_outline_error = candidate.error;

  result.kept_dense_indices = kept;
  result.source_vertex_indices.clear();
  result.source_vertex_indices.reserve(kept.size());
  for (int dense_index: kept) {
    result.source_vertex_indices.push_back(
        dense_index >= 0 && dense_index < static_cast<int>(combined_dense.size())
            ? combined_dense[static_cast<std::size_t>(dense_index)].source_vertex_index
: -1);
  }

  const double tolerance = std::max(options.outline_tolerance, 0.0);
  if (!result.target_met) {
    result.warning = "outline fraction target vertex count not met";
    return result;
  }
  if (candidate.error > tolerance) {
    result.warning = "shape_flat frame fit exceeds tolerance";
    return result;
  }
  if (result.fitted_vertex_count >= result.source_vertex_count) {
    result.warning = "shape_flat frame unchanged";
    return result;
  }

  result.applied = true;
  return result;
}

}  // namespace bbsolver
