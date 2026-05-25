// Per-segment cubic Bezier fitter implementations declared in
// path_frame_fit_cubic_span.hpp. Behavior is stable with the previous
// anonymous-namespace and pff_cubic_span definitions: same clamping, same parameter-set fallback
// order (arc-length, uniform, centripetal), same `det <= 1e-12`
// degeneracy guards, same `1e-9` improvement tolerance for accepting the
// unconstrained-then-clamped variant.
//
// Diagnostics decision: **none / pure layout**. Pure geometric fitting.
// Failure is surfaced via `CubicSpanFit::max_error == infinity` (see
// `InvalidCubicSpanFit`). No DiagnosticsWriter, no progress events, no
// cancellation, no operator state. Diagnostics ownership: caller-owned —
// `BuildSegmentFitFlat` and the decimation pipeline interpret the fit
// quality through their own acceptance gates.

#include "bbsolver/path/frame_fit/path_frame_fit_cubic_span.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include "bbsolver/path/dense/path_dense_polyline.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit_geometry.hpp"

namespace bbsolver {
namespace pff_cubic_span {
namespace {

using pff_geom::Add;
using pff_geom::Cubic;
using pff_geom::Distance;
using pff_geom::Length;
using pff_geom::Mul;
using pff_geom::Point;
using pff_geom::PointSegmentDistance;
using pff_geom::Sub;

// Private TU-local copies of the small geometric helpers (DenseAtOffset,
// SegmentArcLength, ForwardDenseSpan, IsFinitePoint, IsSharpDenseIndex,
// NormalizeOr). The corresponding anonymous-namespace originals stay in
// path_frame_fit.cpp because they are also used by BuildFlat /
// BuildSegmentFitFlat. Duplication keeps both TUs self-contained without
// promoting generic helpers into a "utils" header.

bool IsFinitePoint(Point p) {
  return std::isfinite(p.x) && std::isfinite(p.y);
}

int ForwardDenseSpan(int begin, int end, int dense_count, bool closed) {
  if (begin < 0 || end < 0 || dense_count <= 0) {
    return 0;
  }
  if (end >= begin) {
    return end - begin;
  }
  return closed ? dense_count - begin + end : 0;
}

Point DenseAtOffset(const std::vector<DensePoint>& dense, int begin, int offset) {
  const int n = static_cast<int>(dense.size());
  if (n <= 0) {
    return {0.0, 0.0};
  }
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

bool IsSharpDenseIndex(const std::vector<DensePoint>& dense,
                       int dense_index,
                       const std::vector<bool>& sharp_source_vertices) {
  if (dense_index < 0 || dense_index >= static_cast<int>(dense.size())) {
    return false;
  }
  const int source_index = dense[static_cast<std::size_t>(dense_index)].source_vertex_index;
  return source_index >= 0 &&
         source_index < static_cast<int>(sharp_source_vertices.size()) &&
         sharp_source_vertices[static_cast<std::size_t>(source_index)];
}

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

CubicSpanFit ScoreDenseSpanCubic(const std::vector<DensePoint>& dense,
                                 int begin,
                                 int span,
                                 Point p0,
                                 Point p1,
                                 Point p2,
                                 Point p3) {
  CubicSpanFit fit;
  fit.p1 = p1;
  fit.p2 = p2;
  if (span <= 0) {
    return fit;
  }
  if (!IsFinitePoint(p1) || !IsFinitePoint(p2)) {
    fit.max_error = std::numeric_limits<double>::infinity();
    return fit;
  }

  const int cubic_steps = std::clamp(span * 2, 16, 96);
  std::vector<Point> cubic_points;
  cubic_points.reserve(static_cast<std::size_t>(cubic_steps + 1));
  for (int i = 0; i <= cubic_steps; ++i) {
    cubic_points.push_back(Cubic(p0, p1, p2, p3,
                                 static_cast<double>(i) / static_cast<double>(cubic_steps)));
  }

  const int dense_count = static_cast<int>(dense.size());
  for (int step = 1; step < span; ++step) {
    const Point source = DenseAtOffset(dense, begin, step);
    double best = std::numeric_limits<double>::infinity();
    for (int i = 0; i < cubic_steps; ++i) {
      best = std::min(best, PointSegmentDistance(
                                source,
                                cubic_points[static_cast<std::size_t>(i)],
                                cubic_points[static_cast<std::size_t>(i + 1)]));
    }
    if (best > fit.max_error) {
      fit.max_error = best;
      fit.worst_dense_index = (begin + step) % dense_count;
    }
  }

  if (span > 1) {
    std::vector<Point> source_points;
    source_points.reserve(static_cast<std::size_t>(span + 1));
    for (int step = 0; step <= span; ++step) {
      source_points.push_back(DenseAtOffset(dense, begin, step));
    }
    for (int i = 1; i < cubic_steps; ++i) {
      const Point candidate = cubic_points[static_cast<std::size_t>(i)];
      double best = std::numeric_limits<double>::infinity();
      for (int step = 0; step < span; ++step) {
        best = std::min(best, PointSegmentDistance(
                                  candidate,
                                  source_points[static_cast<std::size_t>(step)],
                                  source_points[static_cast<std::size_t>(step + 1)]));
      }
      if (best > fit.max_error) {
        fit.max_error = best;
        const int split_step = std::clamp(
            static_cast<int>(std::llround(
                static_cast<double>(i) * static_cast<double>(span) /
                static_cast<double>(cubic_steps))),
            1,
            span - 1);
        fit.worst_dense_index = (begin + split_step) % dense_count;
      }
    }
  }
  return fit;
}

std::vector<double> InitialCubicSpanParameters(int span,
                                               double arc_len,
                                               const std::vector<double>& cumulative) {
  std::vector<double> params;
  params.reserve(static_cast<std::size_t>(std::max(0, span - 1)));
  for (int step = 1; step < span; ++step) {
    params.push_back(arc_len > 1e-9
                         ? cumulative[static_cast<std::size_t>(step)] / arc_len
                         : static_cast<double>(step) / static_cast<double>(span));
  }
  return params;
}

std::vector<double> UniformCubicSpanParameters(int span) {
  std::vector<double> params;
  params.reserve(static_cast<std::size_t>(std::max(0, span - 1)));
  for (int step = 1; step < span; ++step) {
    params.push_back(static_cast<double>(step) / static_cast<double>(span));
  }
  return params;
}

std::vector<double> CentripetalCubicSpanParameters(const std::vector<DensePoint>& dense,
                                                   int begin,
                                                   int span) {
  std::vector<double> cumulative(static_cast<std::size_t>(span + 1), 0.0);
  double total = 0.0;
  for (int step = 1; step <= span; ++step) {
    total += std::sqrt(std::max(
        0.0,
        Distance(DenseAtOffset(dense, begin, step - 1),
                 DenseAtOffset(dense, begin, step))));
    cumulative[static_cast<std::size_t>(step)] = total;
  }
  std::vector<double> params;
  params.reserve(static_cast<std::size_t>(std::max(0, span - 1)));
  for (int step = 1; step < span; ++step) {
    params.push_back(total > 1e-9
                         ? cumulative[static_cast<std::size_t>(step)] / total
                         : static_cast<double>(step) / static_cast<double>(span));
  }
  return params;
}

bool SolveUnconstrainedCubicControls(
    const std::vector<DensePoint>& dense,
    int begin,
    int span,
    Point p0,
    Point p3,
    bool force_start,
    bool force_end,
    const std::vector<double>& params,
    Point* out_p1,
    Point* out_p2) {
  if (out_p1 == nullptr || out_p2 == nullptr || span <= 0 ||
      static_cast<int>(params.size()) != span - 1) {
    return false;
  }
  *out_p1 = p0;
  *out_p2 = p3;
  if (force_start && force_end) {
    return true;
  }

  double a00 = 0.0;
  double a01 = 0.0;
  double a11 = 0.0;
  Point rhs0{0.0, 0.0};
  Point rhs1{0.0, 0.0};

  for (int step = 1; step < span; ++step) {
    const double u = std::clamp(params[static_cast<std::size_t>(step - 1)], 0.0, 1.0);
    const double omt = 1.0 - u;
    const double b0 = omt * omt * omt;
    const double b1 = 3.0 * omt * omt * u;
    const double b2 = 3.0 * omt * u * u;
    const double b3 = u * u * u;

    Point base = Add(Mul(p0, b0), Mul(p3, b3));
    if (force_start) {
      base = Add(base, Mul(p0, b1));
    }
    if (force_end) {
      base = Add(base, Mul(p3, b2));
    }
    const Point residual = Sub(DenseAtOffset(dense, begin, step), base);

    if (!force_start) {
      a00 += b1 * b1;
      rhs0 = Add(rhs0, Mul(residual, b1));
    }
    if (!force_start && !force_end) {
      a01 += b1 * b2;
    }
    if (!force_end) {
      a11 += b2 * b2;
      rhs1 = Add(rhs1, Mul(residual, b2));
    }
  }

  if (!force_start && !force_end) {
    const double det = a00 * a11 - a01 * a01;
    if (std::abs(det) <= 1e-12) {
      return false;
    }
    *out_p1 = {
        (rhs0.x * a11 - rhs1.x * a01) / det,
        (rhs0.y * a11 - rhs1.y * a01) / det,
    };
    *out_p2 = {
        (a00 * rhs1.x - a01 * rhs0.x) / det,
        (a00 * rhs1.y - a01 * rhs0.y) / det,
    };
  } else if (!force_start) {
    if (a00 <= 1e-12) {
      return false;
    }
    *out_p1 = Mul(rhs0, 1.0 / a00);
  } else if (!force_end) {
    if (a11 <= 1e-12) {
      return false;
    }
    *out_p2 = Mul(rhs1, 1.0 / a11);
  }

  return IsFinitePoint(*out_p1) && IsFinitePoint(*out_p2);
}

CubicSpanFit InvalidCubicSpanFit() {
  CubicSpanFit invalid;
  invalid.p1 = {std::numeric_limits<double>::infinity(), 0.0};
  invalid.p2 = {std::numeric_limits<double>::infinity(), 0.0};
  invalid.max_error = std::numeric_limits<double>::infinity();
  return invalid;
}

CubicSpanFit SolveUnconstrainedDenseSpanCubic(
    const std::vector<DensePoint>& dense,
    int begin,
    int span,
    Point p0,
    Point p3,
    bool force_start,
    bool force_end,
    double arc_len,
    const std::vector<double>& cumulative) {
  CubicSpanFit best = InvalidCubicSpanFit();

  std::vector<std::vector<double>> parameter_sets;
  parameter_sets.push_back(InitialCubicSpanParameters(span, arc_len, cumulative));
  parameter_sets.push_back(UniformCubicSpanParameters(span));
  parameter_sets.push_back(CentripetalCubicSpanParameters(dense, begin, span));

  for (const std::vector<double>& params : parameter_sets) {
    Point p1 = p0;
    Point p2 = p3;
    if (!SolveUnconstrainedCubicControls(
            dense, begin, span, p0, p3, force_start, force_end, params, &p1, &p2)) {
      continue;
    }
    CubicSpanFit fit = ScoreDenseSpanCubic(dense, begin, span, p0, p1, p2, p3);
    if (fit.max_error < best.max_error) {
      best = fit;
    }
  }

  return std::isfinite(best.max_error) ? best : InvalidCubicSpanFit();
}

}  // namespace

CubicSpanFit FitDenseSpanCubic(const std::vector<DensePoint>& dense,
                               int begin,
                               int end,
                               const std::vector<bool>& sharp_source_vertices,
                               bool closed) {
  CubicSpanFit fit;
  const int span = ForwardDenseSpan(begin, end, static_cast<int>(dense.size()), closed);
  if (span <= 0) {
    return fit;
  }

  const pff_geom::Point p0 = dense[static_cast<std::size_t>(begin)].p;
  const pff_geom::Point p3 = dense[static_cast<std::size_t>(end)].p;
  const pff_geom::Point chord = pff_geom::Sub(p3, p0);
  const double chord_len = pff_geom::Length(chord);
  if (!(chord_len > 1e-9)) {
    fit.p1 = p0;
    fit.p2 = p3;
    return fit;
  }

  const bool force_start = IsSharpDenseIndex(dense, begin, sharp_source_vertices);
  const bool force_end = IsSharpDenseIndex(dense, end, sharp_source_vertices);
  const pff_geom::Point chord_dir = NormalizeOr(chord, {1.0, 0.0});
  const pff_geom::Point start_dir = force_start
      ? pff_geom::Point{0.0, 0.0}
      : NormalizeOr(pff_geom::Sub(DenseAtOffset(dense, begin, 1), p0), chord_dir);
  const pff_geom::Point end_dir = force_end
      ? pff_geom::Point{0.0, 0.0}
      : NormalizeOr(pff_geom::Sub(p3, DenseAtOffset(dense, begin, span - 1)), chord_dir);

  std::vector<double> cumulative;
  const double arc_len = SegmentArcLength(dense, begin, span, &cumulative);
  double a00 = 0.0;
  double a01 = 0.0;
  double a11 = 0.0;
  double b0 = 0.0;
  double b1 = 0.0;
  for (int step = 1; step < span; ++step) {
    const double u = arc_len > 1e-9
        ? cumulative[static_cast<std::size_t>(step)] / arc_len
        : static_cast<double>(step) / static_cast<double>(span);
    const double omt = 1.0 - u;
    const double b_0 = omt * omt * omt;
    const double b_1 = 3.0 * omt * omt * u;
    const double b_2 = 3.0 * omt * u * u;
    const double b_3 = u * u * u;
    const pff_geom::Point base =
        pff_geom::Add(pff_geom::Mul(p0, b_0 + b_1), pff_geom::Mul(p3, b_2 + b_3));
    const pff_geom::Point residual = pff_geom::Sub(DenseAtOffset(dense, begin, step), base);
    const pff_geom::Point c0 = force_start ? pff_geom::Point{0.0, 0.0} : pff_geom::Mul(start_dir, b_1);
    const pff_geom::Point c1 = force_end ? pff_geom::Point{0.0, 0.0} : pff_geom::Mul(end_dir, -b_2);
    a00 += pff_geom::Dot(c0, c0);
    a01 += pff_geom::Dot(c0, c1);
    a11 += pff_geom::Dot(c1, c1);
    b0 += pff_geom::Dot(c0, residual);
    b1 += pff_geom::Dot(c1, residual);
  }

  const double fallback = chord_len / 3.0;
  const double max_handle = std::max(chord_len, arc_len) * 1.5;
  double h0 = force_start ? 0.0 : fallback;
  double h1 = force_end ? 0.0 : fallback;
  if (!force_start && !force_end) {
    const double det = a00 * a11 - a01 * a01;
    if (std::abs(det) > 1e-12) {
      h0 = (b0 * a11 - b1 * a01) / det;
      h1 = (a00 * b1 - a01 * b0) / det;
    }
  } else if (!force_start && a00 > 1e-12) {
    h0 = b0 / a00;
  } else if (!force_end && a11 > 1e-12) {
    h1 = b1 / a11;
  }
  if (!std::isfinite(h0)) {
    h0 = fallback;
  }
  if (!std::isfinite(h1)) {
    h1 = fallback;
  }
  h0 = std::clamp(h0, 0.0, max_handle);
  h1 = std::clamp(h1, 0.0, max_handle);
  fit = ScoreDenseSpanCubic(
      dense, begin, span, p0,
      pff_geom::Add(p0, pff_geom::Mul(start_dir, h0)),
      pff_geom::Sub(p3, pff_geom::Mul(end_dir, h1)),
      p3);

  const CubicSpanFit unconstrained = SolveUnconstrainedDenseSpanCubic(
      dense, begin, span, p0, p3, force_start, force_end, arc_len, cumulative);
  if (unconstrained.max_error <= fit.max_error + 1e-9) {
    fit = unconstrained;
  } else if (std::isfinite(unconstrained.max_error)) {
    const pff_geom::Point clamped_p1 = force_start
        ? p0
        : pff_geom::Add(p0, pff_geom::ClampLength(pff_geom::Sub(unconstrained.p1, p0), max_handle));
    const pff_geom::Point clamped_p2 = force_end
        ? p3
        : pff_geom::Add(p3, pff_geom::ClampLength(pff_geom::Sub(unconstrained.p2, p3), max_handle));
    const CubicSpanFit clamped =
        ScoreDenseSpanCubic(dense, begin, span, p0, clamped_p1, clamped_p2, p3);
    if (clamped.max_error <= fit.max_error + 1e-9) {
      fit = clamped;
    }
  }
  return fit;
}

}  // namespace pff_cubic_span
}  // namespace bbsolver
