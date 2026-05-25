#include "bbsolver/fit/segment_fit_bezier.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "bbsolver/metrics/ae_curve.hpp"
#include "bbsolver/fit/segment_fit_samples.hpp"
#include "bbsolver/metrics/unified_spatial.hpp"

namespace bbsolver::segment_fit {

std::vector<TemporalEase> HermiteEase(const PropertySamples& ps,
                                      int i,
                                      int j,
                                      bool out_ease,
                                      const SolverConfig& cfg) {
  std::vector<TemporalEase> eases;
  const int channels = TemporalChannels(ps);
  eases.reserve(static_cast<std::size_t>(channels));
  for (int c = 0; c < channels; ++c) {
    if (IsUnifiedSpatial(ps)) {
      eases.push_back({EndpointSpatialSpeed(ps, i, j, out_ease),
                       ClampInfluence(kDefaultInfluence, cfg)});
    } else {
      const int dim = ps.property.is_separated ? c : 0;
      const double slope = out_ease ? EndpointSlopeOut(ps, i, j, dim)
                                    : EndpointSlopeIn(ps, i, j, dim);
      eases.push_back({slope, ClampInfluence(kDefaultInfluence, cfg)});
    }
  }
  return eases;
}

TemporalEase EaseForDim(const std::vector<TemporalEase>& eases, int dim) {
  if (eases.empty()) {
    return {};
  }
  if (eases.size() == 1) {
    return eases.front();
  }
  return eases[std::min(static_cast<std::size_t>(std::max(dim, 0)),
                        eases.size() - 1)];
}

std::vector<double> HermiteSpatialTangents(const PropertySamples& ps,
                                           int i,
                                           int j,
                                           bool out_tangent,
                                           const SolverConfig& cfg) {
  std::vector<double> tangents(static_cast<std::size_t>(Dimensions(ps)), 0.0);
  const double dt = SampleTime(ps, j) - SampleTime(ps, i);
  const double influence = ClampInfluence(kDefaultInfluence, cfg) / 100.0;
  for (int d = 0; d < Dimensions(ps); ++d) {
    const double slope = out_tangent ? EndpointSlopeOut(ps, i, j, d)
                                     : EndpointSlopeIn(ps, i, j, d);
    tangents[static_cast<std::size_t>(d)] =
        (out_tangent ? 1.0 : -1.0) * slope * influence * dt;
  }
  return tangents;
}

std::vector<double> ReconstructBezier(
    const PropertySamples& ps,
    int i,
    int j,
    const std::vector<TemporalEase>& ease_out,
    const std::vector<TemporalEase>& ease_in,
    const std::vector<double>& spatial_out,
    const std::vector<double>& spatial_in,
    double t) {
  std::vector<double> values(static_cast<std::size_t>(Dimensions(ps)), 0.0);
  const double t0 = SampleTime(ps, i);
  const double t1 = SampleTime(ps, j);
  if (IsUnifiedSpatial(ps)) {
    return EvalUnifiedSpatialBezier(t,
                                    t0,
                                    SampleVector(ps, i),
                                    EaseForDim(ease_out, 0),
                                    spatial_out,
                                    t1,
                                    SampleVector(ps, j),
                                    EaseForDim(ease_in, 0),
                                    spatial_in);
  }
  for (int d = 0; d < Dimensions(ps); ++d) {
    const double v0 = SampleValue(ps, i, d);
    const double v1 = SampleValue(ps, j, d);
    const TemporalEase out = EaseForDim(ease_out, d);
    const TemporalEase in = EaseForDim(ease_in, d);
    if (ps.property.is_spatial) {
      const double u = SolveTemporalParam(t, t0, out, t1, in);
      values[static_cast<std::size_t>(d)] =
          EvalSpatialBezierU(u,
                             v0,
                             ComponentOrZero(spatial_out,
                                             static_cast<std::size_t>(d)),
                             v1,
                             ComponentOrZero(spatial_in,
                                             static_cast<std::size_t>(d)));
    } else {
      values[static_cast<std::size_t>(d)] =
          EvalTemporalBezier(t, t0, v0, out, t1, v1, in);
    }
  }
  return values;
}

}  // namespace bbsolver::segment_fit
