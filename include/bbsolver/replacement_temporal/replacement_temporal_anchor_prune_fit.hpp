#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/path/temporal/path_temporal_validation.hpp"
#include "bbsolver/dp/dp_placer.hpp"

#include <vector>

namespace bbsolver {
namespace replacement_temporal {

bool SampleValuesEqualWithin(const std::vector<double>& a,
                             const std::vector<double>& b,
                             double epsilon);

SegmentFitResult FitFallbackLinearPruneSegment(
    int i,
    int j,
    const std::vector<double>& current_anchor_value,
    const PropertySamples& reduced,
    const PropertySamples& original,
    const ShapeMorphProgressBandOptions& band_options);

}  // namespace replacement_temporal
}  // namespace bbsolver
