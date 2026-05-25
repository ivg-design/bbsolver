#include "bbsolver/metrics/ae_curve.hpp"
#include "bbsolver/domain.hpp"

#include <cmath>
#include <algorithm>

namespace bbsolver {
namespace {

constexpr double kMinInfluence = 0.1;
constexpr double kMaxInfluence = 100.0;
constexpr double kSolveTolerance = 1e-12;
constexpr int kNewtonIterations = 24;
constexpr int kBisectionIterations = 80;

double ClampInfluence(double influence) {
  if (!std::isfinite(influence)) {
    return 33.3;
  }
  return std::clamp(influence, kMinInfluence, kMaxInfluence);
}

double CubicBezier(double u, double p0, double p1, double p2, double p3) {
  const double omu = 1.0 - u;
  return omu * omu * omu * p0 +
         3.0 * omu * omu * u * p1 +
         3.0 * omu * u * u * p2 +
         u * u * u * p3;
}

double CubicBezierDerivative(double u, double p0, double p1, double p2, double p3) {
  const double omu = 1.0 - u;
  return 3.0 * omu * omu * (p1 - p0) +
         6.0 * omu * u * (p2 - p1) +
         3.0 * u * u * (p3 - p2);
}

struct TemporalHandles {
  double out_time;
  double in_time;
};

TemporalHandles BuildTemporalHandles(double t0,
                                      TemporalEase out_ease,
                                      double t1,
                                      TemporalEase in_ease) {
  const double duration = t1 - t0;
  const double out_influence = ClampInfluence(out_ease.influence) / 100.0;
  const double in_influence = ClampInfluence(in_ease.influence) / 100.0;
  return {
      t0 + out_influence * duration,
      t1 - in_influence * duration,
  };
}

}  // namespace

double SolveTemporalParam(double t,
                          double t0,
                          TemporalEase out_ease,
                          double t1,
                          TemporalEase in_ease) {
  if (!(t1 > t0)) {
    return 0.0;
  }
  if (t <= t0) {
    return 0.0;
  }
  if (t >= t1) {
    return 1.0;
  }

  const TemporalHandles handles = BuildTemporalHandles(t0, out_ease, t1, in_ease);
  const double u_linear = std::clamp((t - t0) / (t1 - t0), 0.0, 1.0);
  double u = u_linear;

  for (int i = 0; i < kNewtonIterations; ++i) {
    const double x = CubicBezier(u, t0, handles.out_time, handles.in_time, t1);
    const double error = x - t;
    if (std::abs(error) <= kSolveTolerance) {
      return std::clamp(u, 0.0, 1.0);
    }

    const double dx = CubicBezierDerivative(u, t0, handles.out_time, handles.in_time, t1);
    if (std::abs(dx) <= kSolveTolerance) {
      break;
    }

    const double next = u - error / dx;
    if (next < 0.0 || next > 1.0 || !std::isfinite(next)) {
      break;
    }
    u = next;
  }

  double lo = 0.0;
  double hi = 1.0;
  for (int i = 0; i < kBisectionIterations; ++i) {
    const double mid = 0.5 * (lo + hi);
    const double x = CubicBezier(mid, t0, handles.out_time, handles.in_time, t1);
    const double error = x - t;
    if (std::abs(error) <= kSolveTolerance) {
      return mid;
    }
    if (error < 0.0) {
      lo = mid;
    } else {
      hi = mid;
    }
  }

  return std::clamp(0.5 * (lo + hi), 0.0, 1.0);
}

double EvalTemporalBezier(double t,
                          double t0,
                          double v0,
                          TemporalEase out_ease,
                          double t1,
                          double v1,
                          TemporalEase in_ease) {
  if (!(t1 > t0)) {
    return v0;
  }

  const TemporalHandles time_handles = BuildTemporalHandles(t0, out_ease, t1, in_ease);
  const double out_value = v0 + out_ease.speed * (time_handles.out_time - t0);
  const double in_value = v1 - in_ease.speed * (t1 - time_handles.in_time);
  const double u = SolveTemporalParam(t, t0, out_ease, t1, in_ease);
  return CubicBezier(u, v0, out_value, in_value, v1);
}

double EvalSpatialBezierU(double u,
                          double v0,
                          double tan_out,
                          double v1,
                          double tan_in) {
  const double clamped_u = std::clamp(u, 0.0, 1.0);
  return CubicBezier(clamped_u, v0, v0 + tan_out, v1 + tan_in, v1);
}

}  // namespace bbsolver

#ifdef AE_CURVE_SELFTEST
int main() {
  const bbsolver::TemporalEase ease{0.0, 33.3};
  const double u = bbsolver::SolveTemporalParam(0.5, 0.0, ease, 1.0, ease);
  const double temporal = bbsolver::EvalTemporalBezier(0.5, 0.0, 0.0, ease, 1.0, 10.0, ease);
  const double spatial = bbsolver::EvalSpatialBezierU(0.5, 0.0, 0.0, 10.0, 0.0);
  std::cout << "u_mid=" << u << "\n";
  std::cout << "temporal_mid=" << temporal << "\n";
  std::cout << "spatial_mid=" << spatial << "\n";
  return 0;
}
#endif
