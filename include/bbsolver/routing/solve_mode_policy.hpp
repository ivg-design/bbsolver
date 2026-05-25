#pragma once

#include "bbsolver/domain.hpp"

#include <string>

namespace bbsolver {

struct SolverConfig;

std::string NormalizeSolveOptimizationMode(std::string mode);

bool SolveModeAllowsTemporal(const SolverConfig& config);

bool SolveModeAllowsVertex(const SolverConfig& config);

bool SolveModeAllowsSpatialTopology(const SolverConfig& config);

bool SolveModeIsMotionSmooth(const SolverConfig& config);

bool SolveModeIsMotionPathSmooth(const SolverConfig& config);

bool SolveModeUsesMotionSmoothing(const SolverConfig& config);

}  // namespace bbsolver
