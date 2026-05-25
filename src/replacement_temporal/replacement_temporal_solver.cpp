#include "bbsolver/replacement_temporal/replacement_temporal_solver.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/path/multimode/path_multimode_solver.hpp"
#include "bbsolver/replacement_temporal/replacement_temporal_anchor_prune.hpp"
#include "bbsolver/replacement_temporal/replacement_temporal_forward_span.hpp"
#include "bbsolver/replacement_temporal/replacement_temporal_keys.hpp"
#include "bbsolver/replacement_temporal/replacement_temporal_options.hpp"
#include "bbsolver/replacement_temporal/replacement_temporal_segment_fit.hpp"

namespace bbsolver {
namespace {

constexpr int kMultiModePrecheckMinSamples = 24;
namespace rt = replacement_temporal;

double ClampAeInfluence(double influence) {
  if (!std::isfinite(influence)) {
    return 33.3;
  }
  return std::clamp(influence, 0.1, 100.0);
}

PropertyKeys SolveMultiModeCandidate(
    const PropertySamples& original,
    const PropertySamples& reduced,
    const ReplacementTemporalSolverOptions& options);

PropertyKeys MaybeUseMultiModeCandidate(
    const PropertySamples& original,
    const PropertySamples& reduced,
    const PropertyKeys& current,
    const ReplacementTemporalSolverOptions& options,
    const PropertyKeys* precomputed_multimode) {
  if (!options.allow_multimode_anchor_union ||
      current.notes == "cancelled" ||
      reduced.samples.size() < 3) {
    return current;
  }

  PropertyKeys multimode = precomputed_multimode != nullptr
                               ? *precomputed_multimode
                               : SolveMultiModeCandidate(original,
                                                         reduced,
                                                         options);
  if (multimode.notes == "cancelled") {
    return multimode;
  }

  const bool multimode_better =
      multimode.converged &&
      (current.keys.empty() ||
       !current.converged ||
       multimode.keys.size() < current.keys.size());
  if (multimode_better) {
    const std::string previous_note =
        "replacement_multimode_accepted=true; previous_keys=" +
        std::to_string(static_cast<int>(current.keys.size())) +
        "; previous_converged=" +
        std::string(current.converged ? "true" : "false");
    multimode.notes =
        multimode.notes.empty() ? previous_note
                                : multimode.notes + "; " + previous_note;
    return multimode;
  }

  PropertyKeys out = current;
  const std::string note =
      "replacement_multimode_accepted=false; multimode_converged=" +
      std::string(multimode.converged ? "true" : "false") +
      "; multimode_keys=" +
      std::to_string(static_cast<int>(multimode.keys.size())) +
      "; multimode_note=" + multimode.notes;
  out.notes = out.notes.empty() ? note : out.notes + "; " + note;
  return out;
}

bool MultiModeFastAccepts(const PropertyKeys& multimode,
                          const PropertySamples& reduced,
                          const ReplacementTemporalSolverOptions& options) {
  if (!multimode.converged ||
      multimode.keys.empty() ||
      reduced.samples.size() < kMultiModePrecheckMinSamples ||
      !(options.multimode_fast_accept_key_ratio > 0.0)) {
    return false;
  }
  const double max_keys =
      static_cast<double>(reduced.samples.size()) *
      options.multimode_fast_accept_key_ratio;
  return static_cast<double>(multimode.keys.size()) <= max_keys + 1e-9;
}

PropertyKeys SolveMultiModeCandidate(
    const PropertySamples& original,
    const PropertySamples& reduced,
    const ReplacementTemporalSolverOptions& options) {
  ShapeFlatMultiModeOptions multimode_options;
  multimode_options.frame_fit_options = options.band_options.frame_fit_options;
  multimode_options.max_regions = options.multimode_max_regions;
  multimode_options.max_gap_samples = options.multimode_max_gap_samples;
  multimode_options.region_tolerance = options.multimode_region_tolerance;
  multimode_options.max_region_segment_checks =
      options.multimode_max_region_segment_checks;
  multimode_options.max_validation_samples =
      options.multimode_max_validation_samples;
  multimode_options.max_validation_work_units =
      options.multimode_max_validation_work_units;
  multimode_options.max_candidate_key_ratio =
      options.multimode_max_candidate_key_ratio;
  multimode_options.cancel_fn = options.cancel_fn;
  return SolveShapeFlatMultiModeTemporal(original, reduced, multimode_options);
}

}  // namespace

PropertyKeys SolveReplacementShapeFlatTemporal(
    const PropertySamples& original,
    const PropertySamples& reduced,
    const SolverConfig& config,
    const CompInfo& comp,
    const ReplacementTemporalSolverOptions& options) {
  if (!rt::IsShapeFlatPath(original) || !rt::IsShapeFlatPath(reduced)) {
    PropertyKeys out;
    out.property_id = reduced.property.id;
    out.converged = false;
    out.notes = "replacement_temporal_requires_shape_flat";
    return out;
  }
  if (original.samples.empty() || reduced.samples.empty()) {
    PropertyKeys out;
    out.property_id = reduced.property.id;
    out.converged = false;
    out.notes = "replacement_temporal_no_samples";
    return out;
  }
  if (!rt::SameSampleTimes(original, reduced)) {
    PropertyKeys out;
    out.property_id = reduced.property.id;
    out.converged = false;
    out.notes = "replacement_temporal_sample_time_mismatch";
    return out;
  }

  SolverConfig solver_config = config;
  // Replacement shape paths must always pass through the source-outline oracle.
  // RunDPPlacement has a reduced-value constant shortcut behind allow_hold; that
  // shortcut is valid for ordinary properties but can accept a constant reduced
  // path even when the original source outline still changes.
  solver_config.allow_hold = false;

  const ReplacementTemporalSolverOptions normalized_options =
      NormalizeReplacementTemporalOptions(options, solver_config);

  PropertyKeys multimode_precheck;
  bool have_multimode_precheck = false;
  if (normalized_options.allow_multimode_anchor_union) {
    multimode_precheck =
        SolveMultiModeCandidate(original, reduced, normalized_options);
    have_multimode_precheck = true;
    if (multimode_precheck.notes == "cancelled") {
      return multimode_precheck;
    }
    if (MultiModeFastAccepts(multimode_precheck, reduced, normalized_options)) {
      const std::string note =
          "replacement_multimode_precheck=true; replacement_multimode_fast_accept_ratio=" +
          std::to_string(normalized_options.multimode_fast_accept_key_ratio) +
          "; replacement_shape_temporal_solver=skipped_after_validated_multimode";
      multimode_precheck.notes =
          multimode_precheck.notes.empty()
              ? note
              : multimode_precheck.notes + "; " + note;
      return rt::MaybeUseForwardLongestSpanCandidate(original,
                                                     reduced,
                                                     multimode_precheck,
                                                     normalized_options);
    }
  }

  const DPPlacement placement =
      RunDPPlacement(
          reduced,
          solver_config,
          comp,
          [&original, &normalized_options](
              int i,
              int j,
              const PropertySamples& ps,
              const SolverConfig& cfg,
              const CompInfo&) {
            return rt::FitReplacementSegment(
                i,
                j,
                ps,
                original,
                cfg,
                normalized_options.band_options,
                normalized_options.allow_relaxed_endpoint_fit);
          },
          normalized_options.max_gap_samples,
          normalized_options.cancel_fn,
          normalized_options.placement_progress_fn);

  PropertyKeys out = rt::AssembleReplacementKeys(reduced, placement);
  const std::string note =
      "replacement_shape_temporal_solver; total_segments_tried=" +
      std::to_string(placement.total_segments_tried) +
      "; total_segments_feasible=" +
      std::to_string(placement.total_segments_feasible) +
      "; replacement_temporal_max_gap_samples=" +
      std::to_string(normalized_options.max_gap_samples) +
      "; replacement_temporal_progress_steps=" +
      std::to_string(normalized_options.band_options.progress_steps) +
      "; replacement_temporal_oracle_subdivisions=" +
      std::to_string(normalized_options.band_options.frame_fit_options.max_subdivisions_per_segment) +
      "; replacement_temporal_fit_bezier_pairs=" +
      std::string(normalized_options.band_options.fit_bezier_influence_pairs ? "true" : "false") +
      "; replacement_temporal_max_bezier_pairs=" +
      std::to_string(normalized_options.band_options.max_bezier_influence_pairs) +
      "; replacement_temporal_relaxed_endpoints=" +
      std::string(normalized_options.allow_relaxed_endpoint_fit ? "true" : "false") +
      "; replacement_temporal_multimode=" +
      std::string(normalized_options.allow_multimode_anchor_union ? "true" : "false") +
      "; replacement_temporal_multimode_gap=" +
      std::to_string(normalized_options.multimode_max_gap_samples);
  rt::AppendNote(out, note);
  rt::PromoteValidatedAnchorFallback(original,
                                  reduced,
                                  solver_config,
                                  normalized_options,
                                  out);
  out = rt::MaybeUseAllSampleLinearPruneCandidate(original,
                                              reduced,
                                              solver_config,
                                              normalized_options,
                                              out);
  out = MaybeUseMultiModeCandidate(original,
                                   reduced,
                                   out,
                                   normalized_options,
                                   have_multimode_precheck
                                       ? &multimode_precheck
                                       : nullptr);
  return rt::MaybeUseForwardLongestSpanCandidate(original,
                                             reduced,
                                             out,
                                             normalized_options);
}

}  // namespace bbsolver
