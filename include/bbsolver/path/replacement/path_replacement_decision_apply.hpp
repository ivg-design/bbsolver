#pragma once

#include "bbsolver/domain.hpp"
#include "bbsolver/path/replacement/path_replacement_acceptance.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"

#include <string>

namespace bbsolver {

struct ReplacementDecisionApplyRequest {
  const ReplacementAcceptanceVerdict* verdict = nullptr;
  const PathTemporalValidationResult* source_validation = nullptr;
  const ReplacementValidationSummary* validation_summary = nullptr;
  const std::string* source_validation_note = nullptr;
  const PropertySamples* original_property_samples = nullptr;
  bool replacement_retry_accepted = false;
};

struct ReplacementDecisionApplyResult {
  bool initial_candidate_accepted = false;
};

struct ReplacementRetrySkippedNoteRequest {
  const ReplacementAcceptanceVerdict* verdict = nullptr;
  const ReplacementRetryEligibility* retry_eligibility = nullptr;
  int candidate_key_count = 0;
  int original_key_count = 0;
};

bool AppendReplacementRetrySkippedNote(
    const ReplacementRetrySkippedNoteRequest& request,
    std::string* path_fit_note);

ReplacementDecisionApplyResult ApplyReplacementInitialOrFallbackDecision(
    const ReplacementDecisionApplyRequest& request,
    PropertyKeys* property_keys,
    PropertySamples* property_samples,
    PropertyKeys* original_keys);

}  // namespace bbsolver
