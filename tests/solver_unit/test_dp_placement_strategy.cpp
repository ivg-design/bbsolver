#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/domain.hpp"
#include "oneapi/tbb/global_control.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#ifdef BBSOLVER_HAVE_TBB
#include <tbb/global_control.h>
#include <algorithm>
#include <cstddef>
#endif

namespace {

bbsolver::PropertySamples MakeScalarSamples(int count) {
  bbsolver::PropertySamples ps;
  ps.property.id = "unit/synthetic-placement";
  ps.property.kind = bbsolver::ValueKind::Scalar;
  ps.property.dimensions = 1;
  ps.t_start_sec = 0.0;
  ps.t_end_sec = static_cast<double>(count - 1);
  ps.samples_per_frame = 1;
  for (int i = 0; i < count; ++i) {
    bbsolver::Sample sample;
    sample.t_sec = static_cast<double>(i);
    sample.v = {static_cast<double>(i)};
    ps.samples.push_back(sample);
  }
  return ps;
}

bbsolver::CompInfo Comp() {
  bbsolver::CompInfo comp;
  comp.fps = 24.0;
  return comp;
}

bbsolver::SegmentFitFn SyntheticGraphFit(
    const std::set<std::pair<int, int>>& feasible_edges) {
  return [feasible_edges](int i,
                          int j,
                          const bbsolver::PropertySamples&,
                          const bbsolver::SolverConfig&,
                          const bbsolver::CompInfo&) {
    bbsolver::SegmentFitResult result;
    result.feasible = feasible_edges.count({i, j}) != 0;
    result.interp = bbsolver::InterpType::Linear;
    result.max_err = result.feasible ? 0.0 : 100.0;
    result.max_err_screen_px = result.max_err;
    result.reason = result.feasible ? "synthetic_ok" : "synthetic_infeasible";
    return result;
  };
}

void RequireKeyTimes(const bbsolver::PropertyKeys& keys,
                     const std::vector<int>& expected_indices) {
  if (keys.keys.size() != expected_indices.size()) {
    std::cerr << "expected " << expected_indices.size() << " keys, got "
              << keys.keys.size() << "\n";
    std::abort();
  }
  for (std::size_t i = 0; i < expected_indices.size(); ++i) {
    const double expected = static_cast<double>(expected_indices[i]);
    if (std::abs(keys.keys[i].t_sec - expected) > 1e-9) {
      std::cerr << "key " << i << ": expected t=" << expected
                << ", got t=" << keys.keys[i].t_sec << "\n";
      std::abort();
    }
  }
}

void TestPlacementStrategyDefaultsToDP() {
  bbsolver::SolverConfig cfg;
  if (cfg.placement_strategy != "dp") {
    std::cerr << "default placement_strategy is not dp\n";
    std::abort();
  }

  const bbsolver::PropertySamples ps = MakeScalarSamples(6);
  const std::set<std::pair<int, int>> feasible_edges = {
      {0, 2},
      {0, 3},
      {2, 5},
      {3, 4},
      {4, 5},
  };

  const bbsolver::PropertyKeys keys = bbsolver::SolveProperty(
      ps, cfg, Comp(), SyntheticGraphFit(feasible_edges), {}, 5);

  if (!keys.converged) {
    std::cerr << "default DP placement did not converge\n";
    std::abort();
  }
  RequireKeyTimes(keys, {0, 2, 5});
  if (keys.segments.size() != 2) {
    std::cerr << "default DP placement expected 2 segments, got "
              << keys.segments.size() << "\n";
    std::abort();
  }
}

void TestForwardLongestSpanCanSelectDifferentAnchors() {
  bbsolver::SolverConfig cfg;
  cfg.placement_strategy = "forward_longest_span";

  const bbsolver::PropertySamples ps = MakeScalarSamples(6);
  const std::set<std::pair<int, int>> feasible_edges = {
      {0, 2},
      {0, 3},
      {2, 5},
      {3, 4},
      {4, 5},
  };

  const bbsolver::PropertyKeys keys = bbsolver::SolveProperty(
      ps, cfg, Comp(), SyntheticGraphFit(feasible_edges), {}, 5);

  if (!keys.converged) {
    std::cerr << "forward-longest-span placement did not converge\n";
    std::abort();
  }
  RequireKeyTimes(keys, {0, 3, 4, 5});
  if (keys.segments.size() != 3) {
    std::cerr << "forward-longest-span placement expected 3 segments, got "
              << keys.segments.size() << "\n";
    std::abort();
  }
  if (keys.notes.find("forward_longest_span_placement") == std::string::npos) {
    std::cerr << "missing forward-longest-span placement note\n";
    std::abort();
  }
}

void RequireAttributedDPProgressAggregates(int max_parallelism) {
#ifdef BBSOLVER_HAVE_TBB
  tbb::global_control control(
      tbb::global_control::max_allowed_parallelism,
      static_cast<std::size_t>(std::max(1, max_parallelism)));
#else
  (void)max_parallelism;
#endif
  bbsolver::SolverConfig cfg;
  const bbsolver::PropertySamples ps = MakeScalarSamples(4);
  std::vector<bbsolver::PlacementProgress> progress_events;

  const bbsolver::SegmentFitFn attributed_fit =
      [](int i,
         int j,
         const bbsolver::PropertySamples&,
         const bbsolver::SolverConfig&,
         const bbsolver::CompInfo&) {
        bbsolver::SegmentFitResult result;
        result.feasible = true;
        result.interp = bbsolver::InterpType::Linear;
        result.reason = "synthetic_attributed_ok";
        result.fit_segment_hold_attempts = 1;
        result.fit_segment_linear_attempts = j - i;
        result.fit_segment_hold_units_evaluated = 10;
        result.fit_segment_linear_units_evaluated = 20;
        result.fit_segment_hold_fail_fast_exits = 1;
        result.fit_segment_linear_fail_fast_exits = 1;
        result.fit_shape_temporal_attempts = (i == 0) ? 1 : 0;
        result.fit_shape_temporal_gate_rejections = (j == 3) ? 1 : 0;
        result.fit_shape_temporal_outline_evaluations = (j - i) * 2;
        result.fit_segment_hold_wall_ms = 0.25;
        result.fit_segment_linear_wall_ms = 0.5 * static_cast<double>(j - i);
        result.fit_segment_hold_shape_outline_wall_ms = 0.125;
        result.fit_segment_linear_shape_outline_wall_ms = 0.25;
        result.fit_shape_temporal_ceres_wall_ms = (i == 0) ? 0.75 : 0.0;
        result.fit_shape_temporal_outline_wall_ms =
            static_cast<double>((j - i) * 2);
        result.fit_shape_temporal_total_wall_ms =
            result.fit_shape_temporal_ceres_wall_ms +
            result.fit_shape_temporal_outline_wall_ms;
        return result;
      };

  const bbsolver::DPPlacement placement = bbsolver::RunDPPlacement(
      ps,
      cfg,
      Comp(),
      attributed_fit,
      3,
      {},
      [&](const bbsolver::PlacementProgress& progress) {
        progress_events.push_back(progress);
      });

  if (!placement.converged) {
    std::cerr << "attributed DP placement did not converge\n";
    std::abort();
  }
  if (progress_events.empty()) {
    std::cerr << "attributed DP placement emitted no progress\n";
    std::abort();
  }

  const bbsolver::PlacementProgress& final = progress_events.back();
  if (final.fit_segment_hold_attempts != 6 ||
      final.fit_segment_linear_attempts != 10 ||
      final.fit_segment_hold_units_evaluated != 60 ||
      final.fit_segment_linear_units_evaluated != 120 ||
      final.fit_segment_hold_fail_fast_exits != 6 ||
      final.fit_segment_linear_fail_fast_exits != 6 ||
      final.fit_shape_temporal_attempts != 3 ||
      final.fit_shape_temporal_gate_rejections != 3 ||
      final.fit_shape_temporal_outline_evaluations != 20) {
    std::cerr << "unexpected attributed DP counts\n";
    std::abort();
  }
  if (std::abs(final.fit_segment_hold_wall_ms - 1.5) > 1e-12 ||
      std::abs(final.fit_segment_linear_wall_ms - 5.0) > 1e-12 ||
      std::abs(final.fit_segment_hold_shape_outline_wall_ms - 0.75) > 1e-12 ||
      std::abs(final.fit_segment_linear_shape_outline_wall_ms - 1.5) > 1e-12 ||
      std::abs(final.fit_shape_temporal_ceres_wall_ms - 2.25) > 1e-12 ||
      std::abs(final.fit_shape_temporal_outline_wall_ms - 20.0) > 1e-12 ||
      std::abs(final.fit_shape_temporal_total_wall_ms - 22.25) > 1e-12) {
      std::cerr << "unexpected attributed DP timings\n";
    std::abort();
  }
}

void TestDPProgressAggregatesSegmentFitAttribution() {
  RequireAttributedDPProgressAggregates(1);
  RequireAttributedDPProgressAggregates(4);
}

}  // namespace

int main() {
  TestPlacementStrategyDefaultsToDP();
  TestForwardLongestSpanCanSelectDifferentAnchors();
  TestDPProgressAggregatesSegmentFitAttribution();
  return 0;
}
