#include "bbsolver/path/temporal/path_temporal_progress.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace bbsolver {
namespace {

double CubicScalar(double t, double p0, double p1, double p2, double p3) {
  const double omt = 1.0 - t;
  return omt * omt * omt * p0 +
         3.0 * omt * omt * t * p1 +
         3.0 * omt * t * t * p2 +
         t * t * t * p3;
}

}  // namespace

bool PathTemporalShapeFlatIsValid(const std::vector<double>& values) {
  if (values.size() < 2) {
    return false;
  }
  const int n = static_cast<int>(std::llround(values[1]));
  return n >= 1 && static_cast<int>(values.size()) == 2 + 6 * n;
}

std::vector<double> LerpShapeFlatChord(const std::vector<double>& a,
                                       const std::vector<double>& b,
                                       double u) {
  std::vector<double> out(a.size(), 0.0);
  if (a.empty() || a.size() != b.size()) {
    return out;
  }
  out[0] = a[0];
  if (a.size() > 1) {
    out[1] = a[1];
  }
  for (std::size_t idx = 2; idx < a.size(); ++idx) {
    out[idx] = a[idx] + (b[idx] - a[idx]) * u;
  }
  return out;
}

double ClampShapeTemporalInfluencePercent(double influence,
                                          double min_influence,
                                          double max_influence) {
  if (!std::isfinite(influence)) {
    return 33.3;
  }
  const double lo = std::max(0.1, std::min(min_influence, max_influence));
  const double hi = std::min(100.0, std::max(min_influence, max_influence));
  return std::clamp(influence, lo, std::max(lo, hi));
}

double ShapeTemporalBezierProgress(double alpha,
                                   double out_influence_percent,
                                   double in_influence_percent,
                                   double min_influence_percent,
                                   double max_influence_percent) {
  const double out_influence =
      ClampShapeTemporalInfluencePercent(out_influence_percent,
                                         min_influence_percent,
                                         max_influence_percent) /
      100.0;
  const double in_influence =
      ClampShapeTemporalInfluencePercent(in_influence_percent,
                                         min_influence_percent,
                                         max_influence_percent) /
      100.0;
  const double x1 = out_influence;
  const double x2 = 1.0 - in_influence;
  double lo = 0.0;
  double hi = 1.0;
  for (int iter = 0; iter < 40; ++iter) {
    const double mid = (lo + hi) * 0.5;
    const double x = CubicScalar(mid, 0.0, x1, x2, 1.0);
    if (x < alpha) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  const double t = (lo + hi) * 0.5;
  return CubicScalar(t, 0.0, 0.0, 1.0, 1.0);
}

double DefaultShapeTemporalBezierProgress(double alpha) {
  return ShapeTemporalBezierProgress(alpha, 33.3, 33.3);
}

int ProgressStepForLinear(double alpha, int progress_steps) {
  return std::clamp(
      static_cast<int>(std::llround(alpha * progress_steps)),
      0,
      progress_steps);
}

int ProgressStepForDefaultBezier(double alpha, int progress_steps) {
  return std::clamp(
      static_cast<int>(std::llround(DefaultShapeTemporalBezierProgress(alpha) *
                                    progress_steps)),
      0,
      progress_steps);
}

}  // namespace bbsolver
