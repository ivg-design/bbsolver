#pragma once

#include "bbsolver/domain.hpp"

#include <utility>
#include <vector>

#include "bbsolver/dp/dp_placer.hpp"

namespace bbsolver::segment_fit {

struct PathProjectionLut {
  std::vector<std::vector<double>> points;
  std::vector<double> length;
};

std::vector<double> EvalPathPoint(double u,
                                  const std::vector<double>& v0,
                                  const std::vector<double>& spatial_out,
                                  const std::vector<double>& v1,
                                  const std::vector<double>& spatial_in);
double VectorDistanceSquared(const std::vector<double>& a,
                             const std::vector<double>& b);
PathProjectionLut BuildPathProjectionLut(
    const std::vector<double>& v0,
    const std::vector<double>& spatial_out,
    const std::vector<double>& v1,
    const std::vector<double>& spatial_in);
std::vector<std::pair<double, double>> ProjectSegmentSamplesToPathDistances(
    const PropertySamples& ps,
    int i,
    int j,
    const std::vector<double>& spatial_out,
    const std::vector<double>& spatial_in);
double EaseHandleDistance(TemporalEase ease, double dt, double path_length);
double SpeedFromHandleDistance(double handle_distance,
                               double influence,
                               double dt);

SegmentFitResult FitUnifiedSpatialTiming(int i,
                                         int j,
                                         const PropertySamples& ps,
                                         const SolverConfig& cfg,
                                         const CompInfo& comp,
                                         SegmentFitResult result);

}  // namespace bbsolver::segment_fit
