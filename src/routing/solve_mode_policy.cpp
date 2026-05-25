#include "bbsolver/routing/solve_mode_policy.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

namespace bbsolver {

std::string NormalizeSolveOptimizationMode(std::string mode) {
  std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) {
    if (c == '-') {
      return static_cast<char>('_');
    }
    return static_cast<char>(std::tolower(c));
  });
  if (mode.empty()) {
    mode = "full";
  }
  if (mode == "default" || mode == "auto") {
    mode = "full";
  }
  if (mode != "full" && mode != "temporal_only" &&
      mode != "vertex_only" && mode != "motion_smooth" &&
      mode != "motion_path_smooth") {
    throw std::runtime_error(
        "Invalid solve_optimization_mode '" + mode +
        "' (expected full, temporal_only, vertex_only, motion_smooth, "
        "or motion_path_smooth)");
  }
  return mode;
}

bool SolveModeAllowsTemporal(const SolverConfig& config) {
  const std::string mode =
      NormalizeSolveOptimizationMode(config.solve_optimization_mode);
  return mode == "full" || mode == "temporal_only";
}

bool SolveModeAllowsVertex(const SolverConfig& config) {
  const std::string mode =
      NormalizeSolveOptimizationMode(config.solve_optimization_mode);
  return mode == "full" || mode == "vertex_only";
}

bool SolveModeAllowsSpatialTopology(const SolverConfig& config) {
  return NormalizeSolveOptimizationMode(config.solve_optimization_mode) ==
         "full";
}

bool SolveModeIsMotionSmooth(const SolverConfig& config) {
  return NormalizeSolveOptimizationMode(config.solve_optimization_mode) ==
         "motion_smooth";
}

bool SolveModeIsMotionPathSmooth(const SolverConfig& config) {
  return NormalizeSolveOptimizationMode(config.solve_optimization_mode) ==
         "motion_path_smooth";
}

bool SolveModeUsesMotionSmoothing(const SolverConfig& config) {
  const std::string mode =
      NormalizeSolveOptimizationMode(config.solve_optimization_mode);
  return mode == "motion_smooth" || mode == "motion_path_smooth";
}

}  // namespace bbsolver
