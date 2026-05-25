#pragma once

#include "bbsolver/domain.hpp"

#include <cstddef>

namespace bbsolver {

class ProgressWriter;

struct ReplacementFastVertexAcceptanceRequest {
  const PropertySamples* original_property_samples = nullptr;
  const PropertySamples* property_samples = nullptr;
  const SolverConfig* config = nullptr;
  const ProgressWriter* progress = nullptr;
  std::size_t property_idx = 0;
  std::size_t property_count = 1;
  bool visible_outline_reference = false;
  int replacement_fitted_vertices = 0;
  int replacement_original_max_vertices = 0;
  int replacement_estimated_candidate_keys = 0;
  int replacement_estimated_original_keys = 0;
};

struct ReplacementFastVertexAcceptanceResult {
  bool accepted = false;
  int candidate_key_count = 0;
  int source_sample_count = 0;
};

ReplacementFastVertexAcceptanceResult
TryAcceptReplacementFastVertexPreference(
    const ReplacementFastVertexAcceptanceRequest& request,
    PropertyKeys* property_keys);

}  // namespace bbsolver
