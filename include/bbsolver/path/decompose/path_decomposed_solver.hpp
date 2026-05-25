#pragma once

#include "bbsolver/domain.hpp"

#include <cstddef>

#include "bbsolver/app/cli_options.hpp"

namespace bbsolver {

class ProgressWriter;

PropertyKeys SolvePathDecomposedProperty(const PropertySamples& property_samples,
                                         const SolverConfig& config,
                                         const CompInfo& comp,
                                         const SolveOptions& options,
                                         const ProgressWriter& progress,
                                         std::size_t property_idx,
                                         std::size_t property_count);

}  // namespace bbsolver
