#include "bbsolver/path/multimode/path_multimode_visible_probe.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/path/multimode/path_multimode_landmark_emission.hpp"
#include "bbsolver/path/multimode/path_multimode_geometry.hpp"
#include "bbsolver/path/multimode/path_multimode_landmark_partition.hpp"
#include "bbsolver/path/multimode/path_multimode_notes.hpp"
#include "bbsolver/path/multimode/path_multimode_solver.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>
#include <cstddef>

namespace bbsolver {
namespace path_multimode {
namespace {

constexpr int kVisibleChannelProbeMinChannels = 2;
constexpr int kVisibleChannelProbeMaxChannels = 4;
constexpr int kVisibleChannelProbeMaxRegionChecks = 5000;

}  // namespace

bool SameRegionList(const std::vector<VertexRegion>& a,
                    const std::vector<VertexRegion>& b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (std::size_t idx = 0; idx < a.size(); ++idx) {
    if (a[idx].first_vertex != b[idx].first_vertex ||
        a[idx].end_vertex != b[idx].end_vertex) {
      return false;
    }
  }
  return true;
}

void AddVisibleProbePartition(
    const std::vector<VertexRegion>& regions,
    int vertex_count,
    std::vector<std::vector<VertexRegion>>* partitions) {
  if (partitions == nullptr || regions.empty()) {
    return;
  }
  int expected_first = 0;
  for (VertexRegion region: regions) {
    if (region.first_vertex != expected_first ||
        region.end_vertex <= region.first_vertex ||
        region.end_vertex > vertex_count) {
      return;
    }
    expected_first = region.end_vertex;
  }
  if (expected_first != vertex_count) {
    return;
  }
  for (const std::vector<VertexRegion>& existing: *partitions) {
    if (SameRegionList(existing, regions)) {
      return;
    }
  }
  partitions->push_back(regions);
}

std::vector<std::vector<VertexRegion>> VisibleProbePartitions(
    const PropertySamples& reduced,
    int vertex_count,
    int channel_count) {
  std::vector<std::vector<VertexRegion>> partitions;
  if (channel_count < 1 || vertex_count < channel_count) {
    return partitions;
  }
  AddVisibleProbePartition(BuildVertexRegions(vertex_count, channel_count),
                           vertex_count,
                           &partitions);
  AddVisibleProbePartition(
      BuildMotionAwareVertexRegions(reduced, vertex_count, channel_count),
      vertex_count,
      &partitions);
  return partitions;
}

VisibleChannelProbeCandidate EvaluateVisibleProbePartition(
    const PropertySamples& reduced,
    const std::vector<VertexRegion>& regions,
    int channel_count,
    const ShapeFlatLandmarkSubpathOptions& options,
    int max_gap) {
  VisibleChannelProbeCandidate candidate;
  candidate.channel_count = channel_count;
  candidate.ranges = JoinRegionRanges(regions);
  if (regions.empty()) {
    candidate.status = "empty_partition";
    return candidate;
  }

  ShapeFlatLandmarkSubpathOptions probe_options = options;
  probe_options.diagnose_dense_runs = false;
  probe_options.diagnose_segment_gaps = false;
  probe_options.diagnose_outlier_slots = false;
  probe_options.diagnose_mask_channels = false;
  probe_options.fast_summary_only = false;
  if (probe_options.max_region_segment_checks > 0) {
    probe_options.max_region_segment_checks =
        std::min(probe_options.max_region_segment_checks,
                 kVisibleChannelProbeMaxRegionChecks);
  } else {
    probe_options.max_region_segment_checks =
        kVisibleChannelProbeMaxRegionChecks;
  }

  std::vector<LandmarkRegionEmissionResult> emissions;
  emissions.reserve(regions.size());
  for (VertexRegion region: regions) {
    if (probe_options.cancel_fn && probe_options.cancel_fn()) {
      candidate.status = "cancelled";
      return candidate;
    }
    LandmarkRegionEmissionResult emission =
        EvaluateLandmarkRegionEmission(reduced, region, probe_options, max_gap);
    candidate.segment_checks += emission.region_segment_checks +
                                emission.temporal_segment_checks;
    if (!emission.ok || !emission.reconstruction.ok ||
        emission.keys.keys.empty()) {
      candidate.status = emission.temporal_status.empty()
                             ? "region_failed"
: emission.temporal_status;
      return candidate;
    }
    emissions.push_back(std::move(emission));
  }
  const int vertex_count = ShapeFlatVertexCount(reduced.samples.front().v);
  if (!RegionsCoverFullRange(emissions, vertex_count)) {
    candidate.status = "coverage_failed";
    return candidate;
  }

  candidate.ok = true;
  candidate.key_count = TotalEmissionKeyCount(emissions);
  candidate.max_outline_error = MaxEmissionReconstructionError(emissions);
  candidate.status = "ok";
  return candidate;
}

VisibleChannelProbeResult RunVisibleChannelProbe(
    const PropertySamples& reduced,
    const ShapeFlatLandmarkSubpathOptions& options,
    int max_gap,
    int vertex_count) {
  VisibleChannelProbeResult result;
  result.attempted = true;
  result.baseline_keys = options.visible_baseline_keys;

  VisibleChannelProbeCandidate best_overall;
  bool have_best = false;
  for (int channel_count = kVisibleChannelProbeMinChannels;
       channel_count <= kVisibleChannelProbeMaxChannels;
       ++channel_count) {
    VisibleChannelProbeCandidate best_for_channel;
    best_for_channel.channel_count = channel_count;
    best_for_channel.status = "no_valid_candidate";
    const std::vector<std::vector<VertexRegion>> partitions =
        VisibleProbePartitions(reduced, vertex_count, channel_count);
    best_for_channel.partition_count = static_cast<int>(partitions.size());
    for (const std::vector<VertexRegion>& regions: partitions) {
      VisibleChannelProbeCandidate candidate =
          EvaluateVisibleProbePartition(reduced,
                                        regions,
                                        channel_count,
                                        options,
                                        max_gap);
      candidate.partition_count = static_cast<int>(partitions.size());
      if (candidate.status == "cancelled") {
        best_for_channel = candidate;
        result.candidates.push_back(best_for_channel);
        result.status = "cancelled";
        result.reason = "cancelled";
        return result;
      }
      if (!candidate.ok) {
        if (!candidate.status.empty()) {
          best_for_channel.status = candidate.status;
        }
        best_for_channel.segment_checks += candidate.segment_checks;
        continue;
      }
      if (!best_for_channel.ok ||
          candidate.key_count < best_for_channel.key_count ||
          (candidate.key_count == best_for_channel.key_count &&
           candidate.max_outline_error < best_for_channel.max_outline_error)) {
        best_for_channel = candidate;
      }
    }
    result.candidates.push_back(best_for_channel);
    if (best_for_channel.ok &&
        (!have_best || best_for_channel.key_count < best_overall.key_count ||
         (best_for_channel.key_count == best_overall.key_count &&
          best_for_channel.max_outline_error < best_overall.max_outline_error))) {
      best_overall = best_for_channel;
      have_best = true;
    }
  }

  if (!have_best) {
    result.status = "blocked_no_valid_candidate";
    result.reason = "forced_contiguous_candidates_failed";
    result.selected_channel_count = 0;
    return result;
  }

  result.visible_channel_keys = best_overall.key_count;
  result.max_outline_error = best_overall.max_outline_error;
  result.segment_checks = best_overall.segment_checks;
  result.selected_ranges = best_overall.ranges;
  if (result.baseline_keys > 0 &&
      best_overall.key_count >= result.baseline_keys) {
    result.status = "blocked_no_key_reduction";
    result.reason = "best_contiguous_not_lower_than_baseline";
    result.selected_channel_count = 0;
    return result;
  }
  if (result.baseline_keys <= 0) {
    result.status = "blocked_baseline_missing";
    result.reason = "visible_baseline_keys_not_provided";
    result.selected_channel_count = 0;
    return result;
  }

  result.status = "accepted_key_reduction";
  result.reason = "best_contiguous_lower_than_baseline";
  result.selected_channel_count = best_overall.channel_count;
  return result;
}

}  // namespace path_multimode
}  // namespace bbsolver
