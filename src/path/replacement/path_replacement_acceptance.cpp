#include "bbsolver/path/replacement/path_replacement_acceptance.hpp"

#include "bbsolver/domain.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"
#include "bbsolver/shape/sharp_corner_policy.hpp"

#include <string>

namespace bbsolver {

ReplacementAcceptanceVerdict EvaluateReplacementAcceptance(
    int    candidate_keys,
    double candidate_max_err,
    bool   candidate_converged,
    int    candidate_fitted_vertices,
    int    original_keys,
    double original_max_err,
    int    original_source_vertices,
    bool   original_converged,
    double tolerance,
    bool   prefer_vertices,
    double max_key_growth_ratio,
    double min_vertex_reduction_ratio,
    int    original_sample_count) {
  ReplacementAcceptanceVerdict verdict;

  const bool original_is_valid =
      original_converged && original_max_err <= tolerance + 1e-9;
  const bool candidate_is_valid =
      candidate_converged && candidate_max_err <= tolerance + 1e-9;

  // If the original solve is not a valid fallback, a valid candidate may still
  // be useful, but replacement must never increase key count versus the temporal
  // baseline. Otherwise the vertex-reduction lane can regress the user's main
  // key-count win.
  if (!original_is_valid) {
    if (candidate_is_valid) {
      verdict.use_candidate = candidate_keys <= original_keys;
      verdict.decision_note =
          std::string(verdict.use_candidate
                          ? "path_replacement_accepted; original_not_valid"
                          : "path_replacement_fallback_original_not_valid_key_gate") +
          "; candidate_keys=" + std::to_string(candidate_keys) +
          "; original_keys=" + std::to_string(original_keys) +
          "; original_converged=" + std::string(original_converged ? "true" : "false") +
          "; original_max_err=" + std::to_string(original_max_err);
      return verdict;
    }

    verdict.use_candidate =
        candidate_keys <= original_keys &&
        candidate_converged &&
        (!original_converged || candidate_max_err <= original_max_err);
    verdict.decision_note =
        std::string(verdict.use_candidate
                        ? "path_replacement_candidate_selected_both_invalid"
                        : "path_replacement_fallback_both_invalid") +
        "; candidate_converged=" + std::string(candidate_converged ? "true" : "false") +
        "; candidate_err=" + std::to_string(candidate_max_err) +
        "; original_converged=" + std::string(original_converged ? "true" : "false") +
        "; original_err=" + std::to_string(original_max_err);
    return verdict;
  }

  if (!candidate_is_valid) {
    verdict.use_candidate = false;
    verdict.decision_note =
        "path_replacement_fallback_candidate_invalid"
        "; candidate_converged=" + std::string(candidate_converged ? "true" : "false") +
        "; candidate_err=" + std::to_string(candidate_max_err) +
        "; original_fallback_keys=" + std::to_string(original_keys);
    return verdict;
  }

  if (candidate_keys < original_keys) {
    // Candidate has strictly fewer keys: always accept.
    verdict.use_candidate = true;
    verdict.decision_note =
        "path_replacement_accepted; candidate_keys=" + std::to_string(candidate_keys) +
        " < original_keys=" + std::to_string(original_keys);
    return verdict;
  }

  if (candidate_keys == original_keys) {
    // Equal key counts: accept if the candidate reduces the fitted vertex count.
    // Fewer fitted vertices means cheaper writeback, smaller KeyBundle, and a
    // better starting point for future per-channel Bezier solves.
    if (candidate_fitted_vertices < original_source_vertices) {
      verdict.use_candidate = true;
      verdict.decision_note =
          "path_replacement_accepted_equal_keys"
          "; candidate_keys=" + std::to_string(candidate_keys) +
          "; candidate_fitted_vertices=" + std::to_string(candidate_fitted_vertices) +
          " < original_source_vertices=" + std::to_string(original_source_vertices);
    } else {
      verdict.use_candidate = false;
      verdict.decision_note =
          "path_replacement_fallback_equal_keys"
          "; candidate_keys=" + std::to_string(candidate_keys) +
          "; candidate_fitted_vertices=" + std::to_string(candidate_fitted_vertices) +
          "; original_source_vertices=" + std::to_string(original_source_vertices) +
          "; candidate_err=" + std::to_string(candidate_max_err) +
          "; original_err=" + std::to_string(original_max_err);
    }
    return verdict;
  }

  // candidate_keys > original_keys: fall back to original. Even when the UI
  // asks for vertex priority, the fitted replacement is only a scout; the
  // guarded post-temporal vertex pass can prune the accepted key solve without
  // trading away the keyframe reduction.
  // Keep the same note format as the pre-refactor inline code so that any
  // tooling parsing these notes does not break.
  verdict.use_candidate = false;
  verdict.decision_note =
      "path_replacement_candidate_keys=" + std::to_string(candidate_keys) +
      "; candidate_err=" + std::to_string(candidate_max_err) +
      "; original_fallback_keys=" + std::to_string(original_keys);
  if (prefer_vertices) {
    double key_growth_ratio = 0.0;
    if (original_keys > 0) {
      key_growth_ratio =
          static_cast<double>(candidate_keys) / static_cast<double>(original_keys);
    }
    double vertex_reduction_ratio = 0.0;
    if (original_source_vertices > 0) {
      vertex_reduction_ratio =
          static_cast<double>(original_source_vertices - candidate_fitted_vertices) /
          static_cast<double>(original_source_vertices);
    }
    verdict.decision_note +=
        "; vertex_preference_rejected_key_growth"
        "; candidate_keys=" + std::to_string(candidate_keys) +
        "; original_keys=" + std::to_string(original_keys) +
        "; key_growth_ratio=" + std::to_string(key_growth_ratio) +
        "; max_key_growth_ratio=" + std::to_string(max_key_growth_ratio) +
        "; candidate_fitted_vertices=" + std::to_string(candidate_fitted_vertices) +
        "; original_source_vertices=" + std::to_string(original_source_vertices) +
        "; vertex_reduction_ratio=" + std::to_string(vertex_reduction_ratio) +
        "; min_vertex_reduction_ratio=" +
        std::to_string(min_vertex_reduction_ratio) +
        "; original_sample_count=" + std::to_string(original_sample_count) +
        "; post_temporal_vertex_prune_allowed";
  }
  return verdict;
}

ReplacementValidationSummary SummarizeReplacementCandidateValidation(
    const PathTemporalValidationResult& validation,
    const SharpCornerValidationResult& sharp_validation,
    const PropertyKeys& candidate_keys) {
  ReplacementValidationSummary summary;
  summary.candidate_converged =
      validation.samples_checked > 0
          ? (validation.ok && sharp_validation.ok)
          : (candidate_keys.converged && sharp_validation.ok);
  summary.candidate_max_err =
      validation.samples_checked > 0
          ? validation.max_outline_error
          : candidate_keys.max_err;
  return summary;
}

bool ApplyReplacementValidationSummaryToKeys(
    const PathTemporalValidationResult& validation,
    const ReplacementValidationSummary& summary,
    PropertyKeys* keys) {
  if (keys == nullptr || validation.samples_checked <= 0) {
    return false;
  }
  keys->max_err = summary.candidate_max_err;
  keys->converged = summary.candidate_converged;
  return true;
}

ReplacementRetryEligibilityInput BuildReplacementRetryEligibilityInput(
    bool verdict_use_candidate,
    int candidate_key_count,
    int original_key_count,
    const PathTemporalValidationResult& validation,
    const SharpCornerValidationResult& sharp_validation,
    int fitted_vertices,
    int source_min_vertices) {
  ReplacementRetryEligibilityInput input;
  input.verdict_use_candidate = verdict_use_candidate;
  input.candidate_key_count = candidate_key_count;
  input.original_key_count = original_key_count;
  input.validation_samples_checked = validation.samples_checked;
  input.source_validation_ok = validation.ok;
  input.sharp_validation_ok = sharp_validation.ok;
  input.fitted_vertices = fitted_vertices;
  input.source_min_vertices = source_min_vertices;
  return input;
}

ReplacementRetryEligibility EvaluateReplacementRetryEligibility(
    const ReplacementRetryEligibilityInput& input) {
  ReplacementRetryEligibility eligibility;
  eligibility.retry_key_gate =
      input.candidate_key_count <= input.original_key_count;
  eligibility.failed_only_sharp_gate =
      input.validation_samples_checked > 0 &&
      input.source_validation_ok &&
      !input.sharp_validation_ok;
  eligibility.should_retry =
      !input.verdict_use_candidate &&
      !eligibility.failed_only_sharp_gate &&
      eligibility.retry_key_gate &&
      input.validation_samples_checked > 0 &&
      input.fitted_vertices + 1 < input.source_min_vertices;
  return eligibility;
}

}  // namespace bbsolver
