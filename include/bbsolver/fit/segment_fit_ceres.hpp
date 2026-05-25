#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/dp/dp_placer.hpp"

namespace bbsolver::segment_fit {

struct DimCeresResult {
  TemporalEase ease_out;
  TemporalEase ease_in;
  double spatial_out = 0.0;
  double spatial_in = 0.0;
  int iters = 0;
};

// Diagnostics readiness: Ceres adapters return SegmentFitResult decisions only;
// callers decide how those reasons are surfaced to notes or diagnostics.
DimCeresResult RunSingleDimCeres(int dim,
                                 int i,
                                 int j,
                                 const PropertySamples& ps,
                                 const SolverConfig& cfg,
                                 const SegmentFitResult& seed);
SegmentFitResult TrySeparatedCeresBezier(int i,
                                         int j,
                                         const PropertySamples& ps,
                                         const SolverConfig& cfg,
                                         const CompInfo& comp,
                                         const SegmentFitResult& seed);
SegmentFitResult TryHermiteBezier(int i,
                                  int j,
                                  const PropertySamples& ps,
                                  const SolverConfig& cfg,
                                  const CompInfo& comp);
SegmentFitResult TryCeresBezier(int i,
                                int j,
                                const PropertySamples& ps,
                                const SolverConfig& cfg,
                                const CompInfo& comp,
                                const SegmentFitResult& seed);

}  // namespace bbsolver::segment_fit
