#include "bbsolver/solve/solve_property_output.hpp"
#include "bbsolver/domain.hpp"

#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstddef>

#include "bbsolver/path/multimode/path_multimode_solver.hpp"
#include "bbsolver/path/config/path_solver_config.hpp"
#include "bbsolver/progress/progress.hpp"
#include "bbsolver/routing/property_classification.hpp"
#include "bbsolver/solve/solver_reporting.hpp"

namespace bbsolver {
namespace {

int InferredOutputDimensions(const PropertySamples& property_samples,
                             const PropertyKeys& property_keys) {
  if (property_keys.dimensions > 0) {
    return property_keys.dimensions;
  }
  for (const Key& key: property_keys.keys) {
    if (!key.v.empty()) {
      return static_cast<int>(key.v.size());
    }
  }
  return std::max(property_samples.property.dimensions, 1);
}

void FillOutputDimensions(const PropertySamples& property_samples,
                          PropertyKeys* property_keys) {
  if (property_keys != nullptr && property_keys->dimensions <= 0) {
    property_keys->dimensions =
        InferredOutputDimensions(property_samples, *property_keys);
  }
}

}  // namespace

void AppendSolvedPropertyOutput(const PropertyOutputRequest& request) {
  const PropertySamples& property_samples = *request.property_samples;
  PropertyKeys& property_keys = *request.property_keys;
  KeyBundle& keys = *request.keys;
  const SolverConfig& config = *request.config;
  const ProgressWriter& progress = *request.progress;

  std::vector<PropertyKeys> landmark_subpath_keys;
  if (request.emit_landmark_subpaths &&
      request.replacement_output_accepted &&
      IsShapeFlatPath(property_samples)) {
    progress.Emit(PropertyProgressEvent(
        "landmark_subpaths_start",
        "Evaluating visible/landmark subpaths for " +
            ProgressPropertyLabel(property_samples),
        request.property_idx,
        request.property_count,
        0.88,
        property_samples));
    ShapeFlatLandmarkSubpathOptions subpath_options;
    subpath_options.enabled = true;
    subpath_options.region_tolerance = EffectivePathTolerance(config);
    subpath_options.cancel_fn = request.cancel_fn;
    landmark_subpath_keys =
        EmitShapeFlatLandmarkSubpathKeys(property_samples, subpath_options);
    if (!landmark_subpath_keys.empty()) {
      const int subpath_count =
          static_cast<int>(landmark_subpath_keys.size());
      const std::string subpath_note =
          "landmark_subpaths_emitted=" + std::to_string(subpath_count);
      AppendJoinedNote(property_keys.notes, subpath_note);
      progress.Emit({
          {"event", "landmark_subpaths"},
          {"phase", "Subpath evaluation finished for " +
                        ProgressPropertyLabel(property_samples)},
          {"progress", SolveProgressForPropertyStage(
                           request.property_idx, request.property_count, 0.94)},
          {"id", property_samples.property.id},
          {"display_name", ProgressPropertyLabel(property_samples)},
          {"i", request.property_idx},
          {"n", request.property_count},
          {"count", subpath_count},
      });
    }
  }

  keys.total_keys += static_cast<int>(property_keys.keys.size());
  keys.total_samples_input += static_cast<int>(property_samples.samples.size());
  FillOutputDimensions(property_samples, &property_keys);
  for (auto& subpath_keys: landmark_subpath_keys) {
    FillOutputDimensions(property_samples, &subpath_keys);
  }
  const std::size_t key_count = property_keys.keys.size();
  const double max_err = property_keys.max_err;
  keys.property_results.push_back(std::move(property_keys));
  for (auto& subpath_keys: landmark_subpath_keys) {
    keys.total_keys += static_cast<int>(subpath_keys.keys.size());
    keys.property_results.push_back(std::move(subpath_keys));
  }
  progress.Emit({
      {"event", "property_done"},
      {"phase", "Solved " + ProgressPropertyLabel(property_samples)},
      {"progress", SolveProgressForPropertyStage(
                       request.property_idx, request.property_count, 0.98)},
      {"id", property_samples.property.id},
      {"display_name", ProgressPropertyLabel(property_samples)},
      {"i", request.property_idx},
      {"n", request.property_count},
      {"K", key_count},
      {"max_err", max_err},
      {"ms", request.prop_ms},
  });
}

}  // namespace bbsolver
