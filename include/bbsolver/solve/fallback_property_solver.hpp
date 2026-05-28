#pragma once

#ifndef BBSOLVER_HAVE_DP_PLACER

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/domain.hpp"

#if defined(_MSC_VER)
#define BBSOLVER_WEAK_FALLBACK_SOLVE_PROPERTY
#else
#define BBSOLVER_WEAK_FALLBACK_SOLVE_PROPERTY __attribute__((weak))
#endif

namespace bbsolver {
namespace fallback_property_solver {

struct FittedSegment {
  int i = 0;
  int j = 0;
  SegmentFitResult fit;
};

inline int FallbackDimensions(const PropertySamples& ps) {
  return std::max(ps.property.dimensions, 1);
}

inline std::vector<double> FallbackSampleVector(const PropertySamples& ps,
                                                int sample_idx) {
  std::vector<double> values(
      static_cast<std::size_t>(FallbackDimensions(ps)), 0.0);
  if (sample_idx < 0 || sample_idx >= static_cast<int>(ps.samples.size())) {
    return values;
  }
  const auto& source = ps.samples[static_cast<std::size_t>(sample_idx)].v;
  const std::size_t count = std::min(values.size(), source.size());
  std::copy_n(source.begin(), count, values.begin());
  return values;
}

inline std::vector<TemporalEase> FallbackDefaultEases(
    const PropertySamples& ps) {
  const int count = ps.property.is_separated ? FallbackDimensions(ps): 1;
  return std::vector<TemporalEase>(
      static_cast<std::size_t>(std::max(count, 1)), TemporalEase{0.0, 33.3});
}

inline int ChooseFallbackSplit(const PropertySamples& ps, int i, int j) {
  if (j <= i + 1) {
    return i + 1;
  }
  const double t0 = ps.samples[static_cast<std::size_t>(i)].t_sec;
  const double t1 = ps.samples[static_cast<std::size_t>(j)].t_sec;
  const std::vector<double> v0 = FallbackSampleVector(ps, i);
  const std::vector<double> v1 = FallbackSampleVector(ps, j);
  int best_idx = i + (j - i) / 2;
  double best_err = -1.0;

  for (int k = i + 1; k < j; ++k) {
    const double tk = ps.samples[static_cast<std::size_t>(k)].t_sec;
    const double u =
        (t1 > t0) ? std::clamp((tk - t0) / (t1 - t0), 0.0, 1.0): 0.0;
    const std::vector<double> vk = FallbackSampleVector(ps, k);
    double err = 0.0;
    for (int d = 0; d < FallbackDimensions(ps); ++d) {
      const double linear =
          (1.0 - u) * v0[static_cast<std::size_t>(d)] +
          u * v1[static_cast<std::size_t>(d)];
      const double hold = v0[static_cast<std::size_t>(d)];
      err = std::max(
          err, std::min(std::abs(vk[static_cast<std::size_t>(d)] - linear),
                        std::abs(vk[static_cast<std::size_t>(d)] - hold)));
    }
    if (err > best_err) {
      best_err = err;
      best_idx = k;
    }
  }

  return std::clamp(best_idx, i + 1, j - 1);
}

inline void FallbackSolveRange(int i,
                               int j,
                               const PropertySamples& ps,
                               const SolverConfig& cfg,
                               const CompInfo& comp,
                               const SegmentFitFn& fit_fn,
                               std::vector<FittedSegment>& segments,
                               int depth = 0) {
  SegmentFitResult fit = fit_fn(i, j, ps, cfg, comp);
  if (fit.feasible || j <= i + 1 || depth > 32) {
    if (!fit.feasible && j <= i + 1) {
      SolverConfig relaxed = cfg;
      relaxed.allow_hold = false;
      relaxed.allow_linear = true;
      relaxed.allow_bezier = false;
      relaxed.tolerance = std::numeric_limits<double>::infinity();
      fit = fit_fn(i, j, ps, relaxed, comp);
    }
    segments.push_back({i, j, std::move(fit)});
    return;
  }

  const int split = ChooseFallbackSplit(ps, i, j);
  FallbackSolveRange(i, split, ps, cfg, comp, fit_fn, segments, depth + 1);
  FallbackSolveRange(split, j, ps, cfg, comp, fit_fn, segments, depth + 1);
}

inline void ApplyIncomingKeyData(Key& key, const FittedSegment& segment) {
  key.interp_in = segment.fit.interp;
  key.temporal_ease_in = segment.fit.ease_in_at_j;
  key.spatial_in = segment.fit.spatial_in_at_j;
}

inline void ApplyOutgoingKeyData(Key& key, const FittedSegment& segment) {
  key.interp_out = segment.fit.interp;
  key.temporal_ease_out = segment.fit.ease_out_at_i;
  key.spatial_out = segment.fit.spatial_out_at_i;
}

inline SegmentReport ReportForSegment(const FittedSegment& segment) {
  SegmentReport report;
  report.start_idx = segment.i;
  report.end_idx = segment.j;
  report.max_err = segment.fit.max_err;
  report.max_err_screen_px = segment.fit.max_err_screen_px;
  report.rms_err = segment.fit.rms_err;
  report.iters = segment.fit.iters;
  report.reason = segment.fit.reason;
  return report;
}

}  // namespace fallback_property_solver

BBSOLVER_WEAK_FALLBACK_SOLVE_PROPERTY PropertyKeys SolveProperty(
    const PropertySamples& ps,
    const SolverConfig& cfg,
    const CompInfo& comp,
    SegmentFitFn fit_fn,
    CancelFn cancel_fn,
    int,
    PlacementProgressFn);

}  // namespace bbsolver

#undef BBSOLVER_WEAK_FALLBACK_SOLVE_PROPERTY

#endif  // BBSOLVER_HAVE_DP_PLACER
