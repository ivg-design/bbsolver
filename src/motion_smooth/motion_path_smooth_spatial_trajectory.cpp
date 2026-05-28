#include "bbsolver/motion_smooth/motion_path_smooth_spatial_trajectory.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "bbsolver/domain.hpp"
#include "bbsolver/motion_smooth/motion_smooth_bezier_ease.hpp"
#include "bbsolver/motion_smooth/motion_smooth_geometry.hpp"
#include "bbsolver/motion_smooth/motion_path_smooth_fairing.hpp"
#include "bbsolver/motion_smooth/motion_smooth_sample_points.hpp"
#include "bbsolver/samples/sample_key_timing.hpp"

namespace bbsolver {
namespace {

double PositiveOr(double value, double fallback) {
  return value > 0.0 && std::isfinite(value) ? value: fallback;
}

std::vector<double> InterpolatedEmittedKeyValue(
    const std::vector<Key>& keys,
    double t_sec,
    int dims) {
  if (keys.empty()) {
    return std::vector<double>(static_cast<std::size_t>(dims), 0.0);
  }
  if (t_sec <= keys.front().t_sec) {
    return keys.front().v;
  }
  if (t_sec >= keys.back().t_sec) {
    return keys.back().v;
  }
  for (std::size_t i = 1; i < keys.size(); ++i) {
    if (keys[i].t_sec + 1e-9 < t_sec) {
      continue;
    }
    const double left_t = keys[i - 1].t_sec;
    const double right_t = keys[i].t_sec;
    const double span = std::max(right_t - left_t, 1e-12);
    const double u = std::clamp((t_sec - left_t) / span, 0.0, 1.0);
    std::vector<double> out(static_cast<std::size_t>(dims), 0.0);
    for (int d = 0; d < dims; ++d) {
      const std::size_t sd = static_cast<std::size_t>(d);
      out[sd] =
          MotionSmoothComponentOrZero(keys[i - 1].v, sd) * (1.0 - u) +
          MotionSmoothComponentOrZero(keys[i].v, sd) * u;
    }
    return out;
  }
  return keys.back().v;
}

double MaxEmittedPathError(const PropertySamples& property_samples,
                           const std::vector<std::vector<double>>& smoothed,
                           const std::vector<Key>& keys,
                           int dims) {
  double max_err = 0.0;
  for (const Sample& sample: property_samples.samples) {
    const std::vector<double> expected = MotionSmoothInterpolatedPoint(
        property_samples, smoothed, sample.t_sec, dims);
    const std::vector<double> actual =
        InterpolatedEmittedKeyValue(keys, sample.t_sec, dims);
    max_err = std::max(max_err, MotionPointDistance(actual, expected, dims));
  }
  return max_err;
}

void BuildMotionPathTangents(const MotionPathLocks& locks,
                             int dims,
                             std::vector<Key>* keys) {
  if (!keys) {
    return;
  }
  for (std::size_t ki = 0; ki < keys->size(); ++ki) {
    Key& key = (*keys)[ki];
    const bool sharp = ki < locks.sharp.size() && locks.sharp[ki];
    key.spatial_in.assign(static_cast<std::size_t>(dims), 0.0);
    key.spatial_out.assign(static_cast<std::size_t>(dims), 0.0);
    if (sharp) {
      key.spatial_continuous = false;
      key.spatial_auto_bezier = false;
      continue;
    }
    const std::vector<double>& cur = key.v;
    if (ki + 1 < keys->size()) {
      const std::vector<double>& next = (*keys)[ki + 1].v;
      std::vector<double> tangent;
      if (ki == 0) {
        tangent = MotionSmoothScaledVector(
            MotionSmoothDelta(next, cur, dims), 1.0 / 3.0, dims);
      } else {
        const std::vector<double>& prev = (*keys)[ki - 1].v;
        tangent = MotionSmoothScaledVector(
            MotionSmoothDelta(next, prev, dims), 1.0 / 6.0, dims);
      }
      const double max_len = MotionPointDistance(cur, next, dims) * 0.45;
      key.spatial_out = MotionSmoothClampTangent(tangent, max_len, dims);
    }
    if (ki > 0) {
      const std::vector<double>& prev = (*keys)[ki - 1].v;
      std::vector<double> tangent;
      if (ki + 1 == keys->size()) {
        tangent = MotionSmoothScaledVector(
            MotionSmoothDelta(prev, cur, dims), 1.0 / 3.0, dims);
      } else {
        const std::vector<double>& next = (*keys)[ki + 1].v;
        tangent = MotionSmoothScaledVector(
            MotionSmoothDelta(prev, next, dims), 1.0 / 6.0, dims);
      }
      const double max_len = MotionPointDistance(cur, prev, dims) * 0.45;
      key.spatial_in = MotionSmoothClampTangent(tangent, max_len, dims);
    }
  }
}

void ReassertSharpMotionPathKeys(const MotionPathLocks& key_locks,
                                 std::vector<Key>* keys) {
  if (!keys) {
    return;
  }
  for (std::size_t i = 0; i < keys->size() && i < key_locks.sharp.size();
       ++i) {
    if (!key_locks.sharp[i]) {
      continue;
    }
    Key& key = (*keys)[i];
    key.temporal_continuous = false;
    key.temporal_auto_bezier = false;
    key.spatial_continuous = false;
    key.spatial_auto_bezier = false;
    key.roving = false;
  }
}

}  // namespace

PropertyKeys MotionPathSmoothSpatialTrajectoryKeys(
    const PropertySamples& property_samples,
    const SolverConfig& config) {
  PropertyKeys out;
  out.property_id = property_samples.property.id;
  out.converged = true;
  if (property_samples.samples.size() < 2) {
    out.notes = "solve_mode_motion_path_smooth; no_spatial_span";
    return out;
  }

  const int dims = std::max(property_samples.property.dimensions, 1);
  const double strength =
      std::clamp(PositiveOr(config.motion_path_smoothing_tolerance,
                            kMotionPathSmoothingDefault),
                 kMotionPathSmoothingMin,
                 kMotionPathSmoothingMax);
  const double accuracy_tolerance =
      PositiveOr(config.motion_path_accuracy_tolerance,
                 PositiveOr(config.tolerance_screen_px, config.tolerance));
  const double bounds_tolerance =
      std::isfinite(config.motion_path_bounds_tolerance) &&
              config.motion_path_bounds_tolerance > 0.0
          ? config.motion_path_bounds_tolerance
: 0.0;
  const std::vector<std::vector<double>> raw =
      MotionSmoothRawPoints(property_samples, dims);
  const MotionPathLocks locks =
      BuildMotionPathLocks(property_samples, raw, config, dims);

  const MotionPathFairingResult fairing =
      FairMotionPathPoints(raw,
                           locks,
                           strength,
                           config.motion_path_preserve_bounds,
                           bounds_tolerance,
                           dims);
  const std::vector<int> kept =
      MotionPathKeptIndices(property_samples,
                            fairing.points,
                            locks,
                            accuracy_tolerance,
                            dims);

  const bool use_ease =
      config.motion_smooth_use_ease && config.allow_bezier;
  const std::vector<TemporalEase> neutral_ease =
      DefaultEasesForProperty(property_samples);
  out.keys.reserve(kept.size());
  for (std::size_t ki = 0; ki < kept.size(); ++ki) {
    const int sample_idx = kept[ki];
    const std::size_t si = static_cast<std::size_t>(sample_idx);
    Key key;
    key.t_sec = property_samples.samples[si].t_sec;
    const bool locked = IsMotionPathLockedAtIndex(locks, si);
    const bool sharp = si < locks.sharp.size() && locks.sharp[si];
    key.v = locked ? raw[si]: fairing.points[si];
    key.interp_in = ki == 0 ? InterpType::Linear: InterpType::Bezier;
    key.interp_out =
        ki + 1 == kept.size() ? InterpType::Linear: InterpType::Bezier;
    key.temporal_ease_in = neutral_ease;
    key.temporal_ease_out = neutral_ease;
    key.temporal_continuous = use_ease && !sharp;
    key.temporal_auto_bezier = use_ease && !sharp;
    key.spatial_continuous = !sharp;
    key.spatial_auto_bezier = false;
    key.roving = use_ease && ki > 0 && ki + 1 < kept.size() &&
                 !locked;
    out.keys.push_back(std::move(key));
  }

  MotionPathLocks key_locks;
  key_locks.keyed.reserve(kept.size());
  key_locks.sharp.reserve(kept.size());
  key_locks.bounds.reserve(kept.size());
  for (int sample_idx: kept) {
    const std::size_t si = static_cast<std::size_t>(sample_idx);
    key_locks.keyed.push_back(si < locks.keyed.size() && locks.keyed[si]);
    key_locks.sharp.push_back(si < locks.sharp.size() && locks.sharp[si]);
    key_locks.bounds.push_back(si < locks.bounds.size() && locks.bounds[si]);
  }
  BuildMotionPathTangents(key_locks, dims, &out.keys);
  ApplyMotionSmoothBezierEase(property_samples, config, dims, &out.keys);
  ReassertSharpMotionPathKeys(key_locks, &out.keys);

  const double smoothed_path_max_err =
      MaxEmittedPathError(property_samples, fairing.points, out.keys, dims);
  out.max_err = smoothed_path_max_err;
  out.max_err_screen_px = smoothed_path_max_err;

  std::ostringstream notes;
  notes << "solve_mode_motion_path_smooth"
        << "; motion_path_spatial_trajectory_filter=true"
        << "; input_samples=" << property_samples.samples.size()
        << "; source_key_count="
        << property_samples.property.source_key_times.size()
        << "; output_keys=" << out.keys.size()
        << "; smoothing_passes=" << fairing.passes
        << "; smoothing_strength=" << strength
        << "; smoothing_alpha=" << fairing.alpha
        << "; motion_path_accuracy_tolerance=" << accuracy_tolerance
        << "; motion_path_preserve_bounds="
        << (config.motion_path_preserve_bounds ? "true": "false")
        << "; motion_path_bounds_tolerance=" << bounds_tolerance
        << "; motion_path_bounds_points=" << CountMotionPathLocks(locks.bounds)
        << "; source_bounds_width=" << fairing.source_bounds_width
        << "; source_bounds_height=" << fairing.source_bounds_height
        << "; smoothed_bounds_width=" << fairing.smoothed_bounds_width
        << "; smoothed_bounds_height=" << fairing.smoothed_bounds_height
        << "; bounds_max_deviation=" << fairing.bounds_max_deviation
        << "; motion_path_preserve_sharp_points="
        << (config.motion_path_preserve_sharp_points ? "true": "false")
        << "; motion_path_sharp_angle_deg="
        << PositiveOr(config.motion_path_sharp_angle_deg, 75.0)
        << "; motion_path_sharp_points=" << CountMotionPathLocks(locks.sharp)
        << "; motion_path_respect_keyed_frames="
        << (config.motion_path_respect_keyed_frames ? "true": "false")
        << "; motion_path_keyed_points=" << CountMotionPathLocks(locks.keyed)
        << "; raw_source_max_displacement=" << fairing.max_displacement
        << "; source_path_length=" << fairing.source_path_length
        << "; smoothed_path_length=" << fairing.smoothed_path_length
        << "; smoothed_path_max_err=" << smoothed_path_max_err
        << "; motion_smooth_ease=" << (use_ease ? "on": "off")
        << "; source_error_not_constrained=true";
  out.notes = notes.str();

  SegmentReport segment;
  segment.start_idx = 0;
  segment.end_idx = static_cast<int>(property_samples.samples.size()) - 1;
  segment.max_err = smoothed_path_max_err;
  segment.max_err_screen_px = smoothed_path_max_err;
  segment.rms_err = 0.0;
  segment.iters = fairing.passes;
  segment.reason = "motion_path_spatial_trajectory_filter";
  out.segments.push_back(std::move(segment));
  return out;
}

}  // namespace bbsolver
