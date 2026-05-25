#pragma once

#include "bbsolver/domain.hpp"

#include <cstddef>
#include <vector>

namespace bbsolver {

bool TemporalRefitIsCustomProperty(const PropertySamples& source);

std::size_t TemporalRefitExpectedDimensions(const PropertySamples& source);

bool TemporalRefitValuesMatchDimensions(const std::vector<double>& values,
                                        const PropertySamples& source);

bool AllTemporalRefitCandidateKeysMatchDimensions(
    const PropertyKeys& keys,
    const PropertySamples& source);

}  // namespace bbsolver
