// Implements the public bbsolver::FitShapeFlatFrame overloads declared in
// path_frame_fit.hpp. The overloads depend on the decimation pipeline exposed
// through path_frame_fit_decimate.hpp and BuildBestCandidate through
// path_frame_fit_candidate.hpp.
//
// Diagnostics decision: **none / pure layout**. Acceptance-style helper
// that returns PathFrameFitResult with status strings in `warning`. No
// DiagnosticsWriter, no progress events, no cancellation, no operator
// state. Diagnostics ownership: caller-owned.

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

#include "bbsolver/path/dense/path_dense_polyline.hpp"
#include "bbsolver/path/geometry/path_fraction_helpers.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit_candidate.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit_decimate.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit_geometry.hpp"
#include "bbsolver/path/geometry/path_sharp_feature.hpp"

namespace bbsolver {
namespace {

using pff_dense::DensePerimeter;
using pff_dense::DenseArcPositions;
using pff_dense::DenseFractionAtIndex;
using pff_dense::DenseToPoints;
using pff_dense::DirectedPolylineDistance;
using pff_dense::ProjectPointToDenseFraction;
using pff_dense::ShapeFlatToDensePolyline;
using pff_fitter::BuildBestCandidate;
using pff_fitter::Candidate;
using pff_fractions::FractionsInStrictSeamOrder;
using pff_fractions::InsertSplitFraction;
using pff_geom::Add;
using pff_geom::DecodeShapeFlat;
using pff_geom::DecodedShape;
using pff_geom::Distance;
using pff_geom::FlatPoint;
using pff_geom::Lerp;
using pff_geom::Point;
using pff_geom::Sub;
using pff_geom::kPathHeaderScalars;
using pff_geom::kScalarsPerVertex;
using pff_sharp::DetectSharpSourceVertices;
using pff_sharp::DetectTangentLockedSourceVertices;

// Local copy of UniqueSortedIndices (anonymous-namespace in path_frame_fit.cpp).
std::vector<int> UniqueSortedIndices(std::vector<int> indices) {
  std::sort(indices.begin(), indices.end());
  indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
  return indices;
}

std::vector<double> FractionsForKeptDenseIndices(const std::vector<DensePoint>& dense,
                                                 const std::vector<int>& kept,
                                                 bool closed) {
  const double total_length = DensePerimeter(dense, closed);
  const std::vector<double> arcs = DenseArcPositions(dense, closed);
  std::vector<double> fractions;
  fractions.reserve(kept.size());
  if (!(total_length > 1e-9)) {
    return fractions;
  }
  for (int dense_index : kept) {
    if (dense_index < 0 || dense_index >= static_cast<int>(dense.size())) {
      return {};
    }
    fractions.push_back(DenseFractionAtIndex(dense, arcs, closed, dense_index, total_length));
  }
  if (!FractionsInStrictSeamOrder(fractions, closed)) {
    return {};
  }
  return fractions;
}

double SegmentSplitScore(const std::vector<double>& flat,
                         int segment_index,
                         const DecodedShape& decoded) {
  const int next = (segment_index + 1) % decoded.vertex_count;
  const Point p0 = FlatPoint(flat, segment_index, 0);
  const Point p3 = FlatPoint(flat, next, 0);
  const Point p1 = Add(p0, FlatPoint(flat, segment_index, 4));
  const Point p2 = Add(p3, FlatPoint(flat, next, 2));
  return Distance(p0, p1) + Distance(p1, p2) + Distance(p2, p3);
}

std::vector<double> SplitCubicSegment(const std::vector<double>& flat,
                                      int segment_index,
                                      const DecodedShape& decoded) {
  const int next = (segment_index + 1) % decoded.vertex_count;
  const Point p0 = FlatPoint(flat, segment_index, 0);
  const Point p3 = FlatPoint(flat, next, 0);
  const Point p1 = Add(p0, FlatPoint(flat, segment_index, 4));
  const Point p2 = Add(p3, FlatPoint(flat, next, 2));

  const Point q0 = Lerp(p0, p1, 0.5);
  const Point q1 = Lerp(p1, p2, 0.5);
  const Point q2 = Lerp(p2, p3, 0.5);
  const Point r0 = Lerp(q0, q1, 0.5);
  const Point r1 = Lerp(q1, q2, 0.5);
  const Point split = Lerp(r0, r1, 0.5);

  std::vector<double> out;
  out.reserve(static_cast<std::size_t>(kPathHeaderScalars + (decoded.vertex_count + 1) * kScalarsPerVertex));
  out.push_back(decoded.closed ? 1.0 : 0.0);
  out.push_back(static_cast<double>(decoded.vertex_count + 1));

  for (int i = 0; i < decoded.vertex_count; ++i) {
    const Point vertex = FlatPoint(flat, i, 0);
    Point in_tangent = FlatPoint(flat, i, 2);
    Point out_tangent = FlatPoint(flat, i, 4);
    if (i == segment_index) {
      out_tangent = Sub(q0, p0);
    }
    if (i == next) {
      in_tangent = Sub(q2, p3);
    }

    out.push_back(vertex.x);
    out.push_back(vertex.y);
    out.push_back(in_tangent.x);
    out.push_back(in_tangent.y);
    out.push_back(out_tangent.x);
    out.push_back(out_tangent.y);

    if (i == segment_index) {
      const Point split_in = Sub(r0, split);
      const Point split_out = Sub(r1, split);
      out.push_back(split.x);
      out.push_back(split.y);
      out.push_back(split_in.x);
      out.push_back(split_in.y);
      out.push_back(split_out.x);
      out.push_back(split_out.y);
    }
  }

  return out;
}

std::vector<double> GrowFlatBySplittingSegments(std::vector<double> flat,
                                                int target_vertex_count,
                                                std::vector<double>* outline_fractions) {
  for (;;) {
    const DecodedShape decoded = DecodeShapeFlat(flat);
    if (!decoded.ok || target_vertex_count <= decoded.vertex_count) {
      return flat;
    }
    const int segment_count = decoded.closed ? decoded.vertex_count : decoded.vertex_count - 1;
    if (segment_count <= 0) {
      return flat;
    }

    int best_segment = 0;
    double best_score = -1.0;
    for (int i = 0; i < segment_count; ++i) {
      const double score = SegmentSplitScore(flat, i, decoded);
      if (score > best_score) {
        best_score = score;
        best_segment = i;
      }
    }
    flat = SplitCubicSegment(flat, best_segment, decoded);
    if (outline_fractions != nullptr) {
      if (outline_fractions->size() == static_cast<std::size_t>(decoded.vertex_count)) {
        InsertSplitFraction(outline_fractions, best_segment, decoded.closed);
      } else {
        outline_fractions->clear();
      }
    }
  }
}

int WorstSourceDenseIndex(const std::vector<DensePoint>& source_dense,
                          const std::vector<double>& candidate,
                          bool closed,
                          const PathFrameFitOptions& options) {
  const std::vector<Point> source_points = DenseToPoints(source_dense);
  const std::vector<Point> candidate_points =
      DenseToPoints(ShapeFlatToDensePolyline(candidate, options, nullptr));
  int worst = -1;
  DirectedPolylineDistance(source_points, candidate_points, closed, &worst);
  return worst;
}

}  // namespace

PathFrameFitResult FitShapeFlatFrame(const std::vector<double>& shape_flat,
                                     const PathFrameFitOptions& options) {
  PathFrameFitResult result;
  result.fitted = shape_flat;
  result.target_vertex_count = std::max(0, options.target_vertex_count);

  const DecodedShape decoded = DecodeShapeFlat(shape_flat);
  if (!decoded.ok) {
    result.warning = "malformed shape_flat frame";
    return result;
  }
  result.ok = true;
  result.closed = decoded.closed;
  result.source_vertex_count = decoded.vertex_count;
  result.fitted_vertex_count = decoded.vertex_count;
  result.max_outline_error = 0.0;
  result.target_met =
      result.target_vertex_count == 0 || result.target_vertex_count == decoded.vertex_count;

  const int min_vertices = decoded.closed ? 3 : 2;
  if (decoded.vertex_count <= min_vertices) {
    result.warning = "shape_flat frame already minimal";
    return result;
  }

  std::vector<int> source_to_dense;
  const std::vector<DensePoint> dense =
      ShapeFlatToDensePolyline(shape_flat, options, &source_to_dense);
  if (static_cast<int>(dense.size()) <= min_vertices) {
    result.warning = "shape_flat frame has insufficient outline samples";
    return result;
  }

  const double tolerance = std::max(options.outline_tolerance, 0.0);
  const std::vector<bool> required_source_vertices =
      DetectSharpSourceVertices(shape_flat, decoded, options);
  const std::vector<bool> tangent_locked_source_vertices =
      DetectTangentLockedSourceVertices(shape_flat, decoded, options);
  std::vector<int> kept = pff_decimate::SimplifyDensePolyline(
      dense,
      source_to_dense,
      required_source_vertices,
      tangent_locked_source_vertices,
      decoded.closed,
      tolerance);
  kept = pff_decimate::EnsureMinimumKept(dense, std::move(kept), decoded.closed);

  Candidate candidate = BuildBestCandidate(
      dense, kept, tangent_locked_source_vertices, decoded.closed, options);
  int iterations = 0;
  while (candidate.error > tolerance &&
         static_cast<int>(kept.size()) < static_cast<int>(dense.size()) &&
         iterations < std::max(0, options.max_refine_iterations)) {
    const int worst = WorstSourceDenseIndex(dense, candidate.flat, decoded.closed, options);
    if (worst < 0 || std::find(kept.begin(), kept.end(), worst) != kept.end()) {
      break;
    }
    kept.push_back(worst);
    kept = UniqueSortedIndices(std::move(kept));
    candidate = BuildBestCandidate(
        dense, kept, tangent_locked_source_vertices, decoded.closed, options);
    ++iterations;
  }
  std::vector<double> candidate_fractions =
      FractionsForKeptDenseIndices(dense, kept, decoded.closed);
  if (result.target_vertex_count > 0) {
    if (result.target_vertex_count > static_cast<int>(kept.size())) {
      candidate.flat = GrowFlatBySplittingSegments(
          std::move(candidate.flat), result.target_vertex_count, &candidate_fractions);
      candidate.error = ShapeFlatFrameOutlineError(shape_flat, candidate.flat, options);
      candidate.catmull = false;
      const DecodedShape grown = DecodeShapeFlat(candidate.flat);
      if (grown.ok) {
        kept.clear();
        kept.reserve(static_cast<std::size_t>(grown.vertex_count));
        for (int index = 0; index < grown.vertex_count; ++index) {
          kept.push_back(-1);
        }
      }
    }
    const DecodedShape target_decoded = DecodeShapeFlat(candidate.flat);
    result.target_met = target_decoded.ok && target_decoded.vertex_count == result.target_vertex_count;
  } else {
    result.target_met = true;
  }

  result.fitted = std::move(candidate.flat);
  result.max_outline_error = candidate.error;
  const DecodedShape fitted_decoded = DecodeShapeFlat(result.fitted);
  result.fitted_vertex_count = fitted_decoded.ok ? fitted_decoded.vertex_count : static_cast<int>(kept.size());
  result.kept_dense_indices.clear();
  result.source_vertex_indices.clear();
  result.outline_fractions.clear();
  result.kept_dense_indices.reserve(static_cast<std::size_t>(std::max(result.fitted_vertex_count, 0)));
  result.source_vertex_indices.reserve(static_cast<std::size_t>(std::max(result.fitted_vertex_count, 0)));
  if (candidate_fractions.size() == static_cast<std::size_t>(result.fitted_vertex_count) &&
      FractionsInStrictSeamOrder(candidate_fractions, decoded.closed)) {
    result.outline_fractions = candidate_fractions;
  }
  const double total_length = DensePerimeter(dense, decoded.closed);
  const std::vector<double> arcs = DenseArcPositions(dense, decoded.closed);
  for (int vertex_index = 0; vertex_index < result.fitted_vertex_count; ++vertex_index) {
    const Point vertex = FlatPoint(result.fitted, vertex_index, 0);
    int dense_index = -1;
    int source_index = -1;
    for (std::size_t i = 0; i < dense.size(); ++i) {
      if (Distance(vertex, dense[i].p) < 1e-6) {
        dense_index = static_cast<int>(i);
        source_index = dense[i].source_vertex_index;
        break;
      }
    }
    result.kept_dense_indices.push_back(dense_index);
    result.source_vertex_indices.push_back(source_index);
    if (result.outline_fractions.empty()) {
      result.outline_fractions.reserve(
          static_cast<std::size_t>(std::max(result.fitted_vertex_count, 0)));
    }
    if (result.outline_fractions.size() < static_cast<std::size_t>(result.fitted_vertex_count)) {
      result.outline_fractions.push_back(
          dense_index >= 0
              ? DenseFractionAtIndex(dense, arcs, decoded.closed, dense_index, total_length)
              : ProjectPointToDenseFraction(dense, arcs, decoded.closed, vertex, total_length));
    }
  }

  if (candidate.error > tolerance) {
    result.warning = "shape_flat frame fit exceeds tolerance";
    return result;
  }
  if (!result.target_met) {
    result.warning = "shape_flat frame target vertex count not met";
  }
  if (result.fitted_vertex_count >= result.source_vertex_count) {
    result.warning = result.warning.empty() ? "shape_flat frame unchanged"
                                            : result.warning + "; unchanged";
    return result;
  }

  result.applied = true;
  return result;
}

PathFrameFitResult FitShapeFlatFrame(const std::vector<double>& shape_flat,
                                     double outline_tolerance) {
  PathFrameFitOptions options;
  options.outline_tolerance = outline_tolerance;
  return FitShapeFlatFrame(shape_flat, options);
}

}  // namespace bbsolver
