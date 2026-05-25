// Implements bbsolver::BuildShapeFlatReplacementTargetLadder (declared in
// path_frame_fit.hpp). Produces a bounded ladder of fixed-topology target
// vertex counts that callers try in order after computing their initial
// target. Every returned value is < source_min_vertices, so a passing
// candidate remains a vertex-count reduction for every source frame.
//
// Diagnostics decision: **none / pure layout**. This is a pure-arithmetic
// leaf: it consumes ints + a small options struct and emits a vector of
// ints. It never touches DiagnosticsWriter, progress, cancellation, or
// operator state. Acceptance failure is conveyed by an empty result; the
// caller decides how to react.
//
// Extracted from path_frame_fit.cpp without algorithmic change: the function
// body moves byte-faithfully and continues to honor the same option
// semantics (max_target_vertices, min_target_vertices, step_vertices,
// max_candidate_targets, include_source_min_minus_one) the
// PathReplacementTargetLadderOptions struct already documents.

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

#include <algorithm>
#include <vector>

namespace bbsolver {

std::vector<int> BuildShapeFlatReplacementTargetLadder(
    int initial_target_vertices,
    int source_min_vertices,
    const PathReplacementTargetLadderOptions& options) {
  std::vector<int> targets;
  if (initial_target_vertices <= 0 || source_min_vertices <= 1) {
    return targets;
  }

  int max_target = source_min_vertices - 1;
  if (options.max_target_vertices > 0) {
    max_target = std::min(max_target, options.max_target_vertices);
  }
  const int min_target = std::max(0, options.min_target_vertices);
  int current = std::max(initial_target_vertices, min_target);
  if (current > max_target) {
    return targets;
  }

  const int max_candidates = std::max(1, options.max_candidate_targets);
  const int step = std::max(1, options.step_vertices);
  auto push_unique = [&](int target) {
    if (target <= 0 || target > max_target) {
      return;
    }
    if (targets.empty() || targets.back() != target) {
      targets.push_back(target);
    }
  };

  while (current <= max_target &&
         static_cast<int>(targets.size()) < max_candidates) {
    push_unique(current);
    if (current > max_target - step) {
      break;
    }
    current += step;
  }

  if (options.include_source_min_minus_one &&
      !targets.empty() &&
      targets.back() != max_target) {
    if (static_cast<int>(targets.size()) < max_candidates) {
      push_unique(max_target);
    } else {
      targets.back() = max_target;
    }
  }
  return targets;
}

std::vector<int> BuildShapeFlatReplacementRetryTargetLadder(
    int fitted_vertices,
    int source_min_vertices,
    int configured_min_vertices,
    int configured_max_vertices) {
  PathReplacementTargetLadderOptions options;
  options.min_target_vertices =
      std::max(configured_min_vertices, fitted_vertices);
  options.max_target_vertices = configured_max_vertices;
  options.max_candidate_targets = 10;
  return BuildShapeFlatReplacementTargetLadder(
      fitted_vertices, source_min_vertices, options);
}

}  // namespace bbsolver
