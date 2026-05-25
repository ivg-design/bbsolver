#pragma once

#include "bbsolver/domain.hpp"

#include <functional>
#include <vector>

#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/metrics/error_metrics.hpp"

namespace bbsolver::segment_fit {

double ShapeFlatProjectedProgress(const PropertySamples& ps,
                                  int i,
                                  int j,
                                  int sample_idx);
double EvalShapeFlatTemporalProgress(double t,
                                     double t0,
                                     TemporalEase out_ease,
                                     double t1,
                                     TemporalEase in_ease);
std::vector<double> ReconstructShapeFlatTemporalBezier(
    const PropertySamples& ps,
    int i,
    int j,
    TemporalEase ease_out,
    TemporalEase ease_in,
    double t);
std::vector<double> ReconstructShapeFlatKeyBezier(const PropertySamples& ps,
                                                  int i,
                                                  int j,
                                                  TemporalEase ease_out,
                                                  TemporalEase ease_in,
                                                  double t);
ErrorReport ComputeShapeFlatOutlineError(
    const PropertySamples& ps,
    int i,
    int j,
    const std::function<std::vector<double>(double t)>& reconstruct,
    const SolverConfig& cfg,
    int* evaluations = nullptr,
    double* outline_wall_ms = nullptr);

SegmentFitResult TryShapeFlatTemporalBezier(int i,
                                            int j,
                                            const PropertySamples& ps,
                                            const SolverConfig& cfg,
                                            const CompInfo& comp);

}  // namespace bbsolver::segment_fit
