#include "bbsolver/path/multimode/path_multimode_landmark_partition.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/path/multimode/path_multimode_landmark_emission.hpp"
#include "bbsolver/path/multimode/path_multimode_geometry.hpp"
#include "bbsolver/path/multimode/path_multimode_landmark_partition_alternatives.hpp"
#include "bbsolver/path/multimode/path_multimode_mask_channel_diagnostic.hpp"
#include "bbsolver/path/multimode/path_multimode_notes.hpp"
#include "bbsolver/path/multimode/path_multimode_solver.hpp"

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>
#include <cstddef>
#include <vector>

namespace bbsolver {
namespace path_multimode {
namespace {

constexpr int kDefaultMaxRegions = 4;
constexpr int kPartitionBoundaryNeighborhood = 1;
constexpr int kMaxPartitionBoundaryPoints = 12;

}  // namespace

std::vector<int> RegionBoundaryPoints(const std::vector<VertexRegion>& regions,
                                      int vertex_count) {
  std::vector<int> base_boundaries;
  base_boundaries.reserve(regions.size() + 1);
  base_boundaries.push_back(0);
  for (VertexRegion region : regions) {
    base_boundaries.push_back(region.first_vertex);
    base_boundaries.push_back(region.end_vertex);
  }
  base_boundaries.push_back(vertex_count);
  std::sort(base_boundaries.begin(), base_boundaries.end());
  base_boundaries.erase(std::unique(base_boundaries.begin(),
                                    base_boundaries.end()),
                        base_boundaries.end());
  base_boundaries.erase(
      std::remove_if(base_boundaries.begin(), base_boundaries.end(),
                     [vertex_count](int boundary) {
                       return boundary < 0 || boundary > vertex_count;
                     }),
      base_boundaries.end());
  if (base_boundaries.empty() || base_boundaries.front() != 0) {
    base_boundaries.insert(base_boundaries.begin(), 0);
  }
  if (base_boundaries.back() != vertex_count) {
    base_boundaries.push_back(vertex_count);
  }
  if (static_cast<int>(base_boundaries.size()) > kMaxPartitionBoundaryPoints) {
    return {};
  }

  std::vector<int> boundaries = base_boundaries;
  for (int boundary : base_boundaries) {
    if (boundary <= 0 || boundary >= vertex_count) {
      continue;
    }
    for (int delta = -kPartitionBoundaryNeighborhood;
         delta <= kPartitionBoundaryNeighborhood;
         ++delta) {
      const int shifted = boundary + delta;
      if (shifted > 0 && shifted < vertex_count) {
        boundaries.push_back(shifted);
      }
    }
  }
  boundaries.push_back(0);
  boundaries.push_back(vertex_count);
  std::sort(boundaries.begin(), boundaries.end());
  boundaries.erase(std::unique(boundaries.begin(), boundaries.end()),
                   boundaries.end());
  if (static_cast<int>(boundaries.size()) > kMaxPartitionBoundaryPoints) {
    return base_boundaries;
  }
  return boundaries;
}

int BoundaryIndex(const std::vector<int>& boundaries, int boundary) {
  const auto it = std::lower_bound(boundaries.begin(), boundaries.end(), boundary);
  if (it == boundaries.end() || *it != boundary) {
    return -1;
  }
  return static_cast<int>(std::distance(boundaries.begin(), it));
}

bool RegionsCoverFullRange(
    const std::vector<LandmarkRegionEmissionResult>& emissions,
    int vertex_count) {
  int expected_first = 0;
  for (const LandmarkRegionEmissionResult& emission : emissions) {
    if (emission.region.first_vertex != expected_first ||
        emission.region.end_vertex <= emission.region.first_vertex) {
      return false;
    }
    expected_first = emission.region.end_vertex;
  }
  return expected_first == vertex_count;
}

int TotalEmissionKeyCount(
    const std::vector<LandmarkRegionEmissionResult>& emissions) {
  int count = 0;
  for (const LandmarkRegionEmissionResult& emission : emissions) {
    count += static_cast<int>(emission.keys.keys.size());
  }
  return count;
}

double MaxEmissionReconstructionError(
    const std::vector<LandmarkRegionEmissionResult>& emissions) {
  double max_error = 0.0;
  for (const LandmarkRegionEmissionResult& emission : emissions) {
    max_error = std::max(max_error, emission.reconstruction.max_outline_error);
  }
  return max_error;
}

bool DenseFullRangeNeedsSemanticSplit(
    const std::vector<LandmarkRegionEmissionResult>& selected,
    int vertex_count) {
  if (selected.size() != 1 ||
      selected.front().region.first_vertex != 0 ||
      selected.front().region.end_vertex != vertex_count) {
    return false;
  }
  const std::string& note = selected.front().dense_run_note;
  return note.find("one_shared_progress_chord_infeasible") != std::string::npos ||
         note.find("landmark_subpath_temporal_infeasible_chord") !=
             std::string::npos ||
         note.find("infeasible_shape_morph_chord") != std::string::npos;
}

std::vector<VertexRegion> OutlierSlotRegions(int vertex_count, int slot) {
  std::vector<VertexRegion> regions;
  if (slot < 0 || slot >= vertex_count) {
    return regions;
  }
  if (slot > 0) {
    regions.push_back({0, slot});
  }
  regions.push_back({slot, slot + 1});
  if (slot + 1 < vertex_count) {
    regions.push_back({slot + 1, vertex_count});
  }
  return regions;
}

LandmarkPartitionResult BuildKeyCountLandmarkPartition(
    const PropertySamples& reduced,
    const std::vector<VertexRegion>& base_regions, int vertex_count,
    const ShapeFlatLandmarkSubpathOptions& options, int max_gap) {
  LandmarkPartitionResult result;
  result.base_ranges = JoinRegionRanges(base_regions);

  const std::vector<int> boundaries = RegionBoundaryPoints(base_regions,
                                                           vertex_count);
  const int boundary_count = static_cast<int>(boundaries.size());
  result.boundary_count = boundary_count;
  if (boundary_count < 2) {
    return result;
  }
  const int interval_count = boundary_count - 1;
  const int max_regions = std::max(
      1, std::min(interval_count, options.max_regions > 0
                                      ? options.max_regions
                                      : kDefaultMaxRegions));

  std::vector<std::vector<LandmarkRegionEmissionResult>> intervals(
      static_cast<std::size_t>(boundary_count),
      std::vector<LandmarkRegionEmissionResult>(
          static_cast<std::size_t>(boundary_count)));
  std::vector<std::vector<bool>> valid(
      static_cast<std::size_t>(boundary_count),
      std::vector<bool>(static_cast<std::size_t>(boundary_count), false));
  for (int start = 0; start < boundary_count - 1; ++start) {
    for (int end = start + 1; end < boundary_count; ++end) {
      if (options.cancel_fn && options.cancel_fn()) {
        return {};
      }
      ++result.interval_evaluations;
      VertexRegion region{boundaries[static_cast<std::size_t>(start)],
                          boundaries[static_cast<std::size_t>(end)]};
      LandmarkRegionEmissionResult emission =
          EvaluateLandmarkRegionEmission(reduced, region, options, max_gap);
      if (emission.ok) {
        valid[static_cast<std::size_t>(start)]
             [static_cast<std::size_t>(end)] = true;
      }
      intervals[static_cast<std::size_t>(start)]
               [static_cast<std::size_t>(end)] = std::move(emission);
    }
  }

  constexpr double kInf = 1.0e100;
  constexpr double kRegionPenalty = 0.05;
  std::vector<std::vector<double>> dp(
      static_cast<std::size_t>(max_regions + 1),
      std::vector<double>(static_cast<std::size_t>(boundary_count), kInf));
  std::vector<std::vector<int>> prev(
      static_cast<std::size_t>(max_regions + 1),
      std::vector<int>(static_cast<std::size_t>(boundary_count), -1));
  dp[0][0] = 0.0;

  for (int region_count = 1; region_count <= max_regions; ++region_count) {
    for (int end = 1; end < boundary_count; ++end) {
      for (int start = 0; start < end; ++start) {
        if (!valid[static_cast<std::size_t>(start)]
                  [static_cast<std::size_t>(end)] ||
            dp[static_cast<std::size_t>(region_count - 1)]
              [static_cast<std::size_t>(start)] >= kInf) {
          continue;
        }
        const LandmarkRegionEmissionResult& emission =
            intervals[static_cast<std::size_t>(start)]
                     [static_cast<std::size_t>(end)];
        const double candidate =
            dp[static_cast<std::size_t>(region_count - 1)]
              [static_cast<std::size_t>(start)] +
            static_cast<double>(emission.keys.keys.size()) + kRegionPenalty;
        if (candidate + 1e-12 <
            dp[static_cast<std::size_t>(region_count)]
              [static_cast<std::size_t>(end)]) {
          dp[static_cast<std::size_t>(region_count)]
            [static_cast<std::size_t>(end)] = candidate;
          prev[static_cast<std::size_t>(region_count)]
              [static_cast<std::size_t>(end)] = start;
        }
      }
    }
  }

  int best_region_count = -1;
  double best_score = kInf;
  for (int region_count = 1; region_count <= max_regions; ++region_count) {
    const double score =
        dp[static_cast<std::size_t>(region_count)]
          [static_cast<std::size_t>(boundary_count - 1)];
    if (score + 1e-12 < best_score) {
      best_score = score;
      best_region_count = region_count;
    }
  }
  if (best_region_count < 0) {
    return result;
  }

  int best_nonfull_region_count = -1;
  double best_nonfull_score = kInf;
  for (int region_count = 2; region_count <= max_regions; ++region_count) {
    const double score =
        dp[static_cast<std::size_t>(region_count)]
          [static_cast<std::size_t>(boundary_count - 1)];
    if (score + 1e-12 < best_nonfull_score) {
      best_nonfull_score = score;
      best_nonfull_region_count = region_count;
    }
  }
  std::vector<LandmarkRegionEmissionResult> selected =
      ReconstructCachedPartition(best_region_count,
                                 boundary_count,
                                 prev,
                                 intervals);
  if (selected.empty()) {
    return {};
  }

  std::vector<VertexRegion> selected_regions;
  selected_regions.reserve(selected.size());
  for (const LandmarkRegionEmissionResult& emission : selected) {
    selected_regions.push_back(emission.region);
  }

  result.ok = true;
  result.emissions = std::move(selected);
  result.chosen_ranges = JoinRegionRanges(selected_regions);
  result.score = best_score;

  LandmarkPartitionResult semantic_split;
  if (best_nonfull_region_count >= 2 &&
      best_nonfull_region_count != best_region_count) {
    std::vector<LandmarkRegionEmissionResult> nonfull =
        ReconstructCachedPartition(best_nonfull_region_count, boundary_count,
                                   prev, intervals);
    semantic_split = TrySemanticSplitFromCandidate(
        result, std::move(nonfull), best_nonfull_score, vertex_count,
        "best_nonfull_partition");
  } else {
    semantic_split = TrySemanticSplitFromCachedRegions(
        base_regions, boundaries, intervals, valid, result, vertex_count);
  }
  if (semantic_split.ok) {
    return semantic_split;
  }
  if (!semantic_split.semantic_split_note.empty()) {
    result.semantic_split_note = std::move(semantic_split.semantic_split_note);
  }
  LandmarkPartitionResult outlier_partition = TryOutlierPartitionFromSlots(
      reduced, result, boundaries, intervals, valid, options, max_gap,
      vertex_count);
  if (outlier_partition.ok) {
    if (outlier_partition.semantic_split_note.empty()) {
      outlier_partition.semantic_split_note = result.semantic_split_note;
    }
    return outlier_partition;
  }
  if (!outlier_partition.outlier_partition_note.empty()) {
    result.outlier_partition_note =
        std::move(outlier_partition.outlier_partition_note);
  }
  if (options.diagnose_mask_channels) {
    const std::string mask_channel_note =
        DiagnoseNonContiguousMaskChannels(reduced, result, options, max_gap);
    if (mask_channel_note == "cancelled") {
      return {};
    }
    if (!mask_channel_note.empty()) {
      if (!result.outlier_partition_note.empty()) {
        result.outlier_partition_note += "; ";
      }
      result.outlier_partition_note += mask_channel_note;
    }
  }
  return result;
}

}  // namespace path_multimode
}  // namespace bbsolver
