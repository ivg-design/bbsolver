#include "bbsolver/path/replacement/path_replacement_phase2_fit.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

#include "bbsolver/path/replacement/path_replacement_fraction_layout.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/shape/shape_flat_topology.hpp"

namespace bbsolver {

ReplacementPhase2FitResult FitReplacementPhase2Records(
    const PropertySamples& property_samples,
    const PathFrameFitOptions& targeted_options,
    int target_vertices,
    const ReplacementTargetProgressFn& progress_fn) {
  ReplacementPhase2FitResult result;
  result.records.reserve(property_samples.samples.size());

  const int phase2_frame_total =
      static_cast<int>(property_samples.samples.size());
  const int phase2_stride = std::max(1, phase2_frame_total / 24);
  if (progress_fn) {
    progress_fn("path_replacement_target_phase2_start",
                "Fitting replacement target frames",
                0.05,
                0,
                phase2_frame_total);
  }
  for (int sample_idx = 0; sample_idx < phase2_frame_total; ++sample_idx) {
    const Sample& sample =
        property_samples.samples[static_cast<std::size_t>(sample_idx)];
    PathFrameFitResult frame_fit =
        FitShapeFlatFrame(sample.v, targeted_options);
    if (!frame_fit.ok) {
      result.ok = false;
      result.warning = "phase2_target_fit_failed: " + frame_fit.warning +
                       " at t=" + std::to_string(sample.t_sec);
      break;
    }
    const bool unchanged_target_ok =
        !frame_fit.applied &&
        frame_fit.target_met &&
        frame_fit.fitted_vertex_count == target_vertices &&
        ShapeFlatVertexCount(frame_fit.fitted) == target_vertices;
    if ((!frame_fit.applied && !unchanged_target_ok) ||
        !frame_fit.target_met ||
        frame_fit.fitted_vertex_count != target_vertices) {
      result.ok = false;
      result.warning = "phase2_target_not_met; target_vertices=" +
                       std::to_string(target_vertices) +
                       "; t=" + std::to_string(sample.t_sec) +
                       "; fitted_vertices=" +
                       std::to_string(frame_fit.fitted_vertex_count);
      if (!frame_fit.warning.empty()) {
        result.warning += "; " + frame_fit.warning;
      }
      break;
    }
    if (ShapeFlatVertexCount(frame_fit.fitted) != target_vertices) {
      result.ok = false;
      result.warning = "phase2_fitted_malformed; target_vertices=" +
                       std::to_string(target_vertices) +
                       "; t=" + std::to_string(sample.t_sec);
      break;
    }
    ReplacementFrameFitRecord record;
    record.max_outline_error = frame_fit.max_outline_error;
    record.t_sec = sample.t_sec;
    record.fitted = std::move(frame_fit.fitted);
    record.outline_fractions = std::move(frame_fit.outline_fractions);
    result.records.push_back(std::move(record));
    if (progress_fn &&
        (sample_idx == 0 ||
         sample_idx + 1 == phase2_frame_total ||
         ((sample_idx + 1) % phase2_stride) == 0)) {
      progress_fn("path_replacement_target_phase2_progress",
                  "Fitting replacement target frames",
                  0.05 + 0.20 *
                             (static_cast<double>(sample_idx + 1) /
                              static_cast<double>(
                                  std::max(1, phase2_frame_total))),
                  sample_idx + 1,
                  phase2_frame_total);
    }
  }
  if (progress_fn) {
    progress_fn("path_replacement_target_phase2_done",
                result.ok ? "Replacement target frame fit complete"
                          : "Replacement target frame fit rejected",
                0.25,
                static_cast<int>(result.records.size()),
                phase2_frame_total);
  }
  return result;
}

}  // namespace bbsolver
