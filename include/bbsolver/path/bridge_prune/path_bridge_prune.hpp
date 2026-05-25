#pragma once

#include "bbsolver/domain.hpp"

#include <cstddef>
#include <functional>
#include <string>

namespace bbsolver {

class ProgressWriter;

struct PostSolvePathVertexReductionResult {
  bool accepted = false;
  bool attempted = false;
  PropertyKeys keys;
  std::string notes;
  int source_vertices = 0;
  int fitted_vertices = 0;
  double max_outline_error = 0.0;
};

PostSolvePathVertexReductionResult TryPostTemporalBridgePrune(
    const PropertySamples& original,
    const PropertyKeys& solved_keys,
    const SolverConfig& config,
    const CompInfo& comp,
    const ProgressWriter* progress = nullptr,
    std::size_t property_idx = 0,
    std::size_t property_count = 1,
    int source_vertices_override = 0,
    std::function<bool()> cancel_fn = {},
    bool source_vertices_are_semantic_anchors = true);

}  // namespace bbsolver
