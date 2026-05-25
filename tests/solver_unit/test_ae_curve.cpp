#include "bbsolver/metrics/ae_curve.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/metrics/unified_spatial.hpp"

#include <cassert>
#include <vector>

namespace {

double CubicBezier(double u, double p0, double p1, double p2, double p3) {
  const double omu = 1.0 - u;
  return omu * omu * omu * p0 +
         3.0 * omu * omu * u * p1 +
         3.0 * omu * u * u * p2 +
         u * u * u * p3;
}

}  // namespace

int main() {
  const bbsolver::TemporalEase ease{0.0, 33.3};
  const double u = bbsolver::SolveTemporalParam(0.5, 0.0, ease, 1.0, ease);
  assert(std::abs(u - 0.5) < 1e-9);

  const double v = bbsolver::EvalTemporalBezier(0.5, 0.0, 0.0, ease, 1.0, 10.0, ease);
  assert(std::abs(v - 5.0) < 1e-9);

  const bbsolver::TemporalEase out_ease{12.0, 25.0};
  const bbsolver::TemporalEase in_ease{-3.0, 70.0};
  const double t0 = 2.0;
  const double t1 = 5.0;
  for (double t = t0; t <= t1; t += 0.125) {
    const double solved = bbsolver::SolveTemporalParam(t, t0, out_ease, t1, in_ease);
    const double h_out = t0 + (out_ease.influence / 100.0) * (t1 - t0);
    const double h_in = t1 - (in_ease.influence / 100.0) * (t1 - t0);
    const double round_trip = CubicBezier(solved, t0, h_out, h_in, t1);
    assert(std::abs(round_trip - t) < 1e-9);
  }

  double prev = bbsolver::EvalTemporalBezier(0.0, 0.0, 0.0, ease, 1.0, 10.0, ease);
  for (int i = 1; i <= 100; ++i) {
    const double t = static_cast<double>(i) / 100.0;
    const double cur = bbsolver::EvalTemporalBezier(t, 0.0, 0.0, ease, 1.0, 10.0, ease);
    assert(cur + 1e-12 >= prev);
    prev = cur;
  }

  const double spatial_mid = bbsolver::EvalSpatialBezierU(0.5, 2.0, 0.0, 8.0, 0.0);
  assert(std::abs(spatial_mid - 5.0) < 1e-12);

  const std::vector<double> p0{0.0, 0.0};
  const std::vector<double> p1{100.0, 0.0};
  const std::vector<double> zero_tan{0.0, 0.0};
  const bbsolver::TemporalEase slow{0.0, 50.0};
  const bbsolver::TemporalEase fast{500.0, 50.0};
  const std::vector<double> slow_pos =
      bbsolver::EvalUnifiedSpatialBezier(0.1, 0.0, p0, slow, zero_tan, 1.0, p1, slow, zero_tan);
  const std::vector<double> fast_pos =
      bbsolver::EvalUnifiedSpatialBezier(0.1, 0.0, p0, fast, zero_tan, 1.0, p1, slow, zero_tan);
  assert(fast_pos[0] > slow_pos[0] + 1.0);

  assert(std::abs(bbsolver::EvalLinear(0.25, 0.0, 10.0, 1.0, 20.0) - 12.5) < 1e-12);
  assert(std::abs(bbsolver::EvalHold(0.75, 0.0, 10.0, 1.0, 20.0) - 10.0) < 1e-12);
  return 0;
}
