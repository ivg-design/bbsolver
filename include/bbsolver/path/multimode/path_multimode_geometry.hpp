#pragma once

#include "bbsolver/domain.hpp"

#include <vector>

namespace bbsolver::path_multimode {

struct VertexRegion {
  int first_vertex = 0;
  int end_vertex = 0;
};

bool IsShapeFlatPath(const PropertySamples& ps);

int ShapeFlatVertexCount(const std::vector<double>& flat);

int MaxShapeFlatVertexCount(const PropertySamples& ps);

bool SameShapeFlatTopology(const std::vector<double>& a,
                           const std::vector<double>& b);

bool SameSampleTimes(const PropertySamples& a, const PropertySamples& b);

double ShapeComponent(const std::vector<double>& flat,
                      int vertex,
                      int component);

double VertexMotionBoundaryScore(const PropertySamples& reduced,
                                 int left_vertex,
                                 int right_vertex);

std::vector<VertexRegion> BuildVertexRegions(int vertex_count,
                                             int requested_regions);

std::vector<VertexRegion> BuildMotionAwareVertexRegions(
    const PropertySamples& reduced,
    int vertex_count,
    int requested_regions);

std::vector<double> ShapeFlatRegion(const std::vector<double>& flat,
                                    VertexRegion region);

bool InsertShapeFlatRegion(std::vector<double>* full,
                           VertexRegion region,
                           const std::vector<double>& region_shape);

PropertySamples BuildLandmarkRegionSamples(const PropertySamples& reduced,
                                           VertexRegion region);

}  // namespace bbsolver::path_multimode
