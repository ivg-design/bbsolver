#pragma once

#include "bbsolver/domain.hpp"

namespace bbsolver {

struct CompInfo;
struct SolverConfig;

int InteractivePathMaxGap(const CompInfo& comp);

int PathSpecificMaxGap(const CompInfo& comp,
                       const SolverConfig& config);

}  // namespace bbsolver
