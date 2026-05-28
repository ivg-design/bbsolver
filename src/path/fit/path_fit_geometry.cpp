#include "bbsolver/path/fit/path_fit_geometry.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace bbsolver {
namespace path_fit_geometry {
namespace {

double ComponentOrZero(const std::vector<double>& values, std::size_t idx) {
  return idx < values.size() ? values[idx]: 0.0;
}

std::size_t VertexOffset(int vertex_index) {
  return static_cast<std::size_t>(
      kPathHeaderScalars + vertex_index * kScalarsPerVertex);
}

Point InTangentAt(const std::vector<double>& flat, int vertex_index) {
  const std::size_t offset = VertexOffset(vertex_index) + 2;
  return {
      ComponentOrZero(flat, offset),
      ComponentOrZero(flat, offset + 1),
  };
}

Point OutTangentAt(const std::vector<double>& flat, int vertex_index) {
  const std::size_t offset = VertexOffset(vertex_index) + 4;
  return {
      ComponentOrZero(flat, offset),
      ComponentOrZero(flat, offset + 1),
  };
}

Point Cubic(Point p0, Point p1, Point p2, Point p3, double t) {
  const double u = 1.0 - t;
  const double uu = u * u;
  const double tt = t * t;
  const double uuu = uu * u;
  const double ttt = tt * t;
  return {
      p0.x * uuu + 3.0 * p1.x * uu * t + 3.0 * p2.x * u * tt + p3.x * ttt,
      p0.y * uuu + 3.0 * p1.y * uu * t + 3.0 * p2.y * u * tt + p3.y * ttt,
  };
}

void PushDensePoint(std::vector<Point>& points, Point p) {
  if (!points.empty() && Distance(points.back(), p) < 1e-6) {
    return;
  }
  points.push_back(p);
}

}  // namespace

bool IsShapeFlatPath(const PropertySamples& ps) {
  return ps.property.kind == ValueKind::Custom &&
         ps.property.units_label == "shape_flat";
}

DecodedShape DecodeHeader(const std::vector<double>& flat) {
  DecodedShape decoded;
  if (flat.size() < static_cast<std::size_t>(kPathHeaderScalars)) {
    return decoded;
  }
  decoded.closed = static_cast<int>(std::llround(flat[0])) != 0;
  decoded.vertex_count = static_cast<int>(std::llround(flat[1]));
  const int required =
      kPathHeaderScalars + decoded.vertex_count * kScalarsPerVertex;
  decoded.ok = decoded.vertex_count > 0 &&
               required <= static_cast<int>(flat.size());
  return decoded;
}

Point VertexAt(const std::vector<double>& flat, int vertex_index) {
  const std::size_t offset = VertexOffset(vertex_index);
  return {
      ComponentOrZero(flat, offset),
      ComponentOrZero(flat, offset + 1),
  };
}

double Length(Point p) {
  return std::sqrt(p.x * p.x + p.y * p.y);
}

Point Sub(Point a, Point b) {
  return {a.x - b.x, a.y - b.y};
}

double Distance(Point a, Point b) {
  return Length(Sub(a, b));
}

double PointSegmentDistance(Point p, Point a, Point b) {
  const Point ab = Sub(b, a);
  const double denom = ab.x * ab.x + ab.y * ab.y;
  if (!(denom > 1e-18)) {
    return Distance(p, a);
  }
  const double u =
      std::clamp(((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / denom,
                 0.0,
                 1.0);
  return Distance(p, {a.x + ab.x * u, a.y + ab.y * u});
}

std::vector<Point> FlatToDensePolyline(const std::vector<double>& flat) {
  std::vector<Point> points;
  const DecodedShape decoded = DecodeHeader(flat);
  if (!decoded.ok) {
    return points;
  }

  const int seg_count = decoded.closed
      ? decoded.vertex_count
: std::max(0, decoded.vertex_count - 1);
  if (seg_count <= 0) {
    points.push_back(VertexAt(flat, 0));
    return points;
  }

  for (int i = 0; i < seg_count; ++i) {
    const int next = (i + 1) % decoded.vertex_count;
    const Point p0 = VertexAt(flat, i);
    const Point p3 = VertexAt(flat, next);
    const Point out_tangent = OutTangentAt(flat, i);
    const Point in_tangent = InTangentAt(flat, next);
    const Point p1 = {p0.x + out_tangent.x, p0.y + out_tangent.y};
    const Point p2 = {p3.x + in_tangent.x, p3.y + in_tangent.y};
    const double control_len =
        Distance(p0, p1) + Distance(p1, p2) + Distance(p2, p3);
    const double chord_len = Distance(p0, p3);
    const int divs = static_cast<int>(std::ceil(std::max(
        6.0,
        std::min(32.0,
                 (control_len + std::abs(control_len - chord_len) * 2.0) /
                     10.0))));
    if (i == 0) {
      PushDensePoint(points, p0);
    }
    for (int j = 1; j <= divs; ++j) {
      if (decoded.closed && i == seg_count - 1 && j == divs) {
        continue;
      }
      PushDensePoint(points,
                     Cubic(p0, p1, p2, p3,
                           static_cast<double>(j) / divs));
    }
  }
  return points;
}

double DirectedPolylineDistance(const std::vector<Point>& a_points,
                                const std::vector<Point>& b_points,
                                bool closed) {
  if (a_points.empty() || b_points.empty()) {
    return std::numeric_limits<double>::infinity();
  }
  const int seg_count = closed
      ? static_cast<int>(b_points.size())
: std::max(0, static_cast<int>(b_points.size()) - 1);
  if (seg_count <= 0) {
    return Distance(a_points.front(), b_points.front());
  }

  double max_err = 0.0;
  for (Point p: a_points) {
    double best = std::numeric_limits<double>::infinity();
    for (int i = 0; i < seg_count; ++i) {
      best = std::min(
          best,
          PointSegmentDistance(
              p,
              b_points[static_cast<std::size_t>(i)],
              b_points[static_cast<std::size_t>(
                  (i + 1) % b_points.size())]));
    }
    max_err = std::max(max_err, best);
  }
  return max_err;
}

bool HasZeroTangentFeature(const std::vector<double>& flat, int vertex_index) {
  return Length(InTangentAt(flat, vertex_index)) <= kZeroTangentEps &&
         Length(OutTangentAt(flat, vertex_index)) <= kZeroTangentEps;
}

}  // namespace path_fit_geometry
}  // namespace bbsolver
