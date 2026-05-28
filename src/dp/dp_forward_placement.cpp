#include "bbsolver/dp/dp_forward_placement.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/dp/dp_key_assembly.hpp"
#include "bbsolver/dp/dp_placement_limits.hpp"
#include "bbsolver/dp/dp_placement_progress.hpp"
#include "bbsolver/dp/dp_placer.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>
#include <cstddef>

namespace bbsolver {

DPPlacement RunForwardLongestSpanPlacement(
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

  const int sample_count = static_cast<int>(ps.samples.size());
  if (sample_count < 2) {
    out.converged = (sample_count == 1);
    if (sample_count == 1) {
      out.sample_indices = {0};
    }
    return out;
  }
  if (!fit_fn) {
    out.notes = "no fit_fn provided";
    return out;
  }
  EmitPlacementProgress(progress_fn,
                        "forward_start",
                        0,
                        sample_count - 1,
                        0,
                        sample_count,
                        out.total_segments_tried,
                        out.total_segments_feasible);

  if (cfg.allow_hold) {
    const auto& v0 = ps.samples[0].v;
    bool constant = true;
    for (int sample_idx = 1; sample_idx < sample_count && constant;
         ++sample_idx) {
      const auto& vk = ps.samples[static_cast<std::size_t>(sample_idx)].v;
      const std::size_t dims = std::min(v0.size(), vk.size());
      for (std::size_t dim = 0; dim < dims; ++dim) {
        if (std::abs(vk[dim] - v0[dim]) > cfg.tolerance) {
          constant = false;
          break;
        }
      }
      if (v0.size() != vk.size()) {
        constant = false;
      }
    }
    if (constant) {
      out.converged = true;
      out.sample_indices = {0};
      out.notes = "constant_short_circuit";
      return out;
    }
  }

  int max_gap = (max_gap_samples > 0)
                    ? std::min(max_gap_samples, sample_count - 1)
: AutoMaxGap(comp, sample_count);
  const bool unified_spatial_large =
      ps.property.is_spatial && !ps.property.is_separated && sample_count > 360;
  if (max_gap_samples <= 0 && unified_spatial_large) {
    const int fps_third =
        static_cast<int>(std::round(std::max(1.0, comp.fps) / 3.0));
    const int interactive_gap = std::max(6, std::min(24, fps_third));
    max_gap = std::min(max_gap, interactive_gap);
  }

  const int dims = std::max(1, ps.property.dimensions);
  std::vector<int> anchors;
  std::vector<SegmentFitResult> segments;
  anchors.reserve(sample_count);
  segments.reserve(sample_count - 1);
  anchors.push_back(0);

  int current = 0;
  std::vector<double> current_anchor_value = SampleValueAt(ps, current, dims);
  while (current < sample_count - 1) {
    if (is_cancelled()) {
      out.converged = false;
      out.notes = "cancelled";
      return out;
    }

    const bool widen_final_anchor =
        max_gap_samples <= 0 && !unified_spatial_large;
    const int j_hi = widen_final_anchor
        ? sample_count - 1
: std::min(sample_count - 1, current + max_gap);
    int accepted_j = -1;
    SegmentFitResult accepted_segment;

    for (int j = j_hi; j > current; --j) {
      if (is_cancelled()) {
        out.converged = false;
        out.notes = "cancelled";
        return out;
      }
      SegmentFitResult candidate = fit_fn(current, j, ps, cfg, comp);
      out.total_segments_tried++;
      if (!candidate.feasible) {
        continue;
      }
      out.total_segments_feasible++;
      const std::vector<double> candidate_start =
          SegmentStartValue(ps, candidate, current, dims);
      if (!ValuesCompatible(current_anchor_value, candidate_start)) {
        continue;
      }
      accepted_j = j;
      accepted_segment = std::move(candidate);
      break;
    }

    if (accepted_j < 0) {
      out.converged = false;
      out.notes = "forward_longest_span_no_feasible_segment; tolerance=" +
                  std::to_string(cfg.tolerance) + " max_gap_samples=" +
                  std::to_string(max_gap) +
                  "; falling back to all-samples-as-anchors";
      EmitPlacementProgress(progress_fn,
                            "forward_fallback_all_samples",
                            current,
                            sample_count - 1,
                            current,
                            sample_count,
                            out.total_segments_tried,
                            out.total_segments_feasible);
      out.sample_indices.reserve(sample_count);
      for (int sample_idx = 0; sample_idx < sample_count; ++sample_idx) {
        out.sample_indices.push_back(sample_idx);
      }
      out.segments.reserve(sample_count - 1);
      for (int segment_idx = 0; segment_idx < sample_count - 1;
           ++segment_idx) {
        SegmentFitResult linear;
        linear.feasible = true;
        linear.interp = InterpType::Linear;
        linear.reason = "fallback_linear_anchor";
        linear.max_err = 0.0;
        out.segments.push_back(std::move(linear));
      }
      return out;
    }

    if (accepted_segment.max_err > out.max_err) {
      out.max_err = accepted_segment.max_err;
    }
    if (accepted_segment.max_err_screen_px > out.max_err_screen_px) {
      out.max_err_screen_px = accepted_segment.max_err_screen_px;
    }
    current_anchor_value =
        SegmentEndValue(ps, accepted_segment, accepted_j, dims);
    segments.push_back(std::move(accepted_segment));
    anchors.push_back(accepted_j);
    current = accepted_j;
    EmitPlacementProgress(progress_fn,
                          "forward_anchor",
                          current,
                          sample_count - 1,
                          current,
                          sample_count,
                          out.total_segments_tried,
                          out.total_segments_feasible);
  }

  out.sample_indices = std::move(anchors);
  out.segments = std::move(segments);
  out.converged = true;
  out.notes = "forward_longest_span_placement; max_gap_samples=" +
              std::to_string(max_gap);
  EmitPlacementProgress(progress_fn,
                        "forward_done",
                        sample_count - 1,
                        sample_count - 1,
                        sample_count - 1,
                        sample_count,
                        out.total_segments_tried,
                        out.total_segments_feasible);
  return out;
}

}  // namespace bbsolver
