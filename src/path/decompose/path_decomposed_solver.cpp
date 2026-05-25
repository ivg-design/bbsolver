#include "bbsolver/path/decompose/path_decomposed_solver.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/app/cli_options.hpp"
#include "oneapi/tbb/parallel_for.h"
#include "bbsolver/metrics/error_metrics.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#ifdef BBSOLVER_HAVE_TBB
#include <tbb/parallel_for.h>
#endif

#include "bbsolver/path/decompose/path_decompose.hpp"
#include "bbsolver/path/config/path_solver_config.hpp"
#include "bbsolver/solve/plain_property_solver.hpp"
#include "bbsolver/progress/progress.hpp"
#include "bbsolver/progress/solve_cancellation.hpp"
#include "bbsolver/verify/verifier.hpp"

namespace bbsolver {
namespace {

bool PassesKeyValidation(const ErrorReport& report,
                         const SolverConfig& config) {
  const bool property_ok = report.max_err <= config.tolerance + 1e-9;
  const double screen_tolerance =
      config.tolerance_screen_px > 0.0 ? config.tolerance_screen_px
                                       : config.tolerance;
  const bool screen_ok =
      (config.tolerance_screen_px <= 0.0 && config.weight_screen <= 0.0) ||
      report.max_err_screen_px <= screen_tolerance + 1e-9;
  return property_ok && screen_ok;
}

}  // namespace

PropertyKeys SolvePathDecomposedProperty(const PropertySamples& property_samples,
                                         const SolverConfig& config,
                                         const CompInfo& comp,
                                         const SolveOptions& options,
                                         const ProgressWriter& progress,
                                         std::size_t property_idx,
                                         std::size_t property_count) {
  PropertyKeys flat_keys =
      SolvePlainProperty(property_samples, config, comp, options);
  if (flat_keys.notes == "cancelled" ||
      CancelFileExists(options.cancel_file)) {
    return flat_keys;
  }

  PathDecomposeResult decomposed = DecomposePathBundle(property_samples);
  if (!decomposed.is_shape_flat) {
    return flat_keys;
  }
  if (!decomposed.stable_topology) {
    const std::string warning =
        decomposed.warning.empty() ? "topology unstable" : decomposed.warning;
    flat_keys.notes =
        flat_keys.notes.empty() ? warning : flat_keys.notes + "; " + warning;
    return flat_keys;
  }

  std::vector<PropertyKeys> child_results(decomposed.children.size());
  std::atomic<bool> cancelled{false};
  std::atomic<int> child_done_count{0};
  const SolverConfig child_config = PathChildConfig(config);
  const auto solve_child = [&](std::size_t child_index) {
    if (cancelled.load(std::memory_order_relaxed)) {
      return;
    }
    const PathChildSamples& child = decomposed.children[child_index];
    PropertyKeys child_keys =
        SolvePlainProperty(child.samples,
                           child_config,
                           comp,
                           options,
                           PathChildMaxGap(comp));
    const int child_done =
        child_done_count.fetch_add(1, std::memory_order_relaxed) + 1;
    const double child_stage =
        0.42 + 0.24 * static_cast<double>(child_done) /
                   static_cast<double>(std::max<std::size_t>(
                       std::size_t{1}, decomposed.children.size()));
    progress.Emit({
        {"event", "vert_done"},
        {"phase", "Solved path vertex " +
                      std::to_string(child.vertex_index) + " " +
                      PathChannelName(child.channel) + " for " +
                      ProgressPropertyLabel(property_samples)},
        {"progress", SolveProgressForPropertyStage(
                         property_idx, property_count, child_stage)},
        {"id", property_samples.property.id},
        {"display_name", ProgressPropertyLabel(property_samples)},
        {"i", property_idx},
        {"n", property_count},
        {"vidx", child.vertex_index},
        {"child_i", child_index},
        {"child_done", child_done},
        {"child_n", decomposed.children.size()},
        {"channel", PathChannelName(child.channel)},
        {"K", child_keys.keys.size()},
    });
    if (child_keys.notes == "cancelled" ||
        CancelFileExists(options.cancel_file)) {
      cancelled.store(true, std::memory_order_relaxed);
    }
    child_results[child_index] = std::move(child_keys);
  };

#ifdef BBSOLVER_HAVE_TBB
  tbb::parallel_for(std::size_t{0}, decomposed.children.size(), solve_child);
#else
  for (std::size_t child_index = 0;
       child_index < decomposed.children.size();
       ++child_index) {
    solve_child(child_index);
  }
#endif

  if (cancelled.load(std::memory_order_relaxed)) {
    PropertyKeys cancelled_keys;
    cancelled_keys.property_id = property_samples.property.id;
    cancelled_keys.converged = false;
    cancelled_keys.notes = "cancelled";
    return cancelled_keys;
  }

  PropertyKeys reassembled =
      ReassemblePathKeys(property_samples.property,
                         child_results,
                         flat_keys.keys,
                         decomposed.closed);
  const ErrorReport validation =
      ValidateKeys(property_samples, reassembled.keys, config, comp);
  reassembled.max_err = validation.max_err;
  reassembled.max_err_screen_px = validation.max_err_screen_px;
  reassembled.converged =
      reassembled.converged && PassesKeyValidation(validation, config);

  const std::string child_note =
      "path_decomposed_flat_anchor; children=" +
      std::to_string(child_results.size()) +
      "; flat_keys=" + std::to_string(flat_keys.keys.size()) +
      "; flat_err=" + std::to_string(flat_keys.max_err);
  reassembled.notes =
      reassembled.notes.empty() ? child_note : reassembled.notes + "; " + child_note;

  if (!reassembled.converged ||
      (flat_keys.converged &&
       flat_keys.max_err <= reassembled.max_err + 1e-12)) {
    const std::string fallback_note =
        "path_decomposed_candidate_keys=" +
        std::to_string(reassembled.keys.size()) +
        "; candidate_err=" + std::to_string(reassembled.max_err) +
        "; flat_fallback_keys=" + std::to_string(flat_keys.keys.size());
    flat_keys.notes =
        flat_keys.notes.empty() ? fallback_note : flat_keys.notes + "; " + fallback_note;
    return flat_keys;
  }
  return reassembled;
}

}  // namespace bbsolver
