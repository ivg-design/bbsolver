#pragma once

#include "bbsolver/domain.hpp"

#include <cstddef>
#include <string>

#include "bbsolver/motion_smooth/motion_smooth_reduction_gate.hpp"
#include "bbsolver/routing/property_solver_routing.hpp"

namespace bbsolver {

class ProgressWriter;

struct PropertyTemporalPreludeRequest {
  const PropertySamples* original_property_samples = nullptr;
  const PropertySamples* property_samples = nullptr;
  const SolverConfig* config = nullptr;
  const CompInfo* comp = nullptr;
  const ProgressWriter* progress = nullptr;
  ShapeFlatNearOptimalResult* near_optimal_fast_path = nullptr;
  std::string* path_fit_note = nullptr;
  std::size_t property_idx = 0;
  std::size_t property_count = 0;
  bool replacement_path_applied = false;
  bool canonical_path_applied = false;
  bool decompose_paths = false;
  double replacement_frame_fit_error = 0.0;
};

struct PropertyTemporalPreludeState {
  PropertySamples temporal_source_samples;
  PropertySamples temporal_property_samples;
  std::string final_static_trim_note;
  bool motion_smooth_enabled = false;
  bool temporal_optimization_enabled = false;
  bool path_temporal_reduced_by_fit = false;
  int replacement_temporal_max_gap = 0;
  SolverConfig temporal_config;
  PropertySolveRoute property_solve_route = PropertySolveRoute::PlainTemporal;
};

PropertyTemporalPreludeState PreparePropertyTemporalPrelude(
    const PropertyTemporalPreludeRequest& request);

}  // namespace bbsolver
