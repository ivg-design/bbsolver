#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"

namespace bbsolver {
namespace path_multimode {

SegmentFitResult FitLandmarkRegionShapeSegment(
    int i,
    int j,
    const PropertySamples& region_samples,
    const SolverConfig& config,
    const ShapeMorphProgressBandOptions& band_options,
    bool allow_relaxed_endpoints);

}  // namespace path_multimode
}  // namespace bbsolver
