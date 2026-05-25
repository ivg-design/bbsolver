#include "bbsolver/path/multimode/path_multimode_landmark_partition_alternatives.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/path/multimode/path_multimode_geometry.hpp"
#include "bbsolver/path/multimode/path_multimode_landmark_emission.hpp"
#include "bbsolver/path/multimode/path_multimode_landmark_partition.hpp"
#include "bbsolver/path/multimode/path_multimode_notes.hpp"
#include "bbsolver/path/multimode/path_multimode_solver.hpp"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <cstddef>
#include <vector>

namespace bbsolver {
namespace path_multimode {
namespace {

constexpr double kSemanticSplitMaxKeyRatio = 1.5;
constexpr int kOutlierPartitionCandidateSlots = 2;

bool LookupCachedEmission(
    VertexRegion region,
    const std::vector<int>& boundaries,
    const std::vector<std::vector<LandmarkRegionEmissionResult>>& intervals,
    const std::vector<std::vector<bool>>& valid,
    LandmarkRegionEmissionResult* emission) {
  const int start = BoundaryIndex(boundaries, region.first_vertex);
  const int end = BoundaryIndex(boundaries, region.end_vertex);
  if (start < 0 || end <= start ||
      !valid[static_cast<std::size_t>(start)]
            [static_cast<std::size_t>(end)]) {
    return false;
  }
  if (emission != nullptr) {
    *emission = intervals[static_cast<std::size_t>(start)]
                         [static_cast<std::size_t>(end)];
  }
  return true;
}

std::vector<LandmarkRegionEmissionResult> BuildOutlierSlotCandidate(
    const PropertySamples& reduced,
    int slot,
    const std::vector<int>& boundaries,
    const std::vector<std::vector<LandmarkRegionEmissionResult>>& intervals,
    const std::vector<std::vector<bool>>& valid,
    const ShapeFlatLandmarkSubpathOptions& options,
    int max_gap,
    int vertex_count,
    int* on_demand_evaluations,
    std::string* failure_reason) {
  std::vector<LandmarkRegionEmissionResult> emissions;
  const std::vector<VertexRegion> regions = OutlierSlotRegions(vertex_count, slot);
  if (regions.empty()) {
    if (failure_reason != nullptr) {
      *failure_reason = "bad_slot";
    }
    return emissions;
  }
  emissions.reserve(regions.size());
  for (VertexRegion region : regions) {
    LandmarkRegionEmissionResult emission;
    if (!LookupCachedEmission(region, boundaries, intervals, valid, &emission)) {
      if (on_demand_evaluations != nullptr) {
        ++(*on_demand_evaluations);
      }
      emission = EvaluateLandmarkRegionEmission(reduced, region, options, max_gap);
    }
    if (!emission.ok || !emission.reconstruction.ok ||
        emission.keys.keys.empty()) {
      if (failure_reason != nullptr) {
        *failure_reason = "region_failed_" +
                          std::to_string(region.first_vertex) + "-" +
                          std::to_string(region.end_vertex);
      }
      return {};
    }
    emissions.push_back(std::move(emission));
  }
  if (!RegionsCoverFullRange(emissions, vertex_count)) {
    if (failure_reason != nullptr) {
      *failure_reason = "coverage_failed";
    }
    return {};
  }
  return emissions;
}

}  // namespace

LandmarkPartitionResult TrySemanticSplitFromCachedRegions(
    const std::vector<VertexRegion>& base_regions,
    const std::vector<int>& boundaries,
    const std::vector<std::vector<LandmarkRegionEmissionResult>>& intervals,
    const std::vector<std::vector<bool>>& valid,
    const LandmarkPartitionResult& selected,
    int vertex_count) {
  LandmarkPartitionResult result;
  result.base_ranges = selected.base_ranges;
  result.chosen_ranges = selected.chosen_ranges;
  result.boundary_count = selected.boundary_count;
  result.interval_evaluations = selected.interval_evaluations;

  if (!selected.ok ||
      !DenseFullRangeNeedsSemanticSplit(selected.emissions, vertex_count)) {
    return result;
  }
  if (base_regions.size() < 2) {
    result.semantic_split_note =
        "subpath_semantic_split=not_selected; reason=single_base_region";
    return result;
  }

  std::vector<LandmarkRegionEmissionResult> split_emissions;
  split_emissions.reserve(base_regions.size());
  for (VertexRegion region : base_regions) {
    const int start = BoundaryIndex(boundaries, region.first_vertex);
    const int end = BoundaryIndex(boundaries, region.end_vertex);
    if (start < 0 || end <= start ||
        !valid[static_cast<std::size_t>(start)]
              [static_cast<std::size_t>(end)]) {
      result.semantic_split_note =
          "subpath_semantic_split=not_selected; reason=base_region_invalid";
      return result;
    }
    split_emissions.push_back(
        intervals[static_cast<std::size_t>(start)]
                 [static_cast<std::size_t>(end)]);
  }

  if (!RegionsCoverFullRange(split_emissions, vertex_count)) {
    result.semantic_split_note =
        "subpath_semantic_split=not_selected; reason=coverage_failed";
    return result;
  }

  const int full_key_count = TotalEmissionKeyCount(selected.emissions);
  const int split_key_count = TotalEmissionKeyCount(split_emissions);
  if (full_key_count <= 0 || split_key_count <= 0) {
    result.semantic_split_note =
        "subpath_semantic_split=not_selected; reason=bad_key_count";
    return result;
  }
  const double allowed =
      static_cast<double>(full_key_count) * kSemanticSplitMaxKeyRatio + 1e-9;
  if (static_cast<double>(split_key_count) > allowed) {
    result.semantic_split_note =
        "subpath_semantic_split=not_selected; reason=key_ratio_exceeded" +
        std::string("; subpath_semantic_split_full_key_count=") +
        std::to_string(full_key_count) +
        "; subpath_semantic_split_key_count=" +
        std::to_string(split_key_count) +
        "; subpath_semantic_split_max_key_ratio=" +
        std::to_string(kSemanticSplitMaxKeyRatio);
    return result;
  }

  std::vector<VertexRegion> split_regions;
  split_regions.reserve(split_emissions.size());
  for (const LandmarkRegionEmissionResult& emission : split_emissions) {
    split_regions.push_back(emission.region);
  }

  result.ok = true;
  result.emissions = std::move(split_emissions);
  result.chosen_ranges = JoinRegionRanges(split_regions);
  result.score = static_cast<double>(split_key_count);
  result.semantic_split_note =
      "subpath_representation=semantic_split" +
      std::string("; subpath_semantic_split_reason=dense_full_range_one_shared_progress_chord_infeasible") +
      "; subpath_semantic_split_full_key_count=" +
      std::to_string(full_key_count) +
      "; subpath_semantic_split_key_count=" +
      std::to_string(split_key_count) +
      "; subpath_semantic_split_max_key_ratio=" +
      std::to_string(kSemanticSplitMaxKeyRatio) +
      "; subpath_semantic_split_max_outline_error=" +
      std::to_string(MaxEmissionReconstructionError(result.emissions));
  return result;
}

std::vector<LandmarkRegionEmissionResult> ReconstructCachedPartition(
    int region_count,
    int boundary_count,
    const std::vector<std::vector<int>>& prev,
    const std::vector<std::vector<LandmarkRegionEmissionResult>>& intervals) {
  std::vector<LandmarkRegionEmissionResult> selected;
  int end = boundary_count - 1;
  for (int count = region_count; count > 0; --count) {
    const int start =
        prev[static_cast<std::size_t>(count)]
            [static_cast<std::size_t>(end)];
    if (start < 0) {
      return {};
    }
    selected.push_back(intervals[static_cast<std::size_t>(start)]
                                [static_cast<std::size_t>(end)]);
    end = start;
  }
  if (end != 0) {
    return {};
  }
  std::reverse(selected.begin(), selected.end());
  return selected;
}

LandmarkPartitionResult TrySemanticSplitFromCandidate(
    const LandmarkPartitionResult& selected,
    std::vector<LandmarkRegionEmissionResult> candidate_emissions,
    double candidate_score,
    int vertex_count,
    const std::string& candidate_kind) {
  LandmarkPartitionResult result;
  result.base_ranges = selected.base_ranges;
  result.chosen_ranges = selected.chosen_ranges;
  result.boundary_count = selected.boundary_count;
  result.interval_evaluations = selected.interval_evaluations;

  if (!selected.ok ||
      !DenseFullRangeNeedsSemanticSplit(selected.emissions, vertex_count)) {
    return result;
  }
  if (candidate_emissions.empty()) {
    result.semantic_split_note =
        "subpath_semantic_split=not_selected; reason=no_valid_nonfull_partition";
    return result;
  }
  if (!RegionsCoverFullRange(candidate_emissions, vertex_count)) {
    result.semantic_split_note =
        "subpath_semantic_split=not_selected; reason=coverage_failed";
    return result;
  }

  const int full_key_count = TotalEmissionKeyCount(selected.emissions);
  const int split_key_count = TotalEmissionKeyCount(candidate_emissions);
  if (full_key_count <= 0 || split_key_count <= 0) {
    result.semantic_split_note =
        "subpath_semantic_split=not_selected; reason=bad_key_count";
    return result;
  }
  const std::vector<LandmarkRegionEmissionResult>& full =
      selected.emissions;
  const double full_error = MaxEmissionReconstructionError(full);
  const double split_error = MaxEmissionReconstructionError(candidate_emissions);
  std::vector<VertexRegion> split_regions;
  split_regions.reserve(candidate_emissions.size());
  for (const LandmarkRegionEmissionResult& emission : candidate_emissions) {
    split_regions.push_back(emission.region);
  }
  const std::string split_ranges = JoinRegionRanges(split_regions);
  const std::string common =
      std::string("; subpath_semantic_split_candidate=") + candidate_kind +
      "; subpath_semantic_split_full_key_count=" +
      std::to_string(full_key_count) +
      "; subpath_semantic_split_key_count=" +
      std::to_string(split_key_count) +
      "; subpath_semantic_split_ranges=" + split_ranges +
      "; subpath_semantic_split_full_max_outline_error=" +
      std::to_string(full_error) +
      "; subpath_semantic_split_max_outline_error=" +
      std::to_string(split_error);

  if (split_key_count >= full_key_count) {
    result.semantic_split_note =
        "subpath_semantic_split=not_selected; reason=best_nonfull_not_lower_key_count" +
        common;
    return result;
  }

  result.ok = true;
  result.emissions = std::move(candidate_emissions);
  result.chosen_ranges = split_ranges;
  result.score = candidate_score;
  result.semantic_split_note =
      "subpath_representation=semantic_split" +
      std::string("; subpath_semantic_split_reason=dense_full_range_one_shared_progress_chord_infeasible") +
      common;
  return result;
}

LandmarkPartitionResult TryOutlierPartitionFromSlots(
    const PropertySamples& reduced,
    const LandmarkPartitionResult& selected,
    const std::vector<int>& boundaries,
    const std::vector<std::vector<LandmarkRegionEmissionResult>>& intervals,
    const std::vector<std::vector<bool>>& valid,
    const ShapeFlatLandmarkSubpathOptions& options,
    int max_gap,
    int vertex_count) {
  LandmarkPartitionResult result;
  result.base_ranges = selected.base_ranges;
  result.chosen_ranges = selected.chosen_ranges;
  result.boundary_count = selected.boundary_count;
  result.interval_evaluations = selected.interval_evaluations;

  if (!selected.ok ||
      !DenseFullRangeNeedsSemanticSplit(selected.emissions, vertex_count)) {
    return result;
  }
  if (selected.emissions.empty() ||
      selected.emissions.front().outlier_slots.empty()) {
    result.outlier_partition_note =
        "subpath_outlier_partition=not_selected; reason=no_outlier_scores";
    return result;
  }

  const int full_key_count = TotalEmissionKeyCount(selected.emissions);
  if (full_key_count <= 0) {
    result.outlier_partition_note =
        "subpath_outlier_partition=not_selected; reason=bad_full_key_count";
    return result;
  }

  std::vector<LandmarkRegionEmissionResult> best_emissions;
  int best_slot = -1;
  int best_key_count = std::numeric_limits<int>::max();
  double best_error = std::numeric_limits<double>::infinity();
  int on_demand_evaluations = 0;
  std::string failure_reason = "no_valid_candidate";
  const std::vector<OutlierSlotScore>& slots =
      selected.emissions.front().outlier_slots;
  const int candidate_count =
      std::min(kOutlierPartitionCandidateSlots, static_cast<int>(slots.size()));
  for (int idx = 0; idx < candidate_count; ++idx) {
    const int slot = slots[static_cast<std::size_t>(idx)].vertex;
    std::string local_failure;
    int local_on_demand = 0;
    std::vector<LandmarkRegionEmissionResult> candidate =
        BuildOutlierSlotCandidate(reduced, slot, boundaries, intervals, valid,
                                  options, max_gap, vertex_count,
                                  &local_on_demand, &local_failure);
    on_demand_evaluations += local_on_demand;
    if (candidate.empty()) {
      failure_reason = local_failure.empty() ? "candidate_failed" : local_failure;
      continue;
    }
    const int key_count = TotalEmissionKeyCount(candidate);
    const double error = MaxEmissionReconstructionError(candidate);
    if (key_count < best_key_count ||
        (key_count == best_key_count && error < best_error)) {
      best_key_count = key_count;
      best_error = error;
      best_slot = slot;
      best_emissions = std::move(candidate);
    }
  }

  const std::string candidate_slots = JoinOutlierCandidateSlots(slots);
  if (best_emissions.empty()) {
    result.outlier_partition_note =
        "subpath_outlier_partition=not_selected; reason=" + failure_reason +
        "; subpath_outlier_candidate_slots=" + candidate_slots +
        "; subpath_outlier_on_demand_evaluations=" +
        std::to_string(on_demand_evaluations);
    return result;
  }

  std::vector<VertexRegion> best_regions;
  best_regions.reserve(best_emissions.size());
  for (const LandmarkRegionEmissionResult& emission : best_emissions) {
    best_regions.push_back(emission.region);
  }
  const std::string ranges = JoinRegionRanges(best_regions);
  const std::string common =
      "; subpath_outlier_candidate_slots=" + candidate_slots +
      "; subpath_outlier_slot=" + std::to_string(best_slot) +
      "; subpath_outlier_full_key_count=" + std::to_string(full_key_count) +
      "; subpath_outlier_key_count=" + std::to_string(best_key_count) +
      "; subpath_outlier_ranges=" + ranges +
      "; subpath_outlier_max_outline_error=" + std::to_string(best_error) +
      "; subpath_outlier_on_demand_evaluations=" +
      std::to_string(on_demand_evaluations) +
      "; subpath_outlier_next_representation=non_contiguous_masked_channels_or_recombined_regional_timing";
  if (best_key_count >= full_key_count) {
    result.outlier_partition_note =
        "subpath_outlier_partition=not_selected; reason=best_outlier_not_lower_key_count" +
        common;
    return result;
  }

  result.ok = true;
  result.emissions = std::move(best_emissions);
  result.chosen_ranges = ranges;
  result.score = static_cast<double>(best_key_count);
  result.outlier_partition_note =
      "subpath_representation=outlier_contiguous_split" +
      std::string("; subpath_outlier_partition=selected") +
      "; subpath_outlier_reason=dense_full_range_vertex_slot_dominant" +
      common;
  return result;
}

}  // namespace path_multimode
}  // namespace bbsolver
