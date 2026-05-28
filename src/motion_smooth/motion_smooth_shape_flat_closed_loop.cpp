#include "bbsolver/motion_smooth/motion_smooth_shape_flat_closed_loop.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "bbsolver/domain.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_loop.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_loop_adaptive.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_loop_schedule.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_quality.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_trajectory_smooth.hpp"

namespace bbsolver {

ClosedLoopAdaptiveResampleResult BuildShapeFlatClosedLoopAdaptiveResample(
    const ShapeMotionTrajectorySmoothResult& smooth_result,
    const std::vector<double>& key_times,
    const SolverConfig& config,
    int vertex_count,
    int dims,
    double strength,
    bool closed_loop) {
  ClosedLoopAdaptiveResampleResult result;
  result.schedule_times = key_times;
  result.schedule_values = config.motion_smooth_source_fidelity
      ? smooth_result.original_values
: smooth_result.smoothed_values;

  if (closed_loop) {
    const std::vector<std::vector<double>>& loop_values =
        config.motion_smooth_source_fidelity
            ? smooth_result.original_values
: smooth_result.smoothed_values;
    result.adaptive_loop = BuildAdaptiveClosedLoopShapeSamples(
        loop_values,
        dims,
        vertex_count,
        strength,
        config.motion_smooth_source_fidelity);
    result.adaptive_loop_resample = true;
    result.schedule_values = result.adaptive_loop.values;
    result.loop_subdivisions = std::max(
        1,
        static_cast<int>(
            std::llround(static_cast<double>(
                             std::max<std::size_t>(
                                 1, result.schedule_values.size() - 1)) /
                         static_cast<double>(
                             std::max<std::size_t>(
                                 1, loop_values.size() - 1)))));
    if (config.motion_smooth_source_fidelity) {
      result.source_pose_interval_schedule =
          TimesForClosedLoopParamsByIntervalTravel(
              key_times,
              result.adaptive_loop.params,
              result.schedule_values,
              vertex_count);
      result.schedule_times = result.source_pose_interval_schedule.times;
    } else {
      result.schedule_times = EvenTimesForValueCount(
          key_times.front(),
          key_times.back(),
          result.schedule_values.size());
    }
    if (config.motion_smooth_source_fidelity) {
      result.source_pose_constraint_indices.assign(
          result.schedule_values.size(), false);
      for (std::size_t i = 0;
           i < result.adaptive_loop.params.size() &&
           i < result.source_pose_constraint_indices.size();
           ++i) {
        const double nearest = std::round(result.adaptive_loop.params[i]);
        if (std::abs(result.adaptive_loop.params[i] - nearest) <= 1e-9) {
          result.source_pose_constraint_indices[i] = true;
        }
      }
    }
    result.trajectory_turn_after_override = result.adaptive_loop.quality.valid
        ? result.adaptive_loop.quality.max_turn_deg
: ShapeFlatClosedDuplicateMaxTurnDeg(result.schedule_values, dims);
    result.trajectory_turn_after_overridden = true;
  } else if (config.motion_smooth_source_fidelity) {
    result.source_pose_constraint_indices.assign(
        result.schedule_values.size(), true);
  }
  return result;
}

}  // namespace bbsolver
