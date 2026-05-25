#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/dp/dp_placer.hpp"

namespace bbsolver {

DPPlacement RunForwardLongestSpanPlacement(
    const PropertySamples& ps,
    const SolverConfig& cfg,
    const CompInfo& comp,
    SegmentFitFn fit_fn,
    int max_gap_samples,
    CancelFn cancel_fn,
    PlacementProgressFn progress_fn);

}  // namespace bbsolver
