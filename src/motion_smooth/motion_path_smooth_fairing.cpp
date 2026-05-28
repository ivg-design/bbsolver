#include "bbsolver/motion_smooth/motion_path_smooth_fairing.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include "bbsolver/domain.hpp"
#include "bbsolver/motion_smooth/motion_path_smooth_bounds.hpp"
#include "bbsolver/motion_smooth/motion_smooth_geometry.hpp"
#include "bbsolver/motion_smooth/motion_smooth_sample_points.hpp"

namespace bbsolver {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kMaxFairingAlpha = 0.72;

double PositiveOr(double value, double fallback) {
  return value > 0.0 && std::isfinite(value) ? value: fallback;
}

double TurnAngleDeg(const std::vector<double>& prev,
                    const std::vector<double>& cur,
                    const std::vector<double>& next,
                    int dims) {
  double dot = 0.0;
  double a_len_sq = 0.0;
  double b_len_sq = 0.0;
  for (int d = 0; d < dims; ++d) {
    const std::size_t sd = static_cast<std::size_t>(d);
    const double ax = MotionSmoothComponentOrZero(cur, sd) -
                      MotionSmoothComponentOrZero(prev, sd);
    const double bx = MotionSmoothComponentOrZero(next, sd) -
                      MotionSmoothComponentOrZero(cur, sd);
    dot += ax * bx;
    a_len_sq += ax * ax;
    b_len_sq += bx * bx;
  }
  if (a_len_sq <= 1e-12 || b_len_sq <= 1e-12) {
    return 0.0;
  }
  const double denom = std::sqrt(a_len_sq * b_len_sq);
  const double cos_angle = std::clamp(dot / denom, -1.0, 1.0);
  return std::acos(cos_angle) * 180.0 / kPi;
}

int NearestSampleIndexAtTime(const PropertySamples& property_samples,
                             double t_sec) {
  int best_idx = -1;
  double best_delta = 0.0;
  for (std::size_t i = 0; i < property_samples.samples.size(); ++i) {
    const double delta =
        std::abs(property_samples.samples[i].t_sec - t_sec);
    if (best_idx < 0 || delta < best_delta) {
      best_idx = static_cast<int>(i);
      best_delta = delta;
    }
  }
  return best_idx;
}

std::vector<double> TimeInterpolatedPoint(
    const PropertySamples& property_samples,
    const std::vector<std::vector<double>>& points,
    int first,
    int last,
    int sample_idx,
    int dims) {
  const std::size_t first_size = static_cast<std::size_t>(first);
  const std::size_t last_size = static_cast<std::size_t>(last);
  const std::size_t sample_size = static_cast<std::size_t>(sample_idx);
  const double first_t = property_samples.samples[first_size].t_sec;
  const double last_t = property_samples.samples[last_size].t_sec;
  const double sample_t = property_samples.samples[sample_size].t_sec;
  const double span = std::max(last_t - first_t, 1e-12);
  const double u = std::clamp((sample_t - first_t) / span, 0.0, 1.0);
  std::vector<double> out(static_cast<std::size_t>(dims), 0.0);
  for (int d = 0; d < dims; ++d) {
    const std::size_t sd = static_cast<std::size_t>(d);
    out[sd] =
        MotionSmoothComponentOrZero(points[first_size], sd) * (1.0 - u) +
        MotionSmoothComponentOrZero(points[last_size], sd) * u;
  }
  return out;
}

void MotionPathTimeRdpKeep(const PropertySamples& property_samples,
                           const std::vector<std::vector<double>>& points,
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
    const std::vector<double> interpolated = TimeInterpolatedPoint(
        property_samples, points, first, last, i, dims);
    const double dist = MotionPointDistance(
        points[static_cast<std::size_t>(i)], interpolated, dims);
    if (dist > best_dist) {
      best_dist = dist;
      best_idx = i;
    }
  }
  if (best_idx >= 0 && best_dist > tolerance) {
    (*keep)[static_cast<std::size_t>(best_idx)] = true;
    MotionPathTimeRdpKeep(
        property_samples, points, first, best_idx, tolerance, dims, keep);
    MotionPathTimeRdpKeep(
        property_samples, points, best_idx, last, tolerance, dims, keep);
  }
}

double MotionPathLength(const std::vector<std::vector<double>>& points,
                        int dims) {
  double length = 0.0;
  for (std::size_t i = 1; i < points.size(); ++i) {
    length += MotionPointDistance(points[i - 1], points[i], dims);
  }
  return length;
}

bool IsHardLockedAtIndex(const MotionPathLocks& locks, std::size_t idx) {
  return (idx < locks.keyed.size() && locks.keyed[idx]) ||
         (idx < locks.sharp.size() && locks.sharp[idx]);
}

}  // namespace

MotionPathLocks BuildMotionPathLocks(
    const PropertySamples& property_samples,
    const std::vector<std::vector<double>>& raw,
    const SolverConfig& config,
    int dims) {
  MotionPathLocks locks;
  locks.keyed.assign(raw.size(), false);
  locks.sharp.assign(raw.size(), false);
  locks.bounds.assign(raw.size(), false);
  if (raw.empty()) {
    return locks;
  }

  locks.keyed.front() = true;
  locks.keyed.back() = true;
  if (config.motion_path_respect_keyed_frames) {
    for (double t: MotionSmoothSourceKeyTimes(property_samples)) {
      const int sample_idx = NearestSampleIndexAtTime(property_samples, t);
      if (sample_idx >= 0) {
        locks.keyed[static_cast<std::size_t>(sample_idx)] = true;
      }
    }
  }

  if (config.motion_path_preserve_sharp_points && raw.size() >= 3) {
    const double angle_threshold =
        std::clamp(PositiveOr(config.motion_path_sharp_angle_deg, 75.0),
                   1.0,
                   179.0);
    for (std::size_t i = 1; i + 1 < raw.size(); ++i) {
      const double angle =
          TurnAngleDeg(raw[i - 1], raw[i], raw[i + 1], dims);
      if (angle >= angle_threshold) {
        locks.sharp[i] = true;
      }
    }
  }
  if (config.motion_path_preserve_bounds) {
    MarkMotionPathBoundsExtrema(raw, dims, &locks.bounds);
  }
  return locks;
}

bool IsMotionPathLockedAtIndex(const MotionPathLocks& locks, std::size_t idx) {
  return IsHardLockedAtIndex(locks, idx);
}

MotionPathFairingResult FairMotionPathPoints(
    const std::vector<std::vector<double>>& raw,
    const MotionPathLocks& locks,
    double strength,
    bool preserve_bounds,
    double bounds_tolerance,
    int dims) {
  MotionPathFairingResult result;
  const MotionPathBounds source_bounds = ComputeMotionPathBounds(raw, dims);
  result.source_bounds_width = MotionPathBoundsSpan(source_bounds, 0);
  result.source_bounds_height =
      dims > 1 ? MotionPathBoundsSpan(source_bounds, 1): 0.0;
  result.bounds_preserved = preserve_bounds;
  result.bounds_tolerance =
      std::isfinite(bounds_tolerance) && bounds_tolerance > 0.0
          ? bounds_tolerance
: 0.0;
  result.source_path_length = MotionPathLength(raw, dims);
  if (raw.size() <= 2) {
    result.points = raw;
    result.smoothed_path_length = result.source_path_length;
    const MotionPathBounds smoothed_bounds =
        ComputeMotionPathBounds(result.points, dims);
    result.smoothed_bounds_width = MotionPathBoundsSpan(smoothed_bounds, 0);
    result.smoothed_bounds_height =
        dims > 1 ? MotionPathBoundsSpan(smoothed_bounds, 1): 0.0;
    result.bounds_max_deviation =
        MotionPathBoundsSideDeviation(source_bounds, smoothed_bounds, dims);
    return result;
  }

  const double clamped_strength =
      std::clamp(strength, kMotionPathSmoothingMin, kMotionPathSmoothingMax);
  result.passes = std::clamp(
      static_cast<int>(std::llround(clamped_strength * clamped_strength * 4.0)),
      2,
      4096);
  result.alpha =
      std::clamp(0.32 + clamped_strength * 0.0125, 0.32, kMaxFairingAlpha);

  std::vector<std::vector<double>> smoothed = raw;
  std::vector<std::vector<double>> next = smoothed;
  for (int pass = 0; pass < result.passes; ++pass) {
    next = smoothed;
    for (std::size_t i = 1; i + 1 < smoothed.size(); ++i) {
      if (IsMotionPathLockedAtIndex(locks, i)) {
        next[i] = raw[i];
        continue;
      }
      for (int d = 0; d < dims; ++d) {
        const std::size_t sd = static_cast<std::size_t>(d);
        const double neighbor_mid =
            (MotionSmoothComponentOrZero(smoothed[i - 1], sd) +
             MotionSmoothComponentOrZero(smoothed[i + 1], sd)) * 0.5;
        next[i][sd] =
            MotionSmoothComponentOrZero(smoothed[i], sd) *
                (1.0 - result.alpha) +
            neighbor_mid * result.alpha;
      }
    }
    next.front() = raw.front();
    next.back() = raw.back();
    smoothed.swap(next);
  }

  if (preserve_bounds) {
    ApplyMotionPathBoundsConstraint(
        raw, locks, source_bounds, result.bounds_tolerance, dims, &smoothed);
  }

  for (std::size_t i = 0; i < raw.size(); ++i) {
    result.max_displacement = std::max(
        result.max_displacement, MotionPointDistance(raw[i], smoothed[i], dims));
  }
  result.smoothed_path_length = MotionPathLength(smoothed, dims);
  const MotionPathBounds smoothed_bounds =
      ComputeMotionPathBounds(smoothed, dims);
  result.smoothed_bounds_width = MotionPathBoundsSpan(smoothed_bounds, 0);
  result.smoothed_bounds_height =
      dims > 1 ? MotionPathBoundsSpan(smoothed_bounds, 1): 0.0;
  result.bounds_max_deviation =
      MotionPathBoundsSideDeviation(source_bounds, smoothed_bounds, dims);
  result.points = std::move(smoothed);
  return result;
}

std::vector<int> MotionPathKeptIndices(
    const PropertySamples& property_samples,
    const std::vector<std::vector<double>>& smoothed,
    const MotionPathLocks& locks,
    double accuracy_tolerance,
    int dims) {
  std::vector<bool> keep(smoothed.size(), false);
  if (!smoothed.empty()) {
    keep.front() = true;
    keep.back() = true;
  }
  for (std::size_t i = 0; i < smoothed.size(); ++i) {
    if (IsMotionPathLockedAtIndex(locks, i) ||
        (i < locks.bounds.size() && locks.bounds[i])) {
      keep[i] = true;
    }
  }
  if (CountMotionPathLocks(locks.bounds) > 0) {
    MarkMotionPathBoundsExtrema(smoothed, dims, &keep);
  }

  int segment_start = -1;
  for (std::size_t i = 0; i < keep.size(); ++i) {
    if (!keep[i]) {
      continue;
    }
    const int anchor_idx = static_cast<int>(i);
    if (segment_start >= 0 && anchor_idx > segment_start) {
      MotionPathTimeRdpKeep(property_samples,
                            smoothed,
                            segment_start,
                            anchor_idx,
                            accuracy_tolerance,
                            dims,
                            &keep);
    }
    segment_start = anchor_idx;
  }

  std::vector<int> kept;
  for (std::size_t i = 0; i < keep.size(); ++i) {
    if (keep[i]) {
      kept.push_back(static_cast<int>(i));
    }
  }
  if (kept.size() < 2 && smoothed.size() >= 2) {
    kept.clear();
    kept.push_back(0);
    kept.push_back(static_cast<int>(smoothed.size()) - 1);
  }
  return kept;
}

int CountMotionPathLocks(const std::vector<bool>& locked) {
  int count = 0;
  for (bool item: locked) {
    if (item) {
      ++count;
    }
  }
  return count;
}

}  // namespace bbsolver
