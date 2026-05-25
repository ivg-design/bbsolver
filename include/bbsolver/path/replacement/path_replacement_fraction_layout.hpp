#pragma once

#include <vector>

namespace bbsolver {

struct ReplacementFrameFitRecord {
  std::vector<double> fitted;
  std::vector<double> outline_fractions;
  double max_outline_error = 0.0;
  double t_sec = 0.0;
};

std::vector<double> BuildMedianStableFractionLayout(
    const std::vector<ReplacementFrameFitRecord>& records,
    int target_vertices);

}  // namespace bbsolver
