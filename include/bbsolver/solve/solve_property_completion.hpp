#pragma once

#include "bbsolver/domain.hpp"

#include <cstddef>
#include <functional>
#include <string>

namespace bbsolver {

class ProgressWriter;

struct PropertyCompletionRequest {
  const PropertySamples* original_property_samples = nullptr;
  PropertySamples* property_samples = nullptr;
  PropertyKeys* property_keys = nullptr;
  KeyBundle* keys = nullptr;
  const SolverConfig* config = nullptr;
  const CompInfo* comp = nullptr;
  const ProgressWriter* progress = nullptr;
  std::string* path_fit_note = nullptr;
  std::function<bool()> cancel_fn;
  std::size_t property_idx = 0;
  std::size_t property_count = 0;
  bool temporal_optimization_enabled = false;
  bool near_optimal_fast_path_applied = false;
  bool decompose_paths = false;
  bool visible_outline_reference = false;
  bool replacement_output_accepted = false;
  bool replacement_fast_vertex_preference_accepted = false;
  bool emit_landmark_subpaths = false;
  double prop_ms = 0.0;
};

struct PropertyCompletionResult {
  bool cancelled = false;
  std::string cancel_phase;
};

PropertyCompletionResult CompleteSolvedProperty(
    const PropertyCompletionRequest& request);

}  // namespace bbsolver
