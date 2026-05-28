#include "bbsolver/path/fit/path_fit.hpp"
#include "bbsolver/domain.hpp"

#include "oneapi/tbb/parallel_for.h"
#include "bbsolver/path/fit/path_fit_geometry.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>

#ifdef BBSOLVER_HAVE_TBB
#include <tbb/parallel_for.h>
#include <cstddef>
#include <vector>
#endif

namespace bbsolver {
namespace {

constexpr double kPi = 3.14159265358979323846;

using path_fit_geometry::DecodedShape;
using path_fit_geometry::DecodeHeader;
using path_fit_geometry::DirectedPolylineDistance;
using path_fit_geometry::FlatToDensePolyline;
using path_fit_geometry::HasZeroTangentFeature;
using path_fit_geometry::IsShapeFlatPath;
using path_fit_geometry::Length;
using path_fit_geometry::Point;
using path_fit_geometry::PointSegmentDistance;
using path_fit_geometry::Sub;
using path_fit_geometry::VertexAt;
using path_fit_geometry::kPathHeaderScalars;
using path_fit_geometry::kScalarsPerVertex;

struct KeepErrorReport {
  double max_error = 0.0;
  double worst_raw_err = -1.0;
  int worst_vertex = -1;
};

std::vector<double> BuildFittedFlat(const std::vector<double>& source,
                                    const std::vector<int>& kept,
                                    bool closed);

double EffectivePathFitTolerance(const SolverConfig& cfg) {
  double tolerance = 0.0;
  if (std::isfinite(cfg.tolerance)) {
    tolerance = std::max(tolerance, cfg.tolerance);
  }
  if (std::isfinite(cfg.tolerance_screen_px)) {
    tolerance = std::max(tolerance, cfg.tolerance_screen_px);
  }
  return std::max(tolerance, 1e-6);
}

double SharpTurnRadians(const SolverConfig& cfg) {
  double deg = 90.0;
  if (std::isfinite(cfg.path_sharp_corner_angle_deg)) {
    deg = std::min(175.0, std::max(5.0, cfg.path_sharp_corner_angle_deg));
  }
  return deg * (kPi / 180.0);
}

double TurnAngle(const std::vector<double>& flat, int vertex_index, int vertex_count, bool closed) {
  if (vertex_count < 3) {
    return 0.0;
  }
  if (!closed && (vertex_index == 0 || vertex_index == vertex_count - 1)) {
    return std::numeric_limits<double>::infinity();
  }
  const int prev = (vertex_index == 0) ? vertex_count - 1: vertex_index - 1;
  const int next = (vertex_index + 1) % vertex_count;
  const Point p = VertexAt(flat, vertex_index);
  const Point a = VertexAt(flat, prev);
  const Point b = VertexAt(flat, next);
  const Point va = Sub(a, p);
  const Point vb = Sub(b, p);
  const double la = Length(va);
  const double lb = Length(vb);
  if (!(la > 1e-9) || !(lb > 1e-9)) {
    return std::numeric_limits<double>::infinity();
  }
  const double cos_theta = std::clamp((va.x * vb.x + va.y * vb.y) / (la * lb), -1.0, 1.0);
  const double interior = std::acos(cos_theta);
  return std::abs(kPi - interior);
}

std::vector<bool> DetectLockedVertices(const PropertySamples& ps,
                                       const SolverConfig& cfg,
                                       int vertex_count,
                                       bool closed) {
  std::vector<bool> locked(static_cast<std::size_t>(vertex_count), false);
  if (vertex_count <= 0) {
    return locked;
  }
  locked[0] = true;
  if (!closed && vertex_count > 1) {
    locked[static_cast<std::size_t>(vertex_count - 1)] = true;
  }

  if (!cfg.path_preserve_sharp_corners) {
    return locked;
  }

  const double sharp_turn = SharpTurnRadians(cfg);
  for (const Sample& sample: ps.samples) {
    for (int i = 0; i < vertex_count; ++i) {
      // AE straight-segment paths commonly store every redundant point with
      // zero handles. A zero-tangent point is therefore not a landmark by
      // itself; protect geometric turns and let collinear zero-handle points
      // decimate under the active tolerance.
      if (TurnAngle(sample.v, i, vertex_count, closed) >= sharp_turn) {
        locked[static_cast<std::size_t>(i)] = true;
      }
    }
  }
  return locked;
}

std::vector<int> BoolToIndices(const std::vector<bool>& flags) {
  std::vector<int> out;
  for (std::size_t i = 0; i < flags.size(); ++i) {
    if (flags[i]) {
      out.push_back(static_cast<int>(i));
    }
  }
  return out;
}

int PreviousKept(const std::vector<bool>& keep, int vertex_index, bool closed) {
  const int n = static_cast<int>(keep.size());
  for (int step = 1; step < n; ++step) {
    const int idx = vertex_index - step;
    if (idx >= 0 && keep[static_cast<std::size_t>(idx)]) {
      return idx;
    }
    if (closed && idx < 0) {
      const int wrapped = idx + n;
      if (keep[static_cast<std::size_t>(wrapped)]) {
        return wrapped;
      }
    }
    if (!closed && idx < 0) {
      break;
    }
  }
  return -1;
}

int NextKept(const std::vector<bool>& keep, int vertex_index, bool closed) {
  const int n = static_cast<int>(keep.size());
  for (int step = 1; step < n; ++step) {
    const int idx = vertex_index + step;
    if (idx < n && keep[static_cast<std::size_t>(idx)]) {
      return idx;
    }
    if (closed && idx >= n) {
      const int wrapped = idx - n;
      if (keep[static_cast<std::size_t>(wrapped)]) {
        return wrapped;
      }
    }
    if (!closed && idx >= n) {
      break;
    }
  }
  return -1;
}

KeepErrorReport ComputeKeepErrorForSample(const Sample& sample,
                                          const std::vector<int>& kept,
                                          const std::vector<bool>& keep,
                                          bool closed) {
  KeepErrorReport report;
  const int vertex_count = static_cast<int>(keep.size());
  if (kept.size() >= 2) {
    const std::vector<double> fitted = BuildFittedFlat(sample.v, kept, closed);
    const std::vector<Point> source_dense = FlatToDensePolyline(sample.v);
    const std::vector<Point> fitted_dense = FlatToDensePolyline(fitted);
    const double outline_err = std::max(
        DirectedPolylineDistance(source_dense, fitted_dense, closed),
        DirectedPolylineDistance(fitted_dense, source_dense, closed));
    report.max_error = std::max(report.max_error, outline_err);
  }
  for (int i = 0; i < vertex_count; ++i) {
    if (keep[static_cast<std::size_t>(i)]) {
      continue;
    }
    const int prev = PreviousKept(keep, i, closed);
    const int next = NextKept(keep, i, closed);
    if (prev < 0 || next < 0 || prev == next) {
      continue;
    }
    const double err = PointSegmentDistance(
        VertexAt(sample.v, i), VertexAt(sample.v, prev), VertexAt(sample.v, next));
    if (err > report.max_error) {
      report.max_error = err;
    }
    if (err > report.worst_raw_err) {
      report.worst_raw_err = err;
      report.worst_vertex = i;
    }
  }
  return report;
}

KeepErrorReport ReduceKeepErrorReports(
    const std::vector<KeepErrorReport>& reports) {
  KeepErrorReport out;
  for (const KeepErrorReport& report: reports) {
    if (report.max_error > out.max_error) {
      out.max_error = report.max_error;
    }
    if (report.worst_raw_err > out.worst_raw_err) {
      out.worst_raw_err = report.worst_raw_err;
      out.worst_vertex = report.worst_vertex;
    }
  }
  return out;
}

bool ShouldParallelKeepErrorScan(const SolverConfig& cfg,
                                 std::size_t sample_count) {
#ifdef BBSOLVER_HAVE_TBB
  return cfg.parallel_jobs > 1 && sample_count >= 64;
#else
  (void)cfg;
  (void)sample_count;
  return false;
#endif
}

double MaxOutlineErrorForKeep(const PropertySamples& ps,
                              const SolverConfig& cfg,
                              const std::vector<bool>& keep,
                              bool closed,
                              int* worst_vertex) {
  const int vertex_count = static_cast<int>(keep.size());
  std::vector<int> kept;
  for (int i = 0; i < vertex_count; ++i) {
    if (keep[static_cast<std::size_t>(i)]) {
      kept.push_back(i);
    }
  }

  std::vector<KeepErrorReport> reports(ps.samples.size());
  if (ShouldParallelKeepErrorScan(cfg, ps.samples.size())) {
#ifdef BBSOLVER_HAVE_TBB
    tbb::parallel_for(std::size_t{0},
                      ps.samples.size(),
                      [&](std::size_t sample_idx) {
                        reports[sample_idx] = ComputeKeepErrorForSample(
                            ps.samples[sample_idx], kept, keep, closed);
                      });
#endif
  } else {
    for (std::size_t sample_idx = 0; sample_idx < ps.samples.size(); ++sample_idx) {
      reports[sample_idx] = ComputeKeepErrorForSample(
          ps.samples[sample_idx], kept, keep, closed);
    }
  }

  const KeepErrorReport report = ReduceKeepErrorReports(reports);
  if (worst_vertex != nullptr) {
    *worst_vertex = report.worst_vertex;
  }
  return report.max_error;
}

void EnsureMinimumKept(std::vector<bool>& keep, bool closed) {
  const int n = static_cast<int>(keep.size());
  const int min_keep = closed ? std::min(n, 3): std::min(n, 2);
  while (static_cast<int>(std::count(keep.begin(), keep.end(), true)) < min_keep) {
    int best_idx = -1;
    int best_gap = -1;
    for (int i = 0; i < n; ++i) {
      if (keep[static_cast<std::size_t>(i)]) {
        continue;
      }
      const int prev = PreviousKept(keep, i, closed);
      const int next = NextKept(keep, i, closed);
      int gap = 0;
      if (prev >= 0 && next >= 0) {
        gap = next >= prev ? next - prev: n - prev + next;
      } else {
        gap = n;
      }
      if (gap > best_gap) {
        best_gap = gap;
        best_idx = i;
      }
    }
    if (best_idx < 0) {
      break;
    }
    keep[static_cast<std::size_t>(best_idx)] = true;
  }
}

std::vector<int> SelectKeptVertices(const PropertySamples& ps,
                                    const SolverConfig& cfg,
                                    int vertex_count,
                                    bool closed,
                                    int* locked_count,
                                    double* max_outline_error) {
  std::vector<bool> keep = DetectLockedVertices(ps, cfg, vertex_count, closed);
  if (locked_count != nullptr) {
    *locked_count = static_cast<int>(std::count(keep.begin(), keep.end(), true));
  }
  EnsureMinimumKept(keep, closed);

  const double tolerance = EffectivePathFitTolerance(cfg);
  for (;;) {
    int worst_vertex = -1;
    const double err = MaxOutlineErrorForKeep(ps, cfg, keep, closed, &worst_vertex);
    if (err <= tolerance || worst_vertex < 0 ||
        static_cast<int>(std::count(keep.begin(), keep.end(), true)) >= vertex_count) {
      if (max_outline_error != nullptr) {
        *max_outline_error = err;
      }
      break;
    }
    keep[static_cast<std::size_t>(worst_vertex)] = true;
  }
  return BoolToIndices(keep);
}

Point CatmullIn(const std::vector<double>& flat, const std::vector<int>& kept, std::size_t kept_index, bool closed) {
  const std::size_t n = kept.size();
  if (n < 2 || (!closed && kept_index == 0)) {
    return {0.0, 0.0};
  }
  const std::size_t prev_idx = kept_index == 0 ? n - 1: kept_index - 1;
  const std::size_t next_idx = (kept_index + 1) % n;
  if (!closed && kept_index + 1 >= n) {
    return {0.0, 0.0};
  }
  const Point prev = VertexAt(flat, kept[prev_idx]);
  const Point next = VertexAt(flat, kept[next_idx]);
  return {(prev.x - next.x) / 6.0, (prev.y - next.y) / 6.0};
}

Point CatmullOut(const std::vector<double>& flat, const std::vector<int>& kept, std::size_t kept_index, bool closed) {
  const std::size_t n = kept.size();
  if (n < 2 || (!closed && kept_index + 1 >= n)) {
    return {0.0, 0.0};
  }
  const std::size_t prev_idx = kept_index == 0 ? n - 1: kept_index - 1;
  const std::size_t next_idx = (kept_index + 1) % n;
  if (!closed && kept_index == 0) {
    return {0.0, 0.0};
  }
  const Point prev = VertexAt(flat, kept[prev_idx]);
  const Point next = VertexAt(flat, kept[next_idx]);
  return {(next.x - prev.x) / 6.0, (next.y - prev.y) / 6.0};
}

std::vector<double> BuildFittedFlat(const std::vector<double>& source,
                                    const std::vector<int>& kept,
                                    bool closed) {
  std::vector<double> out;
  out.reserve(static_cast<std::size_t>(kPathHeaderScalars + kept.size() * kScalarsPerVertex));
  out.push_back(closed ? 1.0: 0.0);
  out.push_back(static_cast<double>(kept.size()));

  for (std::size_t out_index = 0; out_index < kept.size(); ++out_index) {
    const int source_index = kept[out_index];
    const Point v = VertexAt(source, source_index);
    Point in = CatmullIn(source, kept, out_index, closed);
    Point out_tan = CatmullOut(source, kept, out_index, closed);
    if (HasZeroTangentFeature(source, source_index)) {
      in = {0.0, 0.0};
      out_tan = {0.0, 0.0};
    }
    out.push_back(v.x);
    out.push_back(v.y);
    out.push_back(in.x);
    out.push_back(in.y);
    out.push_back(out_tan.x);
    out.push_back(out_tan.y);
  }
  return out;
}

bool StableTopology(const PropertySamples& ps, DecodedShape* first_out) {
  if (ps.samples.empty()) {
    return false;
  }
  const DecodedShape first = DecodeHeader(ps.samples.front().v);
  if (!first.ok) {
    return false;
  }
  for (const Sample& sample: ps.samples) {
    const DecodedShape decoded = DecodeHeader(sample.v);
    if (!decoded.ok || decoded.closed != first.closed || decoded.vertex_count != first.vertex_count) {
      return false;
    }
  }
  if (first_out != nullptr) {
    *first_out = first;
  }
  return true;
}

}  // namespace

PathFitResult FitCanonicalPathProperty(const PropertySamples& ps,
                                       const SolverConfig& cfg) {
  PathFitResult result;
  result.samples = ps;
  result.is_shape_flat = IsShapeFlatPath(ps);
  if (!result.is_shape_flat) {
    result.notes = "not shape_flat";
    return result;
  }

  DecodedShape first;
  result.stable_topology = StableTopology(ps, &first);
  if (!result.stable_topology) {
    result.notes = "path_spatial_fit skipped: topology unstable";
    return result;
  }
  result.closed = first.closed;
  result.source_vertex_count = first.vertex_count;
  result.fitted_vertex_count = first.vertex_count;

  int locked_count = 0;
  double max_outline_error = 0.0;
  result.kept_indices = SelectKeptVertices(
      ps, cfg, first.vertex_count, first.closed, &locked_count, &max_outline_error);
  result.locked_vertex_count = locked_count;
  result.max_outline_error = max_outline_error;
  result.fitted_vertex_count = static_cast<int>(result.kept_indices.size());

  if (result.fitted_vertex_count >= result.source_vertex_count) {
    result.notes = "path_spatial_fit unchanged";
    return result;
  }

  result.samples.property.dimensions = kPathHeaderScalars + result.fitted_vertex_count * kScalarsPerVertex;
  result.samples.property.display_name = ps.property.display_name;
  result.samples.property.units_label = ps.property.units_label;
  result.samples.samples.clear();
  result.samples.samples.reserve(ps.samples.size());
  for (const Sample& sample: ps.samples) {
    Sample fitted;
    fitted.t_sec = sample.t_sec;
    fitted.v = BuildFittedFlat(sample.v, result.kept_indices, first.closed);
    result.samples.samples.push_back(std::move(fitted));
  }

  result.applied = true;
  result.notes = "path_spatial_fit; source_vertices=" + std::to_string(result.source_vertex_count) +
                 "; fitted_vertices=" + std::to_string(result.fitted_vertex_count) +
                 "; locked_vertices=" + std::to_string(result.locked_vertex_count) +
                 "; fit_error=" + std::to_string(result.max_outline_error);
  return result;
}

}  // namespace bbsolver
