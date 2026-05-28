#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "bbsolver/path/dense/path_dense_landmarks.hpp"  // IWYU pragma: keep
#include "bbsolver/path/dense/path_dense_polyline.hpp"
#include "bbsolver/path/geometry/path_feature_anchor.hpp"   // IWYU pragma: keep
#include "bbsolver/path/geometry/path_fraction_helpers.hpp" // IWYU pragma: keep
#include "bbsolver/path/fit/path_fraction_layout_eval.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit_candidate.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit_cubic_span.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit_geometry.hpp"
#include "bbsolver/path/geometry/path_sharp_feature.hpp"    // IWYU pragma: keep

namespace bbsolver {
namespace {

// pff_geom carries the shared 2D geometry primitives (Point, Add/Sub/Mul,
// Distance, FlatPoint, DecodeShapeFlat, Cubic, PointSegmentDistance, kPi,
// kPathHeaderScalars,...) used across the path_frame_fit family. Bringing
// them into the anonymous namespace lets the existing unqualified call sites
// keep compiling unchanged.
using namespace pff_geom;
// pff_fractions carries the shared outline-fraction array utilities and
// kFractionEpsilon — see path_fraction_helpers.hpp.
using namespace pff_fractions;
// pff_dense carries the dense-polyline / outline-polyline pipeline — see
// path_dense_polyline.hpp.
using namespace pff_dense;
// pff_sharp carries the sharp-feature classifier (turn-angle threshold,
// zero-tangent cue, tangent-lock cutoff) — see path_sharp_feature.hpp.
using namespace pff_sharp;
// pff_anchor carries the per-frame feature-anchor snap helper +
// FeatureClusterRadiusForCount — see path_feature_anchor.hpp. The public
// ExtractShapeFlatFeatureAnchors implementation also lives there but stays
// in the top-level bbsolver:: namespace (unchanged public surface).
using namespace pff_anchor;
// pff_landmarks carries the arc-length sampling + dense-with-fraction
// landmark insertion helpers (SampleDenseAtArc, BuildDenseWithFractionLandmarks)
// + supporting structs (ArcLengthPoint, SampledArcPoint) — see
// path_dense_landmarks.hpp. Geometry refinement and fixed-fraction fitting
// share these helpers.
using namespace pff_landmarks;
// pff_fitter carries the Candidate struct + BuildBestCandidate forward
// declaration — see path_frame_fit_candidate.hpp. The function body stays in
// this file while geometry refinement can call it from a separate TU.
using namespace pff_fitter;
// pff_cubic_span carries CubicSpanFit + FitDenseSpanCubic forward
// declaration — see path_frame_fit_cubic_span.hpp. The body and its peer
// helpers (ScoreDenseSpanCubic, SolveUnconstrainedDenseSpanCubic,...) stay
// in this TU while the decimation module can call the public linkage.
using namespace pff_cubic_span;

Point NormalizeOr(Point p, Point fallback) {
  const double len = Length(p);
  if (len > 1e-9) {
    return Mul(p, 1.0 / len);
  }
  const double fallback_len = Length(fallback);
  if (fallback_len > 1e-9) {
    return Mul(fallback, 1.0 / fallback_len);
  }
  return {1.0, 0.0};
}

double OutlineErrorFromDenseSource(const std::vector<DensePoint>& source_dense,
                                   bool closed,
                                   const std::vector<double>& fitted,
                                   const PathFrameFitOptions& options) {
  const DecodedShape fitted_decoded = DecodeShapeFlat(fitted);
  if (!fitted_decoded.ok || fitted_decoded.closed != closed) {
    return std::numeric_limits<double>::infinity();
  }
  const std::vector<Point> source_points = DenseToPoints(source_dense);
  const std::vector<Point> fitted_points =
      DenseToPoints(ShapeFlatToDensePolyline(fitted, options, nullptr));
  return std::max(DirectedPolylineDistance(source_points, fitted_points, closed),
                  DirectedPolylineDistance(fitted_points, source_points, closed));
}

std::vector<int> UniqueSortedIndices(std::vector<int> indices) {
  std::sort(indices.begin(), indices.end());
  indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
  return indices;
}

int ForwardDenseSpan(int begin, int end, int dense_count, bool closed);
Point DenseAtOffset(const std::vector<DensePoint>& dense, int begin, int offset);
double SegmentArcLength(const std::vector<DensePoint>& dense,
                        int begin,
                        int span,
                        std::vector<double>* cumulative);
bool IsSharpDenseIndex(const std::vector<DensePoint>& dense,
                       int dense_index,
                       const std::vector<bool>& sharp_source_vertices);

bool IsFinitePoint(Point p) {
  return std::isfinite(p.x) && std::isfinite(p.y);
}

}  // namespace
}  // namespace bbsolver

namespace bbsolver {
namespace {

using namespace pff_geom;
using namespace pff_fractions;
using namespace pff_dense;
using namespace pff_sharp;
using namespace pff_anchor;
using namespace pff_landmarks;
using namespace pff_fitter;
using namespace pff_cubic_span;

std::vector<Point> KeptPoints(const std::vector<DensePoint>& dense, const std::vector<int>& kept) {
  std::vector<Point> points;
  points.reserve(kept.size());
  for (int index: kept) {
    points.push_back(dense[static_cast<std::size_t>(index)].p);
  }
  return points;
}

bool IsSharpDenseIndex(const std::vector<DensePoint>& dense,
                       int dense_index,
                       const std::vector<bool>& sharp_source_vertices) {
  const int source_index = dense[static_cast<std::size_t>(dense_index)].source_vertex_index;
  return source_index >= 0 &&
         source_index < static_cast<int>(sharp_source_vertices.size()) &&
         sharp_source_vertices[static_cast<std::size_t>(source_index)];
}

Point CatmullIn(const std::vector<Point>& vertices, std::size_t vertex_index, bool closed) {
  const std::size_t n = vertices.size();
  if (n < 2 || (!closed && vertex_index == 0)) {
    return {0.0, 0.0};
  }
  if (!closed && vertex_index + 1 >= n) {
    return {0.0, 0.0};
  }
  const std::size_t prev_index = vertex_index == 0 ? n - 1: vertex_index - 1;
  const std::size_t next_index = (vertex_index + 1) % n;
  return Mul(Sub(vertices[prev_index], vertices[next_index]), 1.0 / 6.0);
}

Point CatmullOut(const std::vector<Point>& vertices, std::size_t vertex_index, bool closed) {
  const std::size_t n = vertices.size();
  if (n < 2 || (!closed && vertex_index + 1 >= n)) {
    return {0.0, 0.0};
  }
  if (!closed && vertex_index == 0) {
    return {0.0, 0.0};
  }
  const std::size_t prev_index = vertex_index == 0 ? n - 1: vertex_index - 1;
  const std::size_t next_index = (vertex_index + 1) % n;
  return Mul(Sub(vertices[next_index], vertices[prev_index]), 1.0 / 6.0);
}

std::vector<double> BuildFlat(const std::vector<DensePoint>& dense,
                              const std::vector<int>& kept,
                              const std::vector<bool>& sharp_source_vertices,
                              bool closed,
                              bool use_catmull) {
  const std::vector<Point> vertices = KeptPoints(dense, kept);
  std::vector<double> out;
  out.reserve(static_cast<std::size_t>(kPathHeaderScalars + kept.size() * kScalarsPerVertex));
  out.push_back(closed ? 1.0: 0.0);
  out.push_back(static_cast<double>(kept.size()));
  for (std::size_t i = 0; i < kept.size(); ++i) {
    Point in_tangent{0.0, 0.0};
    Point out_tangent{0.0, 0.0};
    const int source_index = dense[static_cast<std::size_t>(kept[i])].source_vertex_index;
    const bool force_corner =
        source_index >= 0 &&
        source_index < static_cast<int>(sharp_source_vertices.size()) &&
        sharp_source_vertices[static_cast<std::size_t>(source_index)];
    if (use_catmull && !force_corner) {
      in_tangent = CatmullIn(vertices, i, closed);
      out_tangent = CatmullOut(vertices, i, closed);
    }
    out.push_back(vertices[i].x);
    out.push_back(vertices[i].y);
    out.push_back(in_tangent.x);
    out.push_back(in_tangent.y);
    out.push_back(out_tangent.x);
    out.push_back(out_tangent.y);
  }
  return out;
}

int ForwardDenseSpan(int begin, int end, int dense_count, bool closed) {
  if (begin < 0 || end < 0 || dense_count <= 0) {
    return 0;
  }
  if (end >= begin) {
    return end - begin;
  }
  return closed ? dense_count - begin + end: 0;
}

Point DenseAtOffset(const std::vector<DensePoint>& dense, int begin, int offset) {
  const int n = static_cast<int>(dense.size());
  return dense[static_cast<std::size_t>((begin + offset) % n)].p;
}

double SegmentArcLength(const std::vector<DensePoint>& dense,
                        int begin,
                        int span,
                        std::vector<double>* cumulative) {
  if (cumulative != nullptr) {
    cumulative->assign(static_cast<std::size_t>(span + 1), 0.0);
  }
  double total = 0.0;
  for (int step = 1; step <= span; ++step) {
    total += Distance(DenseAtOffset(dense, begin, step - 1),
                      DenseAtOffset(dense, begin, step));
    if (cumulative != nullptr) {
      (*cumulative)[static_cast<std::size_t>(step)] = total;
    }
  }
  return total;
}

std::vector<double> BuildSegmentFitFlat(const std::vector<DensePoint>& dense,
                                        const std::vector<int>& kept,
                                        const std::vector<bool>& sharp_source_vertices,
                                        bool closed) {
  const std::vector<Point> vertices = KeptPoints(dense, kept);
  std::vector<Point> in_tangents(vertices.size(), {0.0, 0.0});
  std::vector<Point> out_tangents(vertices.size(), {0.0, 0.0});

  const int segment_count = closed ? static_cast<int>(kept.size())
: std::max(0, static_cast<int>(kept.size()) - 1);
  for (int segment = 0; segment < segment_count; ++segment) {
    const int start_vertex = segment;
    const int end_vertex = (segment + 1) % static_cast<int>(kept.size());
    const int begin = kept[static_cast<std::size_t>(start_vertex)];
    const int end = kept[static_cast<std::size_t>(end_vertex)];
    const Point p0 = vertices[static_cast<std::size_t>(start_vertex)];
    const Point p3 = vertices[static_cast<std::size_t>(end_vertex)];
    const CubicSpanFit fit = FitDenseSpanCubic(
        dense, begin, end, sharp_source_vertices, closed);
    out_tangents[static_cast<std::size_t>(start_vertex)] = Sub(fit.p1, p0);
    in_tangents[static_cast<std::size_t>(end_vertex)] = Sub(fit.p2, p3);
  }

  std::vector<double> out;
  out.reserve(static_cast<std::size_t>(kPathHeaderScalars + kept.size() * kScalarsPerVertex));
  out.push_back(closed ? 1.0: 0.0);
  out.push_back(static_cast<double>(kept.size()));
  for (std::size_t i = 0; i < vertices.size(); ++i) {
    out.push_back(vertices[i].x);
    out.push_back(vertices[i].y);
    out.push_back(in_tangents[i].x);
    out.push_back(in_tangents[i].y);
    out.push_back(out_tangents[i].x);
    out.push_back(out_tangents[i].y);
  }
  return out;
}

}  // namespace
}  // namespace bbsolver

namespace bbsolver {
namespace pff_fitter {

// Definition declared in path_frame_fit_candidate.hpp. External linkage lets
// path_frame_geometry_refine.cpp reuse the exact same candidate builder.
Candidate BuildBestCandidate(const std::vector<DensePoint>& dense,
                             const std::vector<int>& kept,
                             const std::vector<bool>& sharp_source_vertices,
                             bool closed,
                             const PathFrameFitOptions& options) {
  Candidate zero;
  zero.flat = BuildFlat(dense, kept, sharp_source_vertices, closed, false);
  zero.error = OutlineErrorFromDenseSource(dense, closed, zero.flat, options);
  zero.catmull = false;
  Candidate best = zero;
  if (!options.use_catmull_tangents) {
    return best;
  }

  Candidate catmull;
  catmull.flat = BuildFlat(dense, kept, sharp_source_vertices, closed, true);
  catmull.error = OutlineErrorFromDenseSource(dense, closed, catmull.flat, options);
  catmull.catmull = true;
  if (catmull.error <= best.error) {
    best = std::move(catmull);
  }

  Candidate fitted;
  fitted.flat = BuildSegmentFitFlat(dense, kept, sharp_source_vertices, closed);
  fitted.error = OutlineErrorFromDenseSource(dense, closed, fitted.flat, options);
  fitted.catmull = false;
  if (fitted.error <= best.error) {
    best = std::move(fitted);
  }
  return best;
}

}  // namespace pff_fitter
}  // namespace bbsolver

namespace bbsolver {
namespace {

// Re-open anon namespace for the remaining internal helpers
// (PointBounds + Bounds*, IsLockedRefineVertex, BuildRefinedCandidate,
// SegmentSplitScore,...).
using namespace pff_geom;
using namespace pff_fractions;
using namespace pff_dense;
using namespace pff_sharp;
using namespace pff_anchor;
using namespace pff_landmarks;
using namespace pff_fitter;

}  // namespace

// EvaluateFractionLayout: external linkage so path_outline_fraction_expand.cpp
// can call it without duplicating the dense-polyline pipeline. Body continues
// to call this TU's anonymous-namespace helpers (ShapeFlatToDensePolyline,
// DenseToPoints, DirectedPolylineDistance, DensePerimeter, DenseArcPositions,
// DenseFractionAtIndex, ProjectPointToDenseFraction) — same-TU access works
// because internal linkage names are visible to named-namespace functions in
// the same translation unit.
FractionLayoutEvaluation EvaluateFractionLayout(
    const std::vector<std::vector<double>>& shape_flat_frames,
    const std::vector<double>& fractions,
    const PathFrameFitOptions& fit_options) {
  FractionLayoutEvaluation evaluation;
  if (shape_flat_frames.empty()) {
    evaluation.warning = "no shape_flat frames";
    return evaluation;
  }
  if (fractions.empty()) {
    evaluation.warning = "no outline fractions";
    return evaluation;
  }

  double max_error = 0.0;
  double worst_fraction = 0.0;
  bool has_worst_fraction = false;
  for (const std::vector<double>& frame: shape_flat_frames) {
    PathFrameFitResult fit = FitShapeFlatFrameAtFractions(frame, fractions, fit_options);
    if (!fit.ok || !fit.target_met ||
        fit.fitted_vertex_count != static_cast<int>(fractions.size())) {
      evaluation.warning = fit.warning.empty() ? "outline fractions could not be replayed"
: fit.warning;
      return evaluation;
    }
    if (fit.source_vertex_count <= static_cast<int>(fractions.size())) {
      evaluation.warning = "outline fraction count reaches source vertex count";
      return evaluation;
    }

    const pff_geom::DecodedShape decoded = pff_geom::DecodeShapeFlat(frame);
    const pff_geom::DecodedShape fit_decoded = pff_geom::DecodeShapeFlat(fit.fitted);
    if (!decoded.ok || !fit_decoded.ok || decoded.closed != fit_decoded.closed) {
      evaluation.warning = "malformed fitted frame";
      return evaluation;
    }
    const std::vector<DensePoint> source_dense =
        ShapeFlatToDensePolyline(frame, fit_options, nullptr);
    const std::vector<DensePoint> fitted_dense =
        ShapeFlatToDensePolyline(fit.fitted, fit_options, nullptr);
    const std::vector<pff_geom::Point> source_points = DenseToPoints(source_dense);
    const std::vector<pff_geom::Point> fitted_points = DenseToPoints(fitted_dense);
    int worst_source = -1;
    int worst_fitted = -1;
    const double source_to_fit =
        DirectedPolylineDistance(source_points, fitted_points, decoded.closed, &worst_source);
    const double fit_to_source =
        DirectedPolylineDistance(fitted_points, source_points, decoded.closed, &worst_fitted);
    const double frame_error = std::max(source_to_fit, fit_to_source);
    if (frame_error > max_error) {
      max_error = frame_error;
      const double total_length = DensePerimeter(source_dense, decoded.closed);
      const std::vector<double> arcs = DenseArcPositions(source_dense, decoded.closed);
      if (source_to_fit >= fit_to_source &&
          worst_source >= 0 &&
          worst_source < static_cast<int>(source_dense.size())) {
        worst_fraction = DenseFractionAtIndex(
            source_dense, arcs, decoded.closed, worst_source, total_length);
        has_worst_fraction = true;
      } else if (worst_fitted >= 0 &&
                 worst_fitted < static_cast<int>(fitted_dense.size())) {
        worst_fraction = ProjectPointToDenseFraction(
            source_dense,
            arcs,
            decoded.closed,
            fitted_dense[static_cast<std::size_t>(worst_fitted)].p,
            total_length);
        has_worst_fraction = true;
      }
    }
  }

  evaluation.ok = true;
  evaluation.max_error = max_error;
  evaluation.worst_fraction = worst_fraction;
  evaluation.has_worst_fraction = has_worst_fraction;
  return evaluation;
}

}  // namespace bbsolver
