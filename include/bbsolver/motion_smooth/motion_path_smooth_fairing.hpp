#pragma once

#include "bbsolver/domain.hpp"

#include <cstddef>
#include <vector>

namespace bbsolver {

inline constexpr double kMotionPathSmoothingDefault = 3.0;
inline constexpr double kMotionPathSmoothingMin = 1.0;
inline constexpr double kMotionPathSmoothingMax = 32.0;

struct MotionPathLocks {
  std::vector<bool> keyed;
  std::vector<bool> sharp;
  std::vector<bool> bounds;
};

struct MotionPathFairingResult {
  std::vector<std::vector<double>> points;
  int passes = 0;
  double alpha = 0.0;
  double max_displacement = 0.0;
  double source_path_length = 0.0;
  double smoothed_path_length = 0.0;
  bool bounds_preserved = false;
  double bounds_tolerance = 0.0;
  double source_bounds_width = 0.0;
  double source_bounds_height = 0.0;
  double smoothed_bounds_width = 0.0;
  double smoothed_bounds_height = 0.0;
  double bounds_max_deviation = 0.0;
};

MotionPathLocks BuildMotionPathLocks(
    const PropertySamples& property_samples,
    const std::vector<std::vector<double>>& raw,
    const SolverConfig& config,
    int dims);

bool IsMotionPathLockedAtIndex(const MotionPathLocks& locks, std::size_t idx);

MotionPathFairingResult FairMotionPathPoints(
    const std::vector<std::vector<double>>& raw,
    const MotionPathLocks& locks,
    double strength,
    bool preserve_bounds,
    double bounds_tolerance,
    int dims);

std::vector<int> MotionPathKeptIndices(
    const PropertySamples& property_samples,
    const std::vector<std::vector<double>>& smoothed,
    const MotionPathLocks& locks,
    double accuracy_tolerance,
    int dims);

int CountMotionPathLocks(const std::vector<bool>& locked);

}  // namespace bbsolver
