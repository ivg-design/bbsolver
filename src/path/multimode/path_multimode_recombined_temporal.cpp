#include "bbsolver/path/multimode/path_multimode_recombined_temporal.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/path/multimode/path_multimode_landmark_temporal_solve.hpp"
#include "bbsolver/path/multimode/path_multimode_notes.hpp"
#include "bbsolver/path/multimode/path_multimode_solver.hpp"
#include "bbsolver/path/multimode/path_multimode_geometry.hpp"
#include "bbsolver/path/multimode/path_multimode_reconstruction.hpp"
#include "bbsolver/path/multimode/path_multimode_region_candidate.hpp"
#include "bbsolver/path/multimode/path_multimode_temporal.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <cstddef>

namespace bbsolver {
namespace path_multimode {

RecombinedRegionTemporalResult TryRecombinedRegionTemporalCandidate(
    const PropertySamples& original,
    const PropertySamples& reduced,
    const std::vector<VertexRegion>& regions,
    const ShapeFlatMultiModeOptions& options,
    int max_gap,
    int union_key_count) {
  RecombinedRegionTemporalResult result;
  if (regions.size() < 2 || union_key_count <= 2) {
    result.note =
        "recombined_region_temporal=not_selected; reason=no_key_reduction_opportunity";
    return result;
  }
  result.attempted = true;

  const int n = static_cast<int>(reduced.samples.size());
  const int temporal_budget_per_region =
      options.max_region_segment_checks > 0
          ? std::max(1, options.max_region_segment_checks /
                            std::max(1, static_cast<int>(regions.size())))
          : std::max(128, n * std::max(1, max_gap) * 2);
  const double tolerance = std::max(options.region_tolerance, 0.0);
  const ShapeMorphProgressBandOptions band_options =
      LandmarkBandOptions(tolerance, max_gap);

  std::vector<PropertySamples> region_samples;
  std::vector<PropertyKeys> region_keys;
  region_samples.reserve(regions.size());
  region_keys.reserve(regions.size());
  std::set<int> anchor_set;
  anchor_set.insert(0);
  anchor_set.insert(n - 1);

  int region_idx = 0;
  for (VertexRegion region : regions) {
    if (options.cancel_fn && options.cancel_fn()) {
      result.note = "cancelled";
      return result;
    }
    PropertySamples samples = BuildLandmarkRegionSamples(reduced, region);
    if (samples.samples.empty()) {
      result.note =
          "recombined_region_temporal=not_selected; reason=region_sample_build_failed_" +
          std::to_string(region_idx);
      return result;
    }
    LandmarkSubpathTemporalResult temporal =
        SolveLandmarkRegionTemporal(samples,
                                    tolerance,
                                    max_gap,
                                    temporal_budget_per_region,
                                    options.cancel_fn);
    result.temporal_segment_checks += temporal.segment_checks;
    if (!temporal.ok || temporal.keys.keys.empty()) {
      if (temporal.notes == "cancelled") {
        result.note = "cancelled";
        return result;
      }
      result.note =
          "recombined_region_temporal=not_selected; reason=region_temporal_failed_" +
          std::to_string(region_idx) +
          "; status=" + (temporal.notes.empty() ? "unknown" : temporal.notes) +
          "; recombined_region_temporal_segment_checks=" +
          std::to_string(result.temporal_segment_checks);
      return result;
    }
    if (!result.region_key_counts.empty()) {
      result.region_key_counts += ",";
    }
    result.region_key_counts +=
        std::to_string(static_cast<int>(temporal.keys.keys.size()));

    std::vector<int> sample_indices = SampleIndicesFromKeys(temporal.keys);
    if (sample_indices.empty()) {
      result.note =
          "recombined_region_temporal=not_selected; reason=region_temporal_indices_empty_" +
          std::to_string(region_idx);
      return result;
    }
    anchor_set.insert(sample_indices.begin(), sample_indices.end());
    region_samples.push_back(std::move(samples));
    region_keys.push_back(std::move(temporal.keys));
    ++region_idx;
  }

  result.anchors.assign(anchor_set.begin(), anchor_set.end());
  if (static_cast<int>(result.anchors.size()) >= union_key_count) {
    result.note =
        "recombined_region_temporal=not_selected; reason=not_fewer_than_union" +
        std::string("; recombined_region_temporal_keys=") +
        std::to_string(static_cast<int>(result.anchors.size())) +
        "; recombined_region_temporal_region_key_counts=" +
        result.region_key_counts +
        "; recombined_region_temporal_segment_checks=" +
        std::to_string(result.temporal_segment_checks);
    return result;
  }
  if (CandidateKeyBudgetExceeded(static_cast<int>(result.anchors.size()),
                                 n,
                                 options.max_candidate_key_ratio)) {
    result.note =
        "recombined_region_temporal=not_selected; reason=candidate_key_budget_exceeded" +
        std::string("; recombined_region_temporal_keys=") +
        std::to_string(static_cast<int>(result.anchors.size())) +
        "; max_candidate_key_ratio=" +
        std::to_string(options.max_candidate_key_ratio);
    return result;
  }

  PropertyKeys candidate;
  candidate.property_id = reduced.property.id;
  candidate.converged = false;
  candidate.keys.reserve(result.anchors.size());
  for (std::size_t anchor_idx = 0; anchor_idx < result.anchors.size();
       ++anchor_idx) {
    const int sample_idx = result.anchors[anchor_idx];
    std::vector<double> full =
        reduced.samples[static_cast<std::size_t>(sample_idx)].v;
    for (std::size_t region_i = 0; region_i < regions.size(); ++region_i) {
      std::vector<double> region_value =
          EvaluateTemporalShapeAtSample(region_samples[region_i],
                                        region_keys[region_i],
                                        sample_idx,
                                        band_options);
      if (region_value.empty() ||
          !InsertShapeFlatRegion(&full, regions[region_i], region_value)) {
        result.note =
            "recombined_region_temporal=not_selected; reason=recombine_failed" +
            std::string("; recombined_region_temporal_keys=") +
            std::to_string(static_cast<int>(result.anchors.size()));
        return result;
      }
    }
    candidate.keys.push_back(
        MakeShapeKeyFromValue(reduced,
                              sample_idx,
                              std::move(full),
                              anchor_idx == 0,
                              anchor_idx + 1 == result.anchors.size()));
  }

  candidate.segments.reserve(result.anchors.size() - 1);
  for (std::size_t anchor_idx = 0; anchor_idx + 1 < result.anchors.size();
       ++anchor_idx) {
    SegmentReport report;
    report.start_idx = result.anchors[anchor_idx];
    report.end_idx = result.anchors[anchor_idx + 1];
    report.reason =
        "replacement_shape_multimode_recombined_region_temporal_linear";
    candidate.segments.push_back(std::move(report));
  }

  PathTemporalValidationOptions validation_options;
  validation_options.frame_fit_options = options.frame_fit_options;
  const PathTemporalValidationResult validation =
      ValidatePathTemporalCandidate(original, candidate, validation_options);
  if (!validation.ok) {
    result.note =
        "recombined_region_temporal=not_selected; reason=source_outline_validation_failed" +
        std::string("; recombined_region_temporal_keys=") +
        std::to_string(static_cast<int>(result.anchors.size())) +
        "; recombined_region_temporal_region_key_counts=" +
        result.region_key_counts +
        "; recombined_region_temporal_segment_checks=" +
        std::to_string(result.temporal_segment_checks) +
        "; source_outline_validation: " + validation.notes;
    return result;
  }

  candidate.converged = true;
  candidate.max_err = validation.max_outline_error;
  candidate.max_err_screen_px = validation.max_outline_error;
  for (SegmentReport& report : candidate.segments) {
    report.max_err = validation.max_outline_error;
    report.max_err_screen_px = validation.max_outline_error;
    report.rms_err = validation.max_outline_error;
  }
  candidate.notes =
      "shape_multimode_candidate; recombined_region_temporal=accepted" +
      std::string("; recombined_region_temporal_keys=") +
      std::to_string(static_cast<int>(candidate.keys.size())) +
      "; recombined_region_temporal_anchors=" +
      JoinAnchorIndices(result.anchors) +
      "; recombined_region_temporal_region_key_counts=" +
      result.region_key_counts +
      "; recombined_region_temporal_segment_checks=" +
      std::to_string(result.temporal_segment_checks) +
      "; regions=" + std::to_string(static_cast<int>(regions.size())) +
      "; region_ranges=" + JoinRegionRanges(regions) +
      "; union_keys=" + std::to_string(union_key_count) +
      "; source_outline_validation: " + validation.notes;
  result.accepted = true;
  result.keys = std::move(candidate);
  result.note = "recombined_region_temporal=accepted";
  return result;
}

}  // namespace path_multimode
}  // namespace bbsolver
