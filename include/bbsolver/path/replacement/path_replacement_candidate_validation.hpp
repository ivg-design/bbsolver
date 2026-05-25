#pragma once

#include "bbsolver/domain.hpp"
#include "bbsolver/path/replacement/path_replacement_acceptance.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"
#include "bbsolver/shape/sharp_corner_policy.hpp"

#include <cstddef>
#include <string>

namespace bbsolver {

class ProgressWriter;

struct ReplacementCandidateValidationRequest {
  const PropertySamples* original_property_samples = nullptr;
  const PropertySamples* property_samples = nullptr;
  const PropertyKeys* property_keys = nullptr;
  const PropertyKeys* original_keys = nullptr;
  const SolverConfig* config = nullptr;
  const ProgressWriter* progress = nullptr;
  std::size_t property_idx = 0;
  std::size_t property_count = 1;
  bool visible_outline_reference = false;
  int replacement_fitted_vertices = 0;
  int replacement_original_max_vertices = 0;
};

struct ReplacementCandidateValidationResult {
  PathTemporalValidationOptions validation_options;
  PathTemporalValidationResult source_validation;
  SharpCornerValidationResult source_sharp_validation;
  ReplacementValidationSummary validation_summary;
  ReplacementAcceptanceVerdict verdict;
  std::string source_validation_note;
};

ReplacementCandidateValidationResult ValidateReplacementCandidateAgainstBaseline(
    const ReplacementCandidateValidationRequest& request);

}  // namespace bbsolver
