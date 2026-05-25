#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"
#include "bbsolver/replacement_temporal/replacement_temporal_relaxed_fit.hpp"  // IWYU pragma: export

#include <string>

namespace bbsolver {
namespace replacement_temporal {

SegmentFitResult InfeasibleSegment(std::string reason);

SegmentFitResult FitReplacementSegment(
    int i,
    int j,
    const PropertySamples& reduced,
    const PropertySamples& original,
    const SolverConfig& config,
    const ShapeMorphProgressBandOptions& band_options,
    bool allow_relaxed_endpoint_fit);

}  // namespace replacement_temporal
}  // namespace bbsolver
