#pragma once

#include "bbsolver/domain.hpp"

#include <cstddef>
#include <functional>
#include <string>

namespace bbsolver {

class ProgressWriter;
struct SolveOptions;

struct PostTemporalReplacementRequest {
  const PropertySamples* original_property_samples = nullptr;
  const SolverConfig* config = nullptr;
  const CompInfo* comp = nullptr;
  const SolveOptions* options = nullptr;
  const ProgressWriter* progress = nullptr;
  std::function<bool()> cancel_fn;
  std::size_t property_idx = 0;
  std::size_t property_count = 1;
  bool visible_outline_reference = false;
  bool replacement_output_accepted = false;
  bool replacement_fast_vertex_preference_accepted = false;
  int replacement_fitted_vertices = 0;
  int replacement_original_max_vertices = 0;
  int replacement_source_min_vertices = 0;
  int replacement_estimated_candidate_keys = 0;
  int replacement_estimated_original_keys = 0;
};

struct PostTemporalReplacementResult {
  bool cancelled = false;
  std::string cancel_phase;
  bool replacement_output_accepted = false;
  bool replacement_fast_vertex_preference_accepted = false;
  int replacement_fitted_vertices = 0;
};

PostTemporalReplacementResult ProcessPostTemporalReplacement(
    const PostTemporalReplacementRequest& request,
    PropertyKeys* property_keys,
    PropertySamples* property_samples,
    std::string* path_fit_note);

}  // namespace bbsolver
