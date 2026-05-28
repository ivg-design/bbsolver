#include "bbsolver/motion_smooth/motion_path_smooth_bounds.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include "bbsolver/motion_smooth/motion_path_smooth_fairing.hpp"
#include "bbsolver/motion_smooth/motion_smooth_geometry.hpp"

namespace bbsolver {
namespace {

constexpr double kBoundsEpsilon = 1e-9;

bool IsHardLockedAtIndex(const MotionPathLocks& locks, std::size_t idx) {
  return (idx < locks.keyed.size() && locks.keyed[idx]) ||
         (idx < locks.sharp.size() && locks.sharp[idx]);
}

double ClampedBoundsSide(double side, double source_side, double tolerance) {
  if (tolerance <= 0.0) {
    return source_side;
  }
  return std::clamp(side, source_side - tolerance, source_side + tolerance);
}

}  // namespace

MotionPathBounds ComputeMotionPathBounds(
    const std::vector<std::vector<double>>& points,
    int dims) {
  MotionPathBounds bounds;
  const std::size_t dim_count = static_cast<std::size_t>(std::max(dims, 1));
  bounds.min.assign(dim_count, std::numeric_limits<double>::infinity());
  bounds.max.assign(dim_count, -std::numeric_limits<double>::infinity());
  for (const std::vector<double>& point: points) {
    for (int d = 0; d < dims; ++d) {
      const std::size_t sd = static_cast<std::size_t>(d);
      const double value = MotionSmoothComponentOrZero(point, sd);
      bounds.min[sd] = std::min(bounds.min[sd], value);
      bounds.max[sd] = std::max(bounds.max[sd], value);
    }
  }
  for (int d = 0; d < dims; ++d) {
    const std::size_t sd = static_cast<std::size_t>(d);
    if (!std::isfinite(bounds.min[sd]) || !std::isfinite(bounds.max[sd])) {
      bounds.min[sd] = 0.0;
      bounds.max[sd] = 0.0;
    }
  }
  return bounds;
}

double MotionPathBoundsSpan(const MotionPathBounds& bounds, int dim) {
  const std::size_t sd = static_cast<std::size_t>(dim);
  if (sd >= bounds.min.size() || sd >= bounds.max.size()) {
    return 0.0;
  }
  return bounds.max[sd] - bounds.min[sd];
}

double MotionPathBoundsSideDeviation(const MotionPathBounds& source,
                                     const MotionPathBounds& candidate,
                                     int dims) {
  double deviation = 0.0;
  for (int d = 0; d < dims; ++d) {
    const std::size_t sd = static_cast<std::size_t>(d);
    deviation = std::max(
        deviation, std::abs(candidate.min[sd] - source.min[sd]));
    deviation = std::max(
        deviation, std::abs(candidate.max[sd] - source.max[sd]));
  }
  return deviation;
}

void MarkMotionPathBoundsExtrema(
    const std::vector<std::vector<double>>& points,
    int dims,
    std::vector<bool>* keep) {
  if (!keep || points.empty()) {
    return;
  }
  keep->resize(points.size(), false);
  for (int d = 0; d < dims; ++d) {
    const std::size_t sd = static_cast<std::size_t>(d);
    std::size_t min_idx = 0;
    std::size_t max_idx = 0;
    double min_value = MotionSmoothComponentOrZero(points.front(), sd);
    double max_value = min_value;
    for (std::size_t i = 1; i < points.size(); ++i) {
      const double value = MotionSmoothComponentOrZero(points[i], sd);
      if (value < min_value) {
        min_value = value;
        min_idx = i;
      }
      if (value > max_value) {
        max_value = value;
        max_idx = i;
      }
    }
    (*keep)[min_idx] = true;
    (*keep)[max_idx] = true;
  }
}

void ApplyMotionPathBoundsConstraint(
    const std::vector<std::vector<double>>& raw,
    const MotionPathLocks& locks,
    const MotionPathBounds& source_bounds,
    double bounds_tolerance,
    int dims,
    std::vector<std::vector<double>>* smoothed) {
  if (!smoothed || smoothed->empty()) {
    return;
  }
  const double tolerance =
      std::isfinite(bounds_tolerance) && bounds_tolerance > 0.0
          ? bounds_tolerance
: 0.0;
  for (int d = 0; d < dims; ++d) {
    const std::size_t sd = static_cast<std::size_t>(d);
    double smooth_min = std::numeric_limits<double>::infinity();
    double smooth_max = -std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < smoothed->size(); ++i) {
      if (IsHardLockedAtIndex(locks, i)) {
        continue;
      }
      const double value = MotionSmoothComponentOrZero((*smoothed)[i], sd);
      smooth_min = std::min(smooth_min, value);
      smooth_max = std::max(smooth_max, value);
    }
    if (!std::isfinite(smooth_min) || !std::isfinite(smooth_max)) {
      continue;
    }
    const double source_min = source_bounds.min[sd];
    const double source_max = source_bounds.max[sd];
    const double source_span = std::max(source_max - source_min, 0.0);
    const double smooth_span = smooth_max - smooth_min;
    double target_min = ClampedBoundsSide(smooth_min, source_min, tolerance);
    double target_max = ClampedBoundsSide(smooth_max, source_max, tolerance);
    const double min_allowed_span =
        std::max(source_span - tolerance * 2.0, 0.0);
    if (target_max - target_min < min_allowed_span) {
      if (source_span > tolerance * 2.0) {
        target_min = source_min + tolerance;
        target_max = source_max - tolerance;
      } else {
        const double center = (source_min + source_max) * 0.5;
        target_min = center;
        target_max = center;
      }
    }
    if (smooth_span <= kBoundsEpsilon) {
      const double source_normalizer =
          source_span > kBoundsEpsilon ? source_span: 1.0;
      for (std::size_t i = 0; i < smoothed->size(); ++i) {
        if (IsHardLockedAtIndex(locks, i)) {
          continue;
        }
        std::vector<double>& point = (*smoothed)[i];
        if (sd < point.size()) {
          const double raw_value =
              i < raw.size() ? MotionSmoothComponentOrZero(raw[i], sd)
: source_min;
          const double u = source_span > kBoundsEpsilon
                               ? (raw_value - source_min) / source_normalizer
: 0.0;
          point[sd] = target_min + std::clamp(u, 0.0, 1.0) *
                                       (target_max - target_min);
        }
      }
      continue;
    }
    const double scale = (target_max - target_min) / smooth_span;
    for (std::size_t i = 0; i < smoothed->size(); ++i) {
      if (IsHardLockedAtIndex(locks, i)) {
        continue;
      }
      std::vector<double>& point = (*smoothed)[i];
      if (sd >= point.size()) {
        continue;
      }
      point[sd] = target_min + (point[sd] - smooth_min) * scale;
    }
  }
}

}  // namespace bbsolver
