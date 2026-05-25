#pragma once

#include "bbsolver/domain.hpp"

#include <string>

#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/metrics/error_metrics.hpp"

namespace bbsolver::segment_fit {

bool Passes(const ErrorReport& report, const SolverConfig& cfg);
double ShapeTemporalBezierGateRatio(const SolverConfig& cfg);
bool ShapeTemporalBezierGateAllows(const SegmentFitResult& linear_miss,
                                   const SolverConfig& cfg);
double AcceptancePropertyCeiling(const SolverConfig& cfg);
double AcceptanceScreenCeiling(const SolverConfig& cfg);
double LinearFailFastPropertyCeiling(const PropertySamples& ps,
                                     const SolverConfig& cfg);
double LinearFailFastScreenCeiling(const PropertySamples& ps,
                                   const SolverConfig& cfg);
double ScreenScaleForDim(const PropertySamples& ps, int dim);
double ResidualWeightForDim(const PropertySamples& ps,
                            const SolverConfig& cfg,
                            int dim);

// Diagnostics readiness: these helpers normalize SegmentFitResult fields and
// reason strings only. Command/orchestration code owns any diagnostics emission.
void AddFitAttribution(SegmentFitResult& dst, const SegmentFitResult& src);
SegmentFitResult WithFitAttribution(SegmentFitResult result,
                                    const SegmentFitResult& attribution);
void CopyError(SegmentFitResult& result, const ErrorReport& report);
void AddOrdinaryHoldAttribution(SegmentFitResult& attribution,
                                const ErrorReport& report);
void AddOrdinaryLinearAttribution(SegmentFitResult& attribution,
                                  const ErrorReport& report);
SegmentFitResult ResultFromReport(InterpType interp,
                                  const ErrorReport& report,
                                  const PropertySamples& ps,
                                  std::string reason);

}  // namespace bbsolver::segment_fit
