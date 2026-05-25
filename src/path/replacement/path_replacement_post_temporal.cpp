#include "bbsolver/path/replacement/path_replacement_post_temporal.hpp"
#include "bbsolver/domain.hpp"

#include <stdexcept>
#include <string>
#include <utility>

#include "bbsolver/path/replacement/path_replacement_baseline_solve.hpp"
#include "bbsolver/path/replacement/path_replacement_candidate_validation.hpp"
#include "bbsolver/path/replacement/path_replacement_decision_apply.hpp"
#include "bbsolver/path/replacement/path_replacement_fast_vertex_acceptance.hpp"
#include "bbsolver/path/replacement/path_replacement_acceptance.hpp"
#include "bbsolver/path/replacement/path_replacement_retry_loop.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"
#include "bbsolver/shape/sharp_corner_policy.hpp"

namespace bbsolver {
namespace {

void RequirePostTemporalReplacementRequest(
    const PostTemporalReplacementRequest& request,
    const PropertyKeys* property_keys,
    const PropertySamples* property_samples,
    const std::string* path_fit_note) {
  if (request.original_property_samples == nullptr ||
      request.config == nullptr ||
      request.comp == nullptr ||
      request.options == nullptr ||
      request.progress == nullptr ||
      !request.cancel_fn ||
      property_keys == nullptr ||
      property_samples == nullptr ||
      path_fit_note == nullptr) {
    throw std::invalid_argument(
        "post-temporal replacement request is incomplete");
  }
}

}  // namespace

PostTemporalReplacementResult ProcessPostTemporalReplacement(
    const PostTemporalReplacementRequest& request,
    PropertyKeys* property_keys,
    PropertySamples* property_samples,
    std::string* path_fit_note) {
  RequirePostTemporalReplacementRequest(
      request, property_keys, property_samples, path_fit_note);

  PostTemporalReplacementResult result;
  result.replacement_output_accepted = request.replacement_output_accepted;
  result.replacement_fast_vertex_preference_accepted =
      request.replacement_fast_vertex_preference_accepted;
  result.replacement_fitted_vertices = request.replacement_fitted_vertices;

  ReplacementFastVertexAcceptanceRequest fast_acceptance_request;
  fast_acceptance_request.original_property_samples =
      request.original_property_samples;
  fast_acceptance_request.property_samples = property_samples;
  fast_acceptance_request.config = request.config;
  fast_acceptance_request.progress = request.progress;
  fast_acceptance_request.property_idx = request.property_idx;
  fast_acceptance_request.property_count = request.property_count;
  fast_acceptance_request.visible_outline_reference =
      request.visible_outline_reference;
  fast_acceptance_request.replacement_fitted_vertices =
      request.replacement_fitted_vertices;
  fast_acceptance_request.replacement_original_max_vertices =
      request.replacement_original_max_vertices;
  fast_acceptance_request.replacement_estimated_candidate_keys =
      request.replacement_estimated_candidate_keys;
  fast_acceptance_request.replacement_estimated_original_keys =
      request.replacement_estimated_original_keys;
  const ReplacementFastVertexAcceptanceResult fast_acceptance =
      TryAcceptReplacementFastVertexPreference(
          fast_acceptance_request, property_keys);
  const int candidate_key_count = fast_acceptance.candidate_key_count;

  if (fast_acceptance.accepted) {
    result.replacement_output_accepted = true;
    result.replacement_fast_vertex_preference_accepted = true;
    return result;
  }

  ReplacementBaselineSolveRequest baseline_request;
  baseline_request.original_property_samples = request.original_property_samples;
  baseline_request.config = request.config;
  baseline_request.comp = request.comp;
  baseline_request.options = request.options;
  baseline_request.progress = request.progress;
  baseline_request.cancel_fn = request.cancel_fn;
  baseline_request.property_idx = request.property_idx;
  baseline_request.property_count = request.property_count;
  ReplacementBaselineSolveResult baseline_result =
      SolveReplacementBaseline(baseline_request);
  if (baseline_result.cancelled) {
    result.cancelled = true;
    result.cancel_phase = baseline_result.cancel_phase;
    return result;
  }

  PropertyKeys original_keys = std::move(baseline_result.keys);
  ReplacementCandidateValidationRequest validation_request;
  validation_request.original_property_samples =
      request.original_property_samples;
  validation_request.property_samples = property_samples;
  validation_request.property_keys = property_keys;
  validation_request.original_keys = &original_keys;
  validation_request.config = request.config;
  validation_request.progress = request.progress;
  validation_request.property_idx = request.property_idx;
  validation_request.property_count = request.property_count;
  validation_request.visible_outline_reference =
      request.visible_outline_reference;
  validation_request.replacement_fitted_vertices =
      request.replacement_fitted_vertices;
  validation_request.replacement_original_max_vertices =
      request.replacement_original_max_vertices;
  const ReplacementCandidateValidationResult candidate_validation =
      ValidateReplacementCandidateAgainstBaseline(validation_request);
  const PathTemporalValidationOptions& validation_options =
      candidate_validation.validation_options;
  const PathTemporalValidationResult& source_validation =
      candidate_validation.source_validation;
  const SharpCornerValidationResult& source_sharp_validation =
      candidate_validation.source_sharp_validation;
  const ReplacementValidationSummary& validation_summary =
      candidate_validation.validation_summary;
  const ReplacementAcceptanceVerdict& verdict = candidate_validation.verdict;
  const std::string& source_val_note =
      candidate_validation.source_validation_note;

  ReplacementRetryLoopRequest retry_request;
  retry_request.original_property_samples = request.original_property_samples;
  retry_request.config = request.config;
  retry_request.comp = request.comp;
  retry_request.options = request.options;
  retry_request.progress = request.progress;
  retry_request.cancel_fn = request.cancel_fn;
  retry_request.property_idx = request.property_idx;
  retry_request.property_count = request.property_count;
  retry_request.visible_outline_reference = request.visible_outline_reference;
  retry_request.replacement_fitted_vertices =
      request.replacement_fitted_vertices;
  retry_request.replacement_source_min_vertices =
      request.replacement_source_min_vertices;
  retry_request.candidate_key_count = candidate_key_count;
  retry_request.original_keys = &original_keys;
  retry_request.validation_options = &validation_options;
  retry_request.source_validation = &source_validation;
  retry_request.source_sharp_validation = &source_sharp_validation;
  retry_request.validation_summary = &validation_summary;
  retry_request.verdict = &verdict;
  const ReplacementRetryLoopResult retry_result =
      TryReplacementRetryLoop(
          retry_request, property_keys, property_samples, path_fit_note);
  if (retry_result.cancelled) {
    result.cancelled = true;
    result.cancel_phase = retry_result.cancel_phase;
    return result;
  }
  if (retry_result.accepted) {
    result.replacement_fitted_vertices =
        retry_result.replacement_fitted_vertices;
    result.replacement_output_accepted = true;
  }

  ReplacementDecisionApplyRequest decision_request;
  decision_request.verdict = &verdict;
  decision_request.source_validation = &source_validation;
  decision_request.validation_summary = &validation_summary;
  decision_request.source_validation_note = &source_val_note;
  decision_request.original_property_samples = request.original_property_samples;
  decision_request.replacement_retry_accepted = retry_result.accepted;
  const ReplacementDecisionApplyResult decision_result =
      ApplyReplacementInitialOrFallbackDecision(
          decision_request, property_keys, property_samples, &original_keys);
  result.replacement_output_accepted =
      result.replacement_output_accepted ||
      decision_result.initial_candidate_accepted;
  return result;
}

}  // namespace bbsolver
