#include "bbsolver/metrics/unified_spatial.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include "bbsolver/metrics/ae_curve.hpp"

namespace bbsolver {
namespace {

constexpr int kPathLutSteps = 256;

double ComponentOrZero(const std::vector<double>& values, std::size_t idx) {
  return idx < values.size() ? values[idx] : 0.0;
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

int Dimensions(const std::vector<double>& v0, const std::vector<double>& v1) {
  return static_cast<int>(std::max<std::size_t>(1, std::max(v0.size(), v1.size())));
}

std::vector<double> EvalPathAtU(double u,
                                const std::vector<double>& v0,
                                const std::vector<double>& spatial_out,
                                const std::vector<double>& v1,
                                const std::vector<double>& spatial_in) {
  const int dims = Dimensions(v0, v1);
  std::vector<double> out(static_cast<std::size_t>(dims), 0.0);
  const double cu = std::clamp(u, 0.0, 1.0);
  for (int d = 0; d < dims; ++d) {
    const double a = ComponentOrZero(v0, static_cast<std::size_t>(d));
    const double b = a + ComponentOrZero(spatial_out, static_cast<std::size_t>(d));
    const double e = ComponentOrZero(v1, static_cast<std::size_t>(d));
    const double c = e + ComponentOrZero(spatial_in, static_cast<std::size_t>(d));
    out[static_cast<std::size_t>(d)] = CubicBezier(cu, a, b, c, e);
  }
  return out;
}

double Distance(const std::vector<double>& a, const std::vector<double>& b) {
  const std::size_t n = std::max(a.size(), b.size());
  double sum = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    const double da = ComponentOrZero(a, i) - ComponentOrZero(b, i);
    sum += da * da;
  }
  return std::sqrt(sum);
}

std::vector<double> BuildArcLengthLut(const std::vector<double>& v0,
                                      const std::vector<double>& spatial_out,
                                      const std::vector<double>& v1,
                                      const std::vector<double>& spatial_in) {
  std::vector<double> length(static_cast<std::size_t>(kPathLutSteps + 1), 0.0);
  std::vector<double> prev = EvalPathAtU(0.0, v0, spatial_out, v1, spatial_in);
  double total = 0.0;
  for (int i = 1; i <= kPathLutSteps; ++i) {
    const double u = static_cast<double>(i) / static_cast<double>(kPathLutSteps);
    std::vector<double> cur = EvalPathAtU(u, v0, spatial_out, v1, spatial_in);
    total += Distance(prev, cur);
    length[static_cast<std::size_t>(i)] = total;
    prev = std::move(cur);
  }
  return length;
}

double InvertArcLength(const std::vector<double>& length, double distance) {
  if (length.empty()) {
    return 0.0;
  }
  const double total = length.back();
  if (!(total > 0.0)) {
    return 0.0;
  }
  const double target = std::clamp(distance, 0.0, total);
  auto it = std::lower_bound(length.begin(), length.end(), target);
  if (it == length.begin()) {
    return 0.0;
  }
  if (it == length.end()) {
    return 1.0;
  }
  const std::size_t idx = static_cast<std::size_t>(it - length.begin());
  const double prev = length[idx - 1];
  const double next = length[idx];
  const double local = (next > prev) ? (target - prev) / (next - prev) : 0.0;
  return (static_cast<double>(idx - 1) + local) / static_cast<double>(kPathLutSteps);
}

double EvalDistanceBezier(double t,
                          double t0,
                          TemporalEase out_ease,
                          double t1,
                          TemporalEase in_ease,
                          double path_length) {
  if (!(t1 > t0) || !(path_length > 0.0)) {
    return 0.0;
  }
  const double dt = t1 - t0;
  const double out_influence = std::clamp(out_ease.influence, 0.1, 100.0) / 100.0;
  const double in_influence = std::clamp(in_ease.influence, 0.1, 100.0) / 100.0;
  const double q = SolveTemporalParam(t, t0, out_ease, t1, in_ease);
  const double out_distance =
      std::clamp(std::max(0.0, out_ease.speed) * out_influence * dt, 0.0, path_length);
  const double in_distance =
      path_length -
      std::clamp(std::max(0.0, in_ease.speed) * in_influence * dt, 0.0, path_length);
  return CubicBezier(q, 0.0, out_distance, in_distance, path_length);
}

}  // namespace

std::vector<double> EvalUnifiedSpatialBezier(double t,
                                             double t0,
                                             const std::vector<double>& v0,
                                             TemporalEase out_ease,
                                             const std::vector<double>& spatial_out,
                                             double t1,
                                             const std::vector<double>& v1,
                                             TemporalEase in_ease,
                                             const std::vector<double>& spatial_in) {
  if (!(t1 > t0) || t <= t0) {
    return v0;
  }
  if (t >= t1) {
    return v1;
  }
  const std::vector<double> length = BuildArcLengthLut(v0, spatial_out, v1, spatial_in);
  const double path_length = length.empty() ? 0.0 : length.back();
  const double distance = EvalDistanceBezier(t, t0, out_ease, t1, in_ease, path_length);
  const double u = InvertArcLength(length, distance);
  return EvalPathAtU(u, v0, spatial_out, v1, spatial_in);
}

double ApproxUnifiedSpatialPathLength(const std::vector<double>& v0,
                                      const std::vector<double>& spatial_out,
                                      const std::vector<double>& v1,
                                      const std::vector<double>& spatial_in) {
  const std::vector<double> length = BuildArcLengthLut(v0, spatial_out, v1, spatial_in);
  return length.empty() ? 0.0 : length.back();
}

}  // namespace bbsolver
