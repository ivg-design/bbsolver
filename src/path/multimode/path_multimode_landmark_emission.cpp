#include "bbsolver/path/multimode/path_multimode_landmark_emission.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/path/multimode/path_multimode_geometry.hpp"
#include "bbsolver/path/multimode/path_multimode_landmark_diagnostics.hpp"
#include "bbsolver/path/multimode/path_multimode_landmark_temporal_solve.hpp"
#include "bbsolver/path/multimode/path_multimode_notes.hpp"
#include "bbsolver/path/multimode/path_multimode_reconstruction.hpp"
#include "bbsolver/path/multimode/path_multimode_region_candidate.hpp"
#include "bbsolver/path/multimode/path_multimode_solver.hpp"

#include <string>
#include <utility>
#include <cstddef>
#include <vector>

namespace bbsolver {
namespace path_multimode {

std::string LandmarkSubpathNote(
    int subpath_index,
    int subpath_count,
    VertexRegion region,
    const std::vector<int>& anchors,
    int source_sample_count,
    int max_gap,
    int region_segment_checks,
    int inserted_samples,
    int temporal_segment_checks,
    const std::string& temporal_status,
    const LandmarkSubpathReconstructionResult& reconstruction,
    int source_vertex_count,
    bool visible_shape_channel) {
  const std::string prefix =
      visible_shape_channel ? "shape_channel_subpath" : "landmark_subpath";
  std::string note =
      prefix + "; subpath_index=" + std::to_string(subpath_index) +
         "; subpath_count=" + std::to_string(subpath_count) +
         "; vertex_range=" + std::to_string(region.first_vertex) + "-" +
         std::to_string(region.end_vertex) +
         "; key_count=" + std::to_string(static_cast<int>(anchors.size())) +
         "; source_samples=" + std::to_string(source_sample_count) +
         "; max_gap_samples=" + std::to_string(max_gap) +
         "; region_segment_checks=" +
         std::to_string(region_segment_checks) +
         "; subpath_anchor_refinements=" +
         std::to_string(inserted_samples) +
         "; subpath_temporal_solver=" + temporal_status +
         "; subpath_temporal_segment_checks=" +
         std::to_string(temporal_segment_checks) +
         "; anchors=" + JoinAnchorIndices(anchors) +
         "; subpath_reconstruction_ok=" +
         std::string(reconstruction.ok ? "true" : "false") +
         "; subpath_reconstruction_max_outline_error=" +
         std::to_string(reconstruction.max_outline_error) +
         "; subpath_reconstruction_worst_sample=" +
         std::to_string(reconstruction.worst_sample_idx) +
         "; subpath_reconstruction_worst_t=" +
         std::to_string(reconstruction.worst_t_sec) +
         "; subpath_reconstruction_samples=" +
         std::to_string(reconstruction.samples_checked);
  if (visible_shape_channel) {
    const bool full_shape_group =
        subpath_count == 1 &&
        region.first_vertex == 0 &&
        region.end_vertex == source_vertex_count;
    note +=
        "; visible_channel=true"
        "; visible_protocol=shape_channel_subpath_v1"
        "; visible_replaces_source=true";
    if (full_shape_group) {
      note +=
          "; visibility=shape_group_full"
          "; visible_renderable=true"
          "; visible_channel_mode=shape_group_full";
    } else {
      note +=
          "; visibility=probe_only"
          "; visible_renderable=false"
          "; visible_channel_mode=partial_subpath_probe"
          "; reason=partial_shape_channel_not_ae_ready";
    }
  }
  return note;
}

LandmarkRegionEmissionResult EvaluateLandmarkRegionEmission(
    const PropertySamples& reduced,
    VertexRegion region,
    const ShapeFlatLandmarkSubpathOptions& options,
    int max_gap) {
  LandmarkRegionEmissionResult result;
  result.region = region;
  if (options.cancel_fn && options.cancel_fn()) {
    result.temporal_status = "cancelled";
    return result;
  }

  ShapeFlatMultiModeOptions region_options;
  region_options.max_regions = options.max_regions;
  region_options.max_gap_samples = options.max_gap_samples;
  region_options.region_tolerance = options.region_tolerance;
  region_options.max_region_segment_checks = options.max_region_segment_checks;
  region_options.cancel_fn = options.cancel_fn;

  RegionSolveResult region_result =
      SolveRegionAnchors(reduced, region, region_options, max_gap,
                         &result.region_segment_checks);
  if (region_result.budget_exceeded || region_result.anchors.empty()) {
    result.temporal_status = region_result.budget_exceeded
                                 ? "region_budget_exceeded"
                                 : "region_anchor_failed";
    return result;
  }

  const PropertySamples region_samples =
      BuildLandmarkRegionSamples(reduced, region);
  if (region_samples.samples.size() != reduced.samples.size()) {
    result.temporal_status = "region_sample_failed";
    return result;
  }

  const LandmarkSubpathTemporalResult temporal =
      SolveLandmarkRegionTemporal(region_samples,
                                  options.region_tolerance,
                                  max_gap,
                                  options.max_region_segment_checks,
                                  options.cancel_fn);
  result.temporal_segment_checks = temporal.segment_checks;
  if (temporal.notes == "cancelled") {
    result.temporal_status = "cancelled";
    return result;
  }

  const LandmarkSubpathRefinementResult refinement =
      RefineLandmarkSubpathAnchors(reduced,
                                   region,
                                   region_result.anchors,
                                   options.region_tolerance,
                                   options.cancel_fn);
  if (!refinement.ok && !temporal.ok) {
    result.temporal_status = temporal.notes.empty()
                                 ? "region_refinement_failed"
                                 : "fallback_" + temporal.notes;
    return result;
  }
  if (options.cancel_fn && options.cancel_fn()) {
    result.temporal_status = "cancelled";
    return result;
  }

  const bool use_temporal =
      temporal.ok &&
      (!refinement.ok || temporal.keys.keys.size() < refinement.anchors.size());
  if (use_temporal) {
    result.keys = temporal.keys;
    result.anchors = SampleIndicesFromKeys(result.keys);
    result.reconstruction = temporal.reconstruction;
    result.temporal_status = "accepted";
  } else {
    result.keys.property_id = reduced.property.id;
    result.keys.converged = true;
    result.inserted_samples = refinement.inserted_samples;
    result.anchors = refinement.anchors;
    result.reconstruction = refinement.reconstruction;
    result.temporal_status = temporal.notes.empty()
                                 ? "fallback"
                                 : "fallback_" + temporal.notes;
    result.keys.keys.reserve(refinement.anchors.size());
    for (std::size_t anchor_idx = 0;
         anchor_idx < refinement.anchors.size();
         ++anchor_idx) {
      const int sample_idx = refinement.anchors[anchor_idx];
      Key key = MakeLinearShapeKey(reduced,
                                   sample_idx,
                                   anchor_idx == 0,
                                   anchor_idx + 1 == refinement.anchors.size());
      key.v = ShapeFlatRegion(
          reduced.samples[static_cast<std::size_t>(sample_idx)].v, region);
      if (key.v.empty()) {
        result.temporal_status = "region_key_failed";
        return result;
      }
      result.keys.keys.push_back(std::move(key));
    }
    result.keys.segments.reserve(result.keys.keys.size() > 1
                                     ? result.keys.keys.size() - 1
                                     : 0);
    for (std::size_t anchor_idx = 0;
         anchor_idx + 1 < refinement.anchors.size();
         ++anchor_idx) {
      SegmentReport report;
      report.start_idx = refinement.anchors[anchor_idx];
      report.end_idx = refinement.anchors[anchor_idx + 1];
      report.reason = "landmark_subpath";
      result.keys.segments.push_back(std::move(report));
    }
  }

  if (!result.reconstruction.ok || result.anchors.empty()) {
    result.temporal_status = "subpath_reconstruction_failed";
    return result;
  }
  if (options.diagnose_dense_runs) {
    result.dense_run_note =
        DiagnoseDenseSubpathRuns(region_samples,
                                 result.anchors,
                                 options.region_tolerance,
                                 max_gap,
                                 &result.dense_run_checks);
  }
  if (options.diagnose_segment_gaps) {
    const SegmentGapDiagnostic segment_diagnostic =
        DiagnoseAcceptedSegmentGaps(region_samples,
                                    result.anchors,
                                    options.region_tolerance,
                                    max_gap);
    result.segment_gap_note = segment_diagnostic.gap_histogram;
    result.segment_gap_max = segment_diagnostic.max_gap;
    result.segment_lower_bound_note = segment_diagnostic.lower_bound_top;
    result.segment_rejection_note = segment_diagnostic.rejection_top;
    result.segment_rejection_checks = segment_diagnostic.rejection_checks;
  }
  if (options.diagnose_outlier_slots) {
    const OutlierSlotAnalysis outliers =
        AnalyzeDenseRunOutlierSlots(region_samples, result.anchors);
    result.outlier_slot_checks = outliers.checks;
    result.outlier_slot_note = FormatOutlierSlotNote(outliers);
    result.outlier_slots = outliers.slots;
  }
  result.keys.property_id = reduced.property.id;
  result.keys.converged = true;
  result.ok = true;
  return result;
}

}  // namespace path_multimode
}  // namespace bbsolver
