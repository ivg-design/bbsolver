#pragma once

#include <vector>

namespace bbsolver {

struct ShapeMotionRoveSchedule {
  std::vector<double> times;
  std::vector<std::vector<double>> values;
  int source_key_count = 0;
  int static_keys_removed = 0;
  bool rove_applied = false;
  double total_travel = 0.0;
  double max_segment_travel = 0.0;
  double max_control_step = 0.0;
  double max_time_shift_sec = 0.0;
};

// Build a "rove" schedule by removing interior static-duplicate keys
// (those whose control distance from the previous kept key is below
// `1e-7`) and — when `apply_rove == true` — retiming the surviving
// interior keys proportionally to accumulated chord travel between
// the kept endpoints. Endpoints (first/last index) are always kept
// and their times are preserved. Returns the schedule plus diagnostic
// counters (total/max-segment travel, max control step, max time
// shift) usable for note emission.
ShapeMotionRoveSchedule BuildShapeMotionRoveScheduleFromValues(
    const std::vector<double>& source_key_times,
    const std::vector<std::vector<double>>& source_values,
    int vertex_count,
    bool apply_rove = true);

}  // namespace bbsolver
