#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/path/multimode/path_multimode_geometry.hpp"
#include "bbsolver/path/multimode/path_multimode_notes.hpp"
#include "bbsolver/path/multimode/path_multimode_solver.hpp"

#include <vector>

namespace bbsolver {
namespace path_multimode {

bool SameRegionList(const std::vector<VertexRegion>& a,
                    const std::vector<VertexRegion>& b);

void AddVisibleProbePartition(
    const std::vector<VertexRegion>& regions,
    int vertex_count,
    std::vector<std::vector<VertexRegion>>* partitions);

std::vector<std::vector<VertexRegion>> VisibleProbePartitions(
    const PropertySamples& reduced,
    int vertex_count,
    int channel_count);

VisibleChannelProbeCandidate EvaluateVisibleProbePartition(
    const PropertySamples& reduced,
    const std::vector<VertexRegion>& regions,
    int channel_count,
    const ShapeFlatLandmarkSubpathOptions& options,
    int max_gap);

VisibleChannelProbeResult RunVisibleChannelProbe(
    const PropertySamples& reduced,
    const ShapeFlatLandmarkSubpathOptions& options,
    int max_gap,
    int vertex_count);

}  // namespace path_multimode
}  // namespace bbsolver
