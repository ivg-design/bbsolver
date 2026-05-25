#include "bbsolver/motion_smooth/motion_smooth_spatial_trajectory.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "bbsolver/domain.hpp"
#include "bbsolver/motion_smooth/motion_smooth_bezier_ease.hpp"
#include "bbsolver/motion_smooth/motion_smooth_geometry.hpp"
#include "bbsolver/motion_smooth/motion_smooth_sample_points.hpp"
#include "bbsolver/samples/sample_key_timing.hpp"

namespace bbsolver {

PropertyKeys MotionSmoothSpatialTrajectoryKeys(
    const PropertySamples& property_samples,
    const SolverConfig& config) {
  PropertyKeys keys;
  keys.property_id = property_samples.property.id;
  keys.converged = true;
  keys.max_err = 0.0;
  keys.max_err_screen_px = 0.0;
  if (property_samples.samples.size() < 2) {
    keys.notes = "solve_mode_motion_smooth; no_spatial_span";
    return keys;
  }

  const int dims = std::max(property_samples.property.dimensions, 1);
  const double requested_strength =
      config.motion_smooth_tolerance > 0.0
          ? config.motion_smooth_tolerance
          : (config.tolerance_screen_px > 0.0
                ? config.tolerance_screen_px
                : config.tolerance);
  const double strength = std::max(1.0, requested_strength);
  int smoothing_passes = 0;
  double max_displacement = 0.0;
  std::vector<std::vector<double>> smoothed = MotionSmoothFilteredPoints(
      property_samples, strength, dims, &smoothing_passes, &max_displacement);
  const std::vector<std::vector<double>> raw =
      MotionSmoothRawPoints(property_samples, dims);
  std::vector<double> key_times = MotionSmoothSourceKeyTimes(property_samples);
  const bool using_source_key_times = key_times.size() >= 2;
  double simplify_tolerance = 0.0;
  if (!using_source_key_times) {
    simplify_tolerance = std::max(0.75, strength * 0.75);
    std::vector<int> kept =
        MotionSmoothKeptPointIndices(smoothed, simplify_tolerance, dims);
    if (kept.size() < 2) {
      kept.clear();
      kept.push_back(0);
      kept.push_back(static_cast<int>(smoothed.size()) - 1);
    }
    key_times.reserve(kept.size());
    for (int sample_idx : kept) {
      key_times.push_back(
          property_samples.samples[static_cast<std::size_t>(sample_idx)].t_sec);
    }
  }

  const bool use_ease =
      config.motion_smooth_use_ease && config.allow_bezier;
  const std::vector<TemporalEase> neutral_ease =
      DefaultEasesForProperty(property_samples);
  keys.keys.reserve(key_times.size());
  for (std::size_t ki = 0; ki < key_times.size(); ++ki) {
    Key key;
    key.t_sec = key_times[ki];
    key.v = MotionSmoothInterpolatedPoint(
        property_samples, smoothed, key.t_sec, dims);
    if (using_source_key_times &&
        (ki == 0 || ki + 1 == key_times.size())) {
      key.v = MotionSmoothInterpolatedPoint(
          property_samples, raw, key.t_sec, dims);
    }
    key.interp_in = ki == 0 ? InterpType::Linear : InterpType::Bezier;
    key.interp_out =
        ki + 1 == key_times.size() ? InterpType::Linear : InterpType::Bezier;
    key.temporal_ease_in = neutral_ease;
    key.temporal_ease_out = neutral_ease;
    key.temporal_continuous = use_ease;
    key.temporal_auto_bezier = use_ease;
    key.spatial_continuous = true;
    key.spatial_auto_bezier = false;
    key.roving = ki > 0 && ki + 1 < key_times.size();
    key.spatial_in.assign(static_cast<std::size_t>(dims), 0.0);
    key.spatial_out.assign(static_cast<std::size_t>(dims), 0.0);
    keys.keys.push_back(std::move(key));
  }

  for (std::size_t ki = 0; ki < key_times.size(); ++ki) {
    std::vector<double>& spatial_in = keys.keys[ki].spatial_in;
    std::vector<double>& spatial_out = keys.keys[ki].spatial_out;
    const std::vector<double>& cur = keys.keys[ki].v;
    if (ki + 1 < key_times.size()) {
      const std::vector<double>& next = keys.keys[ki + 1].v;
      std::vector<double> tangent;
      if (ki == 0) {
        tangent = MotionSmoothScaledVector(
            MotionSmoothDelta(next, cur, dims), 1.0 / 3.0, dims);
      } else {
        const std::vector<double>& prev = keys.keys[ki - 1].v;
        tangent = MotionSmoothScaledVector(
            MotionSmoothDelta(next, prev, dims), 1.0 / 6.0, dims);
      }
      const double max_len = MotionPointDistance(cur, next, dims) * 0.45;
      spatial_out = MotionSmoothClampTangent(tangent, max_len, dims);
    }
    if (ki > 0) {
      const std::vector<double>& prev = keys.keys[ki - 1].v;
      std::vector<double> tangent;
      if (ki + 1 == key_times.size()) {
        tangent = MotionSmoothScaledVector(
            MotionSmoothDelta(prev, cur, dims), 1.0 / 3.0, dims);
      } else {
        const std::vector<double>& next = keys.keys[ki + 1].v;
        tangent = MotionSmoothScaledVector(
            MotionSmoothDelta(prev, next, dims), 1.0 / 6.0, dims);
      }
      const double max_len = MotionPointDistance(cur, prev, dims) * 0.45;
      spatial_in = MotionSmoothClampTangent(tangent, max_len, dims);
    }
  }
  ApplyMotionSmoothBezierEase(property_samples, config, dims, &keys.keys);

  keys.max_err = max_displacement;
  keys.max_err_screen_px = max_displacement;
  keys.notes =
      std::string("solve_mode_motion_smooth") +
      "; motion_smooth_spatial_trajectory_filter=true" +
      "; motion_smooth_source_key_times=" +
      std::string(using_source_key_times ? "true" : "false") +
      "; source_key_count=" +
      std::to_string(property_samples.property.source_key_times.size()) +
      "; input_samples=" + std::to_string(property_samples.samples.size()) +
      "; output_keys=" + std::to_string(keys.keys.size()) +
      "; smoothing_passes=" + std::to_string(smoothing_passes) +
      "; smoothing_strength=" + std::to_string(strength) +
      (using_source_key_times
           ? "; key_schedule=source_keys"
           : "; key_schedule=sample_rdp; simplification_tolerance=" +
                 std::to_string(simplify_tolerance)) +
      "; max_smoothing_displacement=" + std::to_string(max_displacement) +
      "; motion_smooth_ease=" + (use_ease ? "on" : "off") +
      "; motion_smooth_bezier=" +
      std::to_string(config.motion_smooth_bezier_x1) + "," +
      std::to_string(config.motion_smooth_bezier_y1) + "," +
      std::to_string(config.motion_smooth_bezier_x2) + "," +
      std::to_string(config.motion_smooth_bezier_y2) +
      "; source_error_not_constrained=true";

  SegmentReport segment;
  segment.start_idx = 0;
  segment.end_idx = static_cast<int>(property_samples.samples.size()) - 1;
  segment.max_err = max_displacement;
  segment.max_err_screen_px = max_displacement;
  segment.rms_err = 0.0;
  segment.iters = smoothing_passes;
  segment.reason = "motion_smooth_spatial_trajectory_filter";
  keys.segments.push_back(std::move(segment));
  return keys;
}

}  // namespace bbsolver
