// Implements bbsolver::RefineShapeFlatFrameGeometry (declared in
// path_frame_fit.hpp). Owns the body and Refine-only helpers
// (BuildRefinedCandidate, IsLockedRefineVertex, BoundsForDense, BoundsDiagonal,
// PointBounds, plus a small IsSharpDenseIndex copy). Behavior is stable: same
// warning strings, same target-vertex/tolerance branches, same step / max_move
// / min_step adaptive sequence, same 8-direction search grid.
//
// Diagnostics decision: **none / pure layout**. The function returns a
// PathFrameGeometryRefineResult whose `warning` and `fit.warning` fields
// carry human-readable status strings. No DiagnosticsWriter, no progress,
// no cancellation, no operator state. Diagnostics ownership: caller-owned
// (the panel-side bake driver surfaces the warning string via its existing
// log facility).

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
namespace {

// Refine-only: the rectangular AABB of a dense polyline.
struct PointBounds {
  pff_geom::Point min{0.0, 0.0};
  pff_geom::Point max{0.0, 0.0};
  bool valid = false;
};

PointBounds BoundsForDense(const std::vector<DensePoint>& dense) {
  PointBounds bounds;
  for (const DensePoint& point : dense) {
    if (!bounds.valid) {
      bounds.min = point.p;
      bounds.max = point.p;
      bounds.valid = true;
      continue;
    }
    bounds.min.x = std::min(bounds.min.x, point.p.x);
    bounds.min.y = std::min(bounds.min.y, point.p.y);
    bounds.max.x = std::max(bounds.max.x, point.p.x);
    bounds.max.y = std::max(bounds.max.y, point.p.y);
  }
  return bounds;
}

double BoundsDiagonal(const PointBounds& bounds) {
  return bounds.valid ? pff_geom::Distance(bounds.min, bounds.max) : 0.0;
}

// Refine-only "sharp" test on a dense index. Inlines the
// IsSharpDenseIndex / source-vertex bounds check from path_frame_fit.cpp so
// this TU stays self-contained.
bool IsLockedRefineVertex(const std::vector<DensePoint>& dense,
                          int dense_index,
                          const std::vector<bool>& sharp_source_vertices) {
  if (dense_index < 0 || dense_index >= static_cast<int>(dense.size())) {
    return true;
  }
  const int source_index = dense[static_cast<std::size_t>(dense_index)].source_vertex_index;
  return source_index >= 0 &&
         source_index < static_cast<int>(sharp_source_vertices.size()) &&
         sharp_source_vertices[static_cast<std::size_t>(source_index)];
}

pff_fitter::Candidate BuildRefinedCandidate(
    const std::vector<double>& source_shape_flat,
    const std::vector<DensePoint>& dense,
    const std::vector<int>& kept,
    const std::vector<bool>& sharp_source_vertices,
    bool closed,
    const PathFrameFitOptions& options) {
  pff_fitter::Candidate candidate = pff_fitter::BuildBestCandidate(
      dense, kept, sharp_source_vertices, closed, options);
  candidate.error = ShapeFlatFrameOutlineError(source_shape_flat, candidate.flat, options);
  return candidate;
}

}  // namespace

PathFrameGeometryRefineResult RefineShapeFlatFrameGeometry(
    const std::vector<double>& shape_flat,
    const PathFrameFitResult& fixed_fraction_fit,
    const PathFrameFitOptions& fit_options,
    const PathFrameGeometryRefineOptions& refine_options) {
  using pff_geom::Add;
  using pff_geom::ClampLength;
  using pff_geom::DecodeShapeFlat;
  using pff_geom::DecodedShape;
  using pff_geom::Distance;
  using pff_geom::FlatPoint;
  using pff_geom::Mul;
  using pff_geom::Point;
  using pff_geom::Sub;

  PathFrameGeometryRefineResult result;
  result.fit = fixed_fraction_fit;

  const DecodedShape source_decoded = DecodeShapeFlat(shape_flat);
  const DecodedShape fit_decoded = DecodeShapeFlat(fixed_fraction_fit.fitted);
  if (!source_decoded.ok || !fit_decoded.ok) {
    result.warning = "malformed shape_flat frame";
    result.fit.warning = result.warning;
    return result;
  }
  if (source_decoded.closed != fit_decoded.closed) {
    result.warning = "source/fitted closed flag mismatch";
    result.fit.warning = result.warning;
    return result;
  }
  if (fixed_fraction_fit.outline_fractions.size() !=
      static_cast<std::size_t>(fit_decoded.vertex_count)) {
    result.warning = "fixed fraction fit has no matching outline fraction layout";
    result.fit.warning = result.warning;
    return result;
  }

  result.ok = true;
  result.initial_max_outline_error =
      ShapeFlatFrameOutlineError(shape_flat, fixed_fraction_fit.fitted, fit_options);
  result.fit.max_outline_error = result.initial_max_outline_error;

  std::vector<int> source_to_dense;
  const std::vector<DensePoint> source_dense =
      pff_dense::ShapeFlatToDensePolyline(shape_flat, fit_options, &source_to_dense);
  const int min_vertices = source_decoded.closed ? 3 : 2;
  if (static_cast<int>(source_dense.size()) <= min_vertices) {
    result.warning = "shape_flat frame has insufficient outline samples";
    result.fit.warning = result.warning;
    return result;
  }

  std::vector<double> normalized_fractions;
  if (!pff_fractions::NormalizeOutlineFractions(
          fixed_fraction_fit.outline_fractions, source_decoded.closed, &normalized_fractions)) {
    result.warning = "invalid fixed outline fractions";
    result.fit.warning = result.warning;
    return result;
  }
  const std::vector<double> landmark_fractions =
      pff_anchor::SnapFractionsToFrameFeatureAnchors(
          normalized_fractions, shape_flat, fit_options, source_decoded.closed);

  std::vector<DensePoint> working_dense;
  std::vector<int> kept;
  if (!pff_landmarks::BuildDenseWithFractionLandmarks(
          source_dense, landmark_fractions, source_decoded.closed, &working_dense, &kept)) {
    result.warning = "outline fractions could not be mapped";
    result.fit.warning = result.warning;
    return result;
  }
  if (kept.size() != static_cast<std::size_t>(fit_decoded.vertex_count)) {
    result.warning = "mapped landmark count mismatch";
    result.fit.warning = result.warning;
    return result;
  }

  const std::vector<bool> sharp_source_vertices =
      pff_sharp::DetectSharpSourceVertices(shape_flat, source_decoded, fit_options);
  std::vector<Point> start_positions;
  start_positions.reserve(kept.size());
  std::vector<bool> locked;
  locked.reserve(kept.size());
  for (int vertex_index = 0; vertex_index < fit_decoded.vertex_count; ++vertex_index) {
    const Point p = FlatPoint(fixed_fraction_fit.fitted, vertex_index, 0);
    const int dense_index = kept[static_cast<std::size_t>(vertex_index)];
    working_dense[static_cast<std::size_t>(dense_index)].p = p;
    start_positions.push_back(p);
    locked.push_back(IsLockedRefineVertex(working_dense, dense_index, sharp_source_vertices));
  }

  pff_fitter::Candidate best = BuildRefinedCandidate(
      shape_flat, working_dense, kept, sharp_source_vertices, source_decoded.closed, fit_options);
  if (!best.flat.empty() &&
      (best.error + refine_options.min_error_improvement < result.initial_max_outline_error ||
       result.fit.fitted.empty())) {
    result.fit.fitted = best.flat;
    result.fit.max_outline_error = best.error;
  } else {
    best.flat = fixed_fraction_fit.fitted;
    best.error = result.initial_max_outline_error;
  }

  const PointBounds bounds = BoundsForDense(source_dense);
  const double diag = std::max(BoundsDiagonal(bounds), 1.0);
  double step = refine_options.initial_step_px > 0.0
                    ? refine_options.initial_step_px
                    : std::max(fit_options.outline_tolerance * 2.0, diag * 0.025);
  const double max_move = refine_options.max_vertex_move_px > 0.0
                              ? refine_options.max_vertex_move_px
                              : std::max(fit_options.outline_tolerance * 6.0, diag * 0.12);
  const double min_improvement = std::max(0.0, refine_options.min_error_improvement);
  const double min_step = std::max(1e-4, fit_options.outline_tolerance * 0.01);
  const std::vector<Point> directions = {
      {1.0, 0.0},
      {-1.0, 0.0},
      {0.0, 1.0},
      {0.0, -1.0},
      {0.7071067811865476, 0.7071067811865476},
      {0.7071067811865476, -0.7071067811865476},
      {-0.7071067811865476, 0.7071067811865476},
      {-0.7071067811865476, -0.7071067811865476},
  };

  const int max_iterations = std::max(0, refine_options.max_iterations);
  for (int iteration = 0; iteration < max_iterations; ++iteration) {
    bool improved_this_iteration = false;
    for (std::size_t vertex_index = 0; vertex_index < kept.size(); ++vertex_index) {
      if (locked[vertex_index]) {
        continue;
      }
      const int dense_index = kept[vertex_index];
      const Point current = working_dense[static_cast<std::size_t>(dense_index)].p;
      pff_fitter::Candidate best_for_vertex = best;
      Point best_point = current;

      for (Point direction : directions) {
        Point proposed = Add(current, Mul(direction, step));
        proposed = Add(start_positions[vertex_index],
                       ClampLength(Sub(proposed, start_positions[vertex_index]), max_move));
        if (Distance(proposed, current) <= 1e-9) {
          continue;
        }

        std::vector<DensePoint> trial_dense = working_dense;
        trial_dense[static_cast<std::size_t>(dense_index)].p = proposed;
        pff_fitter::Candidate trial = BuildRefinedCandidate(
            shape_flat, trial_dense, kept, sharp_source_vertices,
            source_decoded.closed, fit_options);
        ++result.candidate_evaluations;
        if (!trial.flat.empty() &&
            trial.error + min_improvement < best_for_vertex.error) {
          best_for_vertex = std::move(trial);
          best_point = proposed;
        }
      }

      if (best_for_vertex.error + min_improvement < best.error) {
        working_dense[static_cast<std::size_t>(dense_index)].p = best_point;
        best = std::move(best_for_vertex);
        result.fit.fitted = best.flat;
        result.fit.max_outline_error = best.error;
        improved_this_iteration = true;
      }
    }

    result.iterations = iteration + 1;
    if (best.error <= fit_options.outline_tolerance + 1e-9) {
      break;
    }
    if (!improved_this_iteration) {
      step *= 0.5;
      if (step < min_step) {
        break;
      }
    }
  }

  const DecodedShape final_decoded = DecodeShapeFlat(result.fit.fitted);
  result.fit.ok = true;
  result.fit.closed = source_decoded.closed;
  result.fit.source_vertex_count = source_decoded.vertex_count;
  result.fit.fitted_vertex_count = final_decoded.ok ? final_decoded.vertex_count
                                                    : fit_decoded.vertex_count;
  result.fit.target_vertex_count = fit_decoded.vertex_count;
  result.fit.target_met =
      result.fit.fitted_vertex_count == fit_decoded.vertex_count;
  result.fit.outline_fractions = normalized_fractions;
  result.fit.kept_dense_indices = kept;
  result.fit.source_vertex_indices.clear();
  result.fit.source_vertex_indices.reserve(kept.size());
  for (int dense_index : kept) {
    result.fit.source_vertex_indices.push_back(
        dense_index >= 0 && dense_index < static_cast<int>(working_dense.size())
            ? working_dense[static_cast<std::size_t>(dense_index)].source_vertex_index
            : -1);
  }

  result.improved =
      result.fit.max_outline_error + min_improvement < result.initial_max_outline_error;
  const double tolerance = std::max(fit_options.outline_tolerance, 0.0);
  if (!result.fit.target_met) {
    result.fit.applied = false;
    result.warning = "refined frame target vertex count not met";
    result.fit.warning = result.warning;
    return result;
  }
  if (result.fit.max_outline_error > tolerance) {
    result.fit.applied = false;
    result.warning = "refined shape_flat frame fit exceeds tolerance";
    result.fit.warning = result.warning;
    return result;
  }
  if (result.fit.fitted_vertex_count >= result.fit.source_vertex_count) {
    result.fit.applied = false;
    result.warning = "refined shape_flat frame unchanged";
    result.fit.warning = result.warning;
    return result;
  }

  result.fit.applied = true;
  result.fit.warning.clear();
  return result;
}

}  // namespace bbsolver
