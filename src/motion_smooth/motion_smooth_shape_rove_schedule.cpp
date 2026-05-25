#include "bbsolver/motion_smooth/motion_smooth_shape_rove_schedule.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "bbsolver/motion_smooth/motion_smooth_shape_quality.hpp"

namespace bbsolver {

ShapeMotionRoveSchedule BuildShapeMotionRoveScheduleFromValues(
    const std::vector<double>& source_key_times,
    const std::vector<std::vector<double>>& source_values,
    int vertex_count,
    bool apply_rove) {
  ShapeMotionRoveSchedule schedule;
  schedule.source_key_count = static_cast<int>(source_key_times.size());
  if (source_key_times.empty()) {
    return schedule;
  }

  std::vector<double> kept_original_times;
  kept_original_times.reserve(source_key_times.size());
  schedule.times.reserve(source_key_times.size());
  schedule.values.reserve(source_values.size());
  const double static_eps = 1e-7;
  for (std::size_t i = 0;
       i < source_key_times.size() && i < source_values.size();
       ++i) {
    const bool is_endpoint = i == 0 || i + 1 == source_key_times.size();
    if (!is_endpoint && !schedule.values.empty() &&
        ShapeFlatControlDistance(
            schedule.values.back(),
            source_values[i],
            vertex_count) <= static_eps) {
      schedule.static_keys_removed++;
      continue;
    }
    kept_original_times.push_back(source_key_times[i]);
    schedule.times.push_back(source_key_times[i]);
    schedule.values.push_back(source_values[i]);
  }

  if (schedule.values.size() < 2) {
    return schedule;
  }

  std::vector<double> segment_travel(schedule.values.size() - 1, 0.0);
  for (std::size_t i = 1; i < schedule.values.size(); ++i) {
    double max_control = 0.0;
    const double travel = ShapeFlatControlDistance(
        schedule.values[i - 1],
        schedule.values[i],
        vertex_count,
        &max_control);
    segment_travel[i - 1] = travel;
    schedule.total_travel += travel;
    schedule.max_segment_travel =
        std::max(schedule.max_segment_travel, travel);
    schedule.max_control_step =
        std::max(schedule.max_control_step, max_control);
  }

  const double start_t = schedule.times.front();
  const double end_t = schedule.times.back();
  const double duration = end_t - start_t;
  if (schedule.values.size() <= 2 ||
      schedule.total_travel <= 1e-9 ||
      duration <= 1e-9) {
    return schedule;
  }
  if (!apply_rove) {
    return schedule;
  }

  double cumulative = 0.0;
  const double min_step =
      std::max(duration * 1e-9,
               std::min(1e-6, duration / (schedule.values.size() * 100000.0)));
  for (std::size_t i = 1; i + 1 < schedule.times.size(); ++i) {
    cumulative += segment_travel[i - 1];
    double roved_t =
        start_t + duration * (cumulative / schedule.total_travel);
    const double min_t = schedule.times[i - 1] + min_step;
    const double max_t =
        end_t - min_step * static_cast<double>(schedule.times.size() - 1 - i);
    roved_t = std::clamp(roved_t, min_t, max_t);
    schedule.max_time_shift_sec = std::max(
        schedule.max_time_shift_sec,
        std::abs(roved_t - kept_original_times[i]));
    schedule.times[i] = roved_t;
  }
  schedule.rove_applied = schedule.max_time_shift_sec > 1e-9;
  return schedule;
}

}  // namespace bbsolver
