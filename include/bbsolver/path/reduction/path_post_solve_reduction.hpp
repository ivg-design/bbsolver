#pragma once

#include "bbsolver/domain.hpp"

#include <cstddef>
#include <functional>

#include "bbsolver/path/bridge_prune/path_bridge_prune.hpp"

namespace bbsolver {

class ProgressWriter;

PostSolvePathVertexReductionResult TryPostSolvePathVertexReduction(
    const PropertySamples& original,
    const PropertyKeys& solved_keys,
    const SolverConfig& config,
    const CompInfo& comp,
    const ProgressWriter* progress = nullptr,
    std::size_t property_idx = 0,
    std::size_t property_count = 1,
    std::function<bool()> cancel_fn = {},
    bool source_vertices_are_semantic_anchors = true);

}  // namespace bbsolver
