#include "bbsolver/path/reduction/path_post_solve_reduction.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>
#include <cstddef>
#include <functional>

#include "bbsolver/path/bridge_prune/path_bridge_prune.hpp"
#include "bbsolver/path/reduction/path_bridge_refit.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/path/config/path_solver_config.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"
#include "bbsolver/path/reduction/path_vertex_reduction.hpp"
#include "bbsolver/routing/property_classification.hpp"
#include "bbsolver/shape/sharp_corner_policy.hpp"
#include "bbsolver/shape/shape_flat_topology.hpp"

namespace bbsolver {

PostSolvePathVertexReductionResult TryPostSolvePathVertexReduction(
    const PropertySamples& original,
    const PropertyKeys& solved_keys,
    const SolverConfig& config,
    const CompInfo& comp,
    const ProgressWriter* progress,
    std::size_t property_idx,
    std::size_t property_count,
    std::function<bool()> cancel_fn,
    bool source_vertices_are_semantic_anchors) {
  PostSolvePathVertexReductionResult result;
  result.keys = solved_keys;

  if (!IsShapeFlatPath(original)) {
    result.notes = "post_solve_vertex_reduction_skipped: non_shape_flat";
    return result;
  }
  if (!solved_keys.converged || solved_keys.keys.empty()) {
    result.notes = "post_solve_vertex_reduction_skipped: keys_not_converged";
    return result;
  }

  const int source_min_vertices = MinShapeFlatVertexCount(original);
  const int key_min_vertices = MinShapeFlatKeyVertexCount(solved_keys);
  const int key_max_vertices = MaxShapeFlatKeyVertexCount(solved_keys);
  if (source_min_vertices <= 1 || key_min_vertices <= 1) {
    result.notes = "post_solve_vertex_reduction_skipped: malformed_shape_flat";
    return result;
  }
  result.source_vertices = key_max_vertices;
  const bool uniform_key_topology = UniformShapeFlatKeyTopology(solved_keys);
  std::string last_validation_rejection;

  auto validate_candidate = [&](PropertyKeys candidate,
                                const std::string& mode,
                                int fitted_vertices,
                                double key_refit_max_err,
                                const std::string& targets_note) -> bool {
    last_validation_rejection.clear();
    PathFrameFitOptions frame_options = ReplacementFrameFitOptions(config);
    frame_options.outline_tolerance = EffectivePathTolerance(config);
    frame_options.source_vertices_are_semantic_anchors =
        source_vertices_are_semantic_anchors;
    PathTemporalValidationOptions validation_options;
    validation_options.frame_fit_options = frame_options;
    validation_options.frame_fit_options.outline_tolerance =
        EffectivePathTolerance(config);
    const PathTemporalValidationResult validation =
        ValidatePathTemporalCandidate(original, candidate, validation_options);
    if (validation.samples_checked > 0 && validation.ok) {
      const SharpCornerValidationResult sharp_validation =
          ValidateSharpCornerPreservation(
              original, candidate, config, source_vertices_are_semantic_anchors);
      if (!sharp_validation.ok) {
        last_validation_rejection = sharp_validation.notes;
        return false;
      }
      result.accepted = true;
      result.keys = std::move(candidate);
      result.keys.max_err = validation.max_outline_error;
      result.keys.max_err_screen_px = validation.max_outline_error;
      result.keys.converged = true;
      result.fitted_vertices = fitted_vertices;
      result.max_outline_error = validation.max_outline_error;
      result.notes =
          "post_solve_vertex_reduction_accepted"
          "; mode=" + mode +
          "; source_vertices=" + std::to_string(key_max_vertices) +
          "; fitted_vertices=" + std::to_string(fitted_vertices) +
          "; keys=" + std::to_string(result.keys.keys.size()) +
          "; key_refit_error=" + std::to_string(key_refit_max_err) +
          "; temporal_validation_error=" +
          std::to_string(validation.max_outline_error) +
          (sharp_validation.enabled ? "; " + sharp_validation.notes
                                    : std::string{}) +
          targets_note;
      return true;
    }
    last_validation_rejection =
        "temporal_validation_failed; max_outline_error=" +
        std::to_string(validation.max_outline_error);
    return false;
  };

  const double duplicate_eps = std::max(config.tolerance, 1e-6);
  bool all_duplicate_terminal = uniform_key_topology;
  if (all_duplicate_terminal) {
    for (const Key& key : solved_keys.keys) {
      if (!ShapeFlatHasDuplicateTerminalClosure(key.v, duplicate_eps)) {
        all_duplicate_terminal = false;
        break;
      }
    }
  }
  if (all_duplicate_terminal) {
    result.attempted = true;
    PropertyKeys candidate = solved_keys;
    for (Key& key : candidate.keys) {
      key.v = DropShapeFlatDuplicateTerminalClosure(key.v);
    }
    if (validate_candidate(
            std::move(candidate),
            "duplicate_terminal_closure",
            key_min_vertices - 1,
            0.0,
            "; targets_tried=" + std::to_string(key_min_vertices - 1))) {
      PostSolvePathVertexReductionResult duplicate_result = result;
      if (config.path_replacement_prefer_vertices) {
        PostSolvePathVertexReductionResult bridge_prune =
            TryPostTemporalBridgePrune(
                original,
                duplicate_result.keys,
                config,
                comp,
                progress,
                property_idx,
                property_count,
                key_max_vertices,
                cancel_fn,
                source_vertices_are_semantic_anchors);
        if (bridge_prune.notes == "cancelled") {
          return bridge_prune;
        }
        if (bridge_prune.accepted) {
          bridge_prune.notes +=
              "; prepass=duplicate_terminal_closure"
              "; prepass_source_vertices=" + std::to_string(key_max_vertices) +
              "; prepass_fitted_vertices=" +
              std::to_string(key_min_vertices - 1);
          return bridge_prune;
        }
        if (bridge_prune.attempted && !bridge_prune.notes.empty()) {
          duplicate_result.notes +=
              " | post_temporal_bridge_prune_after_duplicate_rejected: " +
              bridge_prune.notes;
        }
      }
      return duplicate_result;
    }
  }

  PostSolvePathVertexReductionResult bridge_prune =
      TryPostTemporalBridgePrune(
          original,
          solved_keys,
          config,
          comp,
          progress,
          property_idx,
          property_count,
          0,
          cancel_fn,
          source_vertices_are_semantic_anchors);
  if (bridge_prune.notes == "cancelled") {
    return bridge_prune;
  }
  if (bridge_prune.accepted) {
    return bridge_prune;
  }

  if (!uniform_key_topology) {
    if (bridge_prune.attempted) {
      return bridge_prune;
    }
    result.attempted = true;
    result.notes =
        "post_solve_vertex_reduction_skipped: mixed_key_topology"
        "; source_vertices=" + std::to_string(key_max_vertices) +
        "; post_temporal_bridge_prune=disabled"
        "; aggressive_reducer=disabled";
    return result;
  }

  if (!config.allow_path_replacement_fit) {
    if (result.attempted) {
      result.notes =
          "post_solve_vertex_reduction_rejected"
          "; source_vertices=" + std::to_string(key_max_vertices) +
          "; targets_tried=" + std::to_string(key_min_vertices - 1) +
          "; failures=duplicate_terminal_closure:validation_failed"
          "; aggressive_reducer=disabled";
    } else {
      result.notes =
          "post_solve_vertex_reduction_skipped: no_duplicate_terminal_closure"
          "; aggressive_reducer=disabled";
    }
    return result;
  }

  const int min_target = std::max(config.path_replacement_min_vertices, 4);
  if (key_min_vertices <= min_target) {
    result.notes =
        "post_solve_vertex_reduction_skipped: source_already_at_min_vertices";
    return result;
  }

  std::vector<std::vector<double>> key_frames;
  key_frames.reserve(solved_keys.keys.size());
  for (const Key& key : solved_keys.keys) {
    key_frames.push_back(key.v);
  }

  PathFrameFitOptions frame_options = ReplacementFrameFitOptions(config);
  frame_options.outline_tolerance = EffectivePathTolerance(config);
  frame_options.source_vertices_are_semantic_anchors =
      source_vertices_are_semantic_anchors;
  PathReplacementTargetLadderOptions ladder_options;
  ladder_options.min_target_vertices = min_target;
  ladder_options.max_target_vertices = config.path_replacement_max_vertices;
  ladder_options.step_vertices = 1;
  ladder_options.max_candidate_targets = 8;
  ladder_options.include_source_min_minus_one = true;

  const int initial_target =
      std::max(min_target, std::min(key_min_vertices - 1,
                                    std::max(min_target, key_min_vertices / 2)));
  const std::vector<int> targets =
      BuildShapeFlatReplacementTargetLadder(
          initial_target, key_min_vertices, ladder_options);
  if (targets.empty()) {
    result.notes = "post_solve_vertex_reduction_skipped: no_legal_targets";
    return result;
  }

  result.attempted = true;
  std::vector<std::string> failures;
  if (bridge_prune.attempted && !bridge_prune.notes.empty()) {
    failures.push_back("bridge_prune:" + bridge_prune.notes);
  }
  for (int target_vertices : targets) {
    const PathFeatureFractionLayoutResult layout =
        BuildShapeFlatFeatureFractionLayout(
            key_frames, target_vertices, frame_options);
    if (layout.ok) {
      PropertyKeys candidate = solved_keys;
      double key_refit_max_err = 0.0;
      bool keys_ok = true;
      for (Key& key : candidate.keys) {
        PathFrameFitResult fit =
            FitShapeFlatFrameAtFractions(
                key.v, layout.outline_fractions, frame_options);
        key_refit_max_err = std::max(key_refit_max_err, fit.max_outline_error);
        if (!fit.ok || !fit.applied ||
            fit.fitted_vertex_count != target_vertices ||
            ShapeFlatVertexCount(fit.fitted) != target_vertices) {
          keys_ok = false;
          break;
        }
        key.v = std::move(fit.fitted);
      }
      if (keys_ok) {
        if (validate_candidate(
                std::move(candidate), "shared_fraction_layout",
                target_vertices,
                key_refit_max_err,
                "; targets_tried=" + JoinInts(targets))) {
          return result;
        }
        failures.push_back("target=" + std::to_string(target_vertices) +
                           ":shared_fraction_layout:validation_failed" +
                           (last_validation_rejection.empty()
                                ? std::string{}
                                : ":" + last_validation_rejection));
      }
      if (!keys_ok) {
        failures.push_back("target=" + std::to_string(target_vertices) +
                           ":shared_fraction_layout:key_refit_failed");
      }
    } else {
      failures.push_back(
          "target=" + std::to_string(target_vertices) +
          ":layout_rejected" +
          (layout.warning.empty() ? std::string{} : ":" + layout.warning));
    }

    PropertyKeys candidate = solved_keys;
    double key_refit_max_err = 0.0;
    bool keys_ok = true;
    for (Key& key : candidate.keys) {
      PathFrameFitOptions target_options = frame_options;
      target_options.target_vertex_count = target_vertices;
      PathFrameFitResult fit = FitShapeFlatFrame(key.v, target_options);
      key_refit_max_err = std::max(key_refit_max_err, fit.max_outline_error);
      if (!fit.ok || !fit.applied ||
          fit.fitted_vertex_count != target_vertices ||
          ShapeFlatVertexCount(fit.fitted) != target_vertices) {
        keys_ok = false;
        break;
      }
      key.v = std::move(fit.fitted);
    }
    if (!keys_ok) {
      failures.push_back("target=" + std::to_string(target_vertices) +
                         ":independent_key_fit:key_refit_failed");
      continue;
    }
    if (validate_candidate(
            std::move(candidate),
            "independent_key_fit",
            target_vertices,
            key_refit_max_err,
            "; targets_tried=" + JoinInts(targets))) {
      return result;
    }
    failures.push_back("target=" + std::to_string(target_vertices) +
                       ":independent_key_fit:validation_failed" +
                       (last_validation_rejection.empty()
                            ? std::string{}
                            : ":" + last_validation_rejection));
  }

  result.notes =
      "post_solve_vertex_reduction_rejected"
      "; source_vertices=" + std::to_string(key_min_vertices) +
      "; targets_tried=" + JoinInts(targets) +
      "; failures=" + JoinNotes(failures);
  return result;
}

}  // namespace bbsolver
