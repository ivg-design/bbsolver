#include "bbsolver/path/temporal/path_temporal_band_helpers.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ratio>
#include <vector>
#include <cstddef>
#include <utility>

#include "bbsolver/path/temporal/path_temporal_influence.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/path/temporal/path_temporal_progress.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"

namespace bbsolver {

std::vector<ShapeMorphProgressInterval> BuildShapeMorphProgressIntervals(
    const std::vector<bool>& accepted,
    int progress_steps) {
  std::vector<ShapeMorphProgressInterval> intervals;
  int start = -1;
  for (int idx = 0; idx <= static_cast<int>(accepted.size()); ++idx) {
    const bool ok =
        idx < static_cast<int>(accepted.size()) &&
        accepted[static_cast<std::size_t>(idx)];
    if (ok && start < 0) {
      start = idx;
    } else if (!ok && start >= 0) {
      ShapeMorphProgressInterval interval;
      interval.first_step = start;
      interval.last_step = idx - 1;
      interval.min_u = static_cast<double>(interval.first_step) /
                       static_cast<double>(progress_steps);
      interval.max_u = static_cast<double>(interval.last_step) /
                       static_cast<double>(progress_steps);
      intervals.push_back(interval);
      start = -1;
    }
  }
  return intervals;
}

bool ShapeMorphHasMonotoneProgressPath(
    const std::vector<std::vector<bool>>& accepted_rows) {
  if (accepted_rows.empty()) {
    return false;
  }
  const int grid_count = static_cast<int>(accepted_rows.front().size());
  std::vector<bool> reachable(static_cast<std::size_t>(grid_count), false);
  reachable[0] = accepted_rows.front().front();
  for (std::size_t row_idx = 1; row_idx < accepted_rows.size(); ++row_idx) {
    std::vector<bool> next(static_cast<std::size_t>(grid_count), false);
    bool prefix_reachable = false;
    for (int step = 0; step < grid_count; ++step) {
      prefix_reachable =
          prefix_reachable || reachable[static_cast<std::size_t>(step)];
      next[static_cast<std::size_t>(step)] =
          prefix_reachable &&
          accepted_rows[row_idx][static_cast<std::size_t>(step)];
    }
    reachable = std::move(next);
  }
  return reachable.back();
}

double EvaluateShapeTemporalInfluencePairMaxError(
    const PropertySamples& original,
    int start_sample_idx,
    int end_sample_idx,
    const std::vector<ShapeFlatOutlinePolyline>& source_outlines,
    const std::vector<double>& endpoint_a,
    const std::vector<double>& endpoint_b,
    const ShapeMorphProgressBandOptions& options,
    ShapeTemporalInfluencePair pair,
    double cutoff_error,
    int* evaluations,
    double* outline_error_wall_ms) {
  double max_error = 0.0;
  const double t0 =
      original.samples[static_cast<std::size_t>(start_sample_idx)].t_sec;
  const double t1 =
      original.samples[static_cast<std::size_t>(end_sample_idx)].t_sec;
  for (int sample_idx = start_sample_idx; sample_idx <= end_sample_idx; ++sample_idx) {
    const Sample& sample = original.samples[static_cast<std::size_t>(sample_idx)];
    const double alpha = (t1 > t0) ? (sample.t_sec - t0) / (t1 - t0) : 0.0;
    const double u = ShapeTemporalBezierProgress(alpha,
                                                 pair.out_influence,
                                                 pair.in_influence,
                                                 options.min_bezier_influence,
                                                 options.max_bezier_influence);
    const std::vector<double> candidate =
        LerpShapeFlatChord(endpoint_a, endpoint_b, u);
    const ShapeFlatOutlinePolyline candidate_outline =
        BuildShapeFlatOutlinePolyline(candidate, options.frame_fit_options);
    const auto outline_start = std::chrono::steady_clock::now();
    const double err = ShapeFlatFrameOutlineErrorFromPolylines(
        source_outlines[static_cast<std::size_t>(sample_idx - start_sample_idx)],
        candidate_outline);
    const auto outline_end = std::chrono::steady_clock::now();
    if (outline_error_wall_ms) {
      *outline_error_wall_ms +=
          std::chrono::duration<double, std::milli>(
              outline_end - outline_start).count();
    }
    if (evaluations) {
      ++(*evaluations);
    }
    max_error = std::max(max_error, err);
    if (std::isfinite(cutoff_error) && max_error >= cutoff_error) {
      break;
    }
  }
  return max_error;
}

}  // namespace bbsolver
