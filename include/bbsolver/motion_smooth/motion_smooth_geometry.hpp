#pragma once

#include "bbsolver/domain.hpp"

#include <cstddef>
#include <vector>

namespace bbsolver {

double MotionSmoothComponentOrZero(const std::vector<double>& values,
                                   std::size_t idx);

double MotionPointDistanceSq(const std::vector<double>& a,
                             const std::vector<double>& b,
                             int dims);

double MotionPointDistance(const std::vector<double>& a,
                           const std::vector<double>& b,
                           int dims);

std::vector<std::vector<double>> MotionSmoothFilteredPoints(
    const PropertySamples& property_samples,
    double strength,
    int dims,
    int* passes_out,
    double* max_displacement_out);

std::vector<int> MotionSmoothKeptPointIndices(
    const std::vector<std::vector<double>>& points,
    double tolerance,
    int dims);

std::vector<double> MotionSmoothScaledVector(const std::vector<double>& v,
                                             double scale,
                                             int dims);

std::vector<double> MotionSmoothDelta(const std::vector<double>& a,
                                      const std::vector<double>& b,
                                      int dims);

std::vector<double> MotionSmoothClampTangent(const std::vector<double>& tangent,
                                             double max_len,
                                             int dims);

std::vector<double> MotionSmoothInterpolatedPoint(
    const PropertySamples& property_samples,
    const std::vector<std::vector<double>>& points,
    double t_sec,
    int dims);

}  // namespace bbsolver
