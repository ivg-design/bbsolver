#pragma once

#include <vector>

namespace bbsolver {

struct SourcePoseIntervalTimeSchedule {
  std::vector<double> times;
  bool applied = false;
  double total_travel = 0.0;
  double max_time_shift_sec = 0.0;
};

// Map closed-loop continuous parameters to wall-clock times by linearly
// interpolating between the source anchor times' segments. Each `param`
// in `[0, unique_count]` lands at the proportional point of the
// segment whose floor index it falls into.
std::vector<double> TimesForClosedLoopParams(
    const std::vector<double>& source_key_times,
    const std::vector<double>& params);

// Refine the linear-time mapping of TimesForClosedLoopParams by
// reshaping each anchor-bounded interval so interior samples are
// spaced proportionally to their accumulated chord-distance travel
// (rather than uniformly in parameter space). The result includes the
// schedule itself plus diagnostics (applied flag, max time shift)
// suitable for note emission.
SourcePoseIntervalTimeSchedule TimesForClosedLoopParamsByIntervalTravel(
    const std::vector<double>& anchor_times,
    const std::vector<double>& params,
    const std::vector<std::vector<double>>& values,
    int vertex_count);

}  // namespace bbsolver
