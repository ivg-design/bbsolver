#include "bbsolver/motion_smooth/motion_smooth_shape_source_key_schedule.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

#include "bbsolver/domain.hpp"
#include "bbsolver/motion_smooth/motion_smooth_sample_points.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_quality.hpp"

namespace bbsolver {

void ShapeMotionSourceKeyRdpKeep(
    const std::vector<double>& times,
    const std::vector<std::vector<double>>& values,
    int first,
    int last,
    double tolerance,
    std::vector<bool>* keep) {
  if (!keep || last <= first + 1) {
    return;
  }
  const double span = std::max(times[static_cast<std::size_t>(last)] -
                                  times[static_cast<std::size_t>(first)],
                              1e-12);
  double best_dist = -1.0;
  int best_idx = -1;
  for (int i = first + 1; i < last; ++i) {
    const double u = std::clamp(
        (times[static_cast<std::size_t>(i)] -
         times[static_cast<std::size_t>(first)]) /
            span,
        0.0,
        1.0);
    const double dist = ShapeFlatVectorDistanceToLinear(
        values[static_cast<std::size_t>(first)],
        values[static_cast<std::size_t>(last)],
        values[static_cast<std::size_t>(i)],
        u);
    if (dist > best_dist) {
      best_dist = dist;
      best_idx = i;
    }
  }
  if (best_idx >= 0 && best_dist > tolerance) {
    (*keep)[static_cast<std::size_t>(best_idx)] = true;
    ShapeMotionSourceKeyRdpKeep(
        times, values, first, best_idx, tolerance, keep);
    ShapeMotionSourceKeyRdpKeep(
        times, values, best_idx, last, tolerance, keep);
  }
}

ShapeMotionSourceKeySchedule BuildShapeMotionSourceKeySchedule(
    const bbsolver::PropertySamples& property_samples,
    const std::vector<double>& source_key_times,
    const std::vector<std::vector<double>>& raw,
    int dims,
    double strength) {
  ShapeMotionSourceKeySchedule schedule;
  schedule.raw_count = static_cast<int>(source_key_times.size());
  schedule.raw_times = source_key_times;
  schedule.raw_values.reserve(source_key_times.size());
  for (double source_key_time : source_key_times) {
    std::vector<double> value = MotionSmoothInterpolatedVector(
        property_samples, raw, source_key_time, dims);
    if (static_cast<int>(value.size()) >= 2) {
      value[0] = property_samples.samples.front().v[0];
      value[1] = property_samples.samples.front().v[1];
    }
    schedule.raw_values.push_back(std::move(value));
  }
  schedule.times = schedule.raw_times;
  schedule.values = schedule.raw_values;
  schedule.simplified_count = static_cast<int>(schedule.times.size());
  if (schedule.times.size() <= 2) {
    return schedule;
  }

  schedule.simplification_enabled = true;
  schedule.simplify_tolerance =
      std::max(0.75, std::min(3.0, strength * 0.5));
  std::vector<bool> keep(schedule.times.size(), false);
  keep.front() = true;
  keep.back() = true;
  ShapeMotionSourceKeyRdpKeep(schedule.times,
                              schedule.values,
                              0,
                              static_cast<int>(schedule.times.size()) - 1,
                              schedule.simplify_tolerance,
                              &keep);

  int kept_count = 0;
  for (bool item : keep) {
    if (item) {
      ++kept_count;
    }
  }
  if (kept_count <= 1 ||
      kept_count == static_cast<int>(schedule.times.size())) {
    return schedule;
  }

  std::vector<double> simplified_times;
  std::vector<std::vector<double>> simplified_values;
  simplified_times.reserve(static_cast<std::size_t>(kept_count));
  simplified_values.reserve(static_cast<std::size_t>(kept_count));
  for (std::size_t i = 0; i < keep.size(); ++i) {
    if (!keep[i]) {
      continue;
    }
    simplified_times.push_back(schedule.times[i]);
    simplified_values.push_back(schedule.values[i]);
  }
  schedule.times = std::move(simplified_times);
  schedule.values = std::move(simplified_values);
  schedule.simplified_count = static_cast<int>(schedule.times.size());
  schedule.redundant_removed = schedule.raw_count - schedule.simplified_count;
  return schedule;
}

}  // namespace bbsolver
