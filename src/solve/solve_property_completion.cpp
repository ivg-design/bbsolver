#include "bbsolver/solve/solve_property_completion.hpp"

#include <stdexcept>

#include "bbsolver/solve/solve_property_output.hpp"
#include "bbsolver/solve/solve_property_post_processing.hpp"

namespace bbsolver {
namespace {

void RequirePropertyCompletionRequest(
    const PropertyCompletionRequest& request) {
  if (request.original_property_samples == nullptr ||
      request.property_samples == nullptr ||
      request.property_keys == nullptr ||
      request.keys == nullptr ||
      request.config == nullptr ||
      request.comp == nullptr ||
      request.progress == nullptr ||
      request.path_fit_note == nullptr ||
      !request.cancel_fn) {
    throw std::invalid_argument("property completion request is incomplete");
  }
}

}  // namespace

PropertyCompletionResult CompleteSolvedProperty(
    const PropertyCompletionRequest& request) {
  RequirePropertyCompletionRequest(request);

  PropertyPostSolveProcessingRequest post_request;
  post_request.original_property_samples = request.original_property_samples;
  post_request.property_samples = request.property_samples;
  post_request.property_keys = request.property_keys;
  post_request.config = request.config;
  post_request.comp = request.comp;
  post_request.progress = request.progress;
  post_request.path_fit_note = request.path_fit_note;
  post_request.cancel_fn = request.cancel_fn;
  post_request.property_idx = request.property_idx;
  post_request.property_count = request.property_count;
  post_request.temporal_optimization_enabled =
      request.temporal_optimization_enabled;
  post_request.near_optimal_fast_path_applied =
      request.near_optimal_fast_path_applied;
  post_request.decompose_paths = request.decompose_paths;
  post_request.visible_outline_reference = request.visible_outline_reference;
  post_request.replacement_output_accepted =
      request.replacement_output_accepted;
  post_request.replacement_fast_vertex_preference_accepted =
      request.replacement_fast_vertex_preference_accepted;
  post_request.prop_ms = request.prop_ms;
  const PropertyPostSolveProcessingResult post_result =
      ProcessSolvedPropertyPostSolve(post_request);
  if (post_result.cancelled) {
    return {true, post_result.cancel_phase};
  }

  PropertyOutputRequest output_request;
  output_request.emit_landmark_subpaths = request.emit_landmark_subpaths;
  output_request.replacement_output_accepted =
      request.replacement_output_accepted;
  output_request.property_samples = request.property_samples;
  output_request.property_keys = request.property_keys;
  output_request.keys = request.keys;
  output_request.config = request.config;
  output_request.progress = request.progress;
  output_request.cancel_fn = request.cancel_fn;
  output_request.property_idx = request.property_idx;
  output_request.property_count = request.property_count;
  output_request.prop_ms = request.prop_ms;
  AppendSolvedPropertyOutput(output_request);
  return {};
}

}  // namespace bbsolver
