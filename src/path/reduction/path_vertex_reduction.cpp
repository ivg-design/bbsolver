#include "bbsolver/path/reduction/path_vertex_reduction.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/path/reduction/path_bridge_refit.hpp"
#include "bbsolver/shape/shape_flat_topology.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>
#include <functional>

namespace bbsolver {
namespace {

void PushUniqueTarget(std::vector<int>* targets, int target) {
  if (targets == nullptr || target <= 0) {
    return;
  }
  if (std::find(targets->begin(), targets->end(), target) == targets->end()) {
    targets->push_back(target);
  }
}

}  // namespace

int MinShapeFlatVertexCount(const PropertySamples& samples) {
  int min_vertices = std::numeric_limits<int>::max();
  for (const Sample& sample: samples.samples) {
    const int n = ShapeFlatVertexCount(sample.v);
    if (n <= 0) {
      return 0;
    }
    min_vertices = std::min(min_vertices, n);
  }
  return min_vertices == std::numeric_limits<int>::max() ? 0: min_vertices;
}

int MaxShapeFlatSampleVertexCount(const PropertySamples& samples) {
  int max_vertices = 0;
  for (const Sample& sample: samples.samples) {
    const int n = ShapeFlatVertexCount(sample.v);
    if (n <= 0) {
      return 0;
    }
    max_vertices = std::max(max_vertices, n);
  }
  return max_vertices;
}

int MinShapeFlatKeyVertexCount(const PropertyKeys& keys) {
  int min_vertices = std::numeric_limits<int>::max();
  for (const Key& key: keys.keys) {
    const int n = ShapeFlatVertexCount(key.v);
    if (n <= 0) {
      return 0;
    }
    min_vertices = std::min(min_vertices, n);
  }
  return min_vertices == std::numeric_limits<int>::max() ? 0: min_vertices;
}

bool UniformShapeFlatKeyTopology(const PropertyKeys& keys) {
  if (keys.keys.empty()) {
    return false;
  }
  const int first_vertices = ShapeFlatVertexCount(keys.keys.front().v);
  if (first_vertices <= 0) {
    return false;
  }
  const bool first_closed = ShapeFlatClosed(keys.keys.front().v);
  for (const Key& key: keys.keys) {
    const int vertices = ShapeFlatVertexCount(key.v);
    if (vertices != first_vertices || ShapeFlatClosed(key.v) != first_closed) {
      return false;
    }
  }
  return true;
}

int MaxShapeFlatKeyVertexCount(const PropertyKeys& keys) {
  int max_vertices = 0;
  for (const Key& key: keys.keys) {
    const int n = ShapeFlatVertexCount(key.v);
    if (n <= 0) {
      return 0;
    }
    max_vertices = std::max(max_vertices, n);
  }
  return max_vertices;
}

int DominantClosedShapeFlatKeyVertexCount(const PropertyKeys& keys,
                                          int min_target_vertices) {
  std::vector<std::pair<int, int>> histogram;
  for (const Key& key: keys.keys) {
    const int n = ShapeFlatVertexCount(key.v);
    if (n <= min_target_vertices || !ShapeFlatClosed(key.v)) {
      continue;
    }
    auto found = std::find_if(
        histogram.begin(), histogram.end(),
        [n](const std::pair<int, int>& item) { return item.first == n; });
    if (found == histogram.end()) {
      histogram.push_back({n, 1});
    } else {
      ++found->second;
    }
  }
  int best_vertices = 0;
  int best_count = 0;
  for (const auto& item: histogram) {
    if (item.second > best_count ||
        (item.second == best_count && item.first > best_vertices)) {
      best_vertices = item.first;
      best_count = item.second;
    }
  }
  return best_vertices;
}

bool BridgePruneShapeFlatKeyClass(PropertyKeys* candidate,
                                  int target_vertices,
                                  int removed_index,
                                  int* affected_keys) {
  if (candidate == nullptr || target_vertices <= 0 || affected_keys == nullptr) {
    return false;
  }
  *affected_keys = 0;
  for (Key& key: candidate->keys) {
    if (ShapeFlatVertexCount(key.v) != target_vertices) {
      continue;
    }
    std::vector<double> pruned =
        BridgeRefitRemoveShapeFlatVertex(key.v, removed_index);
    if (ShapeFlatVertexCount(pruned) != target_vertices - 1) {
      return false;
    }
    key.v = std::move(pruned);
    ++(*affected_keys);
  }
  return *affected_keys > 0;
}

bool BridgePruneCancelled(const std::function<bool()>& cancel_fn) {
  return cancel_fn && cancel_fn();
}

bool BridgePruneCandidateIsBetter(
    const BridgePruneCandidateEvaluation& candidate,
    const BridgePruneCandidateEvaluation& incumbent) {
  constexpr double kTieEpsilon = 1e-9;
  if (candidate.max_err < incumbent.max_err - kTieEpsilon) {
    return true;
  }
  if (incumbent.max_err < candidate.max_err - kTieEpsilon) {
    return false;
  }
  if (candidate.max_err_screen_px <
      incumbent.max_err_screen_px - kTieEpsilon) {
    return true;
  }
  if (incumbent.max_err_screen_px <
      candidate.max_err_screen_px - kTieEpsilon) {
    return false;
  }
  if (candidate.result_vertices != incumbent.result_vertices) {
    return candidate.result_vertices < incumbent.result_vertices;
  }
  return candidate.removed_index < incumbent.removed_index;
}

int PostTemporalBridgePrunePassBudget(const SolverConfig& config,
                                      int initial_vertices,
                                      int min_target_vertices) {
  const int available =
      std::max(0, initial_vertices - min_target_vertices);
  if (available <= 0) {
    return 0;
  }
  double tolerance = 0.0;
  if (std::isfinite(config.tolerance)) {
    tolerance = std::max(tolerance, config.tolerance);
  }
  if (std::isfinite(config.tolerance_screen_px)) {
    tolerance = std::max(tolerance, config.tolerance_screen_px);
  }

  int budget = 16;
  if (tolerance >= 1.0) {
    budget = 24;
  }
  if (tolerance >= 2.0) {
    budget = 32;
  }
  if (tolerance >= 3.0) {
    budget = 48;
  }
  if (tolerance >= 5.0) {
    budget = 64;
  }
  return std::min(available, budget);
}

double BridgePruneLocalProgress(int initial_vertices,
                                int min_target_vertices,
                                int current_vertices,
                                double candidate_fraction) {
  const int reducible_vertices =
      std::max(1, initial_vertices - min_target_vertices);
  const double completed_vertices =
      static_cast<double>(std::max(0, initial_vertices - current_vertices)) +
      std::clamp(candidate_fraction, 0.0, 1.0);
  const double vertex_progress =
      std::clamp(completed_vertices / static_cast<double>(reducible_vertices),
                 0.0,
                 1.0);
  return std::min(0.895, 0.812 + 0.080 * vertex_progress);
}

std::size_t BridgePruneProgressChunkSize(std::size_t candidate_count,
                                         int parallel_jobs) {
  if (candidate_count <= 1) {
    return 1;
  }
  const int bounded_jobs = std::max(1, std::min(parallel_jobs, 8));
  return std::min(candidate_count, static_cast<std::size_t>(
      std::max(2, bounded_jobs)));
}

std::vector<int> BuildReplacementTargetLadder(int auto_max_vertices,
                                              int source_min_vertices,
                                              int source_max_vertices,
                                              const SolverConfig& config) {
  std::vector<int> targets;
  if (source_min_vertices <= 1) {
    return targets;
  }

  // Stable-topology sources need a strict vertex-count reduction. Variable
  // topology streams can still benefit from normalizing to the minimum source
  // topology, because higher-count frames collapse while minimum-count frames
  // stay unchanged.
  const bool variable_source_topology = source_max_vertices > source_min_vertices;
  int max_legal = variable_source_topology
      ? source_max_vertices - 1
: source_min_vertices - 1;
  if (config.path_replacement_max_vertices > 0) {
    max_legal = std::min(max_legal, config.path_replacement_max_vertices);
  }
  const int min_legal = std::max(config.path_replacement_min_vertices, 0);
  if (max_legal <= 0 || (min_legal > 0 && min_legal > max_legal)) {
    return targets;
  }

  int auto_fallback = std::min(auto_max_vertices, max_legal);
  auto_fallback = std::max(auto_fallback, min_legal);
  if (variable_source_topology && auto_fallback <= max_legal) {
    PushUniqueTarget(&targets, auto_fallback);
  }

  const int low_start = std::max({10, min_legal, auto_fallback});
  const int low_end = std::min(18, max_legal);
  if (low_start <= low_end) {
    for (int target = low_start; target <= low_end; target += 2) {
      PushUniqueTarget(&targets, target);
    }
    PushUniqueTarget(&targets, low_end);
  }

  if (auto_fallback <= max_legal) {
    PushUniqueTarget(&targets, auto_fallback);
  }

  if (targets.empty() && min_legal > 0 && min_legal <= max_legal) {
    PushUniqueTarget(&targets, min_legal);
  }
  if (!variable_source_topology) {
    std::sort(targets.begin(), targets.end());
  }
  return targets;
}

std::string JoinInts(const std::vector<int>& values) {
  std::string out;
  for (int value: values) {
    if (!out.empty()) {
      out += ",";
    }
    out += std::to_string(value);
  }
  return out;
}

std::string JoinNotes(const std::vector<std::string>& values) {
  std::string out;
  for (const std::string& value: values) {
    if (value.empty()) {
      continue;
    }
    if (!out.empty()) {
      out += " | ";
    }
    out += value;
  }
  return out;
}

std::string BridgePruneTelemetryNotes(int fit_failures,
                                      int validation_failures,
                                      int sharp_failures,
                                      int accepted_candidates) {
  return "; bridge_prune_fit_failures=" + std::to_string(fit_failures) +
         "; bridge_prune_validation_failures=" +
             std::to_string(validation_failures) +
         "; bridge_prune_sharp_failures=" + std::to_string(sharp_failures) +
         "; bridge_prune_accepted_candidates=" +
             std::to_string(accepted_candidates);
}

}  // namespace bbsolver
