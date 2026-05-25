#include "bbsolver/solve/solve_property_temporal_result.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/progress/progress.hpp"

namespace bbsolver {

PropertyTemporalSolveResult ReportPropertyTemporalSolveResult(
    const PropertyTemporalSolveResultRequest& request) {
  const PropertySamples& property_samples = *request.property_samples;
  const PropertyKeys& property_keys = *request.property_keys;
  const ProgressWriter& progress = *request.progress;

  progress.Emit({
      {"event", "temporal_solve_done"},
      {"phase", "Temporal solve finished for " +
                    ProgressPropertyLabel(property_samples)},
      {"progress", SolveProgressForPropertyStage(
                       request.property_idx, request.property_count, 0.70)},
      {"id", property_samples.property.id},
      {"display_name", ProgressPropertyLabel(property_samples)},
      {"i", request.property_idx},
      {"n", request.property_count},
      {"K", property_keys.keys.size()},
      {"max_err", property_keys.max_err},
      {"ms", request.prop_ms},
  });

  PropertyTemporalSolveResult result;
  if (property_keys.notes == "cancelled" ||
      (request.cancel_fn && request.cancel_fn())) {
    result.cancelled = true;
    result.cancel_phase = "temporal_solve";
  }
  return result;
}

}  // namespace bbsolver
