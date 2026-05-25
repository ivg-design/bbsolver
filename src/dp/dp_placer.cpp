// bbsolver dynamic-programming key placer.
//
// Implements RunDPPlacement / SolveProperty / SolveAll declared in dp_placer.hpp.
//
// Strategy (per docs/solver/DP_ALGORITHM.md):
//
//   dp[j] = min number of SEGMENTS to cover samples [0..j], where a single segment
//           (i -> j) is feasible iff fit_fn(i, j, ...) returns SegmentFitResult.feasible.
//   prev[j] = i for the arg-min.
//   Backtrack from j = N-1 to reconstruct the anchor set.
//   Assemble Key list by stitching the in/out ease & tangents of adjacent segments
//   at each shared anchor.

#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/dp/dp_forward_placement.hpp"
#include "bbsolver/dp/dp_key_assembly.hpp"
#include "bbsolver/dp/dp_placement_limits.hpp"
#include "bbsolver/dp/dp_placement_progress.hpp"
#include "oneapi/tbb/parallel_for.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <limits>
#include <string>
#include <ratio>
#include <utility>
#include <vector>

#ifdef BBSOLVER_HAVE_TBB
#include <tbb/parallel_for.h>
#include <cstddef>
#include <functional>
#endif

namespace bbsolver {

namespace {

constexpr int kInfSegments = std::numeric_limits<int>::max() / 4;

struct CandidateFit {
  int i = -1;
  bool tried = false;
  SegmentFitResult result;
};

}  // namespace

// ---- RunDPPlacement ---------------------------------------------------------

DPPlacement RunDPPlacement(
    const PropertySamples& ps,
    const SolverConfig& cfg,
    const CompInfo& comp,
    SegmentFitFn fit_fn,
    int max_gap_samples,
    CancelFn cancel_fn,
    PlacementProgressFn progress_fn) {
  DPPlacement out;
  const auto is_cancelled = [&cancel_fn]() {
    return cancel_fn && cancel_fn();
  };
  if (is_cancelled()) {
    out.converged = false;
    out.notes = "cancelled";
    return out;
  }

  const int N = static_cast<int>(ps.samples.size());
  if (N < 2) {
    out.converged = (N == 1);
    if (N == 1) out.sample_indices = {0};
    return out;
  }
  if (!fit_fn) {
    out.notes = "no fit_fn provided";
    return out;
  }
  EmitPlacementProgress(progress_fn,
                        "dp_start",
                        0,
                        N - 1,
                        0,
                        N,
                        out.total_segments_tried,
                        out.total_segments_feasible);

  // Fully-constant short-circuit: if every sample's value equals
  // samples[0]'s value within cfg.tolerance (per-dim L∞), emit a single
  // anchor at sample 0. AE holds a single-key property forever.
  // This bypasses the DP entirely for truly constant data — the most common
  // shape for "always-100% opacity" and "rest-pose" rig channels.
  if (cfg.allow_hold) {
    const auto& v0 = ps.samples[0].v;
    bool constant = true;
    for (int k = 1; k < N && constant; ++k) {
      const auto& vk = ps.samples[k].v;
      const std::size_t dims = std::min(v0.size(), vk.size());
      for (std::size_t d = 0; d < dims; ++d) {
        if (std::abs(vk[d] - v0[d]) > cfg.tolerance) {
          constant = false;
          break;
        }
      }
      if (v0.size() != vk.size()) constant = false;
    }
    if (constant) {
      out.converged = true;
      out.sample_indices = {0};
      out.notes = "constant_short_circuit";
      return out;
    }
  }

  int G = (max_gap_samples > 0)
              ? std::min(max_gap_samples, N - 1)
              : AutoMaxGap(comp, N);
  const bool unified_spatial_large =
      ps.property.is_spatial && !ps.property.is_separated && N > 360;
  if (max_gap_samples <= 0 && unified_spatial_large) {
    const int fps_third =
        static_cast<int>(std::round(std::max(1.0, comp.fps) / 3.0));
    const int interactive_gap = std::max(6, std::min(24, fps_third));
    G = std::min(G, interactive_gap);
  }
  const int progress_stride = std::max(1, (N - 1) / 40);

  std::vector<int> dp(N, kInfSegments);
  std::vector<double> dp_max_err(N, std::numeric_limits<double>::infinity());
  std::vector<int> prev_idx(N, -1);
  std::vector<std::vector<double>> dp_anchor_value(static_cast<std::size_t>(N));
  // For each j, store the winning (i, SegmentFitResult).
  std::vector<std::pair<int, SegmentFitResult>> winner(N);
  for (auto& w : winner) w.first = -1;

  dp[0] = 0;
  dp_max_err[0] = 0.0;
  const int dims = std::max(1, ps.property.dimensions);
  dp_anchor_value[0] = SampleValueAt(ps, 0, dims);
  DPPlacementInstrumentation instrumentation;
  const bool capture_dp_instrumentation = static_cast<bool>(progress_fn);

  for (int j = 1; j < N; ++j) {
    if (is_cancelled()) {
      out.converged = false;
      out.notes = "cancelled";
      return out;
    }

    // At the final anchor (j == N-1) we widen the search window to the full
    // range. This lets a single segment span the whole clip when feasible,
    // which is the optimal answer for Hold-dominant fixtures whose length
    // exceeds the auto max-gap cap.
    const bool widen_final_anchor =
        max_gap_samples <= 0 && j == N - 1 && !unified_spatial_large;
    const int i_lo = widen_final_anchor ? 0 : std::max(0, j - G);
    const int candidate_count = j - i_lo;
    std::vector<CandidateFit> candidates(static_cast<std::size_t>(candidate_count));
    std::atomic<bool> cancelled{false};
    std::chrono::steady_clock::time_point fit_start;
    if (capture_dp_instrumentation) {
      fit_start = std::chrono::steady_clock::now();
    }

    const auto fit_candidate = [&](int offset) {
      if (cancelled.load(std::memory_order_relaxed)) return;
      if (is_cancelled()) {
        cancelled.store(true, std::memory_order_relaxed);
        return;
      }
      const int i = i_lo + offset;
      if (dp[i] >= kInfSegments) return;
      CandidateFit candidate;
      candidate.i = i;
      candidate.tried = true;
      candidate.result = fit_fn(i, j, ps, cfg, comp);
      candidates[static_cast<std::size_t>(offset)] = std::move(candidate);
    };

#ifdef BBSOLVER_HAVE_TBB
    tbb::parallel_for(0, candidate_count, fit_candidate);
#else
    for (int offset = 0; offset < candidate_count; ++offset) {
      fit_candidate(offset);
    }
#endif
    std::chrono::steady_clock::time_point fit_end;
    if (capture_dp_instrumentation) {
      fit_end = std::chrono::steady_clock::now();
    }

    if (cancelled.load(std::memory_order_relaxed)) {
      out.converged = false;
      out.notes = "cancelled";
      return out;
    }

    std::chrono::steady_clock::time_point reduction_start;
    if (capture_dp_instrumentation) {
      reduction_start = std::chrono::steady_clock::now();
    }
    int unreachable_candidates_this_anchor = 0;
    int incompatible_candidates_this_anchor = 0;
    for (auto& candidate : candidates) {
      if (!candidate.tried) {
        ++unreachable_candidates_this_anchor;
        continue;
      }
      if (capture_dp_instrumentation) {
        AddSegmentFitAttribution(instrumentation, candidate.result);
      }
      out.total_segments_tried++;
      if (candidate.result.feasible) {
        out.total_segments_feasible++;

        const int i = candidate.i;
        const std::vector<double> candidate_start =
            SegmentStartValue(ps, candidate.result, i, dims);
        if (i > 0 &&
            !ValuesCompatible(dp_anchor_value[static_cast<std::size_t>(i)],
                              candidate_start)) {
          ++incompatible_candidates_this_anchor;
          continue;
        }
        const int segment_count = dp[i] + 1;
        const double max_err = std::max(dp_max_err[static_cast<std::size_t>(i)],
                                        candidate.result.max_err);
        const bool lower_k = segment_count < dp[j];
        const bool tighter_same_k =
            segment_count == dp[j] &&
            max_err + 1e-12 < dp_max_err[static_cast<std::size_t>(j)];
        const bool same_error_prefer_closer =
            segment_count == dp[j] &&
            std::abs(max_err - dp_max_err[static_cast<std::size_t>(j)]) <= 1e-12 &&
            i > prev_idx[j];
        if (lower_k || tighter_same_k || same_error_prefer_closer) {
          dp[j] = segment_count;
          dp_max_err[static_cast<std::size_t>(j)] = max_err;
          prev_idx[j] = i;
          dp_anchor_value[static_cast<std::size_t>(j)] =
              SegmentEndValue(ps, candidate.result, j, dims);
          winner[j] = {i, std::move(candidate.result)};
        }
      }
    }
    std::chrono::steady_clock::time_point reduction_end;
    if (capture_dp_instrumentation) {
      reduction_end = std::chrono::steady_clock::now();
    }
    if (capture_dp_instrumentation) {
      const double fit_wall_ms =
          std::chrono::duration<double, std::milli>(fit_end - fit_start).count();
      const double reduction_wall_ms =
          std::chrono::duration<double, std::milli>(
              reduction_end - reduction_start).count();
      instrumentation.candidate_slots += candidate_count;
      instrumentation.unreachable_candidates +=
          unreachable_candidates_this_anchor;
      instrumentation.incompatible_candidates +=
          incompatible_candidates_this_anchor;
      instrumentation.fit_wall_ms += fit_wall_ms;
      instrumentation.reduction_wall_ms += reduction_wall_ms;
      if (widen_final_anchor) {
        instrumentation.final_anchor_candidate_slots += candidate_count;
        instrumentation.final_anchor_fit_wall_ms += fit_wall_ms;
        instrumentation.final_anchor_reduction_wall_ms += reduction_wall_ms;
      }
    }
    if (j == 1 || j == N - 1 || (j % progress_stride) == 0) {
      EmitPlacementProgress(progress_fn,
                            "dp_anchor",
                            j,
                            N - 1,
                            j,
                            N,
                            out.total_segments_tried,
                            out.total_segments_feasible,
                            &instrumentation);
    }
  }

  if (dp[N - 1] >= kInfSegments) {
    // No feasible segmentation under cfg.tolerance even at max-gap.
    // Fallback: every sample is an anchor, linear segments between them. This
    // satisfies tolerance trivially (sample-to-sample is exact at endpoints),
    // but balloons K. Caller should consider relaxing tolerance.
    out.converged = false;
    out.notes = "no feasible segmentation under tolerance=" +
                std::to_string(cfg.tolerance) + " with max_gap_samples=" +
                std::to_string(G) + "; falling back to all-samples-as-anchors";
    EmitPlacementProgress(progress_fn,
                          "dp_fallback_all_samples",
                          N - 1,
                          N - 1,
                          N - 1,
                          N,
                          out.total_segments_tried,
                          out.total_segments_feasible,
                          &instrumentation);
    out.sample_indices.reserve(N);
    for (int k = 0; k < N; ++k) out.sample_indices.push_back(k);
    out.segments.reserve(N - 1);
    for (int s = 0; s < N - 1; ++s) {
      SegmentFitResult linear;
      linear.feasible = true;
      linear.interp = InterpType::Linear;
      linear.reason = "fallback_linear_anchor";
      linear.max_err = 0.0;
      out.segments.push_back(std::move(linear));
    }
    return out;
  }

  // Backtrack from N-1 to 0.
  std::vector<int> anchors;
  std::vector<SegmentFitResult> segs;
  int j = N - 1;
  while (j > 0) {
    anchors.push_back(j);
    segs.push_back(std::move(winner[j].second));
    j = prev_idx[j];
    if (j < 0) {
      out.converged = false;
      out.notes = "DP backtrack broken (prev_idx invalid)";
      return out;
    }
  }
  anchors.push_back(0);
  std::reverse(anchors.begin(), anchors.end());
  std::reverse(segs.begin(), segs.end());

  out.sample_indices = std::move(anchors);
  out.segments = std::move(segs);
  out.converged = true;
  for (const auto& s : out.segments) {
    if (s.max_err > out.max_err) out.max_err = s.max_err;
    if (s.max_err_screen_px > out.max_err_screen_px)
      out.max_err_screen_px = s.max_err_screen_px;
  }
  EmitPlacementProgress(progress_fn,
                        "dp_done",
                        N - 1,
                        N - 1,
                        N - 1,
                        N,
                        out.total_segments_tried,
                        out.total_segments_feasible,
                        &instrumentation);
  return out;
}

// ---- SolveProperty ----------------------------------------------------------

PropertyKeys SolveProperty(
    const PropertySamples& ps,
    const SolverConfig& cfg,
    const CompInfo& comp,
    SegmentFitFn fit_fn,
    CancelFn cancel_fn,
    int max_gap_samples,
    PlacementProgressFn progress_fn) {
  DPPlacement pl;
  if (cfg.placement_strategy == "forward_longest_span") {
    pl = RunForwardLongestSpanPlacement(
        ps,
        cfg,
        comp,
        std::move(fit_fn),
        max_gap_samples,
        std::move(cancel_fn),
        std::move(progress_fn));
  } else {
    pl = RunDPPlacement(
        ps,
        cfg,
        comp,
        std::move(fit_fn),
        max_gap_samples,
        std::move(cancel_fn),
        std::move(progress_fn));
  }
  PropertyKeys keys = AssembleKeys(ps, pl);
  return keys;
}

// ---- SolveAll ---------------------------------------------------------------
//
// v1: serial fan-out. The expected parallelism in this product is per-segment
// (inside SegmentFitter), not per-property — most users will solve a handful of
// properties at a time, and parallelizing here would race the per-property
// loggers. This loop can move to TBB parallel_for once fit_fn is proven
// thread-safe across all configurations.
std::vector<PropertyKeys> SolveAll(
    const std::vector<PropertySamples>& properties,
    const SolverConfig& cfg,
    const CompInfo& comp,
    const std::function<SegmentFitFn(const PropertySamples&)>& fit_factory) {
  std::vector<PropertyKeys> results;
  results.reserve(properties.size());
  for (const auto& ps : properties) {
    SegmentFitFn fit_fn = fit_factory(ps);
    results.push_back(SolveProperty(ps, cfg, comp, std::move(fit_fn)));
  }
  return results;
}

}  // namespace bbsolver
