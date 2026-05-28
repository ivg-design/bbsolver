#include "bbsolver/solve/static_key_cleanup.hpp"
#include "bbsolver/domain.hpp"

#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "bbsolver/metrics/error_metrics.hpp"
#include "bbsolver/verify/verifier.hpp"
#include "bbsolver/solve/solver_reporting.hpp"

namespace bbsolver {

StaticKeyRunCollapseResult CollapseRedundantStaticKeyRuns(
    const PropertySamples& source_samples,
    const SolverConfig& config,
    const CompInfo& comp,
    PropertyKeys& keys) {
  StaticKeyRunCollapseResult result;
  if (!keys.converged || keys.keys.size() < 2) {
    return result;
  }

  std::vector<Key> collapsed;
  collapsed.reserve(keys.keys.size());
  const std::size_t n = keys.keys.size();
  std::size_t i = 0;
  while (i < n) {
    std::size_t j = i + 1;
    while (j < n && KeyValuesEqualWithin(keys.keys[i].v, keys.keys[j].v)) {
      ++j;
    }

    const std::size_t run_len = j - i;
    if (run_len == 1) {
      collapsed.push_back(keys.keys[i]);
      i = j;
      continue;
    }

    const bool suffix_run = (j == n);
    if (suffix_run) {
      Key first = keys.keys[i];
      first.interp_out = InterpType::Hold;
      collapsed.push_back(std::move(first));
      result.keys_removed += static_cast<int>(run_len - 1);
      ++result.runs_collapsed;
    } else if (run_len > 2) {
      Key first = keys.keys[i];
      Key last = keys.keys[j - 1];
      first.interp_out = InterpType::Hold;
      last.interp_in = InterpType::Hold;
      collapsed.push_back(std::move(first));
      collapsed.push_back(std::move(last));
      result.keys_removed += static_cast<int>(run_len - 2);
      ++result.runs_collapsed;
    } else {
      collapsed.push_back(keys.keys[i]);
      collapsed.push_back(keys.keys[i + 1]);
    }
    i = j;
  }

  if (result.keys_removed <= 0) {
    return result;
  }
  result.attempted = true;

  PropertyKeys candidate = keys;
  candidate.keys = std::move(collapsed);
  const ErrorReport report =
      ValidateKeys(source_samples, candidate.keys, config, comp);
  result.max_err = report.max_err;
  result.max_err_screen_px = report.max_err_screen_px;

  const double err_slop = 1e-6;
  const bool max_err_ok =
      report.max_err <= keys.max_err + err_slop;
  const bool screen_err_ok =
      report.max_err_screen_px <= keys.max_err_screen_px + err_slop;
  if (!max_err_ok || !screen_err_ok) {
    AppendSolverNote(keys,
                     std::string("static_key_run_collapse_rejected")
                         + "; static_key_runs_collapsed=" +
                             std::to_string(result.runs_collapsed)
                         + "; static_keys_would_remove=" +
                             std::to_string(result.keys_removed)
                         + "; static_key_collapse_max_err=" +
                             std::to_string(report.max_err)
                         + "; static_key_collapse_max_err_screen_px=" +
                             std::to_string(report.max_err_screen_px));
    return result;
  }

  candidate.max_err = report.max_err;
  candidate.max_err_screen_px = report.max_err_screen_px;
  keys = std::move(candidate);
  AppendSolverNote(keys,
                   std::string("static_key_run_collapse_accepted")
                       + "; static_key_runs_collapsed=" +
                           std::to_string(result.runs_collapsed)
                       + "; static_keys_removed=" +
                           std::to_string(result.keys_removed)
                       + "; static_key_collapse_max_err=" +
                           std::to_string(report.max_err)
                       + "; static_key_collapse_max_err_screen_px=" +
                           std::to_string(report.max_err_screen_px));
  result.accepted = true;
  return result;
}

int FindFinalStaticSuffixStartSample(const std::vector<Sample>& samples) {
  if (samples.size() < 2) {
    return -1;
  }
  const std::vector<double>& final_value = samples.back().v;
  int start = static_cast<int>(samples.size()) - 1;
  for (int idx = static_cast<int>(samples.size()) - 2; idx >= 0; --idx) {
    if (!KeyValuesEqualWithin(samples[static_cast<std::size_t>(idx)].v,
                              final_value,
                              1e-7)) {
      break;
    }
    start = idx;
  }
  return start < static_cast<int>(samples.size()) - 1 ? start: -1;
}

int TrimSamplesAfterTime(PropertySamples& samples, double end_t_sec) {
  const std::size_t original_count = samples.samples.size();
  if (original_count < 2) {
    return 0;
  }
  const double time_eps = 1e-7;
  while (samples.samples.size() > 1 &&
         samples.samples.back().t_sec > end_t_sec + time_eps) {
    samples.samples.pop_back();
  }
  if (!samples.samples.empty()) {
    samples.t_end_sec = samples.samples.back().t_sec;
  }
  return static_cast<int>(original_count - samples.samples.size());
}

FinalStaticBoundaryAnchorResult AnchorFinalStaticBoundary(
    const PropertySamples& source_samples,
    const SolverConfig& config,
    const CompInfo& comp,
    PropertyKeys& keys) {
  FinalStaticBoundaryAnchorResult result;
  if (!keys.converged || keys.keys.size() < 2 ||
      source_samples.samples.size() < 2) {
    return result;
  }

  const int boundary_sample =
      FindFinalStaticSuffixStartSample(source_samples.samples);
  if (boundary_sample < 0) {
    return result;
  }
  const double boundary_t =
      source_samples.samples[static_cast<std::size_t>(boundary_sample)].t_sec;
  const double key_time_eps = 1e-7;
  if (keys.keys.back().t_sec <= boundary_t + key_time_eps) {
    return result;
  }

  result.attempted = true;
  result.boundary_sample = boundary_sample;
  result.suffix_samples =
      static_cast<int>(source_samples.samples.size()) - boundary_sample;
  result.boundary_t_sec = boundary_t;

  PropertyKeys candidate = keys;
  std::vector<Key> anchored;
  anchored.reserve(keys.keys.size());
  for (const Key& key: keys.keys) {
    if (key.t_sec < boundary_t - key_time_eps) {
      anchored.push_back(key);
    } else {
      break;
    }
  }

  Key anchor = keys.keys.back();
  anchor.t_sec = boundary_t;
  anchor.interp_out = InterpType::Hold;
  anchor.temporal_auto_bezier = false;
  anchor.temporal_continuous = false;
  anchor.spatial_auto_bezier = false;
  anchor.spatial_continuous = false;
  anchor.roving = false;
  anchor.spatial_out.clear();

  std::size_t replaced_at_boundary = 0;
  if (!anchored.empty() &&
      std::abs(anchored.back().t_sec - boundary_t) <= key_time_eps) {
    anchored.back() = anchor;
    replaced_at_boundary = 1;
  } else {
    anchored.push_back(anchor);
  }
  result.tail_keys_removed =
      static_cast<int>(keys.keys.size() - anchored.size() +
                       replaced_at_boundary);

  candidate.keys = std::move(anchored);
  const ErrorReport report =
      ValidateKeys(source_samples, candidate.keys, config, comp);
  result.max_err = report.max_err;
  result.max_err_screen_px = report.max_err_screen_px;

  const double screen_tolerance =
      config.tolerance_screen_px > 0.0 ? config.tolerance_screen_px
: config.tolerance;
  const bool property_ok = report.max_err <= config.tolerance + 1e-9;
  const bool screen_ok =
      (config.tolerance_screen_px <= 0.0 && config.weight_screen <= 0.0) ||
      report.max_err_screen_px <= screen_tolerance + 1e-9;
  if (!property_ok || !screen_ok) {
    AppendSolverNote(keys,
                     std::string("final_static_boundary_anchor_rejected")
                         + "; final_static_boundary_sample=" +
                             std::to_string(result.boundary_sample)
                         + "; final_static_boundary_frame=" +
                             std::to_string(static_cast<int>(
                                 std::llround(boundary_t * comp.fps)))
                         + "; final_static_tail_keys_would_remove=" +
                             std::to_string(result.tail_keys_removed)
                         + "; final_static_anchor_max_err=" +
                             std::to_string(report.max_err)
                         + "; final_static_anchor_max_err_screen_px=" +
                             std::to_string(report.max_err_screen_px));
    return result;
  }

  candidate.max_err = report.max_err;
  candidate.max_err_screen_px = report.max_err_screen_px;
  keys = std::move(candidate);
  AppendSolverNote(keys,
                   std::string("final_static_boundary_anchor_accepted")
                       + "; final_static_boundary_sample=" +
                           std::to_string(result.boundary_sample)
                       + "; final_static_boundary_frame=" +
                           std::to_string(static_cast<int>(
                               std::llround(boundary_t * comp.fps)))
                       + "; final_static_suffix_samples=" +
                           std::to_string(result.suffix_samples)
                       + "; final_static_tail_keys_removed=" +
                           std::to_string(result.tail_keys_removed)
                       + "; final_static_anchor_max_err=" +
                           std::to_string(report.max_err)
                       + "; final_static_anchor_max_err_screen_px=" +
                           std::to_string(report.max_err_screen_px));
  result.accepted = true;
  return result;
}

}  // namespace bbsolver
