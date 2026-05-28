#include "bbsolver/motion_smooth/motion_smooth_shape_loop_schedule.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "bbsolver/motion_smooth/motion_smooth_shape_quality.hpp"

namespace bbsolver {

std::vector<double> TimesForClosedLoopParams(
    const std::vector<double>& source_key_times,
    const std::vector<double>& params) {
  std::vector<double> times;
  if (source_key_times.size() < 2 || params.empty()) {
    return times;
  }
  const int unique_count = static_cast<int>(source_key_times.size()) - 1;
  times.reserve(params.size());
  for (double param: params) {
    if (param <= 0.0) {
      times.push_back(source_key_times.front());
      continue;
    }
    if (param >= static_cast<double>(unique_count)) {
      times.push_back(source_key_times.back());
      continue;
    }
    const int segment = std::clamp(static_cast<int>(std::floor(param)),
                                   0,
                                   unique_count - 1);
    const double u = std::clamp(param - static_cast<double>(segment),
                                0.0,
                                1.0);
    const double left_t = source_key_times[static_cast<std::size_t>(segment)];
    const double right_t =
        source_key_times[static_cast<std::size_t>(segment + 1)];
    times.push_back(left_t + (right_t - left_t) * u);
  }
  return times;
}

SourcePoseIntervalTimeSchedule TimesForClosedLoopParamsByIntervalTravel(
    const std::vector<double>& source_key_times,
    const std::vector<double>& params,
    const std::vector<std::vector<double>>& values,
    int vertex_count) {
  SourcePoseIntervalTimeSchedule schedule;
  schedule.times = TimesForClosedLoopParams(source_key_times, params);
  if (source_key_times.size() < 2 ||
      params.size() != values.size() ||
      schedule.times.size() != params.size() ||
      vertex_count <= 0) {
    return schedule;
  }
  const std::vector<double> linear_times = schedule.times;
  const int unique_count = static_cast<int>(source_key_times.size()) - 1;
  for (int segment = 0; segment < unique_count; ++segment) {
    int start_idx = -1;
    int end_idx = -1;
    const double left_param = static_cast<double>(segment);
    const double right_param = static_cast<double>(segment + 1);
    for (std::size_t i = 0; i < params.size(); ++i) {
      if (start_idx < 0 && std::abs(params[i] - left_param) <= 1e-9) {
        start_idx = static_cast<int>(i);
      }
      if (std::abs(params[i] - right_param) <= 1e-9) {
        end_idx = static_cast<int>(i);
        break;
      }
    }
    if (start_idx < 0 || end_idx <= start_idx) {
      continue;
    }
    const double left_t = source_key_times[static_cast<std::size_t>(segment)];
    const double right_t =
        source_key_times[static_cast<std::size_t>(segment + 1)];
    const double duration = right_t - left_t;
    schedule.times[static_cast<std::size_t>(start_idx)] = left_t;
    schedule.times[static_cast<std::size_t>(end_idx)] = right_t;
    if (end_idx <= start_idx + 1 || duration <= 1e-9) {
      continue;
    }
    std::vector<double> segment_travel(
        static_cast<std::size_t>(end_idx - start_idx), 0.0);
    double total_travel = 0.0;
    for (int i = start_idx + 1; i <= end_idx; ++i) {
      const double travel = ShapeFlatControlDistance(
          values[static_cast<std::size_t>(i - 1)],
          values[static_cast<std::size_t>(i)],
          vertex_count);
      segment_travel[static_cast<std::size_t>(i - start_idx - 1)] = travel;
      total_travel += travel;
    }
    if (total_travel <= 1e-9) {
      continue;
    }
    double cumulative = 0.0;
    for (int i = start_idx + 1; i < end_idx; ++i) {
      cumulative +=
          segment_travel[static_cast<std::size_t>(i - start_idx - 1)];
      schedule.times[static_cast<std::size_t>(i)] =
          left_t + duration * (cumulative / total_travel);
    }
  }
  for (std::size_t i = 0; i < schedule.times.size() &&
                          i < linear_times.size();
       ++i) {
    schedule.max_time_shift_sec = std::max(
        schedule.max_time_shift_sec,
        std::abs(schedule.times[i] - linear_times[i]));
  }
  schedule.applied = schedule.max_time_shift_sec > 1e-9;
  return schedule;
}

}  // namespace bbsolver
