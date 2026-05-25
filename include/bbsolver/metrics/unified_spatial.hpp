#pragma once

#include "bbsolver/domain.hpp"

#include <vector>

namespace bbsolver {

std::vector<double> EvalUnifiedSpatialBezier(double t,
                                             double t0,
                                             const std::vector<double>& v0,
                                             TemporalEase out_ease,
                                             const std::vector<double>& spatial_out,
                                             double t1,
                                             const std::vector<double>& v1,
                                             TemporalEase in_ease,
                                             const std::vector<double>& spatial_in);

double ApproxUnifiedSpatialPathLength(const std::vector<double>& v0,
                                      const std::vector<double>& spatial_out,
                                      const std::vector<double>& v1,
                                      const std::vector<double>& spatial_in);

}  // namespace bbsolver
