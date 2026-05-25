#pragma once

#include "bbsolver/domain.hpp"

#include <algorithm>

namespace bbsolver {

double EvalTemporalBezier(double t,
                          double t0,
                          double v0,
                          TemporalEase out_ease,
                          double t1,
                          double v1,
                          TemporalEase in_ease);

double SolveTemporalParam(double t,
                          double t0,
                          TemporalEase out_ease,
                          double t1,
                          TemporalEase in_ease);

double EvalSpatialBezierU(double u,
                          double v0,
                          double tan_out,
                          double v1,
                          double tan_in);

inline double EvalLinear(double t, double t0, double v0, double t1, double v1) {
  if (t1 <= t0) {
    return v0;
  }
  const double u = std::clamp((t - t0) / (t1 - t0), 0.0, 1.0);
  return (1.0 - u) * v0 + u * v1;
}

inline double EvalHold(double, double, double v0, double, double) {
  return v0;
}

}  // namespace bbsolver
