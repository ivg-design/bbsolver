#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/shape/shape_flat_topology.hpp"

#include <string>
#include <vector>

namespace bbsolver {

double SharpCornerAngleThresholdDeg(const SolverConfig& config);

double SharpCornerLockTolerance(const SolverConfig& config);

double ShapeFlatDeflectionAngleDeg(const std::vector<double>& flat,
                                   int vertex_index);

std::vector<int> ShapeFlatSharpCornerIndices(
    const std::vector<double>& flat,
    const SolverConfig& config);

std::vector<ShapeFlatPoint> ShapeFlatSharpCornerPoints(
    const std::vector<double>& flat,
    const SolverConfig& config);

bool ShapeFlatIndexIsSharpCorner(const std::vector<double>& flat,
                                 int vertex_index,
                                 const SolverConfig& config);

std::vector<std::vector<int>> PersistentShapeFlatSharpCornerIndicesByVertexCount(
    const PropertySamples& original,
    const SolverConfig& config);

bool ShapeFlatKeyIndexIsProtectedCorner(const PropertyKeys& keys,
                                        int target_vertices,
                                        int vertex_index,
                                        const SolverConfig& config);

struct SharpCornerValidationResult {
  bool enabled = false;
  bool ok = true;
  int samples_checked = 0;
  int source_corners = 0;
  int candidate_corners = 0;
  std::string notes;
};

SharpCornerValidationResult ValidateSharpCornerPreservation(
    const PropertySamples& original,
    const PropertyKeys& candidate,
    const SolverConfig& config,
    bool source_vertices_are_semantic_anchors = true);

}  // namespace bbsolver
