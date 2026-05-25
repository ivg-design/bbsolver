#include "bbsolver/motion_smooth/motion_smooth_geometry.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "bbsolver/samples/sample_value_helpers.hpp"

namespace bbsolver {
namespace {

double MotionPointSegmentDistance(const std::vector<double>& p,
                                  const std::vector<double>& a,
                                  const std::vector<double>& b,
                                  int dims) {
  const double ab_len_sq = MotionPointDistanceSq(a, b, dims);
  if (ab_len_sq <= 1e-12) {
    return MotionPointDistance(p, a, dims);
  }
  double dot = 0.0;
  for (int d = 0; d < dims; ++d) {
    const double pd =
        MotionSmoothComponentOrZero(p, static_cast<std::size_t>(d)) -
        MotionSmoothComponentOrZero(a, static_cast<std::size_t>(d));
    const double bd =
        MotionSmoothComponentOrZero(b, static_cast<std::size_t>(d)) -
        MotionSmoothComponentOrZero(a, static_cast<std::size_t>(d));
    dot += pd * bd;
  }
  const double t = std::clamp(dot / ab_len_sq, 0.0, 1.0);
  std::vector<double> projected(static_cast<std::size_t>(dims), 0.0);
  for (int d = 0; d < dims; ++d) {
    projected[static_cast<std::size_t>(d)] =
        MotionSmoothComponentOrZero(a, static_cast<std::size_t>(d)) +
        (MotionSmoothComponentOrZero(b, static_cast<std::size_t>(d)) -
         MotionSmoothComponentOrZero(a, static_cast<std::size_t>(d))) * t;
  }
  return MotionPointDistance(p, projected, dims);
}

void MotionSmoothRdpKeep(const std::vector<std::vector<double>>& points,
                         int first,
                         int last,
                         double tolerance,
                         int dims,
                         std::vector<bool>* keep) {
  if (!keep || last <= first + 1) {
    return;
  }
  double best_dist = -1.0;
  int best_idx = -1;
  for (int i = first + 1; i < last; ++i) {
    const double dist = MotionPointSegmentDistance(
        points[static_cast<std::size_t>(i)],
        points[static_cast<std::size_t>(first)],
        points[static_cast<std::size_t>(last)],
        dims);
    if (dist > best_dist) {
      best_dist = dist;
      best_idx = i;
    }
  }
  if (best_idx >= 0 && best_dist > tolerance) {
    (*keep)[static_cast<std::size_t>(best_idx)] = true;
    MotionSmoothRdpKeep(points, first, best_idx, tolerance, dims, keep);
    MotionSmoothRdpKeep(points, best_idx, last, tolerance, dims, keep);
  }
}

}  // namespace

double MotionSmoothComponentOrZero(const std::vector<double>& values,
                                   std::size_t idx) {
  return idx < values.size() ? values[idx] : 0.0;
}

double MotionPointDistanceSq(const std::vector<double>& a,
                             const std::vector<double>& b,
                             int dims) {
  double sum = 0.0;
  for (int d = 0; d < dims; ++d) {
    const double delta =
        MotionSmoothComponentOrZero(a, static_cast<std::size_t>(d)) -
        MotionSmoothComponentOrZero(b, static_cast<std::size_t>(d));
    sum += delta * delta;
  }
  return sum;
}

double MotionPointDistance(const std::vector<double>& a,
                           const std::vector<double>& b,
                           int dims) {
  return std::sqrt(MotionPointDistanceSq(a, b, dims));
}

std::vector<std::vector<double>> MotionSmoothFilteredPoints(
    const PropertySamples& property_samples,
    double strength,
    int dims,
    int* passes_out,
    double* max_displacement_out) {
  std::vector<std::vector<double>> points;
  points.reserve(property_samples.samples.size());
  for (const Sample& sample : property_samples.samples) {
    points.push_back(SampleVectorOrZeros(property_samples, sample));
  }
  if (points.size() <= 2) {
    if (passes_out) { *passes_out = 0; }
    if (max_displacement_out) { *max_displacement_out = 0.0; }
    return points;
  }

  const int passes = std::clamp(
      static_cast<int>(std::llround(std::max(1.0, strength))), 2, 8);
  if (passes_out) { *passes_out = passes; }
  const double alpha = std::clamp(0.28 + strength * 0.035, 0.28, 0.55);
  std::vector<std::vector<double>> smoothed = points;
  std::vector<std::vector<double>> next = smoothed;
  for (int pass = 0; pass < passes; ++pass) {
    next = smoothed;
    for (std::size_t i = 1; i + 1 < smoothed.size(); ++i) {
      for (int d = 0; d < dims; ++d) {
        const std::size_t sd = static_cast<std::size_t>(d);
        const double neighbor_mid =
            (MotionSmoothComponentOrZero(smoothed[i - 1], sd) +
             MotionSmoothComponentOrZero(smoothed[i + 1], sd)) * 0.5;
        next[i][sd] =
            MotionSmoothComponentOrZero(smoothed[i], sd) * (1.0 - alpha) +
            neighbor_mid * alpha;
      }
    }
    next.front() = points.front();
    next.back() = points.back();
    smoothed.swap(next);
  }

  double max_displacement = 0.0;
  for (std::size_t i = 0; i < points.size(); ++i) {
    max_displacement = std::max(
        max_displacement, MotionPointDistance(points[i], smoothed[i], dims));
  }
  if (max_displacement_out) { *max_displacement_out = max_displacement; }
  return smoothed;
}

std::vector<int> MotionSmoothKeptPointIndices(
    const std::vector<std::vector<double>>& points,
    double tolerance,
    int dims) {
  std::vector<int> kept;
  if (points.empty()) {
    return kept;
  }
  std::vector<bool> keep(points.size(), false);
  keep.front() = true;
  keep.back() = true;
  MotionSmoothRdpKeep(points,
                      0,
                      static_cast<int>(points.size()) - 1,
                      tolerance,
                      dims,
                      &keep);
  for (std::size_t i = 0; i < keep.size(); ++i) {
    if (keep[i]) {
      kept.push_back(static_cast<int>(i));
    }
  }
  return kept;
}

std::vector<double> MotionSmoothScaledVector(const std::vector<double>& v,
                                             double scale,
                                             int dims) {
  std::vector<double> out(static_cast<std::size_t>(dims), 0.0);
  for (int d = 0; d < dims; ++d) {
    out[static_cast<std::size_t>(d)] =
        MotionSmoothComponentOrZero(v, static_cast<std::size_t>(d)) * scale;
  }
  return out;
}

std::vector<double> MotionSmoothDelta(const std::vector<double>& a,
                                      const std::vector<double>& b,
                                      int dims) {
  std::vector<double> out(static_cast<std::size_t>(dims), 0.0);
  for (int d = 0; d < dims; ++d) {
    out[static_cast<std::size_t>(d)] =
        MotionSmoothComponentOrZero(a, static_cast<std::size_t>(d)) -
        MotionSmoothComponentOrZero(b, static_cast<std::size_t>(d));
  }
  return out;
}

std::vector<double> MotionSmoothClampTangent(const std::vector<double>& tangent,
                                             double max_len,
                                             int dims) {
  const double len = std::sqrt(MotionPointDistanceSq(
      tangent, std::vector<double>(static_cast<std::size_t>(dims), 0.0), dims));
  if (len <= max_len || len <= 1e-9) {
    return tangent;
  }
  return MotionSmoothScaledVector(tangent, max_len / len, dims);
}

std::vector<double> MotionSmoothInterpolatedPoint(
    const PropertySamples& property_samples,
    const std::vector<std::vector<double>>& points,
    double t_sec,
    int dims) {
  if (points.empty() || property_samples.samples.empty()) {
    return std::vector<double>(static_cast<std::size_t>(dims), 0.0);
  }
  if (t_sec <= property_samples.samples.front().t_sec) {
    return points.front();
  }
  if (t_sec >= property_samples.samples.back().t_sec) {
    return points.back();
  }
  for (std::size_t i = 1; i < property_samples.samples.size(); ++i) {
    const double right_t = property_samples.samples[i].t_sec;
    if (right_t + 1e-9 < t_sec) {
      continue;
    }
    const double left_t = property_samples.samples[i - 1].t_sec;
    const double span = std::max(right_t - left_t, 1e-12);
    const double u = std::clamp((t_sec - left_t) / span, 0.0, 1.0);
    std::vector<double> out(static_cast<std::size_t>(dims), 0.0);
    for (int d = 0; d < dims; ++d) {
      const std::size_t sd = static_cast<std::size_t>(d);
      out[sd] =
          MotionSmoothComponentOrZero(points[i - 1], sd) * (1.0 - u) +
          MotionSmoothComponentOrZero(points[i], sd) * u;
    }
    return out;
  }
  return points.back();
}

}  // namespace bbsolver
