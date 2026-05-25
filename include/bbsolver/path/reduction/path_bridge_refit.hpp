#pragma once

#include <vector>

namespace bbsolver {

std::vector<double> BridgeRefitRemoveShapeFlatVertex(
    const std::vector<double>& flat,
    int removed_index);

bool ShapeFlatHasDuplicateTerminalClosure(const std::vector<double>& flat,
                                          double tolerance);

std::vector<double> DropShapeFlatDuplicateTerminalClosure(
    const std::vector<double>& flat);

}  // namespace bbsolver
