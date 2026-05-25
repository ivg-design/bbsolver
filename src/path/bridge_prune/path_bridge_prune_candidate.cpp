#include "bbsolver/path/bridge_prune/path_bridge_prune_candidate.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/metrics/error_metrics.hpp"
#include "bbsolver/path/reduction/path_vertex_reduction.hpp"
#include "bbsolver/shape/sharp_corner_policy.hpp"
#include "bbsolver/verify/verifier.hpp"
#include "bbsolver/solve/solver_observability.hpp"

#include <chrono>
#include <string>
#include <utility>

namespace bbsolver {
namespace {

BridgePruneCandidateEvaluation EvaluateBridgePruneCandidateImpl(
    const PropertySamples& original,
    const PropertyKeys& current,
    const SolverConfig& config,
    const CompInfo& comp,
    int target_vertices,
    int removed_index,
    bool source_vertices_are_semantic_anchors,
    bool batch) {
  BridgePruneCandidateEvaluation evaluation;
  evaluation.removed_index = removed_index;
  evaluation.result_vertices = target_vertices - 1;

  PropertyKeys candidate = current;
  const auto fit_start = std::chrono::steady_clock::now();
  const bool fit_ok = BridgePruneShapeFlatKeyClass(
      &candidate, target_vertices, evaluation.removed_index,
      &evaluation.affected_keys);
  evaluation.fit_wall_ms = MillisecondsSince(fit_start);
  if (!fit_ok) {
    evaluation.failure_note =
        std::to_string(target_vertices) +
        (batch ? "v_batch_remove_" : "v_remove_") +
        std::to_string(evaluation.removed_index) + ":bridge_fit_failed";
    return evaluation;
  }
  evaluation.fit_ok = true;

  const auto validation_start = std::chrono::steady_clock::now();
  const ErrorReport validation =
      ValidateKeys(original, candidate.keys, config, comp);
  evaluation.validation_wall_ms = MillisecondsSince(validation_start);
  evaluation.max_err = validation.max_err;
  evaluation.max_err_screen_px = validation.max_err_screen_px;
  if (!PassesBridgePruneKeyValidation(validation, config)) {
    evaluation.failure_note =
        std::to_string(target_vertices) +
        (batch ? "v_batch_remove_" : "v_remove_") +
        std::to_string(evaluation.removed_index) + ":err=" +
        std::to_string(validation.max_err);
    return evaluation;
  }
  evaluation.validation_ok = true;

  const auto sharp_start = std::chrono::steady_clock::now();
  const SharpCornerValidationResult sharp_validation =
      ValidateSharpCornerPreservation(
          original, candidate, config, source_vertices_are_semantic_anchors);
  evaluation.sharp_wall_ms = MillisecondsSince(sharp_start);
  if (!sharp_validation.ok) {
    evaluation.failure_note =
        std::to_string(target_vertices) +
        (batch ? "v_batch_remove_" : "v_remove_") +
        std::to_string(evaluation.removed_index) +
        ":sharp_corner_preserve_failed:" + sharp_validation.notes;
    return evaluation;
  }
  evaluation.sharp_ok = true;

  candidate.max_err = validation.max_err;
  candidate.max_err_screen_px = validation.max_err_screen_px;
  candidate.converged = true;
  evaluation.candidate = std::move(candidate);
  evaluation.accepted = true;
  return evaluation;
}

}  // namespace

bool PassesBridgePruneKeyValidation(const ErrorReport& report,
                                    const SolverConfig& config) {
  const bool property_ok = report.max_err <= config.tolerance + 1e-9;
  const double screen_tolerance =
      config.tolerance_screen_px > 0.0 ? config.tolerance_screen_px :
                                         config.tolerance;
  const bool screen_ok =
      (config.tolerance_screen_px <= 0.0 && config.weight_screen <= 0.0) ||
      report.max_err_screen_px <= screen_tolerance + 1e-9;
  return property_ok && screen_ok;
}

BridgePruneCandidateEvaluation EvaluateBridgePruneCandidate(
    const PropertySamples& original,
    const PropertyKeys& current,
    const SolverConfig& config,
    const CompInfo& comp,
    int target_vertices,
    int removed_index,
    bool source_vertices_are_semantic_anchors) {
  return EvaluateBridgePruneCandidateImpl(
      original, current, config, comp, target_vertices, removed_index,
      source_vertices_are_semantic_anchors, false);
}

BridgePruneCandidateEvaluation EvaluateBridgePruneBatchCandidate(
    const PropertySamples& original,
    const PropertyKeys& current,
    const SolverConfig& config,
    const CompInfo& comp,
    int current_vertices,
    int shifted_removed_index,
    bool source_vertices_are_semantic_anchors) {
  return EvaluateBridgePruneCandidateImpl(
      original, current, config, comp, current_vertices, shifted_removed_index,
      source_vertices_are_semantic_anchors, true);
}

}  // namespace bbsolver
