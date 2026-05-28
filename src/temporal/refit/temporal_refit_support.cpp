#include "bbsolver/temporal/refit/temporal_refit_support.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/temporal/refit/temporal_refit.hpp"

#include <string>

namespace bbsolver {

void EmitTemporalRefitProgress(const TemporalRefitOptions& options,
                               const std::string& stage,
                               int step_index,
                               int step_total) {
  if (!options.progress_fn) {
    return;
  }
  PlacementProgress event;
  event.stage = stage;
  event.step_index = step_index;
  event.step_total = step_total;
  // sample_index / samples / segments_tried / segments_feasible remain at
  // their defaults until the inner DP loop reports native refit progress.
  options.progress_fn(event);
}

bool TemporalRefitCancelled(const TemporalRefitOptions& options) {
  return options.cancel_fn && options.cancel_fn();
}

std::string BuildTemporalRefitNotes(const TemporalRefitResult& result) {
  std::string out =
      "temporal_refit_attempted=" +
      std::string(result.attempted ? "true": "false") +
      "; temporal_refit_input_keys=" +
      std::to_string(result.input_key_count) +
      "; temporal_refit_output_keys=" +
      std::to_string(result.output_key_count);
  if (result.accepted) {
    out += "; temporal_refit_accepted=true";
    out += "; temporal_refit_reduction=" +
           std::to_string(result.input_key_count - result.output_key_count);
  } else if (!result.rejection_reason.empty()) {
    out += "; temporal_refit_rejected=" + result.rejection_reason;
  }
  if (result.attempted || result.max_err > 0.0) {
    out += "; temporal_refit_max_err=" + std::to_string(result.max_err);
  }
  if (result.attempted || result.max_err_screen_px > 0.0) {
    out += "; temporal_refit_max_err_screen_px=" +
           std::to_string(result.max_err_screen_px);
  }
  return out;
}

std::string TemporalRefitValidationNote(const PropertySamples& source) {
  if (source.property.kind == ValueKind::Custom &&
      source.property.units_label == "shape_flat") {
    return "; temporal_refit_validation=shape_outline";
  }
  if (source.property.kind == ValueKind::Custom) {
    return "; temporal_refit_validation=unsupported_custom";
  }
  return "; temporal_refit_validation=numeric_linf";
}

}  // namespace bbsolver
