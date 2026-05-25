#include "bbsolver/path/bridge_prune/path_bridge_prune_round.hpp"
#include "bbsolver/domain.hpp"

#include "oneapi/tbb/parallel_for.h"
#include "bbsolver/path/bridge_prune/path_bridge_prune_candidate.hpp"
#include "bbsolver/path/bridge_prune/path_bridge_prune_progress.hpp"
#include "bbsolver/path/reduction/path_vertex_reduction.hpp"
#include "bbsolver/progress/progress.hpp"

#ifdef BBSOLVER_HAVE_TBB
#include <tbb/parallel_for.h>
#endif

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <functional>
#include <vector>

namespace bbsolver {

BridgePruneRoundEvaluationResult EvaluateBridgePruneCandidateRound(
    const PropertySamples& original,
    const PropertyKeys& current,
    const SolverConfig& config,
    const CompInfo& comp,
    const ProgressWriter* progress,
    std::size_t property_idx,
    std::size_t property_count,
    int initial_max_vertices,
    int min_target,
    int target_vertices,
    const std::vector<int>& removal_candidates,
    int first_attempt,
    const BridgePruneTimingTotals& initial_timing,
    const std::function<bool()>& cancel_fn,
    bool source_vertices_are_semantic_anchors) {
  BridgePruneRoundEvaluationResult result;
  result.timing = initial_timing;
  result.evaluations.resize(removal_candidates.size());
  if (progress != nullptr) {
    progress->Emit(BridgePruneCandidateStartEvent(
        original, property_idx, property_count, initial_max_vertices,
        min_target, target_vertices, removal_candidates.front(),
        removal_candidates.size(), first_attempt, result.timing));
  }

  std::atomic<bool> cancelled{false};
  const auto evaluate_candidate = [&](std::size_t candidate_idx) {
    if (cancelled.load(std::memory_order_relaxed)) {
      return;
    }
    if (BridgePruneCancelled(cancel_fn)) {
      cancelled.store(true, std::memory_order_relaxed);
      return;
    }
    result.evaluations[candidate_idx] = EvaluateBridgePruneCandidate(
        original, current, config, comp, target_vertices,
        removal_candidates[candidate_idx],
        source_vertices_are_semantic_anchors);
  };

  const std::size_t progress_chunk_size =
      BridgePruneProgressChunkSize(removal_candidates.size(),
                                   config.parallel_jobs);
  for (std::size_t chunk_begin = 0;
       chunk_begin < removal_candidates.size();
       chunk_begin += progress_chunk_size) {
    const std::size_t chunk_end =
        std::min(removal_candidates.size(), chunk_begin + progress_chunk_size);
#ifdef BBSOLVER_HAVE_TBB
    if (removal_candidates.size() >= 4 && chunk_end - chunk_begin >= 4) {
      tbb::parallel_for(chunk_begin, chunk_end, evaluate_candidate);
    } else {
      for (std::size_t candidate_idx = 0;
           candidate_idx < chunk_end - chunk_begin;
           ++candidate_idx) {
        evaluate_candidate(chunk_begin + candidate_idx);
      }
    }
#else
    for (std::size_t candidate_idx = 0;
         candidate_idx < chunk_end - chunk_begin;
         ++candidate_idx) {
      evaluate_candidate(chunk_begin + candidate_idx);
    }
#endif

    if (cancelled.load(std::memory_order_relaxed) ||
        BridgePruneCancelled(cancel_fn)) {
      result.cancelled = true;
      return result;
    }
    for (std::size_t candidate_idx = chunk_begin;
         candidate_idx < chunk_end;
         ++candidate_idx) {
      AccumulateBridgePruneTiming(
          &result.timing, result.evaluations[candidate_idx], false);
    }
    if (progress != nullptr) {
      const double candidate_fraction =
          static_cast<double>(chunk_end) /
          static_cast<double>(std::max<std::size_t>(
              1, removal_candidates.size()));
      progress->Emit(BridgePruneCandidateProgressEvent(
          original, property_idx, property_count, initial_max_vertices,
          min_target, target_vertices, removal_candidates[chunk_begin],
          removal_candidates.size(), chunk_end, candidate_fraction,
          first_attempt + static_cast<int>(chunk_begin), result.timing));
    }
  }
  return result;
}

}  // namespace bbsolver
