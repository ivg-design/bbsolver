#pragma once

#include "bbsolver/domain.hpp"
#include "bbsolver/app/cli_options.hpp"

#include <cstddef>

#include "bbsolver/motion_smooth/motion_smooth_reduction_gate.hpp"
#include "bbsolver/routing/property_solver_routing.hpp"

namespace bbsolver {

class ProgressWriter;

struct PropertyRouteSolveRequest {
  PropertySolveRoute route = PropertySolveRoute::PlainTemporal;
  const PropertySamples* original_property_samples = nullptr;
  const PropertySamples* property_samples = nullptr;
  const PropertySamples* temporal_source_samples = nullptr;
  const PropertySamples* temporal_property_samples = nullptr;
  const SolverConfig* config = nullptr;
  const SolverConfig* temporal_config = nullptr;
  const CompInfo* comp = nullptr;
  const SolveOptions* options = nullptr;
  const ProgressWriter* progress = nullptr;
  ShapeFlatNearOptimalResult* near_optimal_fast_path = nullptr;
  std::size_t property_idx = 0;
  std::size_t property_count = 1;
  int replacement_temporal_max_gap = 0;
  bool canonical_path_applied = false;
};

PropertyKeys SolvePropertyRoute(const PropertyRouteSolveRequest& request);

}  // namespace bbsolver
