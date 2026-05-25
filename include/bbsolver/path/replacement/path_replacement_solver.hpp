#pragma once

#include "bbsolver/domain.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace bbsolver {

class ProgressWriter;

struct ReplacementPathFitResult {
  bool applied = false;
  PropertySamples samples;
  std::string notes;
  double max_outline_error = 0.0;
  int source_min_vertices = 0;
  int source_max_vertices = 0;
  int auto_min_vertices = 0;
  int auto_max_vertices = 0;
  int fitted_vertices = 0;
  int estimated_candidate_keys = 0;
  int estimated_original_keys = 0;
  // Winning fraction layout from the fraction-coherence pass. Populated when
  // fraction_coherence_applied=true so RunSolve can run the geometry-refinement
  // pass (stage 2) on the original source frames at this topology.
  std::vector<double> winning_fractions;
};

ReplacementPathFitResult FitReplacementPathProperty(
    const PropertySamples& property_samples,
    const SolverConfig& config,
    const ProgressWriter* progress = nullptr,
    std::size_t property_idx = 0,
    std::size_t property_count = 1,
    bool source_vertices_are_semantic_anchors = true);

}  // namespace bbsolver
