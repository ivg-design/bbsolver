#pragma once

#include "bbsolver/motion_smooth/motion_path_smooth_fairing.hpp"

#include <vector>

namespace bbsolver {

struct MotionPathBounds {
  std::vector<double> min, max;
};

MotionPathBounds ComputeMotionPathBounds(
    const std::vector<std::vector<double>>& points,
    int dims);

double MotionPathBoundsSpan(const MotionPathBounds& bounds, int dim);

double MotionPathBoundsSideDeviation(const MotionPathBounds& source,
                                     const MotionPathBounds& candidate,
                                     int dims);

void MarkMotionPathBoundsExtrema(
    const std::vector<std::vector<double>>& points,
    int dims,
    std::vector<bool>* keep);

void ApplyMotionPathBoundsConstraint(
    const std::vector<std::vector<double>>& raw,
    const MotionPathLocks& locks,
    const MotionPathBounds& source_bounds,
    double bounds_tolerance,
    int dims,
    std::vector<std::vector<double>>* smoothed);

}  // namespace bbsolver
