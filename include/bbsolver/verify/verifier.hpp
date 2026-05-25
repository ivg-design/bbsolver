#pragma once

#include "bbsolver/domain.hpp"

#include <vector>

#include "bbsolver/metrics/error_metrics.hpp"

namespace bbsolver {

std::vector<double> EvalKeysAt(const std::vector<Key>& keys, double t);

ErrorReport ValidateKeys(const PropertySamples& ps,
                         const std::vector<Key>& keys,
                         const SolverConfig& cfg,
                         const CompInfo& comp);

}  // namespace bbsolver
