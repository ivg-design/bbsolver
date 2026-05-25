#pragma once

// Shared 2D geometry primitives + shape_flat header decoder used across the
// path_frame_fit family. Pure leaf: no DiagnosticsWriter, no progress, no
// acceptance state — only arithmetic and structural decoding.
//
// These primitives previously lived in path_frame_fit.cpp's anonymous
// namespace. They are factored here so the visible-outline extraction module
// (and any future leaf extractions) can share them without duplicating the
// arithmetic. Each function is `inline` so the symbols stay header-local; we
// do not declare them in path_frame_fit.hpp because they are not part of the
// public solver surface — only translation units that opt in via
// `#include "bbsolver/path/frame_fit/path_frame_fit_geometry.hpp"` see them.
//
// Usage from a .cpp:
//   namespace bbsolver { namespace { using namespace pff_geom; ... } }
// keeps existing unqualified call sites compiling unchanged.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace bbsolver {
namespace pff_geom {

constexpr int kPathHeaderScalars = 2;
constexpr int kScalarsPerVertex = 6;
constexpr double kPi = 3.14159265358979323846;

struct Point {
  double x = 0.0;
  double y = 0.0;
};

struct DecodedShape {
  bool ok = false;
  bool closed = false;
  int vertex_count = 0;
};

inline double ComponentOrZero(const std::vector<double>& values, std::size_t idx) {
  return idx < values.size() ? values[idx] : 0.0;
}

inline DecodedShape DecodeShapeFlat(const std::vector<double>& flat) {
  DecodedShape decoded;
  if (flat.size() < static_cast<std::size_t>(kPathHeaderScalars)) {
    return decoded;
  }
  decoded.closed = static_cast<int>(std::llround(flat[0])) != 0;
  decoded.vertex_count = static_cast<int>(std::llround(flat[1]));
  const int required = kPathHeaderScalars + decoded.vertex_count * kScalarsPerVertex;
  decoded.ok = decoded.vertex_count > 0 && required <= static_cast<int>(flat.size());
  return decoded;
}

inline std::size_t VertexOffset(int vertex_index) {
  return static_cast<std::size_t>(kPathHeaderScalars + vertex_index * kScalarsPerVertex);
}

inline Point FlatPoint(const std::vector<double>& flat, int vertex_index, int channel_offset) {
  const std::size_t offset = VertexOffset(vertex_index) + static_cast<std::size_t>(channel_offset);
  return {
      ComponentOrZero(flat, offset),
      ComponentOrZero(flat, offset + 1),
  };
}

inline Point Add(Point a, Point b) {
  return {a.x + b.x, a.y + b.y};
}

inline Point Sub(Point a, Point b) {
  return {a.x - b.x, a.y - b.y};
}

inline Point Mul(Point p, double scale) {
  return {p.x * scale, p.y * scale};
}

inline double Dot(Point a, Point b) {
  return a.x * b.x + a.y * b.y;
}

inline Point Lerp(Point a, Point b, double t) {
  return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

inline double Length(Point p) {
  return std::sqrt(p.x * p.x + p.y * p.y);
}

inline double Distance(Point a, Point b) {
  return Length(Sub(a, b));
}

inline double Cross(Point a, Point b) {
  return a.x * b.y - a.y * b.x;
}

inline Point ClampLength(Point p, double max_length) {
  const double len = Length(p);
  if (max_length >= 0.0 && len > max_length && len > 1e-12) {
    return Mul(p, max_length / len);
  }
  return p;
}

inline std::string QuantizedPointKey(Point p) {
  const double scale = 1000000.0;
  const long long x = static_cast<long long>(std::llround(p.x * scale));
  const long long y = static_cast<long long>(std::llround(p.y * scale));
  return std::to_string(x) + ":" + std::to_string(y);
}

inline Point SegmentPoint(Point a, Point b, double t) {
  return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

// Bernstein-form cubic Bezier evaluator at parameter t in [0, 1]. p0/p3 are
// the endpoints; p1/p2 are the inner control points. Shared here so the
// dense-polyline module can call it without duplication.
inline Point Cubic(Point p0, Point p1, Point p2, Point p3, double t) {
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

// Distance from point `p` to the closed segment a-b. Shared here so the
// dense-polyline module can call it without duplication. Degenerate (a == b)
// segments collapse to point-to-point distance.
inline double PointSegmentDistance(Point p, Point a, Point b) {
  const Point ab = Sub(b, a);
  const double denom = ab.x * ab.x + ab.y * ab.y;
  if (!(denom > 1e-18)) {
    return Distance(p, a);
  }
  const double u =
      std::clamp(((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / denom, 0.0, 1.0);
  return Distance(p, {a.x + ab.x * u, a.y + ab.y * u});
}

}  // namespace pff_geom
}  // namespace bbsolver
