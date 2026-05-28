#include "bbsolver/path/multimode/path_multimode_landmark_output.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/path/multimode/path_multimode_geometry.hpp"
#include "bbsolver/path/multimode/path_multimode_landmark_emission.hpp"
#include "bbsolver/path/multimode/path_multimode_landmark_partition.hpp"
#include "bbsolver/path/multimode/path_multimode_notes.hpp"
#include "bbsolver/path/multimode/path_multimode_solver.hpp"

#include <string>
#include <utility>
#include <vector>
#include <cstddef>

namespace bbsolver {
namespace path_multimode {

LandmarkPartitionResult BuildFastSummaryPartition(
    const PropertySamples& reduced,
    const std::vector<VertexRegion>& base_regions,
    int vertex_count,
    const ShapeFlatLandmarkSubpathOptions& normalized_options,
    int max_gap) {
  LandmarkRegionEmissionResult emission =
      EvaluateLandmarkRegionEmission(reduced,
                                     {0, vertex_count},
                                     normalized_options,
                                     max_gap);
  LandmarkPartitionResult partition;
  if (!emission.ok) {
    return partition;
  }
  partition.ok = true;
  partition.base_ranges = JoinRegionRanges(base_regions);
  partition.chosen_ranges = "0-" + std::to_string(vertex_count);
  partition.score = static_cast<double>(emission.keys.keys.size());
  partition.boundary_count = 2;
  partition.interval_evaluations = 1;
  partition.emissions.push_back(std::move(emission));
  return partition;
}

std::vector<PropertyKeys> BuildLandmarkSubpathOutputKeys(
    const PropertySamples& reduced,
    const ShapeFlatLandmarkSubpathOptions& normalized_options,
    const LandmarkPartitionResult& partition,
    const VisibleChannelProbeResult& visible_probe,
    int source_sample_count,
    int max_gap,
    int vertex_count) {
  if (!partition.ok) {
    return {};
  }
  std::vector<PropertyKeys> out;
  out.reserve(partition.emissions.size());
  for (std::size_t region_idx = 0; region_idx < partition.emissions.size();
       ++region_idx) {
    if (normalized_options.cancel_fn && normalized_options.cancel_fn()) {
      return {};
    }
    const LandmarkRegionEmissionResult& emission =
        partition.emissions[region_idx];
    if (!emission.ok || !emission.reconstruction.ok ||
        emission.anchors.empty()) {
      return {};
    }
    PropertyKeys keys = emission.keys;
    keys.property_id = reduced.property.id;
    keys.converged = true;
    keys.notes = LandmarkSubpathNote(static_cast<int>(region_idx),
                                     static_cast<int>(
                                         partition.emissions.size()),
                                     emission.region,
                                     emission.anchors,
                                     source_sample_count,
                                     max_gap,
                                     emission.region_segment_checks,
                                     emission.inserted_samples,
                                     emission.temporal_segment_checks,
                                     emission.temporal_status,
                                     emission.reconstruction,
                                     vertex_count,
                                     normalized_options
.emit_visible_shape_channels) +
                 "; subpath_partition=key_count_dp" +
                 "; subpath_partition_base_ranges=" + partition.base_ranges +
                 "; subpath_partition_chosen_ranges=" +
                 partition.chosen_ranges +
                 "; subpath_partition_boundary_count=" +
                 std::to_string(partition.boundary_count) +
                 "; subpath_partition_interval_evaluations=" +
                 std::to_string(partition.interval_evaluations) +
                 "; subpath_partition_score=" +
                 std::to_string(partition.score) +
                 "; subpath_diagnostics=" +
                 std::string((normalized_options.diagnose_dense_runs ||
                              normalized_options.diagnose_segment_gaps ||
                              normalized_options.diagnose_outlier_slots ||
                              normalized_options.diagnose_mask_channels)
                                 ? "deep"
: "fast") +
                 "; subpath_fast_summary=" +
                 std::string(normalized_options.fast_summary_only ? "true"
: "false");
    const std::string visible_probe_note =
        VisibleChannelProbeNote(visible_probe);
    if (!visible_probe_note.empty()) {
      keys.notes += "; " + visible_probe_note;
    }
    if (!partition.semantic_split_note.empty()) {
      keys.notes += "; " + partition.semantic_split_note;
    }
    if (!partition.outlier_partition_note.empty()) {
      keys.notes += "; " + partition.outlier_partition_note;
    }
    if (!emission.dense_run_note.empty()) {
      keys.notes += "; subpath_dense_run_checks=" +
                    std::to_string(emission.dense_run_checks) +
                    "; subpath_dense_run_diagnostic=" +
                    emission.dense_run_note +
                    "; subpath_dense_run_next_representation=per_region_independent_timing_or_extra_shape_channels";
    }
    if (!emission.segment_gap_note.empty()) {
      keys.notes += "; subpath_segment_gap_hist=" +
                    emission.segment_gap_note +
                    "; subpath_segment_gap_max=" +
                    std::to_string(emission.segment_gap_max);
      if (emission.segment_rejection_checks > 0) {
        keys.notes += "; subpath_segment_rejection_checks=" +
                      std::to_string(emission.segment_rejection_checks);
      }
      if (!emission.segment_lower_bound_note.empty()) {
        keys.notes += "; subpath_segment_lower_bound_top=" +
                      emission.segment_lower_bound_note;
      }
      if (!emission.segment_rejection_note.empty()) {
        keys.notes += "; subpath_segment_rejection_top=" +
                      emission.segment_rejection_note;
      }
    }
    if (!emission.outlier_slot_note.empty()) {
      keys.notes += "; subpath_outlier_vertex_checks=" +
                    std::to_string(emission.outlier_slot_checks) +
                    "; subpath_outlier_vertex_top=" +
                    emission.outlier_slot_note;
    }
    out.push_back(std::move(keys));
  }
  return out;
}

}  // namespace path_multimode
}  // namespace bbsolver
