#pragma once

#include "bbsolver/domain.hpp"

#include <string>

namespace bbsolver {

struct VisibleOutlinePrepassResult {
  bool applied = false;
  PropertySamples samples;
  std::string notes;
  int source_min_vertices = 0;
  int source_max_vertices = 0;
  int outline_min_vertices = 0;
  int outline_max_vertices = 0;
  int fitted_vertices = 0;
  double max_outline_error = 0.0;
};

VisibleOutlinePrepassResult TryVisibleOutlinePrepass(
    const PropertySamples& original,
    const SolverConfig& config);

}  // namespace bbsolver
