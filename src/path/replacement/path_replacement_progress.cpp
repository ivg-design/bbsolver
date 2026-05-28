#include "bbsolver/path/replacement/path_replacement_progress.hpp"
#include "bbsolver/domain.hpp"

#include <string>
#include <cstddef>
#include "bbsolver/dp/dp_placer.hpp"
#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>

#include "bbsolver/progress/progress.hpp"

namespace bbsolver {

nlohmann::json ReplacementBaselineStartProgressEvent(
    const PropertySamples& original,
    std::size_t property_idx,
    std::size_t property_count) {
  return PropertyProgressEvent(
      "path_replacement_baseline_start",
      "Solving original temporal fallback for " +
          ProgressPropertyLabel(original),
      property_idx,
      property_count,
      0.71,
      original);
}

nlohmann::json ReplacementBaselinePlacementProgressEvent(
    const PropertySamples& original,
    std::size_t property_idx,
    std::size_t property_count,
    const PlacementProgress& placement) {
  return PlacementProgressEvent(
      "path_replacement_baseline_progress",
      "Solving original fallback placement",
      property_idx,
      property_count,
      0.71,
      0.73,
      original,
      placement);
}

nlohmann::json ReplacementBaselineDoneProgressEvent(
    const PropertySamples& original,
    std::size_t property_idx,
    std::size_t property_count,
    std::size_t key_count,
    double max_error,
    bool converged) {
  return {
      {"event", "path_replacement_baseline_done"},
      {"phase", "Original temporal fallback solved for " +
                    ProgressPropertyLabel(original)},
      {"progress", SolveProgressForPropertyStage(
                       property_idx, property_count, 0.73)},
      {"id", original.property.id},
      {"display_name", ProgressPropertyLabel(original)},
      {"i", property_idx},
      {"n", property_count},
      {"K", key_count},
      {"max_err", max_error},
      {"converged", converged},
  };
}

nlohmann::json ReplacementValidationStartProgressEvent(
    const PropertySamples& candidate,
    std::size_t property_idx,
    std::size_t property_count) {
  return PropertyProgressEvent(
      "path_validation_start",
      "Validating replacement outline for " +
          ProgressPropertyLabel(candidate),
      property_idx,
      property_count,
      0.74,
      candidate);
}

nlohmann::json ReplacementRetryStartProgressEvent(
    const PropertySamples& original,
    std::size_t property_idx,
    std::size_t property_count,
    int retry,
    int target_vertices) {
  return {
      {"event", "replacement_retry_start"},
      {"phase", "Retrying replacement topology " +
                    std::to_string(target_vertices) + " vertices for " +
                    ProgressPropertyLabel(original)},
      {"progress", SolveProgressForPropertyStage(
                       property_idx, property_count, 0.82)},
      {"id", original.property.id},
      {"display_name", ProgressPropertyLabel(original)},
      {"i", property_idx},
      {"n", property_count},
      {"retry", retry},
      {"target_vertices", target_vertices},
  };
}

nlohmann::json ReplacementRetryDoneProgressEvent(
    const PropertySamples& original,
    std::size_t property_idx,
    std::size_t property_count,
    int retry,
    int target_vertices,
    int fitted_vertices,
    double max_outline_error,
    bool accepted,
    bool sharp_corners_ok) {
  return {
      {"event", "replacement_retry_done"},
      {"phase", std::string("Replacement retry ") +
                    (accepted ? "accepted": "rejected") + " for " +
                    ProgressPropertyLabel(original)},
      {"progress", SolveProgressForPropertyStage(
                       property_idx, property_count, 0.86)},
      {"id", original.property.id},
      {"display_name", ProgressPropertyLabel(original)},
      {"i", property_idx},
      {"n", property_count},
      {"retry", retry},
      {"target_vertices", target_vertices},
      {"fitted_vertices", fitted_vertices},
      {"max_outline_error", max_outline_error},
      {"accepted", accepted},
      {"sharp_corners_ok", sharp_corners_ok},
  };
}

nlohmann::json ReplacementFastVertexValidationDoneProgressEvent(
    const PropertySamples& candidate,
    std::size_t property_idx,
    std::size_t property_count,
    double max_outline_error,
    int samples_checked,
    std::size_t candidate_key_count,
    bool sharp_corners_ok) {
  return {
      {"event", "path_validation_done"},
      {"phase", "Replacement outline validated for " +
                    ProgressPropertyLabel(candidate)},
      {"progress", SolveProgressForPropertyStage(
                       property_idx, property_count, 0.80)},
      {"id", candidate.property.id},
      {"display_name", ProgressPropertyLabel(candidate)},
      {"i", property_idx},
      {"n", property_count},
      {"ok", true},
      {"max_outline_error", max_outline_error},
      {"samples_checked", samples_checked},
      {"candidate_keys", candidate_key_count},
      {"source_keys", nullptr},
      {"baseline_temporal_skipped", true},
      {"sharp_corners_ok", sharp_corners_ok},
  };
}

nlohmann::json ReplacementValidationDoneProgressEvent(
    const PropertySamples& candidate,
    std::size_t property_idx,
    std::size_t property_count,
    bool ok,
    double max_outline_error,
    int samples_checked,
    std::size_t candidate_key_count,
    std::size_t source_key_count,
    bool sharp_corners_ok) {
  return {
      {"event", "path_validation_done"},
      {"phase", std::string("Replacement outline ") +
                    (ok ? "validated for ": "rejected for ") +
                    ProgressPropertyLabel(candidate)},
      {"progress", SolveProgressForPropertyStage(
                       property_idx, property_count, 0.80)},
      {"id", candidate.property.id},
      {"display_name", ProgressPropertyLabel(candidate)},
      {"i", property_idx},
      {"n", property_count},
      {"ok", ok},
      {"max_outline_error", max_outline_error},
      {"samples_checked", samples_checked},
      {"candidate_keys", candidate_key_count},
      {"source_keys", source_key_count},
      {"sharp_corners_ok", sharp_corners_ok},
  };
}

}  // namespace bbsolver
