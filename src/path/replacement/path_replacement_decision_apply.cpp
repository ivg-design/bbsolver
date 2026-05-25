#include "bbsolver/path/replacement/path_replacement_decision_apply.hpp"

#include <string>
#include <utility>

#include "bbsolver/domain.hpp"
#include "bbsolver/path/replacement/path_replacement_acceptance.hpp"
#include "bbsolver/path/replacement/path_replacement_notes.hpp"
#include "bbsolver/solve/solver_reporting.hpp"

namespace bbsolver {

bool AppendReplacementRetrySkippedNote(
    const ReplacementRetrySkippedNoteRequest& request,
    std::string* path_fit_note) {
  if (request.verdict->use_candidate) {
    return false;
  }
  if (request.retry_eligibility->failed_only_sharp_gate) {
    AppendJoinedNote(
        *path_fit_note,
        BuildReplacementRetrySkippedSharpCornerGateNote(
            request.candidate_key_count, request.original_key_count));
    return true;
  }
  if (!request.retry_eligibility->retry_key_gate) {
    AppendJoinedNote(
        *path_fit_note,
        BuildReplacementRetrySkippedKeyGateNote(
            request.candidate_key_count, request.original_key_count));
    return true;
  }
  return false;
}

ReplacementDecisionApplyResult ApplyReplacementInitialOrFallbackDecision(
    const ReplacementDecisionApplyRequest& request,
    PropertyKeys* property_keys,
    PropertySamples* property_samples,
    PropertyKeys* original_keys) {
  ReplacementDecisionApplyResult result;
  if (!request.verdict->use_candidate &&
      !request.replacement_retry_accepted) {
    const std::string combined = BuildReplacementRejectedFallbackNote(
        request.verdict->decision_note,
        *request.source_validation_note,
        static_cast<int>(original_keys->keys.size()));
    AppendJoinedNote(original_keys->notes, combined);
    *property_keys = std::move(*original_keys);
    *property_samples = *request.original_property_samples;
    return result;
  }

  if (!request.replacement_retry_accepted) {
    ApplyReplacementValidationSummaryToKeys(
        *request.source_validation, *request.validation_summary, property_keys);
    const std::string combined = BuildReplacementAcceptedInitialNote(
        *request.source_validation_note, request.verdict->decision_note);
    if (!combined.empty()) {
      AppendJoinedNote(property_keys->notes, combined);
    }
    result.initial_candidate_accepted = request.verdict->use_candidate;
  }
  return result;
}

}  // namespace bbsolver
