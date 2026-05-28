#include "bbsolver/path/reduction/path_bridge_refit.hpp"

#include "bbsolver/shape/shape_flat_topology.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace bbsolver {
namespace {

ShapeFlatPoint ShapeFlatCubicPoint(ShapeFlatPoint p0,
                                   ShapeFlatPoint p1,
                                   ShapeFlatPoint p2,
                                   ShapeFlatPoint p3,
                                   double u) {
  const double one = 1.0 - u;
  return {
      one * one * one * p0.x +
          3.0 * one * one * u * p1.x +
          3.0 * one * u * u * p2.x +
          u * u * u * p3.x,
      one * one * one * p0.y +
          3.0 * one * one * u * p1.y +
          3.0 * one * u * u * p2.y +
          u * u * u * p3.y,
  };
}

std::vector<std::pair<double, ShapeFlatPoint>> ShapeFlatBridgeSourcePoints(
    const std::vector<double>& flat,
    int start_idx,
    int end_idx,
    int samples_per_segment) {
  const std::vector<ShapeFlatVertex> vertices = ShapeFlatVertices(flat);
  const int n = static_cast<int>(vertices.size());
  std::vector<ShapeFlatPoint> points;
  if (n <= 0 || start_idx < 0 || start_idx >= n ||
      end_idx < 0 || end_idx >= n) {
    return {};
  }
  const int segment_samples = std::max(samples_per_segment, 2);
  int idx = start_idx;
  for (int guard = 0; guard < n; ++guard) {
    const int next_idx = (idx + 1) % n;
    const ShapeFlatVertex& a = vertices[static_cast<std::size_t>(idx)];
    const ShapeFlatVertex& b = vertices[static_cast<std::size_t>(next_idx)];
    const ShapeFlatPoint p0{a.x, a.y};
    const ShapeFlatPoint p1{a.x + a.out_x, a.y + a.out_y};
    const ShapeFlatPoint p2{b.x + b.in_x, b.y + b.in_y};
    const ShapeFlatPoint p3{b.x, b.y};
    for (int step = 0; step < segment_samples; ++step) {
      points.push_back(ShapeFlatCubicPoint(
          p0, p1, p2, p3,
          static_cast<double>(step) /
              static_cast<double>(segment_samples)));
    }
    if (next_idx == end_idx) {
      break;
    }
    idx = next_idx;
  }
  if (points.empty()) {
    return {};
  }
  const ShapeFlatVertex& end_vertex =
      vertices[static_cast<std::size_t>(end_idx)];
  points.push_back({end_vertex.x, end_vertex.y});

  std::vector<double> arc_lengths(points.size(), 0.0);
  double total = 0.0;
  for (std::size_t idx_point = 1; idx_point < points.size(); ++idx_point) {
    total += ShapeFlatDistance(points[idx_point - 1], points[idx_point]);
    arc_lengths[idx_point] = total;
  }

  std::vector<std::pair<double, ShapeFlatPoint>> out;
  out.reserve(points.size());
  for (std::size_t idx_point = 0; idx_point < points.size(); ++idx_point) {
    out.push_back({
        total > 1e-12 ? arc_lengths[idx_point] / total: 0.0,
        points[idx_point],
    });
  }
  return out;
}

bool SolveTwoHandleAxis(
    const std::vector<std::array<double, 3>>& rows,
    double* c1,
    double* c2) {
  double a00 = 0.0;
  double a01 = 0.0;
  double a11 = 0.0;
  double b0 = 0.0;
  double b1 = 0.0;
  for (const std::array<double, 3>& row: rows) {
    a00 += row[0] * row[0];
    a01 += row[0] * row[1];
    a11 += row[1] * row[1];
    b0 += row[0] * row[2];
    b1 += row[1] * row[2];
  }
  const double det = a00 * a11 - a01 * a01;
  if (std::abs(det) <= 1e-12) {
    return false;
  }
  *c1 = (b0 * a11 - b1 * a01) / det;
  *c2 = (a00 * b1 - a01 * b0) / det;
  return std::isfinite(*c1) && std::isfinite(*c2);
}

}  // namespace

std::vector<double> BridgeRefitRemoveShapeFlatVertex(
    const std::vector<double>& flat,
    int removed_index) {
  const bool closed = ShapeFlatClosed(flat);
  std::vector<ShapeFlatVertex> vertices = ShapeFlatVertices(flat);
  const int n = static_cast<int>(vertices.size());
  if (!closed || n < 4 || removed_index <= 0 || removed_index >= n) {
    return {};
  }

  const int prev_idx = (removed_index - 1 + n) % n;
  const int next_idx = (removed_index + 1) % n;
  const ShapeFlatPoint p0{
      vertices[static_cast<std::size_t>(prev_idx)].x,
      vertices[static_cast<std::size_t>(prev_idx)].y,
  };
  const ShapeFlatPoint p3{
      vertices[static_cast<std::size_t>(next_idx)].x,
      vertices[static_cast<std::size_t>(next_idx)].y,
  };

  std::vector<std::array<double, 3>> rows_x;
  std::vector<std::array<double, 3>> rows_y;
  const std::vector<std::pair<double, ShapeFlatPoint>> source_points =
      ShapeFlatBridgeSourcePoints(flat, prev_idx, next_idx, 12);
  if (source_points.empty()) {
    return {};
  }

  bool degenerate_bridge = true;
  const ShapeFlatPoint degenerate_reference = source_points.front().second;
  for (const auto& entry: source_points) {
    if (ShapeFlatDistance(entry.second, degenerate_reference) > 1e-7) {
      degenerate_bridge = false;
      break;
    }
  }
  if (degenerate_bridge) {
    vertices[static_cast<std::size_t>(prev_idx)].out_x = 0.0;
    vertices[static_cast<std::size_t>(prev_idx)].out_y = 0.0;
    vertices[static_cast<std::size_t>(next_idx)].in_x = 0.0;
    vertices[static_cast<std::size_t>(next_idx)].in_y = 0.0;
    vertices.erase(vertices.begin() + removed_index);
    return ShapeFlatFromVertices(vertices, closed);
  }

  rows_x.reserve(source_points.size());
  rows_y.reserve(source_points.size());
  for (const auto& entry: source_points) {
    const double q = entry.first;
    if (q <= 1e-6 || q >= 1.0 - 1e-6) {
      continue;
    }
    const ShapeFlatPoint point = entry.second;
    const double one = 1.0 - q;
    const double b0 = one * one * one;
    const double b1 = 3.0 * one * one * q;
    const double b2 = 3.0 * one * q * q;
    const double b3 = q * q * q;
    rows_x.push_back({b1, b2, point.x - b0 * p0.x - b3 * p3.x});
    rows_y.push_back({b1, b2, point.y - b0 * p0.y - b3 * p3.y});
  }

  double c1x = 0.0;
  double c2x = 0.0;
  double c1y = 0.0;
  double c2y = 0.0;
  if (!SolveTwoHandleAxis(rows_x, &c1x, &c2x) ||
      !SolveTwoHandleAxis(rows_y, &c1y, &c2y)) {
    return {};
  }

  vertices[static_cast<std::size_t>(prev_idx)].out_x = c1x - p0.x;
  vertices[static_cast<std::size_t>(prev_idx)].out_y = c1y - p0.y;
  vertices[static_cast<std::size_t>(next_idx)].in_x = c2x - p3.x;
  vertices[static_cast<std::size_t>(next_idx)].in_y = c2y - p3.y;
  vertices.erase(vertices.begin() + removed_index);
  return ShapeFlatFromVertices(vertices, closed);
}

bool ShapeFlatHasDuplicateTerminalClosure(
    const std::vector<double>& flat,
    double tolerance) {
  const int n = ShapeFlatVertexCount(flat);
  if (!ShapeFlatClosed(flat) || n < 4) {
    return false;
  }
  const double dx = ShapeFlatComponent(flat, 0, 0) -
                    ShapeFlatComponent(flat, n - 1, 0);
  const double dy = ShapeFlatComponent(flat, 0, 1) -
                    ShapeFlatComponent(flat, n - 1, 1);
  const double eps = std::max(tolerance, 1e-6);
  return std::sqrt(dx * dx + dy * dy) <= eps;
}

std::vector<double> DropShapeFlatDuplicateTerminalClosure(
    const std::vector<double>& flat) {
  const int n = ShapeFlatVertexCount(flat);
  if (n <= 1) {
    return flat;
  }

  std::vector<double> out;
  out.reserve(static_cast<std::size_t>(2 + (n - 1) * 6));
  out.push_back(1.0);
  out.push_back(static_cast<double>(n - 1));
  for (int i = 0; i < n - 1; ++i) {
    const std::size_t offset = ShapeFlatVertexOffset(i);
    for (int c = 0; c < 6; ++c) {
      out.push_back(flat[offset + static_cast<std::size_t>(c)]);
    }
  }

  // A duplicated terminal vertex carries the incoming tangent for the closing
  // segment. After dropping that terminal vertex, attach its incoming tangent to
  // the real first vertex so the closing segment keeps the same handle.
  out[ShapeFlatVertexOffset(0) + 2] = ShapeFlatComponent(flat, n - 1, 2);
  out[ShapeFlatVertexOffset(0) + 3] = ShapeFlatComponent(flat, n - 1, 3);
  return out;
}

}  // namespace bbsolver
