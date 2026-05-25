#pragma once

#include <vector>

#include "bbsolver/path/replacement/path_replacement_fraction_layout.hpp"

namespace bbsolver {

std::vector<int> SelectReplacementPhase2SeedIndices(
    const std::vector<ReplacementFrameFitRecord>& records,
    int target_vertices);

}  // namespace bbsolver
