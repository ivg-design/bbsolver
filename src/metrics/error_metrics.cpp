#include "bbsolver/metrics/error_metrics.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <ratio>
#include <vector>

#include "oneapi/tbb/parallel_for.h"
#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

#ifdef BBSOLVER_HAVE_TBB
#include <tbb/parallel_for.h>
#include <cstddef>
#include <functional>
#include <utility>
#endif

namespace bbsolver {
namespace {

constexpr int kPathHeaderScalars = 2;
constexpr int kScalarsPerVertex = 6;

struct Point {
  double x = 0.0;
  double y = 0.0;
};

struct DecodedShapeFlat {
  bool ok = false;
  bool closed = false;
  int vertex_count = 0;
};

struct ErrorUnitReport {
  double sample_max = 0.0;
  double max_err_screen_px = 0.0;
  double squared_sum = 0.0;
  int component_count = 0;
  int sample_idx = -1;
  int units_evaluated = 0;
  double shape_outline_wall_ms = 0.0;
};

double ComponentOrZero(const std::vector<double>& values, std::size_t idx) {
  return idx < values.size() ? values[idx] : 0.0;
}

bool IsShapeFlatPath(const PropertySamples& ps) {
  return ps.property.kind == ValueKind::Custom && ps.property.units_label == "shape_flat";
}

DecodedShapeFlat DecodeShapeFlat(const std::vector<double>& flat) {
  DecodedShapeFlat decoded;
  if (flat.size() < static_cast<std::size_t>(kPathHeaderScalars)) {
    return decoded;
  }
  decoded.closed = static_cast<int>(std::llround(flat[0])) != 0;
  decoded.vertex_count = static_cast<int>(std::llround(flat[1]));
  const int required = kPathHeaderScalars + decoded.vertex_count * kScalarsPerVertex;
  decoded.ok = decoded.vertex_count > 0 && required <= static_cast<int>(flat.size());
  return decoded;
}

bool IsExactValidShapeFlatMatch(const std::vector<double>& expected,
                                const std::vector<double>& actual) {
  return expected == actual && DecodeShapeFlat(expected).ok;
}

std::size_t VertexOffset(int vertex_index) {
  return static_cast<std::size_t>(kPathHeaderScalars + vertex_index * kScalarsPerVertex);
}

Point FlatPoint(const std::vector<double>& flat, int vertex_index, int offset) {
  const std::size_t base = VertexOffset(vertex_index) + static_cast<std::size_t>(offset);
  return {
      ComponentOrZero(flat, base),
      ComponentOrZero(flat, base + 1),
  };
}

Point Add(Point a, Point b) {
  return {a.x + b.x, a.y + b.y};
}

Point Sub(Point a, Point b) {
  return {a.x - b.x, a.y - b.y};
}

double Distance(Point a, Point b) {
  const Point delta = Sub(a, b);
  return std::sqrt(delta.x * delta.x + delta.y * delta.y);
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

std::vector<Point> ShapeFlatToDensePolyline(const std::vector<double>& flat) {
  std::vector<Point> points;
  const DecodedShapeFlat decoded = DecodeShapeFlat(flat);
  if (!decoded.ok) {
    return points;
  }

  const int seg_count = decoded.closed ? decoded.vertex_count : std::max(0, decoded.vertex_count - 1);
  if (seg_count <= 0) {
    points.push_back(FlatPoint(flat, 0, 0));
    return points;
  }

  for (int i = 0; i < seg_count; ++i) {
    const int next = (i + 1) % decoded.vertex_count;
    const Point p0 = FlatPoint(flat, i, 0);
    const Point p3 = FlatPoint(flat, next, 0);
    const Point p1 = Add(p0, FlatPoint(flat, i, 4));
    const Point p2 = Add(p3, FlatPoint(flat, next, 2));
    const double control_len = Distance(p0, p1) + Distance(p1, p2) + Distance(p2, p3);
    const double chord_len = Distance(p0, p3);
    const int divs = static_cast<int>(std::ceil(
        std::max(6.0, std::min(32.0, (control_len + std::abs(control_len - chord_len) * 2.0) / 10.0))));
    if (i == 0) {
      PushDensePoint(points, p0);
    }
    for (int j = 1; j <= divs; ++j) {
      if (decoded.closed && i == seg_count - 1 && j == divs) {
        continue;
      }
      PushDensePoint(points, Cubic(p0, p1, p2, p3, static_cast<double>(j) / divs));
    }
  }
  return points;
}

double PointSegmentDistance(Point p, Point a, Point b) {
  const Point ab = Sub(b, a);
  const double denom = ab.x * ab.x + ab.y * ab.y;
  if (!(denom > 1e-18)) {
    return Distance(p, a);
  }
  const double u = std::clamp(((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / denom, 0.0, 1.0);
  return Distance(p, {a.x + ab.x * u, a.y + ab.y * u});
}

double DirectedPolylineDistance(const std::vector<Point>& a_points,
                                const std::vector<Point>& b_points,
                                bool closed) {
  if (a_points.empty() || b_points.empty()) {
    return std::numeric_limits<double>::infinity();
  }
  const int seg_count = closed ? static_cast<int>(b_points.size())
                               : std::max(0, static_cast<int>(b_points.size()) - 1);
  if (seg_count <= 0) {
    return Distance(a_points.front(), b_points.front());
  }

  double max_err = 0.0;
  for (Point p : a_points) {
    double best = std::numeric_limits<double>::infinity();
    for (int i = 0; i < seg_count; ++i) {
      best = std::min(best, PointSegmentDistance(p, b_points[static_cast<std::size_t>(i)],
                                                 b_points[static_cast<std::size_t>((i + 1) % b_points.size())]));
    }
    max_err = std::max(max_err, best);
  }
  return max_err;
}

double ShapeFlatOutlineError(const std::vector<double>& expected,
                             const std::vector<double>& actual,
                             double cutoff_error) {
  return ShapeFlatFrameOutlineError(expected, actual, {}, cutoff_error);
}

std::vector<double> ExpectedValueAt(const Sample& sample, int dimensions, int sample_idx, int samples_per_frame) {
  std::vector<double> expected(static_cast<std::size_t>(dimensions), 0.0);
  const std::size_t base = static_cast<std::size_t>(std::max(sample_idx, 0) * dimensions);
  for (int d = 0; d < dimensions; ++d) {
    expected[static_cast<std::size_t>(d)] = ComponentOrZero(sample.v, base + static_cast<std::size_t>(d));
  }
  if (sample.v.size() < static_cast<std::size_t>(samples_per_frame * dimensions)) {
    for (int d = 0; d < dimensions; ++d) {
      expected[static_cast<std::size_t>(d)] = ComponentOrZero(sample.v, static_cast<std::size_t>(d));
    }
  }
  return expected;
}

double SubSampleTime(const Sample& sample,
                     int sub_idx,
                     int samples_per_frame,
                     const CompInfo& comp) {
  if (samples_per_frame <= 1 || comp.fps <= 0.0 || comp.shutter_angle_deg == 0.0) {
    return sample.t_sec;
  }

  const double frame_dur = 1.0 / comp.fps;
  const double shutter_ratio = comp.shutter_angle_deg / 360.0;
  const double shutter_open =
      sample.t_sec - shutter_ratio * ((comp.shutter_phase_deg / 360.0) + 0.5) * frame_dur;
  const double shutter_span = frame_dur * shutter_ratio;
  return shutter_open +
         (static_cast<double>(sub_idx) + 0.5) * shutter_span / static_cast<double>(samples_per_frame);
}

std::pair<double, double> ProjectToScreen(const std::vector<double>& value,
                                          const LayerXform& layer_xform) {
  // Round-3 screen metric model: an orthographic layer-space approximation.
  // We apply start-of-clip anchor, position, and XY scale only. Rotation,
  // camera perspective, pixel aspect, and animated layer transforms remain AE
  // verifier territory for v1.
  const double anchor_x = ComponentOrZero(layer_xform.anchor_point, 0);
  const double anchor_y = ComponentOrZero(layer_xform.anchor_point, 1);
  const double pos_x = ComponentOrZero(layer_xform.position, 0);
  const double pos_y = ComponentOrZero(layer_xform.position, 1);
  const double scale_x = layer_xform.scale.empty() ? 1.0 : ComponentOrZero(layer_xform.scale, 0) / 100.0;
  const double scale_y = layer_xform.scale.size() < 2 ? scale_x : ComponentOrZero(layer_xform.scale, 1) / 100.0;
  return {
      pos_x + (ComponentOrZero(value, 0) - anchor_x) * scale_x,
      pos_y + (ComponentOrZero(value, 1) - anchor_y) * scale_y,
  };
}

bool ShouldParallelComputeShapeError(const SolverConfig& cfg,
                                     bool is_shape_flat,
                                     int total_units) {
#ifdef BBSOLVER_HAVE_TBB
  return is_shape_flat && cfg.parallel_jobs > 1 && total_units >= 96;
#else
  (void)cfg;
  (void)is_shape_flat;
  (void)total_units;
  return false;
#endif
}

double ElapsedMs(std::chrono::steady_clock::time_point start,
                 std::chrono::steady_clock::time_point end) {
  return std::chrono::duration<double, std::milli>(end - start).count();
}

ErrorUnitReport ComputeErrorUnit(
    const PropertySamples& ps,
    int sample_idx,
    int sub_idx,
    int dimensions,
    int samples_per_frame,
    bool is_shape_flat,
    bool compute_screen,
    const std::function<std::vector<double>(double t)>& reconstruct,
    const CompInfo& comp,
    const LayerXform* layer_xform_opt,
    double shape_outline_cutoff,
    bool collect_attribution) {
  const Sample& sample = ps.samples[static_cast<std::size_t>(sample_idx)];
  const double t = SubSampleTime(sample, sub_idx, samples_per_frame, comp);
  const std::vector<double> expected =
      ExpectedValueAt(sample, dimensions, sub_idx, samples_per_frame);
  const std::vector<double> actual = reconstruct(t);

  ErrorUnitReport unit;
  unit.sample_idx = sample_idx;
  unit.units_evaluated = collect_attribution ? 1 : 0;
  if (is_shape_flat) {
    if (IsExactValidShapeFlatMatch(expected, actual)) {
      unit.sample_max = 0.0;
    } else if (collect_attribution) {
      const auto outline_start = std::chrono::steady_clock::now();
      unit.sample_max = ShapeFlatOutlineError(expected, actual, shape_outline_cutoff);
      unit.shape_outline_wall_ms =
          ElapsedMs(outline_start, std::chrono::steady_clock::now());
    } else {
      unit.sample_max = ShapeFlatOutlineError(expected, actual, shape_outline_cutoff);
    }
    unit.squared_sum = unit.sample_max * unit.sample_max;
    unit.component_count = 1;
    unit.max_err_screen_px = unit.sample_max;
    return unit;
  }

  for (int d = 0; d < dimensions; ++d) {
    const double err = std::abs(ComponentOrZero(actual, static_cast<std::size_t>(d)) -
                                expected[static_cast<std::size_t>(d)]);
    unit.sample_max = std::max(unit.sample_max, err);
    unit.squared_sum += err * err;
    ++unit.component_count;
  }

  if (compute_screen && layer_xform_opt != nullptr) {
    const auto expected_screen = ProjectToScreen(expected, *layer_xform_opt);
    const auto actual_screen = ProjectToScreen(actual, *layer_xform_opt);
    const double dx = actual_screen.first - expected_screen.first;
    const double dy = actual_screen.second - expected_screen.second;
    unit.max_err_screen_px = std::sqrt(dx * dx + dy * dy);
  }
  return unit;
}

void AccumulateErrorUnit(ErrorReport& report,
                         double& squared_sum,
                         int& component_count,
                         const ErrorUnitReport& unit) {
  if (unit.sample_max > report.max_err) {
    report.max_err = unit.sample_max;
    report.worst_sample_idx = unit.sample_idx;
  }
  report.max_err_screen_px =
      std::max(report.max_err_screen_px, unit.max_err_screen_px);
  squared_sum += unit.squared_sum;
  component_count += unit.component_count;
  report.units_evaluated += unit.units_evaluated;
  report.shape_outline_wall_ms += unit.shape_outline_wall_ms;
}

}  // namespace

ErrorReport ComputeError(
    const PropertySamples& ps,
    int i,
    int j,
    const std::function<std::vector<double>(double t)>& reconstruct,
    const SolverConfig& cfg,
    const CompInfo& comp,
    const LayerXform* layer_xform_opt,
    double fail_fast_property_ceiling,
    double fail_fast_screen_ceiling,
    bool collect_attribution) {
  ErrorReport report;
  if (ps.samples.empty()) {
    return report;
  }

  const int begin = std::clamp(i, 0, static_cast<int>(ps.samples.size()) - 1);
  const int end = std::clamp(j, begin, static_cast<int>(ps.samples.size()) - 1);
  const int dimensions = std::max(ps.property.dimensions, 1);
  const int samples_per_frame = std::max(ps.samples_per_frame, 1);
  const bool is_shape_flat = IsShapeFlatPath(ps);
  const bool compute_screen =
      !is_shape_flat &&
      layer_xform_opt != nullptr &&
      dimensions >= 2 &&
      (cfg.tolerance_screen_px > 0.0 || cfg.weight_screen > 0.0);
  double squared_sum = 0.0;
  int component_count = 0;
  const int sample_count = end - begin + 1;
  const int total_units = sample_count * samples_per_frame;
  const bool allow_fail_fast =
      std::isfinite(fail_fast_property_ceiling) ||
      std::isfinite(fail_fast_screen_ceiling);
  double shape_outline_cutoff = std::numeric_limits<double>::infinity();
  if (is_shape_flat && allow_fail_fast) {
    shape_outline_cutoff =
        std::min(fail_fast_property_ceiling, fail_fast_screen_ceiling);
  }

  if (!allow_fail_fast && ShouldParallelComputeShapeError(cfg, is_shape_flat, total_units)) {
    std::vector<ErrorUnitReport> units(static_cast<std::size_t>(total_units));
#ifdef BBSOLVER_HAVE_TBB
    tbb::parallel_for(std::size_t{0},
                      units.size(),
                      [&](std::size_t unit_idx) {
                        const int sample_offset =
                            static_cast<int>(unit_idx) / samples_per_frame;
                        const int sub_idx =
                            static_cast<int>(unit_idx) % samples_per_frame;
                        units[unit_idx] = ComputeErrorUnit(
                            ps,
                            begin + sample_offset,
                            sub_idx,
                            dimensions,
                            samples_per_frame,
                            is_shape_flat,
                            compute_screen,
                            reconstruct,
                            comp,
                            layer_xform_opt,
                            shape_outline_cutoff,
                            collect_attribution);
                      });
#endif
    for (const ErrorUnitReport& unit : units) {
      AccumulateErrorUnit(report, squared_sum, component_count, unit);
    }
    report.rms_err = component_count > 0
                         ? std::sqrt(squared_sum /
                                     static_cast<double>(component_count))
                         : 0.0;
    return report;
  }

  for (int sample_idx = begin; sample_idx <= end; ++sample_idx) {
    for (int sub_idx = 0; sub_idx < samples_per_frame; ++sub_idx) {
      AccumulateErrorUnit(
          report,
          squared_sum,
          component_count,
          ComputeErrorUnit(ps,
                           sample_idx,
                           sub_idx,
                           dimensions,
                           samples_per_frame,
                           is_shape_flat,
                           compute_screen,
                           reconstruct,
                           comp,
                           layer_xform_opt,
                           shape_outline_cutoff,
                           collect_attribution));
      if (allow_fail_fast &&
          (report.max_err > fail_fast_property_ceiling ||
           report.max_err_screen_px > fail_fast_screen_ceiling)) {
        report.rms_err = component_count > 0
                             ? std::sqrt(squared_sum /
                                         static_cast<double>(component_count))
                             : 0.0;
        report.fail_fast_exit = true;
        return report;
      }
    }
  }

  report.rms_err = component_count > 0 ? std::sqrt(squared_sum / static_cast<double>(component_count)) : 0.0;
  return report;
}

}  // namespace bbsolver
