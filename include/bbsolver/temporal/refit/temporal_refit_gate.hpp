#pragma once

#include "bbsolver/domain.hpp"

namespace bbsolver {

bool PipelineAllowsTemporalRefit(const PropertySamples& property_samples,
                                 const PropertyKeys& keys,
                                 const SolverConfig& config);

}  // namespace bbsolver
