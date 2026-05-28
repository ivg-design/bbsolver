#include "bbsolver/replacement_temporal/replacement_temporal_anchor_prune.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/path/temporal/path_temporal_validation.hpp"
#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/replacement_temporal/replacement_temporal_keys.hpp"
#include "bbsolver/replacement_temporal/replacement_temporal_anchor_prune_fit.hpp"
#include "bbsolver/replacement_temporal/replacement_temporal_solver.hpp"

#include <algorithm>
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include <cstddef>

namespace bbsolver {
namespace replacement_temporal {

constexpr int kExactAnchorFallbackLinearPruneChecksPerSample = 16;

struct ExactAnchorFallbackLinearPruneResult {
  bool accepted = false;
  bool cancelled = false;
  bool attempted = false;
  bool budget_exceeded = false;
  int checks = 0;
  int feasible = 0;
  int max_gap_samples = 0;
  int long_gap_samples = 0;
  int static_spans_collapsed = 0;
  int static_samples_skipped = 0;
  std::string reason;
  PropertyKeys keys;
};

int ExactStaticRunEnd(const PropertySamples& original,
                      const PropertySamples& reduced,
                      int start_idx,
                      double epsilon) {
  if (start_idx < 0 ||
      start_idx >= static_cast<int>(original.samples.size()) ||
      original.samples.size() != reduced.samples.size()) {
    return start_idx;
  }
  const std::vector<double>& original_start =
      original.samples[static_cast<std::size_t>(start_idx)].v;
  const std::vector<double>& reduced_start =
      reduced.samples[static_cast<std::size_t>(start_idx)].v;
  int end_idx = start_idx;
  for (int idx = start_idx + 1;
       idx < static_cast<int>(original.samples.size());
       ++idx) {
    if (!SampleValuesEqualWithin(
            original_start,
            original.samples[static_cast<std::size_t>(idx)].v,
            epsilon) ||
        !SampleValuesEqualWithin(
            reduced_start,
            reduced.samples[static_cast<std::size_t>(idx)].v,
            epsilon)) {
      break;
    }
    end_idx = idx;
  }
  return end_idx;
}

std::vector<int> FallbackPruneCandidateEnds(int cursor,
                                            int final_idx,
                                            int max_gap_samples,
                                            int long_gap_samples) {
  std::vector<int> ends;
  const auto add_offset = [&](int offset) {
    if (offset <= 0) {
      return;
    }
    const int end = std::min(final_idx, cursor + offset);
    if (end <= cursor) {
      return;
    }
    if (std::find(ends.begin(), ends.end(), end) == ends.end()) {
      ends.push_back(end);
    }
  };

  if (long_gap_samples > max_gap_samples) {
    add_offset(long_gap_samples);
    add_offset((long_gap_samples * 3) / 4);
    add_offset(long_gap_samples / 2);
    add_offset(long_gap_samples / 3);
    add_offset(max_gap_samples * 4);
    add_offset(max_gap_samples * 3);
    add_offset((max_gap_samples * 5) / 2);
    add_offset(max_gap_samples * 2);
    add_offset((max_gap_samples * 3) / 2);
  }
  const int dense_gap = std::min(long_gap_samples, max_gap_samples * 2);
  for (int offset = dense_gap; offset >= 1; --offset) {
    add_offset(offset);
  }

  std::sort(ends.begin(), ends.end(), std::greater<int>());
  ends.erase(std::unique(ends.begin(), ends.end()), ends.end());
  return ends;
}

void EmitExactAnchorFallbackPruneProgress(
    const ReplacementTemporalSolverOptions& options,
    const ExactAnchorFallbackLinearPruneResult& result,
    const std::string& stage,
    int cursor,
    int sample_count) {
  if (!options.placement_progress_fn) {
    return;
  }
  PlacementProgress progress;
  progress.stage = stage;
  // This fallback cleanup runs after the regular DP has already emitted its
  // terminal step for the same stage range. Keep the progress fraction pinned
  // at the end while still reporting phase/check updates; otherwise the AE
  // progress bar can move backwards during a successful cleanup.
  progress.step_index = std::max(0, sample_count - 1);
  progress.step_total = std::max(0, sample_count - 1);
  progress.sample_index = cursor;
  progress.samples = sample_count;
  progress.segments_tried = result.checks;
  progress.segments_feasible = result.feasible;
  options.placement_progress_fn(progress);
}

ExactAnchorFallbackLinearPruneResult TryLinearPruneExactAnchorFallback(
    const PropertySamples& original,
    const PropertySamples& reduced,
    const ReplacementTemporalSolverOptions& options,
    int fallback_key_count) {
  ExactAnchorFallbackLinearPruneResult result;
  result.attempted = true;
  if (original.samples.size() != reduced.samples.size() ||
      original.samples.size() < 3 ||
      !SameSampleTimes(original, reduced)) {
    result.reason = "ineligible_samples";
    return result;
  }

  const int n = static_cast<int>(reduced.samples.size());
  result.max_gap_samples = std::min(n - 1, std::max(1, options.max_gap_samples));
  result.long_gap_samples =
      std::min(n - 1,
               std::max(result.max_gap_samples,
                        std::min(options.forward_longest_span_max_gap_samples,
                                 result.max_gap_samples * 16)));
  const int max_checks =
      std::min(options.forward_longest_span_max_segment_checks,
               std::max(n,
                        n * std::min(
                                result.max_gap_samples,
                                kExactAnchorFallbackLinearPruneChecksPerSample)));
  std::vector<int> anchors;
  std::vector<SegmentFitResult> segments;
  anchors.reserve(static_cast<std::size_t>(n));
  segments.reserve(static_cast<std::size_t>(n - 1));
  anchors.push_back(0);

  int cursor = 0;
  std::vector<double> current_anchor_value = ValueAt(reduced, 0);
  const int progress_stride = std::max(1, (n - 1) / 20);
  EmitExactAnchorFallbackPruneProgress(
      options, result, "exact_anchor_fallback_linear_prune_start", cursor, n);
  while (cursor < n - 1) {
    if (options.cancel_fn && options.cancel_fn()) {
      result.cancelled = true;
      result.reason = "cancelled";
      return result;
    }
    const int max_end = std::min(n - 1, cursor + result.max_gap_samples);
    int chosen_end = -1;
    SegmentFitResult chosen_fit;
    const int static_end =
        ExactStaticRunEnd(original, reduced, cursor, 1e-9);
    if (static_end > max_end) {
      if (result.checks >= max_checks) {
        result.budget_exceeded = true;
        result.reason = "budget_exceeded";
        return result;
      }
      ++result.checks;
      SegmentFitResult fit = FitFallbackLinearPruneSegment(
          cursor,
          static_end,
          current_anchor_value,
          reduced,
          original,
          options.band_options);
      if (fit.feasible) {
        ++result.feasible;
        ++result.static_spans_collapsed;
        result.static_samples_skipped += static_end - max_end;
        chosen_end = static_end;
        fit.reason = "exact_anchor_fallback_static_span_prune";
        chosen_fit = std::move(fit);
      }
    }
    if (chosen_end < 0) {
      const std::vector<int> candidate_ends =
          FallbackPruneCandidateEnds(cursor,
                                     n - 1,
                                     result.max_gap_samples,
                                     result.long_gap_samples);
      for (int end: candidate_ends) {
        if (result.checks >= max_checks) {
          result.budget_exceeded = true;
          result.reason = "budget_exceeded";
          return result;
        }
        ++result.checks;
        SegmentFitResult fit = FitFallbackLinearPruneSegment(
            cursor,
            end,
            current_anchor_value,
            reduced,
            original,
            options.band_options);
        if (fit.feasible) {
          ++result.feasible;
          chosen_end = end;
          chosen_fit = std::move(fit);
          break;
        }
      }
    }
    if (chosen_end <= cursor) {
      result.reason = "no_feasible_span";
      return result;
    }
    anchors.push_back(chosen_end);
    segments.push_back(std::move(chosen_fit));
    current_anchor_value =
        segments.back().key_value_at_j.empty()
            ? ValueAt(reduced, chosen_end)
: segments.back().key_value_at_j;
    cursor = chosen_end;
    if (cursor >= n - 1 || anchors.size() == 2 ||
        (cursor % progress_stride) == 0) {
      EmitExactAnchorFallbackPruneProgress(
          options, result, "exact_anchor_fallback_linear_prune", cursor, n);
    }
  }

  if (static_cast<int>(anchors.size()) >= fallback_key_count) {
    result.reason = "no_key_reduction";
    return result;
  }

  PropertyKeys candidate = AssembleLinearPruneKeys(reduced, anchors, segments);
  PathTemporalValidationOptions validation_options;
  validation_options.frame_fit_options = options.band_options.frame_fit_options;
  const PathTemporalValidationResult validation =
      ValidatePathTemporalCandidate(original, candidate, validation_options);
  if (validation.samples_checked != n || !validation.ok) {
    result.reason =
        "validation_failed; validation_max_err=" +
        std::to_string(validation.max_outline_error) +
        "; validation_note=" + validation.notes;
    return result;
  }

  candidate.max_err = validation.max_outline_error;
  candidate.max_err_screen_px = validation.max_outline_error;
  result.accepted = true;
  result.reason = "ok";
  result.keys = std::move(candidate);
  EmitExactAnchorFallbackPruneProgress(
      options, result, "exact_anchor_fallback_linear_prune_done", n - 1, n);
  return result;
}

void PromoteValidatedAnchorFallback(
    const PropertySamples& original,
    const PropertySamples& reduced,
    const SolverConfig& config,
    const ReplacementTemporalSolverOptions& options,
    PropertyKeys& keys) {
  if (keys.converged || keys.keys.empty() ||
      !IsAllSamplesAnchorFallback(keys, original)) {
    return;
  }

  PathTemporalValidationOptions validation_options;
  validation_options.frame_fit_options =
      options.band_options.frame_fit_options;
  const PathTemporalValidationResult validation =
      ValidatePathTemporalCandidate(original, keys, validation_options);
  if (validation.samples_checked != static_cast<int>(original.samples.size()) ||
      !validation.ok) {
    AppendNote(keys,
               "exact_anchor_fallback_validation_failed=true; "
               "exact_anchor_fallback_max_err=" +
                   std::to_string(validation.max_outline_error));
    return;
  }

  keys.converged = true;
  keys.max_err = validation.max_outline_error;
  keys.max_err_screen_px = validation.max_outline_error;
  const std::string prior_notes = keys.notes;
  const int fallback_key_count = static_cast<int>(keys.keys.size());
  const std::string exact_note =
      "exact_anchor_fallback_validated=true; exact_anchor_fallback_keys=" +
      std::to_string(fallback_key_count) +
      "; exact_anchor_fallback_max_err=" +
      std::to_string(validation.max_outline_error);
  if (config.allow_linear) {
    ExactAnchorFallbackLinearPruneResult prune =
        TryLinearPruneExactAnchorFallback(original,
                                          reduced,
                                          options,
                                          fallback_key_count);
    if (prune.cancelled) {
      keys.converged = false;
      keys.notes = "cancelled";
      return;
    }
    if (prune.accepted) {
      keys = std::move(prune.keys);
      keys.notes = prior_notes;
      AppendNote(keys,
                 exact_note +
                     "; exact_anchor_fallback_linear_prune_accepted=true; "
                     "previous_keys=" +
                     std::to_string(fallback_key_count) +
                     "; pruned_keys=" +
                     std::to_string(static_cast<int>(keys.keys.size())) +
                     "; prune_checks=" +
                     std::to_string(prune.checks) +
                     "; prune_segments_feasible=" +
                     std::to_string(prune.feasible) +
                     "; prune_max_gap_samples=" +
                     std::to_string(prune.max_gap_samples) +
                     "; prune_long_gap_samples=" +
                     std::to_string(prune.long_gap_samples) +
                     (prune.static_spans_collapsed > 0
                          ? "; static_spans_collapsed=" +
                                std::to_string(prune.static_spans_collapsed) +
                                "; static_samples_skipped=" +
                                std::to_string(prune.static_samples_skipped)
: std::string{}));
      return;
    }
    AppendNote(keys,
               "exact_anchor_fallback_linear_prune_accepted=false; "
               "prune_reason=" +
                   prune.reason +
                   "; prune_checks=" + std::to_string(prune.checks) +
                   "; prune_segments_feasible=" +
                   std::to_string(prune.feasible) +
                   "; prune_long_gap_samples=" +
                   std::to_string(prune.long_gap_samples) +
                   "; prune_budget_exceeded=" +
                   std::string(prune.budget_exceeded ? "true": "false"));
  } else {
    AppendNote(keys,
               "exact_anchor_fallback_linear_prune_accepted=false; "
               "prune_reason=linear_disabled");
  }
  MarkAnchorFallbackAsHoldForExport(keys);
  AppendNote(keys, exact_note + "; exact_anchor_fallback_hold_export=true");
}

PropertyKeys MaybeUseAllSampleLinearPruneCandidate(
    const PropertySamples& original,
    const PropertySamples& reduced,
    const SolverConfig& config,
    const ReplacementTemporalSolverOptions& options,
    const PropertyKeys& current) {
  if (!current.converged ||
      current.notes == "cancelled" ||
      !config.allow_linear ||
      current.keys.size() != reduced.samples.size() ||
      current.keys.size() < 3 ||
      original.samples.size() != reduced.samples.size() ||
      current.notes.find("exact_anchor_fallback_linear_prune") !=
          std::string::npos) {
    return current;
  }

  ExactAnchorFallbackLinearPruneResult prune =
      TryLinearPruneExactAnchorFallback(original,
                                        reduced,
                                        options,
                                        static_cast<int>(current.keys.size()));
  if (prune.cancelled) {
    PropertyKeys cancelled = current;
    cancelled.converged = false;
    cancelled.notes = "cancelled";
    return cancelled;
  }
  if (!prune.accepted ||
      prune.keys.keys.size() >= current.keys.size()) {
    PropertyKeys out = current;
    AppendNote(out,
               "all_sample_linear_prune_accepted=false; prune_reason=" +
                   prune.reason +
                   "; prune_checks=" + std::to_string(prune.checks) +
                   "; prune_segments_feasible=" +
                   std::to_string(prune.feasible));
    return out;
  }

  PropertyKeys out = std::move(prune.keys);
  out.notes = current.notes;
  AppendNote(out,
             "all_sample_linear_prune_accepted=true; previous_keys=" +
                 std::to_string(static_cast<int>(current.keys.size())) +
                 "; pruned_keys=" +
                 std::to_string(static_cast<int>(out.keys.size())) +
                 "; prune_checks=" + std::to_string(prune.checks) +
                 "; prune_segments_feasible=" +
                 std::to_string(prune.feasible) +
                 "; prune_max_gap_samples=" +
                 std::to_string(prune.max_gap_samples) +
                 "; prune_long_gap_samples=" +
                 std::to_string(prune.long_gap_samples));
  return out;
}

}  // namespace replacement_temporal
}  // namespace bbsolver
