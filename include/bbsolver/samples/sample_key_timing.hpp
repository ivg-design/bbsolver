#pragma once

#include "bbsolver/domain.hpp"

#include <vector>

namespace bbsolver {

std::vector<TemporalEase> DefaultEasesForProperty(
    const PropertySamples& property_samples);

bool ApplySampleKeyTiming(const PropertySamples& property_samples,
                          const Sample& sample,
                          Key& key);

}  // namespace bbsolver
