#pragma once

#include "bbsolver/domain.hpp"

#include <string>
#include <vector>

namespace bbsolver {

struct PathFitResult {
  bool is_shape_flat = false;
  bool stable_topology = false;
  bool applied = false;
  bool closed = false;
  int source_vertex_count = 0;
  int fitted_vertex_count = 0;
  int locked_vertex_count = 0;
  double max_outline_error = 0.0;
  std::string notes;
  std::vector<int> kept_indices;
  PropertySamples samples;
};

// Builds a smaller fixed-topology shape_flat sample stream by selecting one
// stable vertex layout over the full sampled range. Sharp/zero-tangent vertices
// are locked before simplification so AE interpolation preserves hard features.
PathFitResult FitCanonicalPathProperty(const PropertySamples& ps,
                                       const SolverConfig& cfg);

}  // namespace bbsolver
