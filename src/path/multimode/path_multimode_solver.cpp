#include "bbsolver/path/multimode/path_multimode_solver.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "bbsolver/path/multimode/path_multimode_geometry.hpp"
#include "bbsolver/path/multimode/path_multimode_input_validation.hpp"
#include "bbsolver/path/multimode/path_multimode_landmark_options.hpp"
#include "bbsolver/path/multimode/path_multimode_landmark_output.hpp"
#include "bbsolver/path/multimode/path_multimode_landmark_partition.hpp"
#include "bbsolver/path/multimode/path_multimode_notes.hpp"
#include "bbsolver/path/multimode/path_multimode_recombined_temporal.hpp"
#include "bbsolver/path/multimode/path_multimode_region_candidate.hpp"
#include "bbsolver/path/multimode/path_multimode_solver_notes.hpp"
#include "bbsolver/path/multimode/path_multimode_temporal.hpp"  // IWYU pragma: keep
#include "bbsolver/path/multimode/path_multimode_visible_probe.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"

namespace bbsolver {
namespace {

constexpr int kDefaultMaxGapSamples = 24;

using path_multimode::BuildMotionAwareVertexRegions;
using path_multimode::BuildLandmarkSubpathOutputKeys;
using path_multimode::BuildFastSummaryPartition;
using path_multimode::BuildKeyCountLandmarkPartition;
using path_multimode::BuildCandidate;
using path_multimode::CandidateKeyBudgetExceeded;
using path_multimode::LandmarkPartitionResult;
using path_multimode::MultiModeCandidateKeyBudgetExceededNote;
using path_multimode::MultiModeCandidateNote;
using path_multimode::MultiModeRegionBudgetExceededNote;
using path_multimode::MultiModeValidationBudgetExceededNote;
using path_multimode::NormalizeLandmarkSubpathOptions;
using path_multimode::RecombinedRegionTemporalResult;
using path_multimode::RegionSolveResult;
using path_multimode::RunVisibleChannelProbe;
using path_multimode::ShapeFlatInputValidation;
using path_multimode::SolveRegionAnchors;
using path_multimode::TryRecombinedRegionTemporalCandidate;
using path_multimode::ValidateShapeFlatLandmarkInput;
using path_multimode::ValidateShapeFlatMultiModeInputs;
using path_multimode::ValidationWorkUnits;
using path_multimode::VertexRegion;
using path_multimode::VisibleChannelProbeOnlyNote;
using path_multimode::VisibleChannelProbeResult;

}  // namespace

PropertyKeys SolveShapeFlatMultiModeTemporal(
    const PropertySamples& original,
    const PropertySamples& reduced,
    const ShapeFlatMultiModeOptions& options) {
  PropertyKeys out;
  out.property_id = reduced.property.id;
  out.converged = false;

  const ShapeFlatInputValidation input =
      ValidateShapeFlatMultiModeInputs(original, reduced);
  if (!input.ok) {
    out.notes = input.note;
    return out;
  }
  const int n = static_cast<int>(reduced.samples.size());
  const int vertex_count = input.vertex_count;

  const int max_gap =
      std::max(1, std::min(n - 1,
                           options.max_gap_samples > 0
                               ? options.max_gap_samples
                               : kDefaultMaxGapSamples));
  const std::vector<VertexRegion> regions =
      BuildMotionAwareVertexRegions(reduced, vertex_count, options.max_regions);

  std::set<int> union_anchors;
  union_anchors.insert(0);
  union_anchors.insert(n - 1);
  int region_idx = 0;
  std::vector<int> per_region_key_counts;
  per_region_key_counts.reserve(regions.size());
  int region_segment_checks = 0;
  for (VertexRegion region : regions) {
    if (options.cancel_fn && options.cancel_fn()) {
      out.notes = "cancelled";
      return out;
    }
    RegionSolveResult region_result =
        SolveRegionAnchors(reduced, region, options, max_gap,
                           &region_segment_checks);
    if (region_result.budget_exceeded) {
      out.notes = MultiModeRegionBudgetExceededNote(
          region_segment_checks, options.max_region_segment_checks, regions);
      return out;
    }
    if (region_result.anchors.empty()) {
      out.notes = "shape_multimode_region_failed_" +
                  std::to_string(region_idx);
      return out;
    }
    per_region_key_counts.push_back(
        static_cast<int>(region_result.anchors.size()));
    union_anchors.insert(region_result.anchors.begin(),
                         region_result.anchors.end());
    ++region_idx;
  }

  std::vector<int> anchors(union_anchors.begin(), union_anchors.end());
  out = BuildCandidate(reduced, anchors);
  const int validation_work_units =
      ValidationWorkUnits(original, vertex_count, options.frame_fit_options);
  if ((options.max_validation_samples > 0 &&
       n > options.max_validation_samples) ||
      (options.max_validation_work_units > 0 &&
       validation_work_units > options.max_validation_work_units)) {
    out.notes = MultiModeValidationBudgetExceededNote(
        static_cast<int>(anchors.size()),
        n,
        options.max_validation_samples,
        validation_work_units,
        options.max_validation_work_units,
        regions);
    return out;
  }

  const RecombinedRegionTemporalResult recombined =
      TryRecombinedRegionTemporalCandidate(
          original, reduced, regions, options, max_gap,
          static_cast<int>(anchors.size()));
  if (recombined.note == "cancelled") {
    out.notes = "cancelled";
    return out;
  }
  if (recombined.accepted) {
    return recombined.keys;
  }
  const std::string recombined_note =
      recombined.attempted ? recombined.note : std::string();

  if (CandidateKeyBudgetExceeded(static_cast<int>(anchors.size()),
                                 n,
                                 options.max_candidate_key_ratio)) {
    out.notes = MultiModeCandidateKeyBudgetExceededNote(
        static_cast<int>(anchors.size()),
        n,
        options.max_candidate_key_ratio,
        regions,
        recombined_note);
    return out;
  }

  PathTemporalValidationOptions validation_options;
  validation_options.frame_fit_options = options.frame_fit_options;
  const PathTemporalValidationResult validation =
      ValidatePathTemporalCandidate(original, out, validation_options);
  out.converged = validation.ok;
  out.max_err = validation.max_outline_error;
  out.max_err_screen_px = validation.max_outline_error;
  for (SegmentReport& report : out.segments) {
    report.max_err = validation.max_outline_error;
    report.max_err_screen_px = validation.max_outline_error;
    report.rms_err = validation.max_outline_error;
  }

  out.notes = MultiModeCandidateNote(
      regions, max_gap, per_region_key_counts, region_segment_checks,
      validation_work_units, static_cast<int>(out.keys.size()), anchors,
      validation.notes, recombined_note);
  return out;
}

std::vector<PropertyKeys> EmitShapeFlatLandmarkSubpathKeys(
    const PropertySamples& reduced,
    const ShapeFlatLandmarkSubpathOptions& options) {
  const ShapeFlatLandmarkSubpathOptions normalized_options =
      NormalizeLandmarkSubpathOptions(options);
  if (!normalized_options.enabled) {
    return {};
  }
  const ShapeFlatInputValidation input =
      ValidateShapeFlatLandmarkInput(reduced);
  if (!input.ok) {
    return {};
  }
  const int vertex_count = input.vertex_count;

  const std::vector<VertexRegion> regions =
      BuildMotionAwareVertexRegions(reduced, vertex_count,
                                    normalized_options.max_regions);
  const int n = static_cast<int>(reduced.samples.size());
  const int max_gap =
      std::max(1, std::min(n > 1 ? n - 1 : 1,
                           normalized_options.max_gap_samples > 0
                               ? normalized_options.max_gap_samples
                               : kDefaultMaxGapSamples));
  VisibleChannelProbeResult visible_probe;
  if (normalized_options.probe_visible_channels) {
    visible_probe = RunVisibleChannelProbe(reduced,
                                           normalized_options,
                                           max_gap,
                                           vertex_count);
    if (visible_probe.status == "cancelled") {
      return {};
    }
    if (normalized_options.fast_summary_only &&
        !normalized_options.emit_visible_shape_channels) {
      PropertyKeys diagnostic;
      diagnostic.property_id = reduced.property.id;
      diagnostic.converged = true;
      diagnostic.notes =
          VisibleChannelProbeOnlyNote(visible_probe, n, vertex_count, max_gap);
      return {std::move(diagnostic)};
    }
  }
  LandmarkPartitionResult partition;
  if (normalized_options.fast_summary_only) {
    partition = BuildFastSummaryPartition(reduced,
                                          regions,
                                          vertex_count,
                                          normalized_options,
                                          max_gap);
  } else {
    partition =
        BuildKeyCountLandmarkPartition(reduced,
                                       regions,
                                       vertex_count,
                                       normalized_options,
                                       max_gap);
  }
  if (!partition.ok) {
    return {};
  }
  return BuildLandmarkSubpathOutputKeys(reduced,
                                        normalized_options,
                                        partition,
                                        visible_probe,
                                        n,
                                        max_gap,
                                        vertex_count);
}

}  // namespace bbsolver
