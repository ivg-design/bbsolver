#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/path/multimode/path_multimode_geometry.hpp"
#include "bbsolver/path/multimode/path_multimode_landmark_emission.hpp"
#include "bbsolver/path/multimode/path_multimode_solver.hpp"

#include <string>
#include <vector>

namespace bbsolver {
namespace path_multimode {

struct LandmarkPartitionResult {
  bool ok = false;
  std::vector<LandmarkRegionEmissionResult> emissions;
  std::string base_ranges;
  std::string chosen_ranges;
  std::string semantic_split_note;
  std::string outlier_partition_note;
  double score = 0.0;
  int boundary_count = 0;
  int interval_evaluations = 0;
};

std::vector<int> RegionBoundaryPoints(const std::vector<VertexRegion>& regions,
                                      int vertex_count);

int BoundaryIndex(const std::vector<int>& boundaries, int boundary);

bool RegionsCoverFullRange(
    const std::vector<LandmarkRegionEmissionResult>& emissions,
    int vertex_count);

int TotalEmissionKeyCount(
    const std::vector<LandmarkRegionEmissionResult>& emissions);

double MaxEmissionReconstructionError(
    const std::vector<LandmarkRegionEmissionResult>& emissions);

bool DenseFullRangeNeedsSemanticSplit(
    const std::vector<LandmarkRegionEmissionResult>& selected,
    int vertex_count);

std::vector<VertexRegion> OutlierSlotRegions(int vertex_count, int slot);

LandmarkPartitionResult BuildKeyCountLandmarkPartition(
    const PropertySamples& reduced,
    const std::vector<VertexRegion>& base_regions,
    int vertex_count,
    const ShapeFlatLandmarkSubpathOptions& options,
    int max_gap);

}  // namespace path_multimode
}  // namespace bbsolver
