#include "bbsolver/replacement_temporal/replacement_temporal_forward_span.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/replacement_temporal/replacement_temporal_keys.hpp"
#include "bbsolver/replacement_temporal/replacement_temporal_solver.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>
#include <cstddef>

namespace bbsolver {
namespace replacement_temporal {
namespace {

void EmitForwardLongestSpanProgress(
    const ReplacementTemporalSolverOptions& options,
    const DPPlacement& placement,
    const std::string& stage,
    int cursor,
    int sample_count) {
  if (!options.placement_progress_fn) {
    return;
  }
  PlacementProgress progress;
  progress.stage = stage;
  progress.step_index = std::clamp(cursor, 0, std::max(0, sample_count - 1));
  progress.step_total = std::max(1, sample_count - 1);
  progress.sample_index = cursor;
  progress.samples = sample_count;
  progress.segments_tried = placement.total_segments_tried;
  progress.segments_feasible = placement.total_segments_feasible;
  options.placement_progress_fn(progress);
}

}  // namespace

bool ForwardLongestSpanEligible(
    const PropertySamples& original,
    const PropertySamples& reduced,
    const ReplacementTemporalSolverOptions& options) {
  if (!options.allow_forward_longest_span ||
      static_cast<int>(reduced.samples.size()) <
          options.forward_longest_span_min_samples ||
      original.samples.size() != reduced.samples.size() ||
      reduced.samples.empty()) {
    return false;
  }
  const std::vector<double>& first = reduced.samples.front().v;
  if (!IsValidShapeFlat(first)) {
    return false;
  }
  const int vertex_count = ShapeFlatVertexCount(first);
  if (vertex_count < options.forward_longest_span_min_vertex_count) {
    return false;
  }
  const int closed = static_cast<int>(std::llround(first[0]));
  const std::size_t value_size = first.size();
  for (std::size_t idx = 0; idx < reduced.samples.size(); ++idx) {
    const std::vector<double>& reduced_value = reduced.samples[idx].v;
    if (!IsValidShapeFlat(reduced_value) ||
        reduced_value.size() != value_size ||
        static_cast<int>(std::llround(reduced_value[0])) != closed ||
        ShapeFlatVertexCount(reduced_value) != vertex_count ||
        !IsValidShapeFlat(original.samples[idx].v)) {
      return false;
    }
  }
  return true;
}

SegmentFitResult FitForwardLongestSpanLinearSegment(
    int i,
    int j,
    const PropertySamples& reduced,
    const PropertySamples& original,
    const PathFrameFitOptions& frame_fit_options) {
  SegmentFitResult result;
  result.interp = InterpType::Linear;
  result.reason = "infeasible_forward_longest_span_linear";
  if (i < 0 || j <= i ||
      j >= static_cast<int>(reduced.samples.size()) ||
      original.samples.size() != reduced.samples.size()) {
    result.reason = "invalid_forward_longest_span_window";
    return result;
  }
  const std::vector<double>& start_value = reduced.samples[i].v;
  const std::vector<double>& end_value = reduced.samples[j].v;
  if (!IsValidShapeFlat(start_value) ||
      !IsValidShapeFlat(end_value) ||
      start_value.size() != end_value.size() ||
      static_cast<int>(std::llround(start_value[0])) !=
          static_cast<int>(std::llround(end_value[0])) ||
      ShapeFlatVertexCount(start_value) != ShapeFlatVertexCount(end_value)) {
    result.reason = "malformed_forward_longest_span_endpoints";
    return result;
  }

  const double tolerance =
      std::max(frame_fit_options.outline_tolerance, 0.0);
  const double t0 = reduced.samples[i].t_sec;
  const double t1 = reduced.samples[j].t_sec;
  double max_error = 0.0;
  double rms_accum = 0.0;
  int checked = 0;
  for (int sample_idx = i; sample_idx <= j; ++sample_idx) {
    const Sample& source_sample = original.samples[sample_idx];
    if (!IsValidShapeFlat(source_sample.v)) {
      result.reason = "malformed_forward_longest_span_source";
      return result;
    }
    const double u = t1 > t0 ? (source_sample.t_sec - t0) / (t1 - t0) : 0.0;
    const std::vector<double> candidate =
        LerpShapeFlatChord(start_value, end_value, std::clamp(u, 0.0, 1.0));
    const double error = ShapeFlatFrameOutlineError(
        source_sample.v, candidate, frame_fit_options);
    max_error = std::max(max_error, error);
    rms_accum += error * error;
    ++checked;
    if (error > tolerance + 1e-9) {
      result.max_err = max_error;
      result.max_err_screen_px = max_error;
      result.rms_err =
          checked > 0 ? std::sqrt(rms_accum / static_cast<double>(checked)) : 0.0;
      return result;
    }
  }

  result.feasible = true;
  result.max_err = max_error;
  result.max_err_screen_px = max_error;
  result.rms_err =
      checked > 0 ? std::sqrt(rms_accum / static_cast<double>(checked)) : 0.0;
  result.iters = checked;
  result.reason = "replacement_shape_morph_forward_longest_span_linear_ok";
  return result;
}

DPPlacement SolveForwardLongestSpanCandidate(
    const PropertySamples& original,
    const PropertySamples& reduced,
    const ReplacementTemporalSolverOptions& options) {
  DPPlacement placement;
  if (!ForwardLongestSpanEligible(original, reduced, options)) {
    placement.notes = "replacement_forward_longest_span_skipped";
    return placement;
  }

  const int n = static_cast<int>(reduced.samples.size());
  placement.sample_indices.push_back(0);
  int cursor = 0;
  const int progress_stride = std::max(1, (n - 1) / 20);
  EmitForwardLongestSpanProgress(
      options, placement, "replacement_forward_longest_span_start", cursor, n);
  while (cursor < n - 1) {
    if (options.cancel_fn && options.cancel_fn()) {
      placement.notes = "cancelled";
      return placement;
    }
    const int max_end =
        std::min(n - 1,
                 cursor + options.forward_longest_span_max_gap_samples);
    int chosen_end = -1;
    SegmentFitResult chosen_fit;
    for (int end = max_end; end > cursor; --end) {
      if (placement.total_segments_tried >=
          options.forward_longest_span_max_segment_checks) {
        placement.converged = false;
        placement.notes =
            "replacement_forward_longest_span_budget_exceeded; "
            "forward_segments_tried=" +
            std::to_string(placement.total_segments_tried);
        EmitForwardLongestSpanProgress(
            options, placement, "replacement_forward_longest_span_done",
            cursor, n);
        return placement;
      }
      ++placement.total_segments_tried;
      SegmentFitResult fit = FitForwardLongestSpanLinearSegment(
          cursor,
          end,
          reduced,
          original,
          options.band_options.frame_fit_options);
      if (fit.feasible) {
        ++placement.total_segments_feasible;
        chosen_end = end;
        chosen_fit = std::move(fit);
        break;
      }
    }
    if (chosen_end <= cursor) {
      placement.converged = false;
      placement.notes =
          "replacement_forward_longest_span_no_feasible_segment; cursor=" +
          std::to_string(cursor) +
          "; forward_segments_tried=" +
          std::to_string(placement.total_segments_tried);
      EmitForwardLongestSpanProgress(
          options, placement, "replacement_forward_longest_span_done",
          cursor, n);
      return placement;
    }
    placement.max_err = std::max(placement.max_err, chosen_fit.max_err);
    placement.max_err_screen_px =
        std::max(placement.max_err_screen_px, chosen_fit.max_err_screen_px);
    placement.segments.push_back(std::move(chosen_fit));
    placement.sample_indices.push_back(chosen_end);
    cursor = chosen_end;
    if (cursor >= n - 1 || placement.sample_indices.size() == 2 ||
        (cursor % progress_stride) == 0) {
      EmitForwardLongestSpanProgress(
          options,
          placement,
          "replacement_forward_longest_span_progress",
          cursor,
          n);
    }
  }

  placement.converged = true;
  placement.notes =
      "replacement_forward_longest_span; forward_segments_tried=" +
      std::to_string(placement.total_segments_tried) +
      "; forward_segments_feasible=" +
      std::to_string(placement.total_segments_feasible) +
      "; forward_max_gap_samples=" +
      std::to_string(options.forward_longest_span_max_gap_samples) +
      "; forward_min_vertex_count=" +
      std::to_string(options.forward_longest_span_min_vertex_count);
  EmitForwardLongestSpanProgress(
      options, placement, "replacement_forward_longest_span_done", n - 1, n);
  return placement;
}

PropertyKeys MaybeUseForwardLongestSpanCandidate(
    const PropertySamples& original,
    const PropertySamples& reduced,
    const PropertyKeys& current,
    const ReplacementTemporalSolverOptions& options) {
  if (!ForwardLongestSpanEligible(original, reduced, options) ||
      current.notes == "cancelled") {
    return current;
  }

  const DPPlacement forward_placement =
      SolveForwardLongestSpanCandidate(original, reduced, options);
  if (forward_placement.notes == "cancelled") {
    PropertyKeys cancelled;
    cancelled.property_id = reduced.property.id;
    cancelled.converged = false;
    cancelled.notes = "cancelled";
    return cancelled;
  }
  PropertyKeys forward = AssembleReplacementKeys(reduced, forward_placement);
  const bool forward_better =
      forward.converged &&
      !forward.keys.empty() &&
      (current.keys.empty() ||
       !current.converged ||
       forward.keys.size() < current.keys.size());
  if (forward_better) {
    const std::string note =
        "replacement_forward_longest_span_accepted=true; previous_keys=" +
        std::to_string(static_cast<int>(current.keys.size())) +
        "; previous_converged=" +
        std::string(current.converged ? "true" : "false");
    forward.notes = forward.notes.empty() ? note : forward.notes + "; " + note;
    return forward;
  }

  PropertyKeys out = current;
  const std::string note =
      "replacement_forward_longest_span_accepted=false; forward_converged=" +
      std::string(forward.converged ? "true" : "false") +
      "; forward_keys=" +
      std::to_string(static_cast<int>(forward.keys.size())) +
      "; forward_note=" + forward.notes;
  out.notes = out.notes.empty() ? note : out.notes + "; " + note;
  return out;
}

}  // namespace replacement_temporal
}  // namespace bbsolver
