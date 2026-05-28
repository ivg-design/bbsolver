#pragma once

#include "bbsolver/domain.hpp"

#include <vector>

namespace bbsolver::segment_fit {

std::vector<TemporalEase> HermiteEase(const PropertySamples& ps,
                                      int i,
                                      int j,
                                      bool out_ease,
                                      const SolverConfig& cfg);
TemporalEase EaseForDim(const std::vector<TemporalEase>& eases, int dim);
std::vector<double> HermiteSpatialTangents(const PropertySamples& ps,
                                           int i,
                                           int j,
                                           bool out_tangent,
                                           const SolverConfig& cfg);
std::vector<double> ReconstructBezier(
    const PropertySamples& ps,
    int i,
    int j,
    const std::vector<TemporalEase>& ease_out,
    const std::vector<TemporalEase>& ease_in,
    const std::vector<double>& spatial_out,
    const std::vector<double>& spatial_in,
    double t);

template <typename T>
T CubicBezierT(const T& u, const T& p0, const T& p1, const T& p2, const T& p3) {
  const T one = T(1.0);
  const T omu = one - u;
  return omu * omu * omu * p0 +
         T(3.0) * omu * omu * u * p1 +
         T(3.0) * omu * u * u * p2 +
         u * u * u * p3;
}

template <typename T>
T CubicBezierDerivativeT(const T& u,
                         const T& p0,
                         const T& p1,
                         const T& p2,
                         const T& p3) {
  const T one = T(1.0);
  const T omu = one - u;
  return T(3.0) * omu * omu * (p1 - p0) +
         T(6.0) * omu * u * (p2 - p1) +
         T(3.0) * u * u * (p3 - p2);
}

template <typename T>
T SolveTemporalParamT(const T& t,
                      double t0,
                      const T& out_influence,
                      double t1,
                      const T& in_influence) {
  if (!(t1 > t0)) {
    return T(0.0);
  }
  T u = (t - T(t0)) / T(t1 - t0);
  const T out_time = T(t0) + (out_influence / T(100.0)) * T(t1 - t0);
  const T in_time = T(t1) - (in_influence / T(100.0)) * T(t1 - t0);
  for (int iter = 0; iter < 10; ++iter) {
    const T x = CubicBezierT(u, T(t0), out_time, in_time, T(t1));
    const T dx = CubicBezierDerivativeT(u, T(t0), out_time, in_time, T(t1));
    const T safe_dx = dx + (dx < T(0.0) ? T(-1e-9): T(1e-9));
    u = u - (x - t) / safe_dx;
    if (u < T(0.0)) {
      u = T(0.0);
    } else if (u > T(1.0)) {
      u = T(1.0);
    }
  }
  return u;
}

}  // namespace bbsolver::segment_fit
