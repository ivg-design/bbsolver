// Implements the bbsolver::pff_dense densified-polyline + outline-polyline
// utilities declared in path_dense_polyline.hpp. Behavior is byte-faithful
// with the previous anonymous-namespace definitions in path_frame_fit.cpp:
// every loop bound, every epsilon, every clamp survives the move unchanged.
//
// Diagnostics decision: **none / pure layout**. Every function here is pure
// geometric arithmetic. No DiagnosticsWriter, no progress events, no
// cancellation, no operator state. Failure modes (empty input, malformed
// shape_flat, zero-length segments, non-positive total_length) are surfaced
// through return-value sentinels (infinity, 0, empty vector) exactly as
// before; callers decide how to react.

#include "bbsolver/path/dense/path_dense_polyline.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit_geometry.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace bbsolver {
namespace pff_dense {
namespace {

// Internal helper used only by DenseFractionAtIndex and
// ProjectPointToDenseFraction. Keeps the seam-wrap snap behavior aligned
// with pff_fractions::kFractionEpsilon (1e-6 in unit-perimeter space).
constexpr double kArcFractionSnapEpsilon = 1e-6;

double NormalizeArcFraction(double arc, double total_length, bool closed) {
  if (!(total_length > 1e-9)) {
    return 0.0;
  }
  double fraction = arc / total_length;
  if (closed) {
    fraction = fraction - std::floor(fraction);
    if (fraction >= 1.0 - kArcFractionSnapEpsilon || fraction < kArcFractionSnapEpsilon) {
      fraction = 0.0;
    }
  } else {
    fraction = std::clamp(fraction, 0.0, 1.0);
  }
  return fraction;
}

}  // namespace

int SegmentSubdivisions(pff_geom::Point p0,
                        pff_geom::Point p1,
                        pff_geom::Point p2,
                        pff_geom::Point p3,
                        const PathFrameFitOptions& options) {
  const double control_len =
      pff_geom::Distance(p0, p1) + pff_geom::Distance(p1, p2) + pff_geom::Distance(p2, p3);
  const double chord_len = pff_geom::Distance(p0, p3);
  const double curvature_len = control_len + std::abs(control_len - chord_len) * 2.0;
  const double tolerance = std::max(options.outline_tolerance, 0.01);
  const int adaptive =
      static_cast<int>(std::ceil(curvature_len / std::max(1.0, tolerance * 0.5)));
  const int max_divs = std::clamp(options.max_subdivisions_per_segment, 8, 256);
  return std::clamp(adaptive, 8, max_divs);
}

void PushDensePoint(std::vector<DensePoint>& points, DensePoint point) {
  if (!points.empty() && pff_geom::Distance(points.back().p, point.p) < 1e-9) {
    if (points.back().source_vertex_index < 0) {
      points.back().source_vertex_index = point.source_vertex_index;
    }
    return;
  }
  points.push_back(point);
}

std::vector<DensePoint> ShapeFlatToDensePolyline(const std::vector<double>& flat,
                                                 const PathFrameFitOptions& options,
                                                 std::vector<int>* source_to_dense) {
  std::vector<DensePoint> points;
  const pff_geom::DecodedShape decoded = pff_geom::DecodeShapeFlat(flat);
  if (!decoded.ok) {
    return points;
  }

  if (source_to_dense != nullptr) {
    source_to_dense->assign(static_cast<std::size_t>(decoded.vertex_count), -1);
  }

  const int seg_count =
      decoded.closed ? decoded.vertex_count : std::max(0, decoded.vertex_count - 1);
  if (seg_count <= 0) {
    points.push_back({pff_geom::FlatPoint(flat, 0, 0), 0});
    if (source_to_dense != nullptr) {
      (*source_to_dense)[0] = 0;
    }
    return points;
  }

  for (int i = 0; i < seg_count; ++i) {
    const int next = (i + 1) % decoded.vertex_count;
    const pff_geom::Point p0 = pff_geom::FlatPoint(flat, i, 0);
    const pff_geom::Point p3 = pff_geom::FlatPoint(flat, next, 0);
    const pff_geom::Point p1 = pff_geom::Add(p0, pff_geom::FlatPoint(flat, i, 4));
    const pff_geom::Point p2 = pff_geom::Add(p3, pff_geom::FlatPoint(flat, next, 2));
    const int divs = SegmentSubdivisions(p0, p1, p2, p3, options);

    if (i == 0) {
      PushDensePoint(points, {p0, i});
      if (source_to_dense != nullptr) {
        (*source_to_dense)[static_cast<std::size_t>(i)] = 0;
      }
    }
    for (int step = 1; step <= divs; ++step) {
      if (decoded.closed && i == seg_count - 1 && step == divs) {
        continue;
      }
      const bool is_endpoint = step == divs;
      const int source_index = is_endpoint ? next : -1;
      PushDensePoint(
          points,
          {pff_geom::Cubic(p0, p1, p2, p3, static_cast<double>(step) / divs), source_index});
      if (is_endpoint && source_to_dense != nullptr && next < decoded.vertex_count) {
        (*source_to_dense)[static_cast<std::size_t>(next)] =
            static_cast<int>(points.size()) - 1;
      }
    }
  }
  return points;
}

std::vector<pff_geom::Point> DenseToPoints(const std::vector<DensePoint>& dense) {
  std::vector<pff_geom::Point> points;
  points.reserve(dense.size());
  for (const DensePoint& point : dense) {
    points.push_back(point.p);
  }
  return points;
}

ShapeFlatOutlinePoint ToOutlinePoint(pff_geom::Point p) {
  return {p.x, p.y};
}

std::vector<ShapeFlatOutlinePoint> DenseToOutlinePoints(
    const std::vector<DensePoint>& dense) {
  std::vector<ShapeFlatOutlinePoint> points;
  points.reserve(dense.size());
  for (const DensePoint& point : dense) {
    points.push_back(ToOutlinePoint(point.p));
  }
  return points;
}

double Distance(ShapeFlatOutlinePoint a, ShapeFlatOutlinePoint b) {
  const double dx = a.x - b.x;
  const double dy = a.y - b.y;
  return std::sqrt(dx * dx + dy * dy);
}

double PointSegmentDistance(ShapeFlatOutlinePoint p,
                            ShapeFlatOutlinePoint a,
                            ShapeFlatOutlinePoint b) {
  const double ab_x = b.x - a.x;
  const double ab_y = b.y - a.y;
  const double denom = ab_x * ab_x + ab_y * ab_y;
  if (!(denom > 1e-18)) {
    return Distance(p, a);
  }
  const double u = std::clamp(
      ((p.x - a.x) * ab_x + (p.y - a.y) * ab_y) / denom,
      0.0,
      1.0);
  return Distance(p, {a.x + ab_x * u, a.y + ab_y * u});
}

double DirectedOutlinePolylineDistance(
    const std::vector<ShapeFlatOutlinePoint>& a_points,
    const std::vector<ShapeFlatOutlinePoint>& b_points,
    bool closed,
    double cutoff_error) {
  if (a_points.empty() || b_points.empty()) {
    return std::numeric_limits<double>::infinity();
  }
  const int seg_count = closed ? static_cast<int>(b_points.size())
                               : std::max(0, static_cast<int>(b_points.size()) - 1);
  if (seg_count <= 0) {
    return Distance(a_points.front(), b_points.front());
  }

  double max_err = 0.0;
  for (const ShapeFlatOutlinePoint p : a_points) {
    double best = std::numeric_limits<double>::infinity();
    for (int i = 0; i < seg_count; ++i) {
      best = std::min(best, PointSegmentDistance(
                                p,
                                b_points[static_cast<std::size_t>(i)],
                                b_points[static_cast<std::size_t>((i + 1) % b_points.size())]));
    }
    max_err = std::max(max_err, best);
    if (std::isfinite(cutoff_error) && max_err > cutoff_error) {
      return max_err;
    }
  }
  return max_err;
}

double DirectedPolylineDistance(const std::vector<pff_geom::Point>& a_points,
                                const std::vector<pff_geom::Point>& b_points,
                                bool closed,
                                int* worst_index) {
  if (worst_index != nullptr) {
    *worst_index = -1;
  }
  if (a_points.empty() || b_points.empty()) {
    return std::numeric_limits<double>::infinity();
  }
  const int seg_count = closed ? static_cast<int>(b_points.size())
                               : std::max(0, static_cast<int>(b_points.size()) - 1);
  if (seg_count <= 0) {
    return pff_geom::Distance(a_points.front(), b_points.front());
  }

  double max_err = 0.0;
  int worst = -1;
  for (std::size_t point_index = 0; point_index < a_points.size(); ++point_index) {
    const pff_geom::Point p = a_points[point_index];
    double best = std::numeric_limits<double>::infinity();
    for (int i = 0; i < seg_count; ++i) {
      best = std::min(best, pff_geom::PointSegmentDistance(
                                p,
                                b_points[static_cast<std::size_t>(i)],
                                b_points[static_cast<std::size_t>((i + 1) % b_points.size())]));
    }
    if (best > max_err) {
      max_err = best;
      worst = static_cast<int>(point_index);
    }
  }
  if (worst_index != nullptr) {
    *worst_index = worst;
  }
  return max_err;
}

double DensePerimeter(const std::vector<DensePoint>& dense, bool closed) {
  const int n = static_cast<int>(dense.size());
  const int segments = closed ? n : std::max(0, n - 1);
  double total = 0.0;
  for (int i = 0; i < segments; ++i) {
    total += pff_geom::Distance(dense[static_cast<std::size_t>(i)].p,
                                dense[static_cast<std::size_t>((i + 1) % n)].p);
  }
  return total;
}

std::vector<double> DenseArcPositions(const std::vector<DensePoint>& dense, bool closed) {
  std::vector<double> arcs;
  arcs.reserve(dense.size());
  double cursor = 0.0;
  for (std::size_t i = 0; i < dense.size(); ++i) {
    arcs.push_back(cursor);
    const bool has_next = closed || i + 1 < dense.size();
    if (has_next) {
      const std::size_t next = (i + 1) % dense.size();
      cursor += pff_geom::Distance(dense[i].p, dense[next].p);
    }
  }
  return arcs;
}

double DenseFractionAtIndex(const std::vector<DensePoint>& dense,
                            const std::vector<double>& arcs,
                            bool closed,
                            int dense_index,
                            double total_length) {
  if (dense_index < 0 || dense_index >= static_cast<int>(dense.size()) ||
      dense_index >= static_cast<int>(arcs.size())) {
    return 0.0;
  }
  return NormalizeArcFraction(
      arcs[static_cast<std::size_t>(dense_index)], total_length, closed);
}

double ProjectPointToDenseFraction(const std::vector<DensePoint>& dense,
                                   const std::vector<double>& arcs,
                                   bool closed,
                                   pff_geom::Point p,
                                   double total_length) {
  const int n = static_cast<int>(dense.size());
  const int segments = closed ? n : std::max(0, n - 1);
  double best_distance = std::numeric_limits<double>::infinity();
  double best_arc = 0.0;
  for (int i = 0; i < segments; ++i) {
    const int next = (i + 1) % n;
    const pff_geom::Point a = dense[static_cast<std::size_t>(i)].p;
    const pff_geom::Point b = dense[static_cast<std::size_t>(next)].p;
    const pff_geom::Point ab = pff_geom::Sub(b, a);
    const double denom = pff_geom::Dot(ab, ab);
    if (!(denom > 1e-18)) {
      continue;
    }
    const double u = std::clamp(pff_geom::Dot(pff_geom::Sub(p, a), ab) / denom, 0.0, 1.0);
    const pff_geom::Point projected = pff_geom::Add(a, pff_geom::Mul(ab, u));
    const double distance = pff_geom::Distance(p, projected);
    if (distance < best_distance) {
      best_distance = distance;
      best_arc = arcs[static_cast<std::size_t>(i)] + pff_geom::Distance(a, b) * u;
    }
  }
  return NormalizeArcFraction(best_arc, total_length, closed);
}

}  // namespace pff_dense
}  // namespace bbsolver
