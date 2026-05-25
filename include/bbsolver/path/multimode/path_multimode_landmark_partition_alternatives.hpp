#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/path/multimode/path_multimode_landmark_emission.hpp"
#include "bbsolver/path/multimode/path_multimode_geometry.hpp"
#include "bbsolver/path/multimode/path_multimode_landmark_partition.hpp"
#include "bbsolver/path/multimode/path_multimode_solver.hpp"

#include <string>
#include <vector>

namespace bbsolver {
namespace path_multimode {

LandmarkPartitionResult TrySemanticSplitFromCachedRegions(
    const std::vector<VertexRegion>& base_regions,
    const std::vector<int>& boundaries,
    const std::vector<std::vector<LandmarkRegionEmissionResult>>& intervals,
    const std::vector<std::vector<bool>>& valid,
    const LandmarkPartitionResult& selected,
    int vertex_count);

std::vector<LandmarkRegionEmissionResult> ReconstructCachedPartition(
    int region_count,
    int boundary_count,
    const std::vector<std::vector<int>>& prev,
    const std::vector<std::vector<LandmarkRegionEmissionResult>>& intervals);

LandmarkPartitionResult TrySemanticSplitFromCandidate(
    const LandmarkPartitionResult& selected,
    std::vector<LandmarkRegionEmissionResult> candidate_emissions,
    double candidate_score,
    int vertex_count,
    const std::string& candidate_kind);

LandmarkPartitionResult TryOutlierPartitionFromSlots(
    const PropertySamples& reduced,
    const LandmarkPartitionResult& selected,
    const std::vector<int>& boundaries,
    const std::vector<std::vector<LandmarkRegionEmissionResult>>& intervals,
    const std::vector<std::vector<bool>>& valid,
    const ShapeFlatLandmarkSubpathOptions& options,
    int max_gap,
    int vertex_count);

}  // namespace path_multimode
}  // namespace bbsolver
