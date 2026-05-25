#pragma once

#include <vector>

#include "bbsolver/domain.hpp"

namespace bbsolver {

struct ShapeMotionSourceKeySchedule {
  std::vector<double> raw_times;
  std::vector<std::vector<double>> raw_values;
  std::vector<double> times;
  std::vector<std::vector<double>> values;
  int raw_count = 0;
  int simplified_count = 0;
  int redundant_removed = 0;
  bool simplification_enabled = false;
  double simplify_tolerance = 0.0;
};

// Ramer-Douglas-Peucker keep-mask helper. Marks `keep[i]` true for any
// interior index `i` in `(first, last)` whose perpendicular distance
// from the chord through `(first, last)` exceeds `tolerance`. Recurses
// into the two surviving sub-intervals. Public so the schedule builder
// can drive it; callers normally invoke through
// BuildShapeMotionSourceKeySchedule.
void ShapeMotionSourceKeyRdpKeep(
    const std::vector<double>& times,
    const std::vector<std::vector<double>>& values,
    int first,
    int last,
    double tolerance,
    std::vector<bool>* keep);

// Builds the source-key schedule for a Motion Smooth pass: samples each
// original key time off the raw trajectory, pins the anchor (first two
// dims) to the property's first sample, and — when more than two keys
// exist — runs an RDP simplification at `tolerance = clamp(strength*0.5,
// 0.75, 3.0)` to drop redundant interior keys.
ShapeMotionSourceKeySchedule BuildShapeMotionSourceKeySchedule(
    const PropertySamples& property_samples,
    const std::vector<double>& source_key_times,
    const std::vector<std::vector<double>>& raw,
    int dims,
    double strength);

}  // namespace bbsolver
