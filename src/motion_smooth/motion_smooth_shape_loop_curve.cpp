#include "bbsolver/motion_smooth/motion_smooth_shape_loop_curve.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include "bbsolver/motion_smooth/motion_smooth_geometry.hpp"

namespace bbsolver {

std::pair<double, double> ShapeFlatVertexPoint(
    const std::vector<double>& value,
    int vertex_index) {
  const int base = 2 + vertex_index * 6;
  return {
      MotionSmoothComponentOrZero(value, static_cast<std::size_t>(base)),
      MotionSmoothComponentOrZero(value, static_cast<std::size_t>(base + 1))};
}

double PointTurnDeg(std::pair<double, double> prev,
                    std::pair<double, double> cur,
                    std::pair<double, double> next) {
  const double ux = cur.first - prev.first;
  const double uy = cur.second - prev.second;
  const double vx = next.first - cur.first;
  const double vy = next.second - cur.second;
  const double u_len_sq = ux * ux + uy * uy;
  const double v_len_sq = vx * vx + vy * vy;
  if (u_len_sq <= 1e-12 || v_len_sq <= 1e-12) {
    return 0.0;
  }
  const double denom = std::sqrt(u_len_sq * v_len_sq);
  const double cos_angle =
      std::clamp((ux * vx + uy * vy) / denom, -1.0, 1.0);
  return std::acos(cos_angle) * 180.0 / 3.14159265358979323846;
}

double MotionSmoothCatmullRomValue(double p0,
                                   double p1,
                                   double p2,
                                   double p3,
                                   double u) {
  const double u2 = u * u;
  const double u3 = u2 * u;
  return 0.5 *
         (2.0 * p1 +
          (-p0 + p2) * u +
          (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * u2 +
          (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * u3);
}

std::vector<double> EvaluateClosedLoopShapeAtParam(
    const std::vector<std::vector<double>>& closed_values,
    int dims,
    double param) {
  const std::size_t unique_count = closed_values.size() - 1;
  if (unique_count < 3) {
    return closed_values.empty() ? std::vector<double>(): closed_values.front();
  }
  if (param >= static_cast<double>(unique_count) - 1e-12) {
    return closed_values.front();
  }
  const double wrapped =
      param < 0.0
          ? std::fmod(std::fmod(param, static_cast<double>(unique_count)) +
                          static_cast<double>(unique_count),
                      static_cast<double>(unique_count))
: std::fmod(param, static_cast<double>(unique_count));
  const int segment = std::clamp(
      static_cast<int>(std::floor(wrapped)),
      0,
      static_cast<int>(unique_count) - 1);
  const double u = wrapped - static_cast<double>(segment);
  const std::vector<double>& p0 =
      closed_values[(segment + unique_count - 1) % unique_count];
  const std::vector<double>& p1 = closed_values[static_cast<std::size_t>(segment)];
  const std::vector<double>& p2 =
      closed_values[(segment + 1) % unique_count];
  const std::vector<double>& p3 =
      closed_values[(segment + 2) % unique_count];
  std::vector<double> value = p1;
  if (static_cast<int>(value.size()) < dims) {
    value.resize(static_cast<std::size_t>(dims), 0.0);
  }
  for (int d = 2; d < dims; ++d) {
    value[static_cast<std::size_t>(d)] = MotionSmoothCatmullRomValue(
        MotionSmoothComponentOrZero(p0, static_cast<std::size_t>(d)),
        MotionSmoothComponentOrZero(p1, static_cast<std::size_t>(d)),
        MotionSmoothComponentOrZero(p2, static_cast<std::size_t>(d)),
        MotionSmoothComponentOrZero(p3, static_cast<std::size_t>(d)),
        u);
  }
  return value;
}

}  // namespace bbsolver
