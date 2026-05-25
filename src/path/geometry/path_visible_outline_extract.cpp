// Implements bbsolver::ExtractVisibleShapeFlatOutline (declared in
// path_frame_fit.hpp). For self-overlapping closed paths this lifts the
// filled-region boundary out of the densified outline; simple non-overlapping
// paths are returned as not applied so callers know to keep the source.
//
// Pure geometry leaf: no DiagnosticsWriter, no progress events, no acceptance
// negotiation. Failure reasons are reported through
// VisibleShapeFlatOutlineResult::warning so callers can route them into their
// own diagnostics if they want; nothing on this path emits events directly.
//
// Extracted from path_frame_fit.cpp without altering algorithmic behavior:
// every helper (segment intersection, winding number, signed area,
// directed-boundary walk, source-polygon dedupe, zero-tangent shape_flat
// builder) had a single call site in the original file and moves wholesale
// here.

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "bbsolver/path/frame_fit/path_frame_fit_geometry.hpp"

namespace bbsolver {
namespace {

using namespace pff_geom;

struct SegmentIntersection {
  bool hit = false;
  double t = 0.0;
  double u = 0.0;
  Point p;
};

struct DirectedBoundaryEdge {
  int from = -1;
  int to = -1;
  double angle = 0.0;
};

SegmentIntersection ProperSegmentIntersection(Point a, Point b, Point c, Point d) {
  SegmentIntersection out;
  const Point r = Sub(b, a);
  const Point s = Sub(d, c);
  const double denom = Cross(r, s);
  if (std::abs(denom) <= 1e-10) {
    return out;
  }
  const Point ca = Sub(c, a);
  const double t = Cross(ca, s) / denom;
  const double u = Cross(ca, r) / denom;
  constexpr double kEndpointEps = 1e-7;
  if (t <= kEndpointEps || t >= 1.0 - kEndpointEps ||
      u <= kEndpointEps || u >= 1.0 - kEndpointEps) {
    return out;
  }
  out.hit = true;
  out.t = t;
  out.u = u;
  out.p = SegmentPoint(a, b, t);
  return out;
}

bool AdjacentClosedEdges(int a, int b, int edge_count) {
  return std::abs(a - b) == 1 || std::abs(a - b) == edge_count - 1;
}

int WindingNumberAtPoint(const std::vector<Point>& polygon, Point p) {
  int winding = 0;
  const int n = static_cast<int>(polygon.size());
  for (int i = 0; i < n; ++i) {
    const Point a = polygon[static_cast<std::size_t>(i)];
    const Point b = polygon[static_cast<std::size_t>((i + 1) % n)];
    if (a.y <= p.y) {
      if (b.y > p.y && Cross(Sub(b, a), Sub(p, a)) > 0.0) {
        ++winding;
      }
    } else if (b.y <= p.y && Cross(Sub(b, a), Sub(p, a)) < 0.0) {
      --winding;
    }
  }
  return winding;
}

std::vector<double> BuildZeroTangentShapeFlat(bool closed,
                                              const std::vector<Point>& vertices) {
  std::vector<double> out;
  out.reserve(static_cast<std::size_t>(
      kPathHeaderScalars + static_cast<int>(vertices.size()) * kScalarsPerVertex));
  out.push_back(closed ? 1.0 : 0.0);
  out.push_back(static_cast<double>(vertices.size()));
  for (Point p : vertices) {
    out.push_back(p.x);
    out.push_back(p.y);
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
  }
  return out;
}

double SignedArea(const std::vector<Point>& vertices) {
  double area = 0.0;
  const int n = static_cast<int>(vertices.size());
  for (int i = 0; i < n; ++i) {
    const Point a = vertices[static_cast<std::size_t>(i)];
    const Point b = vertices[static_cast<std::size_t>((i + 1) % n)];
    area += a.x * b.y - b.x * a.y;
  }
  return area * 0.5;
}

std::vector<Point> ShapeFlatSourcePolygonPoints(const std::vector<double>& flat,
                                                const DecodedShape& decoded) {
  std::vector<Point> points;
  if (!decoded.ok || decoded.vertex_count <= 0) {
    return points;
  }
  points.reserve(static_cast<std::size_t>(decoded.vertex_count));
  for (int i = 0; i < decoded.vertex_count; ++i) {
    Point p = FlatPoint(flat, i, 0);
    if (!points.empty() && Distance(points.back(), p) <= 1e-7) {
      continue;
    }
    points.push_back(p);
  }
  if (decoded.closed && points.size() > 1 &&
      Distance(points.front(), points.back()) <= 1e-7) {
    points.pop_back();
  }
  return points;
}

}  // namespace

VisibleShapeFlatOutlineResult ExtractVisibleShapeFlatOutline(
    const std::vector<double>& shape_flat,
    const PathFrameFitOptions& options) {
  VisibleShapeFlatOutlineResult result;
  const DecodedShape decoded = DecodeShapeFlat(shape_flat);
  if (!decoded.ok) {
    result.warning = "malformed shape_flat frame";
    return result;
  }
  result.ok = true;
  result.closed = decoded.closed;
  result.source_vertex_count = decoded.vertex_count;
  if (!decoded.closed || decoded.vertex_count < 4) {
    result.warning = "visible_outline_skipped: open_or_too_small";
    return result;
  }

  (void)options;
  std::vector<Point> polygon =
      ShapeFlatSourcePolygonPoints(shape_flat, decoded);
  const int edge_count = static_cast<int>(polygon.size());
  if (edge_count < 4) {
    result.warning = "visible_outline_skipped: insufficient_dense_points";
    return result;
  }

  std::vector<std::vector<double>> split_params(
      static_cast<std::size_t>(edge_count));
  for (auto& params : split_params) {
    params.push_back(0.0);
    params.push_back(1.0);
  }

  int intersections = 0;
  for (int i = 0; i < edge_count; ++i) {
    const Point a = polygon[static_cast<std::size_t>(i)];
    const Point b = polygon[static_cast<std::size_t>((i + 1) % edge_count)];
    if (Distance(a, b) <= 1e-9) {
      continue;
    }
    for (int j = i + 1; j < edge_count; ++j) {
      if (AdjacentClosedEdges(i, j, edge_count)) {
        continue;
      }
      const Point c = polygon[static_cast<std::size_t>(j)];
      const Point d = polygon[static_cast<std::size_t>((j + 1) % edge_count)];
      if (Distance(c, d) <= 1e-9) {
        continue;
      }
      const SegmentIntersection hit = ProperSegmentIntersection(a, b, c, d);
      if (!hit.hit) {
        continue;
      }
      split_params[static_cast<std::size_t>(i)].push_back(hit.t);
      split_params[static_cast<std::size_t>(j)].push_back(hit.u);
      ++intersections;
    }
  }
  if (intersections == 0) {
    result.warning = "visible_outline_skipped: no_self_intersections";
    return result;
  }

  std::vector<Point> nodes;
  std::unordered_map<std::string, int> node_by_key;
  auto node_id = [&](Point p) {
    const std::string key = QuantizedPointKey(p);
    const auto found = node_by_key.find(key);
    if (found != node_by_key.end()) {
      return found->second;
    }
    const int id = static_cast<int>(nodes.size());
    nodes.push_back(p);
    node_by_key[key] = id;
    return id;
  };

  std::vector<DirectedBoundaryEdge> boundary_edges;
  const double probe = std::max(
      1e-4, std::min(0.25, std::max(options.outline_tolerance, 0.01) * 0.05));
  for (int edge = 0; edge < edge_count; ++edge) {
    std::vector<double>& params = split_params[static_cast<std::size_t>(edge)];
    std::sort(params.begin(), params.end());
    params.erase(std::unique(params.begin(), params.end(), [](double a, double b) {
      return std::abs(a - b) <= 1e-7;
    }), params.end());

    const Point a = polygon[static_cast<std::size_t>(edge)];
    const Point b = polygon[static_cast<std::size_t>((edge + 1) % edge_count)];
    for (std::size_t idx = 1; idx < params.size(); ++idx) {
      const double t0 = params[idx - 1];
      const double t1 = params[idx];
      if (t1 - t0 <= 1e-7) {
        continue;
      }
      const Point p0 = SegmentPoint(a, b, t0);
      const Point p1 = SegmentPoint(a, b, t1);
      const Point dir = Sub(p1, p0);
      const double len = Length(dir);
      if (len <= 1e-9) {
        continue;
      }
      const Point mid = SegmentPoint(p0, p1, 0.5);
      const Point normal{-dir.y / len, dir.x / len};
      const bool left_inside =
          WindingNumberAtPoint(polygon, Add(mid, Mul(normal, probe))) != 0;
      const bool right_inside =
          WindingNumberAtPoint(polygon, Sub(mid, Mul(normal, probe))) != 0;
      if (left_inside == right_inside) {
        continue;
      }
      const int id0 = node_id(p0);
      const int id1 = node_id(p1);
      if (id0 == id1) {
        continue;
      }
      if (left_inside) {
        boundary_edges.push_back(
            {id0, id1,
             std::atan2(nodes[static_cast<std::size_t>(id1)].y -
                            nodes[static_cast<std::size_t>(id0)].y,
                        nodes[static_cast<std::size_t>(id1)].x -
                            nodes[static_cast<std::size_t>(id0)].x)});
      } else {
        boundary_edges.push_back(
            {id1, id0,
             std::atan2(nodes[static_cast<std::size_t>(id0)].y -
                            nodes[static_cast<std::size_t>(id1)].y,
                        nodes[static_cast<std::size_t>(id0)].x -
                            nodes[static_cast<std::size_t>(id1)].x)});
      }
    }
  }
  if (boundary_edges.size() < 3) {
    result.warning = "visible_outline_skipped: no_boundary_edges";
    return result;
  }

  std::vector<std::vector<int>> outgoing(nodes.size());
  for (std::size_t edge_idx = 0; edge_idx < boundary_edges.size(); ++edge_idx) {
    outgoing[static_cast<std::size_t>(boundary_edges[edge_idx].from)]
        .push_back(static_cast<int>(edge_idx));
  }

  std::vector<bool> used(boundary_edges.size(), false);
  std::vector<Point> best_loop;
  double best_area = 0.0;
  for (std::size_t start_idx = 0; start_idx < boundary_edges.size(); ++start_idx) {
    if (used[start_idx]) {
      continue;
    }
    std::vector<Point> loop;
    int current = static_cast<int>(start_idx);
    for (int guard = 0; guard < static_cast<int>(boundary_edges.size()) + 8; ++guard) {
      if (current < 0 || current >= static_cast<int>(boundary_edges.size()) ||
          used[static_cast<std::size_t>(current)]) {
        break;
      }
      used[static_cast<std::size_t>(current)] = true;
      const DirectedBoundaryEdge& edge =
          boundary_edges[static_cast<std::size_t>(current)];
      if (loop.empty()) {
        loop.push_back(nodes[static_cast<std::size_t>(edge.from)]);
      }
      loop.push_back(nodes[static_cast<std::size_t>(edge.to)]);
      if (edge.to == boundary_edges[start_idx].from) {
        break;
      }

      const std::vector<int>& candidates =
          outgoing[static_cast<std::size_t>(edge.to)];
      int best_next = -1;
      double best_turn = std::numeric_limits<double>::infinity();
      for (int candidate_idx : candidates) {
        if (used[static_cast<std::size_t>(candidate_idx)]) {
          continue;
        }
        double turn =
            boundary_edges[static_cast<std::size_t>(candidate_idx)].angle -
            edge.angle;
        while (turn <= -kPi) {
          turn += 2.0 * kPi;
        }
        while (turn > kPi) {
          turn -= 2.0 * kPi;
        }
        const double score = std::abs(turn);
        if (score < best_turn) {
          best_turn = score;
          best_next = candidate_idx;
        }
      }
      current = best_next;
    }
    if (loop.size() > 1 && Distance(loop.front(), loop.back()) <= 1e-7) {
      loop.pop_back();
    }
    if (loop.size() >= 3) {
      const double area = std::abs(SignedArea(loop));
      if (area > best_area) {
        best_area = area;
        best_loop = std::move(loop);
      }
    }
  }
  if (best_loop.size() < 3) {
    result.warning = "visible_outline_skipped: boundary_trace_failed";
    return result;
  }

  result.outline = BuildZeroTangentShapeFlat(true, best_loop);
  result.outline_vertex_count = static_cast<int>(best_loop.size());
  result.applied = result.outline_vertex_count > 0 &&
                   result.outline_vertex_count != decoded.vertex_count;
  result.warning =
      "visible_outline; self_intersections=" + std::to_string(intersections) +
      "; boundary_edges=" + std::to_string(boundary_edges.size()) +
      "; outline_vertices=" + std::to_string(result.outline_vertex_count);
  return result;
}

}  // namespace bbsolver
