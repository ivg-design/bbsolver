#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/path/multimode/path_multimode_geometry.hpp"
#include "bbsolver/path/multimode/path_multimode_landmark_partition.hpp"
#include "bbsolver/path/multimode/path_multimode_notes.hpp"
#include "bbsolver/path/multimode/path_multimode_solver.hpp"

#include <vector>

namespace bbsolver {
namespace path_multimode {

LandmarkPartitionResult BuildFastSummaryPartition(
    const PropertySamples& reduced,
    const std::vector<VertexRegion>& base_regions,
    int vertex_count,
    const ShapeFlatLandmarkSubpathOptions& normalized_options,
    int max_gap);

std::vector<PropertyKeys> BuildLandmarkSubpathOutputKeys(
    const PropertySamples& reduced,
    const ShapeFlatLandmarkSubpathOptions& normalized_options,
    const LandmarkPartitionResult& partition,
    const VisibleChannelProbeResult& visible_probe,
    int source_sample_count,
    int max_gap,
    int vertex_count);

}  // namespace path_multimode
}  // namespace bbsolver
