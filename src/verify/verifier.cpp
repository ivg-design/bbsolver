#include "bbsolver/verify/verifier.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "bbsolver/metrics/ae_curve.hpp"
#include "bbsolver/metrics/error_metrics.hpp"
#include "bbsolver/metrics/unified_spatial.hpp"

namespace bbsolver {
namespace {

double ComponentOrZero(const std::vector<double>& values, std::size_t idx) {
  return idx < values.size() ? values[idx] : 0.0;
}

TemporalEase EaseForDim(const std::vector<TemporalEase>& eases, int dim) {
  if (eases.empty()) {
    return {};
  }
  if (eases.size() == 1) {
    return eases.front();
  }
  return eases[std::min(static_cast<std::size_t>(std::max(dim, 0)), eases.size() - 1)];
}

std::size_t FindSegment(const std::vector<Key>& keys, double t) {
  for (std::size_t idx = 0; idx + 1 < keys.size(); ++idx) {
    if (t <= keys[idx + 1].t_sec) {
      return idx;
    }
  }
  return keys.size() - 2;
}

}  // namespace

std::vector<double> EvalKeysAt(const std::vector<Key>& keys, double t) {
  if (keys.empty()) {
    return {};
  }
  if (keys.size() == 1 || t <= keys.front().t_sec) {
    return keys.front().v;
  }
  if (t >= keys.back().t_sec) {
    return keys.back().v;
  }

  const std::size_t seg_idx = FindSegment(keys, t);
  const Key& left = keys[seg_idx];
  const Key& right = keys[seg_idx + 1];
  const int dimensions = static_cast<int>(std::max(left.v.size(), right.v.size()));
  std::vector<double> value(static_cast<std::size_t>(std::max(dimensions, 1)), 0.0);

  if (left.interp_out == InterpType::Hold || right.interp_in == InterpType::Hold) {
    return left.v;
  }

  const bool linear = left.interp_out == InterpType::Linear || right.interp_in == InterpType::Linear;
  const bool has_spatial = !left.spatial_out.empty() || !right.spatial_in.empty();
  if (!linear && has_spatial) {
    return EvalUnifiedSpatialBezier(t,
                                    left.t_sec,
                                    left.v,
                                    EaseForDim(left.temporal_ease_out, 0),
                                    left.spatial_out,
                                    right.t_sec,
                                    right.v,
                                    EaseForDim(right.temporal_ease_in, 0),
                                    right.spatial_in);
  }
  for (int d = 0; d < dimensions; ++d) {
    const double v0 = ComponentOrZero(left.v, static_cast<std::size_t>(d));
    const double v1 = ComponentOrZero(right.v, static_cast<std::size_t>(d));
    if (linear) {
      value[static_cast<std::size_t>(d)] = EvalLinear(t, left.t_sec, v0, right.t_sec, v1);
      continue;
    }

    const TemporalEase out_ease = EaseForDim(left.temporal_ease_out, d);
    const TemporalEase in_ease = EaseForDim(right.temporal_ease_in, d);
    value[static_cast<std::size_t>(d)] =
        EvalTemporalBezier(t, left.t_sec, v0, out_ease, right.t_sec, v1, in_ease);
  }

  return value;
}

ErrorReport ValidateKeys(const PropertySamples& ps,
                         const std::vector<Key>& keys,
                         const SolverConfig& cfg,
                         const CompInfo& comp) {
  return ComputeError(
      ps,
      0,
      static_cast<int>(ps.samples.size()) - 1,
      [&keys](double t) { return EvalKeysAt(keys, t); },
      cfg,
      comp,
      ps.layer_xform_at_start ? &*ps.layer_xform_at_start : nullptr);
}

}  // namespace bbsolver
