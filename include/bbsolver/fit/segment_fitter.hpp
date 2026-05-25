#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/dp/dp_placer.hpp"

namespace bbsolver {

SegmentFitResult FitSegment(int i,
                            int j,
                            const PropertySamples& ps,
                            const SolverConfig& cfg,
                            const CompInfo& comp);

}  // namespace bbsolver
