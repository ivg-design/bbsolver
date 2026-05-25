#include "bbsolver/path/multimode/path_multimode_landmark_diagnostics.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/path/multimode/path_multimode_geometry.hpp"
#include "bbsolver/path/multimode/path_multimode_landmark_segment_fit.hpp"
#include "bbsolver/path/multimode/path_multimode_notes.hpp"
#include "bbsolver/path/multimode/path_multimode_reconstruction.hpp"
#include "bbsolver/path/multimode/path_multimode_region_candidate.hpp"
#include "bbsolver/path/multimode/path_multimode_temporal.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>
#include <cstddef>

namespace bbsolver {
namespace path_multimode {
namespace {

constexpr int kDenseRunMinAnchors = 3;
constexpr int kDenseRunMaxChecks = 64;
constexpr int kDenseRunMaxRuns = 6;
constexpr int kSegmentMergeDiagnosticMaxChecks = 128;
constexpr int kSegmentDiagnosticTopReasons = 4;

}  // namespace

PropertySamples WindowSamples(const PropertySamples& samples,
                              int i,
                              int j) {
  PropertySamples out = samples;
  out.samples.clear();
  if (i < 0 || j < i || j >= static_cast<int>(samples.samples.size())) {
    return out;
  }
  out.samples.reserve(static_cast<std::size_t>(j - i + 1));
  for (int idx = i; idx <= j; ++idx) {
    out.samples.push_back(samples.samples[static_cast<std::size_t>(idx)]);
  }
  out.t_start_sec = out.samples.front().t_sec;
  out.t_end_sec = out.samples.back().t_sec;
  return out;
}

PropertyKeys BuildSegmentDiagnosticCandidate(
    const PropertySamples& region_samples,
    int i,
    int j,
    const SegmentFitResult& fit) {
  PropertyKeys candidate;
  candidate.property_id = region_samples.property.id;
  candidate.converged = true;
  if (i < 0 || j <= i ||
      j >= static_cast<int>(region_samples.samples.size())) {
    return candidate;
  }

  Key start = MakeLinearShapeKey(region_samples, i, true, false);
  Key end = MakeLinearShapeKey(region_samples, j, false, true);
  if (!fit.key_value_at_i.empty()) {
    start.v = fit.key_value_at_i;
  }
  if (!fit.key_value_at_j.empty()) {
    end.v = fit.key_value_at_j;
  }
  start.interp_out = fit.interp;
  end.interp_in = fit.interp;
  if (!fit.ease_out_at_i.empty()) {
    start.temporal_ease_out = fit.ease_out_at_i;
  }
  if (!fit.ease_in_at_j.empty()) {
    end.temporal_ease_in = fit.ease_in_at_j;
  }

  candidate.keys.push_back(std::move(start));
  candidate.keys.push_back(std::move(end));
  SegmentReport report;
  report.start_idx = i;
  report.end_idx = j;
  report.reason = fit.reason;
  report.max_err = fit.max_err;
  report.max_err_screen_px = fit.max_err_screen_px;
  report.rms_err = fit.rms_err;
  candidate.segments.push_back(std::move(report));
  return candidate;
}

SegmentMergeAttemptDiagnostic ClassifySegmentMergeAttempt(
    const PropertySamples& region_samples,
    int i,
    int j,
    double tolerance,
    int max_gap) {
  SegmentMergeAttemptDiagnostic diagnostic;
  if (i < 0 || j <= i ||
      j >= static_cast<int>(region_samples.samples.size())) {
    diagnostic.lower_bound_class = "invalid_window";
    diagnostic.reason = "invalid_window";
    return diagnostic;
  }
  if (j - i > max_gap) {
    diagnostic.lower_bound_class = "gap_cap_exceeded";
    diagnostic.reason = "gap_cap_exceeded";
    return diagnostic;
  }

  SolverConfig config;
  config.tolerance = std::max(tolerance, 0.0);
  config.allow_hold = false;
  config.allow_linear = true;
  config.allow_bezier = true;
  config.allow_shape_temporal_bezier = true;

  ShapeMorphProgressBandOptions band_options =
      LandmarkBandOptions(config.tolerance, max_gap);
  band_options.compute_progress_bands = true;
  const ShapeMorphProgressBandResult oracle =
      EvaluateShapeFlatMorphProgressBands(
          region_samples,
          i,
          j,
          region_samples.samples[static_cast<std::size_t>(i)].v,
          region_samples.samples[static_cast<std::size_t>(j)].v,
          band_options);
  if (!oracle.ok) {
    diagnostic.lower_bound_class =
        NormalizeSegmentDiagnosticReason(oracle.reason);
    diagnostic.reason = diagnostic.lower_bound_class;
    return diagnostic;
  }

  diagnostic.lower_bound_class =
      ShapeMorphProgressFeasibilityClass(oracle, config.tolerance);
  if (diagnostic.lower_bound_class != "ae_ease_feasible") {
    diagnostic.reason = diagnostic.lower_bound_class;
    return diagnostic;
  }

  const SegmentFitResult fit =
      FitLandmarkRegionShapeSegment(i, j, region_samples, config,
                                    band_options, true);
  if (!fit.feasible) {
    diagnostic.reason = NormalizeSegmentDiagnosticReason(fit.reason);
    return diagnostic;
  }

  const PropertySamples window = WindowSamples(region_samples, i, j);
  const PropertyKeys candidate =
      BuildSegmentDiagnosticCandidate(region_samples, i, j, fit);
  const LandmarkSubpathReconstructionResult validation =
      EvaluateLandmarkSubpathCandidate(window, candidate, tolerance);
  diagnostic.reason =
      validation.ok ? "merge_feasible" : "exact_validation_failed";
  return diagnostic;
}

SegmentGapDiagnostic DiagnoseAcceptedSegmentGaps(
    const PropertySamples& region_samples,
    const std::vector<int>& anchors,
    double tolerance,
    int max_gap) {
  SegmentGapDiagnostic diagnostic;
  diagnostic.gap_histogram = FormatGapHistogram(anchors, &diagnostic.max_gap);
  if (anchors.size() < 3) {
    return diagnostic;
  }

  std::vector<std::pair<std::string, int>> reasons;
  std::vector<std::pair<std::string, int>> lower_bound_counts;
  for (std::size_t idx = 0;
       idx + 2 < anchors.size() &&
       diagnostic.rejection_checks < kSegmentMergeDiagnosticMaxChecks;
       ++idx) {
    const SegmentMergeAttemptDiagnostic attempt =
        ClassifySegmentMergeAttempt(region_samples,
                                    anchors[idx],
                                    anchors[idx + 2],
                                    tolerance,
                                    max_gap);
    AddReasonCount(lower_bound_counts, attempt.lower_bound_class);
    AddReasonCount(reasons, attempt.reason);
    ++diagnostic.rejection_checks;
  }
  diagnostic.lower_bound_top =
      FormatReasonCounts(std::move(lower_bound_counts),
                         kSegmentDiagnosticTopReasons);
  diagnostic.rejection_top =
      FormatReasonCounts(std::move(reasons), kSegmentDiagnosticTopReasons);
  return diagnostic;
}

std::string DiagnoseDenseSubpathRuns(const PropertySamples& region_samples,
                                     const std::vector<int>& anchors,
                                     double tolerance,
                                     int max_gap,
                                     int* checks_out) {
  if (checks_out != nullptr) {
    *checks_out = 0;
  }
  if (anchors.size() < static_cast<std::size_t>(kDenseRunMinAnchors)) {
    return "";
  }

  SolverConfig config;
  config.tolerance = std::max(tolerance, 0.0);
  config.allow_hold = false;
  config.allow_linear = true;
  config.allow_bezier = true;
  config.allow_shape_temporal_bezier = true;
  const ShapeMorphProgressBandOptions band_options =
      LandmarkBandOptions(config.tolerance, max_gap);

  std::string note;
  int checks = 0;
  int run_count = 0;
  std::size_t run_start = 0;
  while (run_start < anchors.size()) {
    std::size_t run_end = run_start;
    while (run_end + 1 < anchors.size() &&
           anchors[run_end + 1] == anchors[run_end] + 1) {
      ++run_end;
    }

    const int run_len = static_cast<int>(run_end - run_start + 1);
    if (run_len >= kDenseRunMinAnchors && run_count < kDenseRunMaxRuns) {
      std::vector<std::pair<std::string, int>> reasons;
      int feasible = 0;
      int local_checks = 0;
      for (std::size_t idx = run_start; idx + 2 <= run_end; ++idx) {
        if (checks >= kDenseRunMaxChecks) {
          break;
        }
        const int i = anchors[idx];
        const int j = anchors[idx + 2];
        SegmentFitResult fit =
            FitLandmarkRegionShapeSegment(i, j, region_samples, config,
                                          band_options, true);
        ++checks;
        ++local_checks;
        if (fit.feasible) {
          ++feasible;
          AddReasonCount(reasons, fit.reason.empty() ? "feasible" : fit.reason);
        } else {
          AddReasonCount(reasons, fit.reason);
        }
      }

      const std::string dominant = DominantReason(reasons);
      const std::string inference = DenseRunInference(feasible, dominant);
      if (!note.empty()) {
        note += "|";
      }
      note += std::to_string(anchors[run_start]) + "-" +
              std::to_string(anchors[run_end]) +
              ":checks=" + std::to_string(local_checks) +
              ",feasible=" + std::to_string(feasible) +
              ",dominant=" + dominant +
              ",inference=" + inference;
      ++run_count;
    }

    run_start = run_end + 1;
    if (checks >= kDenseRunMaxChecks) {
      break;
    }
  }

  if (checks_out != nullptr) {
    *checks_out = checks;
  }
  return note;
}

double SlotChordDeviation(const PropertySamples& region_samples,
                          int i,
                          int j,
                          int sample_idx,
                          int vertex) {
  if (i < 0 || j <= i || sample_idx <= i || sample_idx >= j ||
      j >= static_cast<int>(region_samples.samples.size())) {
    return 0.0;
  }
  const Sample& start = region_samples.samples[static_cast<std::size_t>(i)];
  const Sample& end = region_samples.samples[static_cast<std::size_t>(j)];
  const Sample& sample =
      region_samples.samples[static_cast<std::size_t>(sample_idx)];
  const double dt = end.t_sec - start.t_sec;
  if (!(dt > 0.0)) {
    return 0.0;
  }
  const double u = std::clamp((sample.t_sec - start.t_sec) / dt, 0.0, 1.0);
  double score = 0.0;
  for (int component = 0; component < 6; component += 2) {
    const double ax = ShapeComponent(start.v, vertex, component);
    const double ay = ShapeComponent(start.v, vertex, component + 1);
    const double bx = ShapeComponent(end.v, vertex, component);
    const double by = ShapeComponent(end.v, vertex, component + 1);
    const double px = ax + (bx - ax) * u;
    const double py = ay + (by - ay) * u;
    const double sx = ShapeComponent(sample.v, vertex, component);
    const double sy = ShapeComponent(sample.v, vertex, component + 1);
    const double dx = sx - px;
    const double dy = sy - py;
    score += std::sqrt(dx * dx + dy * dy);
  }
  return score;
}

OutlierSlotAnalysis AnalyzeDenseRunOutlierSlots(
    const PropertySamples& region_samples,
    const std::vector<int>& anchors) {
  OutlierSlotAnalysis analysis;
  if (region_samples.samples.empty() ||
      anchors.size() < static_cast<std::size_t>(kDenseRunMinAnchors)) {
    return analysis;
  }
  const int vertex_count =
      ShapeFlatVertexCount(region_samples.samples.front().v);
  if (vertex_count <= 0) {
    return analysis;
  }

  std::vector<double> scores(static_cast<std::size_t>(vertex_count), 0.0);
  int checks = 0;
  int run_count = 0;
  std::size_t run_start = 0;
  while (run_start < anchors.size()) {
    std::size_t run_end = run_start;
    while (run_end + 1 < anchors.size() &&
           anchors[run_end + 1] == anchors[run_end] + 1) {
      ++run_end;
    }

    const int run_len = static_cast<int>(run_end - run_start + 1);
    if (run_len >= kDenseRunMinAnchors && run_count < kDenseRunMaxRuns) {
      for (std::size_t idx = run_start; idx + 2 <= run_end; ++idx) {
        if (checks >= kDenseRunMaxChecks) {
          break;
        }
        const int i = anchors[idx];
        const int j = anchors[idx + 2];
        for (int sample_idx = i + 1; sample_idx < j; ++sample_idx) {
          for (int vertex = 0; vertex < vertex_count; ++vertex) {
            scores[static_cast<std::size_t>(vertex)] +=
                SlotChordDeviation(region_samples, i, j, sample_idx, vertex);
          }
        }
        ++checks;
      }
      ++run_count;
    }

    run_start = run_end + 1;
    if (checks >= kDenseRunMaxChecks) {
      break;
    }
  }

  analysis.checks = checks;
  analysis.slots.reserve(static_cast<std::size_t>(vertex_count));
  for (int vertex = 0; vertex < vertex_count; ++vertex) {
    const double score = scores[static_cast<std::size_t>(vertex)];
    if (score > 1e-9) {
      analysis.slots.push_back({vertex, score});
    }
  }
  std::sort(analysis.slots.begin(),
            analysis.slots.end(),
            [](const OutlierSlotScore& a, const OutlierSlotScore& b) {
              if (std::abs(a.score - b.score) > 1e-9) {
                return a.score > b.score;
              }
              return a.vertex < b.vertex;
            });
  return analysis;
}

}  // namespace path_multimode
}  // namespace bbsolver
