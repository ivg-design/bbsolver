#pragma once

#include "bbsolver/domain.hpp"

#include <vector>

namespace bbsolver {

std::vector<double> SampleVectorOrZeros(
    const PropertySamples& property_samples,
    const Sample& sample);

}  // namespace bbsolver
