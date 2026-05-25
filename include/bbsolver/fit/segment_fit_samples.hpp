#pragma once

#include "bbsolver/domain.hpp"

#include <chrono>
#include <cstddef>
#include <vector>

#include <ceres/ceres.h>

namespace bbsolver::segment_fit {

inline constexpr double kDefaultInfluence = 33.3;
inline constexpr double kAcceptanceHeadroom = 0.99;
inline constexpr int kMaxCeresDimensions = 16;

int Dimensions(const PropertySamples& ps);
int TemporalChannels(const PropertySamples& ps);
bool IsUnifiedSpatial(const PropertySamples& ps);
bool IsShapeFlatPath(const PropertySamples& ps);

double ComponentOrZero(const std::vector<double>& values, std::size_t idx);
double SampleValue(const PropertySamples& ps, int sample_idx, int dim);
std::vector<double> SampleVector(const PropertySamples& ps, int sample_idx);
double SampleTime(const PropertySamples& ps, int sample_idx);
double EstimateSlope(const PropertySamples& ps, int a, int b, int dim);
double SampleDistance(const PropertySamples& ps, int a, int b);
double EndpointSpatialSpeed(const PropertySamples& ps, int i, int j, bool out_ease);
double EndpointSlopeOut(const PropertySamples& ps, int i, int j, int dim);
double EndpointSlopeIn(const PropertySamples& ps, int i, int j, int dim);
double ClampInfluence(double value, const SolverConfig& cfg);
std::vector<TemporalEase> DefaultEases(int count);

ceres::Solver::Options CeresOptions(const SolverConfig& cfg);
bool UseFastUnifiedSpatialOnly(const PropertySamples& ps);
double ElapsedMs(std::chrono::steady_clock::time_point start,
                 std::chrono::steady_clock::time_point end);

}  // namespace bbsolver::segment_fit
