#pragma once

#include "bbsolver/domain.hpp"

#include <vector>

namespace bbsolver {

double SourceKeySampleTimeEpsilon(const PropertySamples& property_samples);

const Sample* FindSampleAtSourceKeyTime(const PropertySamples& property_samples,
                                        double t_sec);

PropertyKeys BuildShapeFlatSourceKeyPreservationKeys(
    const PropertySamples& property_samples,
    const std::vector<double>& source_key_times,
    int* preserved_timing_count_out);

}  // namespace bbsolver
