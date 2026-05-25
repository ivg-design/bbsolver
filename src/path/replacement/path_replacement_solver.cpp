#include "bbsolver/path/replacement/path_replacement_solver.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <vector>
#include <cstddef>
#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>

#include "bbsolver/path/fit/path_fit_pipeline.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/path/replacement/path_replacement_adaptive_expansion.hpp"
#include "bbsolver/path/replacement/path_replacement_feature_layout_trial.hpp"
#include "bbsolver/path/replacement/path_replacement_fraction_layout.hpp"
#include "bbsolver/path/replacement/path_replacement_fraction_trial.hpp"
#include "bbsolver/path/replacement/path_replacement_initial_scan.hpp"
#include "bbsolver/path/replacement/path_replacement_notes.hpp"
#include "bbsolver/path/replacement/path_replacement_phase2_fit.hpp"
#include "bbsolver/path/replacement/path_replacement_seed_selection.hpp"
#include "bbsolver/path/config/path_solver_config.hpp"
#include "bbsolver/path/reduction/path_vertex_reduction.hpp"
#include "bbsolver/progress/progress.hpp"

namespace bbsolver {

ReplacementPathFitResult FitReplacementPathProperty(
    const PropertySamples& property_samples,
    const SolverConfig& config,
    const ProgressWriter* progress,
    std::size_t property_idx,
    std::size_t property_count,
    bool source_vertices_are_semantic_anchors) {
  ReplacementPathFitResult result;
  result.samples = property_samples;
  result.notes = "path_replacement_fit skipped";

  if (property_samples.samples.empty()) {
    result.notes = "path_replacement_fit skipped: no samples";
    return result;
  }

  PathFrameFitOptions frame_options = ReplacementFrameFitOptions(config);
  frame_options.source_vertices_are_semantic_anchors =
      source_vertices_are_semantic_anchors;

  const ReplacementInitialFrameScan initial_scan =
      ScanReplacementInitialFrames(property_samples, frame_options);
  if (!initial_scan.ok) {
    result.notes = initial_scan.warning;
    return result;
  }

  const int source_min = initial_scan.source_min_vertices;
  const int source_max = initial_scan.source_max_vertices;
  const int auto_min = initial_scan.auto_min_vertices;
  const int auto_max = initial_scan.auto_max_vertices;
  if (initial_scan.auto_changed_frames == 0) {
    result.notes = "path_replacement_fit unchanged";
    return result;
  }

  const std::vector<int> target_ladder =
      BuildReplacementTargetLadder(auto_max, source_min, source_max, config);
  if (target_ladder.empty()) {
    result.notes = "path_replacement_fit skipped: no legal target vertices" +
                   std::string("; auto_max_vertices=") +
                   std::to_string(auto_max) +
                   "; source_min_vertices=" + std::to_string(source_min);
    return result;
  }

  std::vector<std::vector<double>> shape_flat_frames;
  shape_flat_frames.reserve(property_samples.samples.size());
  for (const Sample& sample : property_samples.samples) {
    shape_flat_frames.push_back(sample.v);
  }

  auto try_target = [&](int target_vertices,
                        double target_stage_start,
                        double target_stage_end) -> ReplacementPathFitResult {
    ReplacementPathFitResult attempt;
    attempt.samples = property_samples;
    attempt.notes = "path_replacement_fit skipped";
    attempt.source_min_vertices = source_min;
    attempt.source_max_vertices = source_max;
    attempt.auto_min_vertices = auto_min;
    attempt.auto_max_vertices = auto_max;

    if (target_vertices <= 0) {
      attempt.notes = "path_replacement_fit skipped: invalid target_vertices=" +
                      std::to_string(target_vertices);
      return attempt;
    }
    if (target_vertices >= source_max ||
        (source_min == source_max && target_vertices >= source_min)) {
      attempt.notes = "path_replacement_fit unchanged; target_vertices=" +
                      std::to_string(target_vertices);
      return attempt;
    }

    PathFrameFitOptions targeted_options = frame_options;
    targeted_options.target_vertex_count = target_vertices;
    PathFrameFitOptions coherence_options = targeted_options;
    coherence_options.outline_tolerance = EffectivePathTolerance(config);
    auto emit_target_progress =
        [&](const char* event,
            const std::string& phase,
            double local_fraction,
            int frame_index = -1,
            int frame_total = 0) {
          if (progress == nullptr) {
            return;
          }
          nlohmann::json progress_event = PropertyProgressEvent(
              event,
              phase + " for " + ProgressPropertyLabel(property_samples),
              property_idx,
              property_count,
              target_stage_start +
                  (target_stage_end - target_stage_start) *
                      std::clamp(local_fraction, 0.0, 1.0),
              property_samples);
          progress_event["target_vertices"] = target_vertices;
          if (frame_total > 0) {
            progress_event["frame_index"] = frame_index;
            progress_event["frame_total"] = frame_total;
          }
          progress->Emit(progress_event);
        };

    // Phase 2: per-frame automatic fit at target vertex count. Low-topology
    // targets may be impossible for independent frames but still work with a
    // semantic shared fraction layout, so a Phase 2 miss is not terminal.
    ReplacementPhase2FitResult phase2_fit =
        FitReplacementPhase2Records(
            property_samples,
            targeted_options,
            target_vertices,
            emit_target_progress);
    std::vector<ReplacementFrameFitRecord> phase2_records =
        std::move(phase2_fit.records);
    const bool phase2_ok = phase2_fit.ok;
    std::string phase2_warning = std::move(phase2_fit.warning);

    std::vector<int> seed_indices;
    if (phase2_ok) {
      seed_indices =
          SelectReplacementPhase2SeedIndices(phase2_records, target_vertices);
    }

    PropertySamples fitted = property_samples;
    fitted.samples.clear();
    fitted.samples.reserve(property_samples.samples.size());
    bool fraction_coherence_applied = false;
    int best_seed_phase2_idx = -1;
    int adaptive_insertions = 0;
    int adaptive_evaluations = 0;
    int coherent_vertices = target_vertices;
    double best_fraction_err = std::numeric_limits<double>::infinity();
    double best_attempt_err = std::numeric_limits<double>::infinity();
    bool median_fraction_layout_tried = false;
    bool median_fraction_layout_applied = false;
    std::vector<double> best_winning_fractions;

    auto try_fraction_layout = [&](const std::vector<double>& fractions,
                                   int seed_idx,
                                   int adaptive_count) -> bool {
      ReplacementFractionTrialResult trial =
          TryReplacementFractionLayout(
              property_samples, fractions, coherence_options,
              emit_target_progress);
      if (trial.ok && trial.max_outline_error < best_fraction_err) {
        best_fraction_err = trial.max_outline_error;
        best_seed_phase2_idx = seed_idx;
        fitted = std::move(trial.samples);
        fraction_coherence_applied = true;
        coherent_vertices = trial.fraction_count;
        adaptive_insertions = adaptive_count;
        best_winning_fractions = std::move(trial.fractions);
        return true;
      } else if (!trial.ok) {
        best_attempt_err =
            std::min(best_attempt_err, trial.max_outline_error);
      }
      return false;
    };

    // Phase 3A: cross-frame feature-layout seed. This is the primary
    // low-topology path: it does not require independent per-frame automatic
    // simplification to hit the target first.
    ReplacementFeatureLayoutTrialResult feature_layout_trial;

    if (phase2_ok) {
      if (!fraction_coherence_applied) {
        const std::vector<double> median_fractions =
            BuildMedianStableFractionLayout(phase2_records, target_vertices);
        if (!median_fractions.empty()) {
          median_fraction_layout_tried = true;
          median_fraction_layout_applied =
              try_fraction_layout(median_fractions, /*seed_idx=*/-2,
                                  /*adaptive_count=*/0);
        }
      }

      if (!fraction_coherence_applied) {
        for (int seed_idx : seed_indices) {
          try_fraction_layout(phase2_records[seed_idx].outline_fractions,
                              seed_idx,
                              /*adaptive_count=*/0);
          if (fraction_coherence_applied) {
            break;
          }
        }
      }
    }

    if (!fraction_coherence_applied && phase2_ok && !seed_indices.empty()) {
      ReplacementAdaptiveExpansionResult adaptive_expansion =
          TryReplacementAdaptiveFractionExpansion(
              shape_flat_frames,
              phase2_records,
              seed_indices,
              config,
              coherence_options,
              source_min,
              target_vertices,
              try_fraction_layout);
      adaptive_evaluations += adaptive_expansion.evaluations;
      best_attempt_err =
          std::min(best_attempt_err, adaptive_expansion.best_attempt_error);
    }

    if (!fraction_coherence_applied) {
      feature_layout_trial = TryReplacementFeatureFractionLayout(
          shape_flat_frames,
          target_vertices,
          source_min,
          config,
          frame_options,
          try_fraction_layout);
    }

    if (fraction_coherence_applied) {
      attempt.max_outline_error = best_fraction_err;
    } else {
      if (!phase2_ok) {
        attempt.notes = "path_replacement_fit skipped: " + phase2_warning +
                        "; target_vertices=" + std::to_string(target_vertices) +
                        "; feature_layout_warning=" +
                        feature_layout_trial.warning +
                        "; best_attempt_err=" +
                        (best_attempt_err < std::numeric_limits<double>::infinity()
                             ? std::to_string(best_attempt_err)
                             : "none");
        return attempt;
      }
      attempt.max_outline_error = 0.0;
      for (ReplacementFrameFitRecord& rec : phase2_records) {
        Sample s;
        s.t_sec = rec.t_sec;
        s.v = std::move(rec.fitted);
        attempt.max_outline_error =
            std::max(attempt.max_outline_error, rec.max_outline_error);
        fitted.samples.push_back(std::move(s));
      }
    }
    emit_target_progress("path_replacement_target_layout_done",
                         fraction_coherence_applied
                             ? "Replacement fraction layout accepted"
                             : "Replacement fraction layout not accepted",
                         0.78);

    ReplacementFractionCoherenceNoteInput fraction_note_input;
    fraction_note_input.seed_indices_empty = seed_indices.empty();
    fraction_note_input.fraction_coherence_applied =
        fraction_coherence_applied;
    fraction_note_input.median_fraction_layout_tried =
        median_fraction_layout_tried;
    fraction_note_input.median_fraction_layout_applied =
        median_fraction_layout_applied;
    fraction_note_input.phase2_ok = phase2_ok;
    fraction_note_input.fraction_seed_count =
        static_cast<int>(seed_indices.size());
    fraction_note_input.best_seed_phase2_idx = best_seed_phase2_idx;
    fraction_note_input.adaptive_insertions = adaptive_insertions;
    fraction_note_input.adaptive_evaluations = adaptive_evaluations;
    fraction_note_input.coherent_vertices = coherent_vertices;
    fraction_note_input.best_fraction_error = best_fraction_err;
    fraction_note_input.best_attempt_error = best_attempt_err;
    fraction_note_input.phase2_warning = phase2_warning;
    const std::string fraction_coherence_note =
        BuildReplacementFractionCoherenceNote(
            fraction_note_input, feature_layout_trial);

    SingleRegimeOptions regime_options;
    regime_options.target_vertex_count = coherent_vertices;
    SingleRegimeResult single_regime =
        BuildSingleStableRegime(fitted, regime_options);
    if (!single_regime.ok) {
      attempt.notes = "path_replacement_fit skipped: " + single_regime.reason +
                      "; target_vertices=" + std::to_string(target_vertices) +
                      "; fraction_coherence=" + fraction_coherence_note;
      return attempt;
    }

    const StablePathRegime& regime = single_regime.regime;
    const ReplacementCandidateAssessment assessment =
        AssessReplacementCandidate(
            regime, property_samples, EffectivePathTolerance(config));
    if (!assessment.worth_attempting) {
      attempt.notes = "path_replacement_fit skipped: " + assessment.reason +
                      "; target_vertices=" + std::to_string(target_vertices) +
                      "; fraction_coherence=" + fraction_coherence_note +
                      "; frame_outline_error=" +
                      std::to_string(attempt.max_outline_error);
      return attempt;
    }

    if (regime.vertex_count >= source_max) {
      attempt.notes = "path_replacement_fit unchanged; fitted_vertices=" +
                      std::to_string(regime.vertex_count);
      return attempt;
    }

    attempt.applied = true;
    attempt.samples = regime.samples;
    attempt.source_min_vertices = source_min;
    attempt.source_max_vertices = source_max;
    attempt.auto_min_vertices = auto_min;
    attempt.auto_max_vertices = auto_max;
    attempt.fitted_vertices = regime.vertex_count;
    attempt.estimated_candidate_keys = assessment.estimated_candidate_keys;
    attempt.estimated_original_keys = assessment.estimated_original_keys;
    if (fraction_coherence_applied) {
      attempt.winning_fractions = std::move(best_winning_fractions);
    }
    ReplacementSuccessNoteInput success_note_input;
    success_note_input.source_min_vertices = source_min;
    success_note_input.source_max_vertices = source_max;
    success_note_input.auto_min_vertices = auto_min;
    success_note_input.auto_max_vertices = auto_max;
    success_note_input.target_vertices = target_vertices;
    success_note_input.fitted_vertices = regime.vertex_count;
    success_note_input.estimated_candidate_keys =
        assessment.estimated_candidate_keys;
    success_note_input.estimated_original_keys =
        assessment.estimated_original_keys;
    success_note_input.frame_count = property_samples.samples.size();
    success_note_input.frame_outline_error = attempt.max_outline_error;
    success_note_input.fraction_coherence_note = fraction_coherence_note;
    attempt.notes = BuildReplacementSuccessNote(success_note_input);
    emit_target_progress("path_replacement_target_done",
                         "Replacement target accepted",
                         1.0);
    return attempt;
  };

  std::vector<int> targets_tried;
  std::vector<std::string> target_failures;
  const double target_count = static_cast<double>(
      std::max<std::size_t>(target_ladder.size(), 1));
  for (std::size_t target_idx = 0; target_idx < target_ladder.size(); ++target_idx) {
    const int target_vertices = target_ladder[target_idx];
    targets_tried.push_back(target_vertices);
    if (progress != nullptr) {
      progress->Emit({
          {"event", "path_replacement_target_start"},
          {"phase", "Trying replacement target " +
                        std::to_string(target_vertices) + " vertices for " +
                        ProgressPropertyLabel(property_samples)},
          {"progress", SolveProgressForPropertyStage(
                           property_idx,
                           property_count,
                           0.10 + 0.10 *
                               (static_cast<double>(target_idx) / target_count))},
          {"id", property_samples.property.id},
          {"display_name", ProgressPropertyLabel(property_samples)},
          {"i", property_idx},
          {"n", property_count},
          {"target_vertices", target_vertices},
      });
    }
    ReplacementPathFitResult attempt = try_target(
        target_vertices,
        0.10 + 0.10 * (static_cast<double>(target_idx) / target_count),
        0.10 + 0.10 * (static_cast<double>(target_idx + 1) / target_count));
    if (attempt.applied) {
      attempt.notes += "; replacement_targets_tried=" + JoinInts(targets_tried) +
                       "; replacement_target_winner=" +
                       std::to_string(target_vertices) +
                       "; replacement_target_failures=" +
                       JoinNotes(target_failures);
      return attempt;
    }
    if (progress != nullptr) {
      progress->Emit({
          {"event", "path_replacement_target_rejected"},
          {"phase", "Rejected replacement target " +
                        std::to_string(target_vertices) + " vertices for " +
                        ProgressPropertyLabel(property_samples)},
          {"progress", SolveProgressForPropertyStage(
                           property_idx,
                           property_count,
                           0.10 + 0.10 *
                               (static_cast<double>(target_idx + 1) /
                                target_count))},
          {"id", property_samples.property.id},
          {"display_name", ProgressPropertyLabel(property_samples)},
          {"i", property_idx},
          {"n", property_count},
          {"target_vertices", target_vertices},
      });
    }
    target_failures.push_back(
        "target_" + std::to_string(target_vertices) + ": " + attempt.notes);
  }

  result.notes =
      "path_replacement_fit skipped: no replacement target passed" +
      std::string("; replacement_targets_tried=") + JoinInts(targets_tried) +
      "; replacement_target_failures=" + JoinNotes(target_failures);
  return result;
}

}  // namespace bbsolver
