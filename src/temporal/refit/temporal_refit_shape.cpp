#include "bbsolver/temporal/refit/temporal_refit_shape.hpp"

#include "bbsolver/domain.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"
#include "bbsolver/temporal/refit/temporal_refit.hpp"
#include "bbsolver/verify/verifier.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace bbsolver {

namespace {

double ShapeStrictPropertyCeiling(const SolverConfig& config) {
  return std::max(0.0, config.tolerance);
}

}  // namespace

bool TemporalRefitIsShapeFlatProperty(const PropertySamples& source) {
  return source.property.kind == ValueKind::Custom &&
         source.property.units_label == "shape_flat";
}

bool IsValidTemporalRefitShapeFlatValue(const std::vector<double>& value) {
  if (value.size() < 2 || !std::isfinite(value[0]) ||
      !std::isfinite(value[1])) {
    return false;
  }
  const int vertex_count = static_cast<int>(std::llround(value[1]));
  if (vertex_count < 1 ||
      static_cast<int>(value.size()) != 2 + 6 * vertex_count) {
    return false;
  }
  for (double channel : value) {
    if (!std::isfinite(channel)) {
      return false;
    }
  }
  return true;
}

bool TemporalRefitShapeFlatTopologyMatches(const std::vector<double>& a,
                                           const std::vector<double>& b) {
  if (!IsValidTemporalRefitShapeFlatValue(a) ||
      !IsValidTemporalRefitShapeFlatValue(b)) {
    return false;
  }
  return a.size() == b.size() &&
         static_cast<int>(std::llround(a[0])) ==
             static_cast<int>(std::llround(b[0])) &&
         static_cast<int>(std::llround(a[1])) ==
             static_cast<int>(std::llround(b[1]));
}

bool AllTemporalRefitSourceSamplesAreValidShapeFlat(
    const PropertySamples& source) {
  if (source.samples.empty()) {
    return false;
  }
  for (const Sample& sample : source.samples) {
    if (!IsValidTemporalRefitShapeFlatValue(sample.v)) {
      return false;
    }
  }
  return true;
}

bool AllTemporalRefitShapeFlatKeysHaveStableTopology(
    const PropertyKeys& keys) {
  if (keys.keys.empty() ||
      !IsValidTemporalRefitShapeFlatValue(keys.keys.front().v)) {
    return false;
  }
  const std::vector<double>& reference = keys.keys.front().v;
  for (const Key& key : keys.keys) {
    if (!TemporalRefitShapeFlatTopologyMatches(reference, key.v)) {
      return false;
    }
  }
  return true;
}

PropertySamples ResampleShapeFlatAcceptedAtSourceTimes(
    const PropertyKeys& accepted_keys,
    const PropertySamples& source_template) {
  PropertySamples out = source_template;
  out.samples.clear();
  if (!AllTemporalRefitShapeFlatKeysHaveStableTopology(accepted_keys)) {
    return out;
  }
  out.property.dimensions =
      static_cast<int>(accepted_keys.keys.front().v.size());
  out.samples.reserve(source_template.samples.size());
  const std::vector<double>& reference = accepted_keys.keys.front().v;

  for (const Sample& source_sample : source_template.samples) {
    Sample sample;
    sample.t_sec = source_sample.t_sec;
    sample.v = EvalKeysAt(accepted_keys.keys, source_sample.t_sec);
    if (!TemporalRefitShapeFlatTopologyMatches(reference, sample.v)) {
      out.samples.clear();
      return out;
    }
    out.samples.push_back(std::move(sample));
  }
  return out;
}

bool ValidateShapeRefitAgainstSource(
    const PropertyKeys& candidate,
    const PropertySamples& source,
    const SolverConfig& config,
    TemporalRefitOptions::BudgetMode budget_mode,
    double budget_relative_ceiling,
    double* max_err_out,
    double* max_err_screen_px_out) {
  double max_err = std::numeric_limits<double>::infinity();
  bool ok = false;

  if (AllTemporalRefitSourceSamplesAreValidShapeFlat(source) &&
      AllTemporalRefitShapeFlatKeysHaveStableTopology(candidate)) {
    PathTemporalValidationOptions path_options;
    path_options.frame_fit_options.outline_tolerance =
        budget_mode == TemporalRefitOptions::BudgetMode::Relative
            ? std::max(0.0, budget_relative_ceiling)
            : ShapeStrictPropertyCeiling(config);
    const PathTemporalValidationResult validation =
        ValidatePathTemporalCandidate(source, candidate, path_options);
    if (validation.samples_checked > 0) {
      max_err = validation.max_outline_error;
    }
    ok = validation.ok;
  }

  if (max_err_out) {
    *max_err_out = max_err;
  }
  if (max_err_screen_px_out) {
    // Path temporal validation currently reports source-outline distance in
    // path units. Keep the legacy screen field populated with the same gate
    // metric so callers do not mistake shape refit for unvalidated output.
    *max_err_screen_px_out = max_err;
  }
  return ok;
}

}  // namespace bbsolver
