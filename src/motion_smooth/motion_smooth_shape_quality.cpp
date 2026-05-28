#include "bbsolver/motion_smooth/motion_smooth_shape_quality.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "bbsolver/motion_smooth/motion_smooth_geometry.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_loop_curve.hpp"
#include "bbsolver/shape/shape_flat_topology.hpp"

namespace bbsolver {

double ShapeFlatControlDistance(const std::vector<double>& a,
                                const std::vector<double>& b,
                                int vertex_count,
                                double* max_control_distance_out) {
  if (vertex_count <= 0) {
    if (max_control_distance_out) { *max_control_distance_out = 0.0; }
    return 0.0;
  }
  const int expected_size = 2 + vertex_count * 6;
  if (static_cast<int>(a.size()) < expected_size ||
      static_cast<int>(b.size()) < expected_size) {
    if (max_control_distance_out) { *max_control_distance_out = 0.0; }
    return 0.0;
  }

  double sum_sq = 0.0;
  double max_control_distance = 0.0;
  for (int i = 0; i < vertex_count; ++i) {
    const int vertex_base = 2 + i * 6;
    for (int channel = 0; channel < 3; ++channel) {
      const int x_idx = vertex_base + channel * 2;
      const int y_idx = x_idx + 1;
      const double dx =
          MotionSmoothComponentOrZero(a, static_cast<std::size_t>(x_idx)) -
          MotionSmoothComponentOrZero(b, static_cast<std::size_t>(x_idx));
      const double dy =
          MotionSmoothComponentOrZero(a, static_cast<std::size_t>(y_idx)) -
          MotionSmoothComponentOrZero(b, static_cast<std::size_t>(y_idx));
      const double control_sq = dx * dx + dy * dy;
      sum_sq += control_sq;
      max_control_distance =
          std::max(max_control_distance, std::sqrt(control_sq));
    }
  }
  if (max_control_distance_out) {
    *max_control_distance_out = max_control_distance;
  }
  return std::sqrt(sum_sq);
}


double ShapeFlatSequenceMaxTurnDeg(
    const std::vector<std::vector<double>>& values,
    int dims) {
  if (values.size() < 3 || dims <= 2) {
    return 0.0;
  }
  double max_angle = 0.0;
  for (std::size_t i = 1; i + 1 < values.size(); ++i) {
    double prev_len_sq = 0.0;
    double next_len_sq = 0.0;
    double dot = 0.0;
    for (int d = 2; d < dims; ++d) {
      const double prev =
          MotionSmoothComponentOrZero(values[i], static_cast<std::size_t>(d)) -
          MotionSmoothComponentOrZero(values[i - 1], static_cast<std::size_t>(d));
      const double next =
          MotionSmoothComponentOrZero(values[i + 1], static_cast<std::size_t>(d)) -
          MotionSmoothComponentOrZero(values[i], static_cast<std::size_t>(d));
      prev_len_sq += prev * prev;
      next_len_sq += next * next;
      dot += prev * next;
    }
    if (prev_len_sq <= 1e-12 || next_len_sq <= 1e-12) {
      continue;
    }
    const double denom = std::sqrt(prev_len_sq * next_len_sq);
    const double cos_angle = std::clamp(dot / denom, -1.0, 1.0);
    max_angle = std::max(
        max_angle,
        std::acos(cos_angle) * 180.0 / 3.14159265358979323846);
  }
  return max_angle;
}

double ShapeFlatClosedDuplicateMaxTurnDeg(
    const std::vector<std::vector<double>>& values,
    int dims) {
  if (values.size() < 4 || dims <= 2) {
    return ShapeFlatSequenceMaxTurnDeg(values, dims);
  }
  const std::size_t unique_count = values.size() - 1;
  double max_angle = 0.0;
  for (std::size_t i = 0; i < unique_count; ++i) {
    const std::vector<double>& prev =
        values[(i + unique_count - 1) % unique_count];
    const std::vector<double>& cur = values[i];
    const std::vector<double>& next = values[(i + 1) % unique_count];
    double prev_len_sq = 0.0;
    double next_len_sq = 0.0;
    double dot = 0.0;
    for (int d = 2; d < dims; ++d) {
      const double prev_delta =
          MotionSmoothComponentOrZero(cur, static_cast<std::size_t>(d)) -
          MotionSmoothComponentOrZero(prev, static_cast<std::size_t>(d));
      const double next_delta =
          MotionSmoothComponentOrZero(next, static_cast<std::size_t>(d)) -
          MotionSmoothComponentOrZero(cur, static_cast<std::size_t>(d));
      prev_len_sq += prev_delta * prev_delta;
      next_len_sq += next_delta * next_delta;
      dot += prev_delta * next_delta;
    }
    if (prev_len_sq <= 1e-12 || next_len_sq <= 1e-12) {
      continue;
    }
    const double denom = std::sqrt(prev_len_sq * next_len_sq);
    const double cos_angle = std::clamp(dot / denom, -1.0, 1.0);
    max_angle = std::max(
        max_angle,
        std::acos(cos_angle) * 180.0 / 3.14159265358979323846);
  }
  return max_angle;
}

double ShapeFlatSequenceExtent(
    const std::vector<std::vector<double>>& values,
    int vertex_count) {
  if (values.empty() || vertex_count <= 0) {
    return 0.0;
  }
  double min_x = std::numeric_limits<double>::infinity();
  double min_y = std::numeric_limits<double>::infinity();
  double max_x = -std::numeric_limits<double>::infinity();
  double max_y = -std::numeric_limits<double>::infinity();
  bool any = false;
  for (const std::vector<double>& value: values) {
    const int expected_size = 2 + vertex_count * 6;
    if (static_cast<int>(value.size()) < expected_size) {
      continue;
    }
    for (int i = 0; i < vertex_count; ++i) {
      const int base = 2 + i * 6;
      const double x = MotionSmoothComponentOrZero(value, static_cast<std::size_t>(base));
      const double y = MotionSmoothComponentOrZero(value, static_cast<std::size_t>(base + 1));
      min_x = std::min(min_x, x);
      min_y = std::min(min_y, y);
      max_x = std::max(max_x, x);
      max_y = std::max(max_y, y);
      any = true;
    }
  }
  if (!any) {
    return 0.0;
  }
  const double dx = max_x - min_x;
  const double dy = max_y - min_y;
  return std::sqrt(dx * dx + dy * dy);
}



//  dedupe: bbsolver::ShapeFlatVertexPoint and bbsolver::PointTurnDeg
// were defined here AND in motion_smooth_shape_loop.cpp's anonymous
// namespace with identical bodies. The canonical definitions now live
// in motion_smooth_shape_loop_curve.hpp/.cpp (public bbsolver surface)
// and this TU consumes them via the new header. PointDistance is
// unique to this file and remains here.
double PointDistance(std::pair<double, double> a,
                     std::pair<double, double> b) {
  const double dx = a.first - b.first;
  const double dy = a.second - b.second;
  return std::sqrt(dx * dx + dy * dy);
}

int EffectiveShapeMotionVertexCount(
    const std::vector<std::vector<double>>& values,
    int vertex_count) {
  if (values.empty() || vertex_count <= 1) {
    return vertex_count;
  }
  const bool closed = MotionSmoothComponentOrZero(values.front(), 0) >= 0.5;
  if (!closed) {
    return vertex_count;
  }
  for (const std::vector<double>& value: values) {
    if (static_cast<int>(value.size()) < 2 + vertex_count * 6) {
      return vertex_count;
    }
    if (PointDistance(ShapeFlatVertexPoint(value, 0),
                      ShapeFlatVertexPoint(value, vertex_count - 1)) > 1e-5) {
      return vertex_count;
    }
  }
  return vertex_count - 1;
}

ShapeMotionQualityMetrics ShapeMotionQuality(
    const std::vector<std::vector<double>>& values,
    int vertex_count,
    const std::vector<double>* times) {
  ShapeMotionQualityMetrics metrics;
  metrics.vertex_count = vertex_count;
  if (values.size() < 2 || vertex_count <= 0) {
    return metrics;
  }
  const int expected_size = 2 + vertex_count * 6;
  for (const std::vector<double>& value: values) {
    if (static_cast<int>(value.size()) < expected_size) {
      return metrics;
    }
  }
  metrics.valid = true;
  metrics.effective_vertex_count =
      EffectiveShapeMotionVertexCount(values, vertex_count);
  std::vector<double> turns;
  double speed_ratio = 0.0;
  const bool closed_loop =
      ShapeFlatControlDistance(values.front(), values.back(), vertex_count) <=
      1e-5;
  for (int vertex = 0; vertex < metrics.effective_vertex_count; ++vertex) {
    std::vector<std::pair<double, double>> pts;
    pts.reserve(values.size());
    for (const std::vector<double>& value: values) {
      pts.push_back(ShapeFlatVertexPoint(value, vertex));
    }
    double travel = 0.0;
    double max_step = 0.0;
    int moving_steps = 0;
    for (std::size_t i = 1; i < pts.size(); ++i) {
      const double step = PointDistance(pts[i - 1], pts[i]);
      if (step <= 1e-9) {
        continue;
      }
      double timed_step = step;
      if (times != nullptr && times->size() == pts.size()) {
        const double dt = (*times)[i] - (*times)[i - 1];
        if (dt <= 1e-9) {
          continue;
        }
        timed_step = step / dt;
      }
      travel += timed_step;
      max_step = std::max(max_step, timed_step);
      ++moving_steps;
    }
    if (moving_steps > 0 && travel > 1e-9) {
      speed_ratio = std::max(
          speed_ratio, max_step / (travel / static_cast<double>(moving_steps)));
    }
    for (std::size_t i = 1; i + 1 < pts.size(); ++i) {
      const double angle = PointTurnDeg(pts[i - 1], pts[i], pts[i + 1]);
      if (angle > 0.0) {
        turns.push_back(angle);
      }
    }
    if (closed_loop && pts.size() > 3) {
      const double boundary =
          PointTurnDeg(pts[pts.size() - 2], pts.front(), pts[1]);
      metrics.boundary_turn_deg =
          std::max(metrics.boundary_turn_deg, boundary);
      if (boundary > 0.0) {
        turns.push_back(boundary);
      }
    }
  }
  metrics.max_speed_ratio = speed_ratio;
  metrics.turn_count = static_cast<int>(turns.size());
  if (!turns.empty()) {
    std::sort(turns.begin(), turns.end());
    metrics.max_turn_deg = turns.back();
    metrics.p95_turn_deg =
        turns[static_cast<std::size_t>(
            std::floor(0.95 * static_cast<double>(turns.size() - 1)))];
    double sum = 0.0;
    for (double angle: turns) {
      sum += angle;
    }
    metrics.avg_turn_deg = sum / static_cast<double>(turns.size());
  }
  return metrics;
}

std::string ShapeMotionQualityNote(const ShapeMotionQualityMetrics& metrics,
                                   const std::string& prefix) {
  if (!metrics.valid) {
    return prefix + "_valid=false";
  }
  return prefix + "_valid=true" +
         "; " + prefix + "_effective_vertices=" +
         std::to_string(metrics.effective_vertex_count) +
         "; " + prefix + "_max_turn_deg=" +
         std::to_string(metrics.max_turn_deg) +
         "; " + prefix + "_p95_turn_deg=" +
         std::to_string(metrics.p95_turn_deg) +
         "; " + prefix + "_boundary_turn_deg=" +
         std::to_string(metrics.boundary_turn_deg) +
         "; " + prefix + "_speed_ratio=" +
         std::to_string(metrics.max_speed_ratio);
}


double ShapeFlatVectorDistanceToLinear(const std::vector<double>& left,
                                       const std::vector<double>& right,
                                       const std::vector<double>& value,
                                       double u) {
  if (left.size() != right.size() || left.size() != value.size() ||
      left.size() < 2) {
    return std::numeric_limits<double>::infinity();
  }
  if (ShapeFlatVertexCount(left) != ShapeFlatVertexCount(right) ||
      ShapeFlatVertexCount(left) != ShapeFlatVertexCount(value) ||
      ShapeFlatClosed(left) != ShapeFlatClosed(right) ||
      ShapeFlatClosed(left) != ShapeFlatClosed(value)) {
    return std::numeric_limits<double>::infinity();
  }
  double max_abs = 0.0;
  for (std::size_t idx = 2; idx < value.size(); ++idx) {
    const double expected = left[idx] + (right[idx] - left[idx]) * u;
    max_abs = std::max(max_abs, std::abs(value[idx] - expected));
  }
  return max_abs;
}


}  // namespace bbsolver
