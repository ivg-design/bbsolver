#include "bbsolver/path/replacement/path_replacement_retry_loop.hpp"

#include "bbsolver/domain.hpp"
#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/path/replacement/path_replacement_acceptance.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/path/geometry/path_geometry_refinement.hpp"
#include "bbsolver/path/replacement/path_replacement_decision_apply.hpp"
#include "bbsolver/path/replacement/path_replacement_notes.hpp"
#include "bbsolver/path/replacement/path_replacement_progress.hpp"
#include "bbsolver/path/replacement/path_replacement_solver.hpp"
#include "bbsolver/path/config/path_solver_config.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"
#include "bbsolver/solve/plain_property_solver.hpp"
#include "bbsolver/progress/progress.hpp"
#include "bbsolver/routing/property_classification.hpp"
#include "bbsolver/replacement_temporal/replacement_temporal_solve_options.hpp"
#include "bbsolver/replacement_temporal/replacement_temporal_solver.hpp"
#include "bbsolver/shape/sharp_corner_policy.hpp"
#include "bbsolver/solve/solver_reporting.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace bbsolver {

namespace {

bool IsRetryCancelled(const PropertyKeys& keys, const CancelFn& cancel_fn) {
  return keys.notes == "cancelled" || (cancel_fn && cancel_fn());
}

}  // namespace

ReplacementRetryLoopResult TryReplacementRetryLoop(
    const ReplacementRetryLoopRequest& request,
    PropertyKeys* property_keys,
    PropertySamples* property_samples,
    std::string* path_fit_note) {
  ReplacementRetryLoopResult result;
  result.replacement_fitted_vertices = request.replacement_fitted_vertices;

  const ReplacementRetryEligibility retry_eligibility =
      EvaluateReplacementRetryEligibility(
          BuildReplacementRetryEligibilityInput(
              request.verdict->use_candidate,
              request.candidate_key_count,
              static_cast<int>(request.original_keys->keys.size()),
              *request.source_validation,
              *request.source_sharp_validation,
              request.replacement_fitted_vertices,
              request.replacement_source_min_vertices));

  if (!retry_eligibility.should_retry) {
    ReplacementRetrySkippedNoteRequest skipped_note_request;
    skipped_note_request.verdict = request.verdict;
    skipped_note_request.retry_eligibility = &retry_eligibility;
    skipped_note_request.candidate_key_count = request.candidate_key_count;
    skipped_note_request.original_key_count =
        static_cast<int>(request.original_keys->keys.size());
    AppendReplacementRetrySkippedNote(skipped_note_request, path_fit_note);
    return result;
  }

  AppendJoinedNote(*path_fit_note,
                   BuildReplacementRetryFirstNote(
                       request.validation_summary->candidate_max_err,
                       request.replacement_fitted_vertices));

  const std::vector<int> retry_targets =
      BuildShapeFlatReplacementRetryTargetLadder(
          request.replacement_fitted_vertices,
          request.replacement_source_min_vertices,
          request.config->path_replacement_min_vertices,
          request.config->path_replacement_max_vertices);
  int retry = 0;
  for (int retry_target: retry_targets) {
    if (result.accepted) {
      break;
    }
    if (retry_target <= request.replacement_fitted_vertices) {
      continue;
    }
    ++retry;
    request.progress->Emit(
        ReplacementRetryStartProgressEvent(
            *request.original_property_samples,
            request.property_idx,
            request.property_count,
            retry,
            retry_target));

    SolverConfig retry_config = *request.config;
    retry_config.path_replacement_min_vertices = retry_target;

    ReplacementPathFitResult retry_fit =
        FitReplacementPathProperty(
            *request.original_property_samples,
            retry_config,
            request.progress,
            request.property_idx,
            request.property_count,
            !request.visible_outline_reference);

    if (!retry_fit.applied) {
      AppendJoinedNote(*path_fit_note,
                       BuildReplacementRetryFitFailedNote(
                           retry, retry_target, retry_fit.notes));
      continue;
    }
    if (retry_fit.fitted_vertices <= request.replacement_fitted_vertices) {
      AppendJoinedNote(*path_fit_note,
                       BuildReplacementRetryNoImprovementNote(
                           retry, retry_target));
      continue;
    }

    PropertySamples retry_samples = std::move(retry_fit.samples);
    double retry_frame_fit_error = retry_fit.max_outline_error;
    if (!retry_fit.winning_fractions.empty()) {
      const PathGeometryRefinementResult retry_refine =
          RefinePathGeometryAtFractions(
              *request.original_property_samples,
              retry_fit.winning_fractions,
              ReplacementFrameFitOptions(*request.config));
      if (retry_refine.applied) {
        retry_samples = std::move(retry_refine.refined_samples);
        retry_frame_fit_error = retry_refine.refined_max_error;
      }
    }

    const int retry_max_gap =
        IsShapeFlatPath(retry_samples)
            ? std::max(6, std::min(8, PathChildMaxGap(*request.comp)))
: 0;
    const SolverConfig retry_temporal_config =
        ReplacementTemporalConfig(*request.config, retry_frame_fit_error);
    PropertyKeys retry_keys =
        IsShapeFlatPath(retry_samples)
            ? SolveReplacementShapeFlatTemporal(
                  *request.original_property_samples,
                  retry_samples,
                  retry_temporal_config,
                  *request.comp,
                  ReplacementTemporalOptions(
                      retry_temporal_config,
                      retry_max_gap,
                      *request.options))
: SolvePlainProperty(
                  retry_samples,
                  retry_temporal_config,
                  *request.comp,
                  *request.options,
                  retry_max_gap);
    if (IsRetryCancelled(retry_keys, request.cancel_fn)) {
      result.cancelled = true;
      result.cancel_phase = "path_replacement_retry";
      return result;
    }

    const PathTemporalValidationResult retry_validation =
        ValidatePathTemporalCandidate(
            *request.original_property_samples,
            retry_keys,
            *request.validation_options);
    const SharpCornerValidationResult retry_sharp_validation =
        ValidateSharpCornerPreservation(
            *request.original_property_samples,
            retry_keys,
            *request.config,
            !request.visible_outline_reference);

    const ReplacementValidationSummary retry_summary =
        SummarizeReplacementCandidateValidation(
            retry_validation, retry_sharp_validation, retry_keys);

    const ReplacementAcceptanceVerdict retry_verdict =
        EvaluateReplacementAcceptance(
            static_cast<int>(retry_keys.keys.size()),
            retry_summary.candidate_max_err,
            retry_summary.candidate_converged,
            retry_fit.fitted_vertices,
            static_cast<int>(request.original_keys->keys.size()),
            request.original_keys->max_err,
            retry_fit.source_max_vertices,
            request.original_keys->converged,
            request.config->tolerance,
            request.config->path_replacement_prefer_vertices,
            request.config->path_replacement_max_key_growth_ratio,
            request.config->path_replacement_min_vertex_reduction_ratio,
            static_cast<int>(
                request.original_property_samples->samples.size()));

    const ReplacementRetryResultNoteInput retry_note_input =
        BuildReplacementRetryResultNoteInput(
            retry,
            retry_target,
            retry_fit.fitted_vertices,
            retry_temporal_config.tolerance,
            retry_summary.candidate_max_err,
            request.visible_outline_reference,
            retry_sharp_validation);
    AppendJoinedNote(*path_fit_note,
                     BuildReplacementRetryResultNote(retry_note_input));
    request.progress->Emit(
        ReplacementRetryDoneProgressEvent(
            *request.original_property_samples,
            request.property_idx,
            request.property_count,
            retry,
            retry_target,
            retry_fit.fitted_vertices,
            retry_summary.candidate_max_err,
            retry_verdict.use_candidate,
            retry_sharp_validation.ok));

    if (retry_verdict.use_candidate) {
      ApplyReplacementValidationSummaryToKeys(
          retry_validation, retry_summary, &retry_keys);
      const std::string accept_note =
          BuildReplacementRetryAcceptedNote(
              retry_summary.candidate_max_err,
              retry_verdict.decision_note);
      AppendJoinedNote(retry_keys.notes, accept_note);
      *property_keys = std::move(retry_keys);
      *property_samples = std::move(retry_samples);
      result.replacement_fitted_vertices = retry_fit.fitted_vertices;
      result.accepted = true;
    }
  }

  if (retry == 0) {
    AppendJoinedNote(*path_fit_note,
                     BuildReplacementRetrySkippedNoLadderNote());
  }
  return result;
}

}  // namespace bbsolver
