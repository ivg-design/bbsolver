#pragma once

#include "bbsolver/domain.hpp"
#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/path/replacement/path_replacement_acceptance.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"
#include "bbsolver/shape/sharp_corner_policy.hpp"

#include <cstddef>
#include <string>

namespace bbsolver {

class ProgressWriter;
struct SolveOptions;

struct ReplacementRetryLoopRequest {
  const PropertySamples* original_property_samples = nullptr;
  const SolverConfig* config = nullptr;
  const CompInfo* comp = nullptr;
  const SolveOptions* options = nullptr;
  const ProgressWriter* progress = nullptr;
  CancelFn cancel_fn = {};
  std::size_t property_idx = 0;
  std::size_t property_count = 1;
  bool visible_outline_reference = false;
  int replacement_fitted_vertices = 0;
  int replacement_source_min_vertices = 0;
  int candidate_key_count = 0;
  const PropertyKeys* original_keys = nullptr;
  const PathTemporalValidationOptions* validation_options = nullptr;
  const PathTemporalValidationResult* source_validation = nullptr;
  const SharpCornerValidationResult* source_sharp_validation = nullptr;
  const ReplacementValidationSummary* validation_summary = nullptr;
  const ReplacementAcceptanceVerdict* verdict = nullptr;
};

struct ReplacementRetryLoopResult {
  bool accepted = false;
  bool cancelled = false;
  const char* cancel_phase = "";
  int replacement_fitted_vertices = 0;
};

ReplacementRetryLoopResult TryReplacementRetryLoop(
    const ReplacementRetryLoopRequest& request,
    PropertyKeys* property_keys,
    PropertySamples* property_samples,
    std::string* path_fit_note);

}  // namespace bbsolver
