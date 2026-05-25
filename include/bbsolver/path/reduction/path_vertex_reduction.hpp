#pragma once

#include "bbsolver/domain.hpp"


#include <cstddef>
#include <functional>
#include <limits>
#include <string>
#include <vector>

namespace bbsolver {

int MinShapeFlatVertexCount(const PropertySamples& samples);

int MaxShapeFlatSampleVertexCount(const PropertySamples& samples);

int MinShapeFlatKeyVertexCount(const PropertyKeys& keys);

bool UniformShapeFlatKeyTopology(const PropertyKeys& keys);

int MaxShapeFlatKeyVertexCount(const PropertyKeys& keys);

int DominantClosedShapeFlatKeyVertexCount(const PropertyKeys& keys,
                                          int min_target_vertices);

bool BridgePruneShapeFlatKeyClass(PropertyKeys* candidate,
                                  int target_vertices,
                                  int removed_index,
                                  int* affected_keys);

struct BridgePruneCandidateEvaluation {
  int removed_index = 0;
  int affected_keys = 0;
  int result_vertices = 0;
  bool fit_ok = false;
  bool validation_ok = false;
  bool sharp_ok = false;
  bool accepted = false;
  double max_err = std::numeric_limits<double>::infinity();
  double max_err_screen_px = std::numeric_limits<double>::infinity();
  double fit_wall_ms = 0.0;
  double validation_wall_ms = 0.0;
  double sharp_wall_ms = 0.0;
  PropertyKeys candidate;
  std::string failure_note;
};

bool BridgePruneCancelled(const std::function<bool()>& cancel_fn);

bool BridgePruneCandidateIsBetter(
    const BridgePruneCandidateEvaluation& candidate,
    const BridgePruneCandidateEvaluation& incumbent);

int PostTemporalBridgePrunePassBudget(const SolverConfig& config,
                                      int initial_vertices,
                                      int min_target_vertices);

double BridgePruneLocalProgress(int initial_vertices,
                                int min_target_vertices,
                                int current_vertices,
                                double candidate_fraction);

std::size_t BridgePruneProgressChunkSize(std::size_t candidate_count,
                                         int parallel_jobs);

std::vector<int> BuildReplacementTargetLadder(int auto_max_vertices,
                                              int source_min_vertices,
                                              int source_max_vertices,
                                              const SolverConfig& config);

std::string JoinInts(const std::vector<int>& values);

std::string JoinNotes(const std::vector<std::string>& values);

std::string BridgePruneTelemetryNotes(int fit_failures,
                                      int validation_failures,
                                      int sharp_failures,
                                      int accepted_candidates);

}  // namespace bbsolver
