#pragma once

#include "bbsolver/domain.hpp"

#include <cstddef>
#include <string>
#include <vector>

#include "bbsolver/motion_smooth/motion_smooth_reduction_gate.hpp"

namespace bbsolver {

class ProgressWriter;

struct PathSolvePreparationRequest {
  const PropertySamples* source_property = nullptr;
  const SolverConfig* config = nullptr;
  const CompInfo* comp = nullptr;
  const ProgressWriter* progress = nullptr;
  std::size_t property_idx = 0;
  std::size_t property_count = 0;
};

struct PathSolvePreparationState {
  PropertySamples original_property_samples;
  PropertySamples property_samples;
  bool visible_outline_reference = false;
  std::string path_fit_note;
  ShapeFlatNearOptimalResult near_optimal_fast_path;
  bool replacement_path_applied = false;
  bool canonical_path_applied = false;
  bool replacement_output_accepted = false;
  bool replacement_fast_vertex_preference_accepted = false;
  int replacement_fitted_vertices = 0;
  int replacement_original_max_vertices = 0;
  int replacement_source_min_vertices = 0;
  int replacement_estimated_candidate_keys = 0;
  int replacement_estimated_original_keys = 0;
  std::vector<double> replacement_winning_fractions;
  double replacement_topology_fit_error = 0.0;
  double replacement_frame_fit_error = 0.0;
};

PathSolvePreparationState PreparePathSolveInputs(
    const PathSolvePreparationRequest& request);

}  // namespace bbsolver
