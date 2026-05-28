#include "bbsolver/motion_smooth/motion_smooth_reduction_gate.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "bbsolver/motion_smooth/motion_smooth_sample_points.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_quality.hpp"
#include "bbsolver/path/reduction/path_bridge_refit.hpp"
#include "bbsolver/path/config/path_solver_config.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"
#include "bbsolver/path/reduction/path_vertex_reduction.hpp"
#include "bbsolver/routing/property_classification.hpp"
#include "bbsolver/shape/shape_flat_topology.hpp"
#include "bbsolver/routing/solve_mode_policy.hpp"
#include "bbsolver/solve/solver_reporting.hpp"
#include "bbsolver/samples/source_key_preservation.hpp"
#include "bbsolver/solve/static_key_cleanup.hpp"

namespace bbsolver {

namespace {

bool ShapeFlatStableLowVertexTopology(
    const PropertySamples& property_samples,
    int* vertex_count_out) {
  if (!IsShapeFlatPath(property_samples) || property_samples.samples.empty()) {
    return false;
  }
  constexpr int kMaxNearOptimalVertices = 8;
  const int first_vertex_count =
      ShapeFlatVertexCount(property_samples.samples.front().v);
  if (first_vertex_count <= 1 ||
      first_vertex_count > kMaxNearOptimalVertices) {
    return false;
  }
  const bool closed = ShapeFlatClosed(property_samples.samples.front().v);
  const std::size_t value_size = property_samples.samples.front().v.size();
  for (const Sample& sample: property_samples.samples) {
    if (ShapeFlatVertexCount(sample.v) != first_vertex_count ||
        ShapeFlatClosed(sample.v) != closed ||
        sample.v.size() != value_size) {
      return false;
    }
  }
  if (vertex_count_out != nullptr) {
    *vertex_count_out = first_vertex_count;
  }
  return true;
}

std::vector<std::vector<double>> ShapeFlatKeyValues(
    const PropertyKeys& keys) {
  std::vector<std::vector<double>> values;
  values.reserve(keys.keys.size());
  for (const Key& key: keys.keys) {
    values.push_back(key.v);
  }
  return values;
}

bool ShapeFlatKeysAreAnimated(const PropertyKeys& keys,
                              double eps = 1e-7) {
  if (keys.keys.size() < 2) {
    return false;
  }
  const std::vector<double>& first = keys.keys.front().v;
  for (std::size_t i = 1; i < keys.keys.size(); ++i) {
    if (!KeyValuesEqualWithin(first, keys.keys[i].v, eps)) {
      return true;
    }
  }
  return false;
}

bool ShapeFlatSourceKeyScheduleLooksReducible(
    const PropertyKeys& source_key_candidate,
    double tolerance) {
  if (source_key_candidate.keys.size() <= 2) {
    return false;
  }
  const double eps = std::max(tolerance, 1e-6);
  const Key& first = source_key_candidate.keys.front();
  const Key& last = source_key_candidate.keys.back();
  const double full_span = std::max(last.t_sec - first.t_sec, 1e-12);
  bool endpoints_only_possible = true;
  for (std::size_t i = 1; i + 1 < source_key_candidate.keys.size(); ++i) {
    const Key& key = source_key_candidate.keys[i];
    const double u = std::clamp((key.t_sec - first.t_sec) / full_span,
                                0.0,
                                1.0);
    if (ShapeFlatVectorDistanceToLinear(first.v, last.v, key.v, u) > eps) {
      endpoints_only_possible = false;
      break;
    }
  }
  if (endpoints_only_possible) {
    return true;
  }

  for (std::size_t remove_idx = 1;
       remove_idx + 1 < source_key_candidate.keys.size();
       ++remove_idx) {
    const Key& left = source_key_candidate.keys[remove_idx - 1];
    const Key& right = source_key_candidate.keys[remove_idx + 1];
    const Key& removed = source_key_candidate.keys[remove_idx];
    const double span = std::max(right.t_sec - left.t_sec, 1e-12);
    const double u = std::clamp((removed.t_sec - left.t_sec) / span,
                                0.0,
                                1.0);
    if (ShapeFlatVectorDistanceToLinear(left.v, right.v, removed.v, u) <= eps) {
      return true;
    }
  }
  return false;
}

bool ShapeFlatHasCheapVertexReductionSignal(
    const PropertyKeys& source_key_candidate,
    const SolverConfig& config) {
  const int source_vertices = MaxShapeFlatKeyVertexCount(source_key_candidate);
  const int min_target = std::max(config.path_replacement_min_vertices, 4);
  if (source_vertices <= min_target) {
    return false;
  }

  const double duplicate_eps = std::max(config.tolerance, 1e-6);
  for (const Key& key: source_key_candidate.keys) {
    if (ShapeFlatHasDuplicateTerminalClosure(key.v, duplicate_eps)) {
      return true;
    }
  }
  return false;
}

}  // namespace

ShapeMotionReductionGateResult GateShapeMotionQualityRegression(
    const PropertySamples& property_samples,
    const PropertyKeys& candidate_keys,
    const SolverConfig& config) {
  ShapeMotionReductionGateResult result;
  if (!IsShapeFlatPath(property_samples) ||
      !candidate_keys.converged ||
      candidate_keys.keys.size() < 3) {
    return result;
  }
  const std::vector<double> source_key_times =
      MotionSmoothSourceKeyTimes(property_samples);
  if (source_key_times.size() < 8 ||
      candidate_keys.keys.size() >= source_key_times.size()) {
    return result;
  }
  int preserved_timing_count = 0;
  PropertyKeys preserved =
      BuildShapeFlatSourceKeyPreservationKeys(
          property_samples, source_key_times, &preserved_timing_count);
  if (!preserved.converged ||
      preserved.keys.size() != source_key_times.size()) {
    return result;
  }
  const int vertex_count =
      ShapeFlatVertexCountFromValues(preserved.keys.front().v);
  if (vertex_count <= 0 ||
      ShapeFlatVertexCountFromValues(candidate_keys.keys.front().v) !=
          vertex_count) {
    return result;
  }
  result.attempted = true;
  std::vector<double> baseline_times;
  baseline_times.reserve(preserved.keys.size());
  for (const Key& key: preserved.keys) {
    baseline_times.push_back(key.t_sec);
  }
  std::vector<double> candidate_times;
  candidate_times.reserve(candidate_keys.keys.size());
  for (const Key& key: candidate_keys.keys) {
    candidate_times.push_back(key.t_sec);
  }
  const ShapeMotionQualityMetrics baseline_quality =
      ShapeMotionQuality(ShapeFlatKeyValues(preserved),
                         vertex_count,
                         &baseline_times);
  const ShapeMotionQualityMetrics candidate_quality =
      ShapeMotionQuality(ShapeFlatKeyValues(candidate_keys),
                         vertex_count,
                         &candidate_times);
  if (!baseline_quality.valid || !candidate_quality.valid) {
    result.note =
        "motion_smooth_temporal_cleanup_gate_skipped=true"
        "; reason=invalid_motion_quality_metrics";
    return result;
  }

  const double tolerance_hint = std::max(
      0.0,
      config.motion_smooth_tolerance > 0.0
          ? config.motion_smooth_tolerance
: (config.tolerance_screen_px > 0.0 ? config.tolerance_screen_px
: config.tolerance));
  const double turn_slack_deg = std::max(4.0, tolerance_hint * 1.5);
  const double p95_slack_deg = std::max(3.0, tolerance_hint);
  const bool max_turn_worse =
      candidate_quality.max_turn_deg >
          baseline_quality.max_turn_deg + turn_slack_deg &&
      candidate_quality.max_turn_deg >
          baseline_quality.max_turn_deg * 1.08;
  const bool p95_turn_worse =
      candidate_quality.p95_turn_deg >
          baseline_quality.p95_turn_deg + p95_slack_deg &&
      candidate_quality.p95_turn_deg >
          baseline_quality.p95_turn_deg * 1.10;
  const bool boundary_worse =
      candidate_quality.boundary_turn_deg >
          baseline_quality.boundary_turn_deg + turn_slack_deg &&
      candidate_quality.boundary_turn_deg >
          baseline_quality.boundary_turn_deg * 1.08;
  const bool speed_worse =
      baseline_quality.max_speed_ratio > 0.0 &&
      candidate_quality.max_speed_ratio >
          baseline_quality.max_speed_ratio * 1.15 + 0.05;

  result.note =
      "motion_smooth_temporal_cleanup_gate=true"
      "; temporal_smoothness_baseline_keys=" +
      std::to_string(preserved.keys.size()) +
      "; temporal_smoothness_candidate_keys=" +
      std::to_string(candidate_keys.keys.size()) +
      "; " + ShapeMotionQualityNote(baseline_quality,
                                    "temporal_smoothness_baseline") +
      "; " + ShapeMotionQualityNote(candidate_quality,
                                    "temporal_smoothness_candidate");
  if (max_turn_worse || p95_turn_worse || boundary_worse || speed_worse) {
    result.rejected = true;
    result.preserved_keys = std::move(preserved);
    result.note +=
        "; motion_smooth_temporal_cleanup_rejected=true"
        "; reason=motion_quality_regression";
    if (max_turn_worse) {
      result.note += "; max_turn_regressed=true";
    }
    if (p95_turn_worse) {
      result.note += "; p95_turn_regressed=true";
    }
    if (boundary_worse) {
      result.note += "; boundary_turn_regressed=true";
    }
    if (speed_worse) {
      result.note += "; speed_ratio_regressed=true";
    }
  } else {
    result.note +=
        "; motion_smooth_temporal_cleanup_rejected=false";
  }
  return result;
}

ShapeFlatNearOptimalResult TryShapeFlatAlreadyNearOptimalFastPath(
    const PropertySamples& property_samples,
    const SolverConfig& config,
    const CompInfo& comp) {
  ShapeFlatNearOptimalResult result;
  result.source_samples = static_cast<int>(property_samples.samples.size());

  constexpr int kMinNearOptimalSamples = 120;
  constexpr int kMaxNearOptimalSourceKeys = 8;
  if (NormalizeSolveOptimizationMode(config.solve_optimization_mode) != "full" ||
      !IsShapeFlatPath(property_samples) ||
      result.source_samples < kMinNearOptimalSamples ||
      config.allow_path_replacement_fit) {
    return result;
  }

  int source_vertices = 0;
  if (!ShapeFlatStableLowVertexTopology(property_samples, &source_vertices)) {
    return result;
  }
  result.source_vertices = source_vertices;

  const std::vector<double> source_key_times =
      MotionSmoothSourceKeyTimes(property_samples);
  result.source_key_count = static_cast<int>(source_key_times.size());
  if (source_key_times.size() < 2 ||
      source_key_times.size() > kMaxNearOptimalSourceKeys ||
      source_key_times.size() !=
          property_samples.property.source_key_times.size()) {
    return result;
  }

  int preserved_timing_count = 0;
  PropertyKeys source_key_candidate =
      BuildShapeFlatSourceKeyPreservationKeys(
          property_samples, source_key_times, &preserved_timing_count);
  if (!source_key_candidate.converged ||
      static_cast<int>(source_key_candidate.keys.size()) !=
          result.source_key_count ||
      !ShapeFlatKeysAreAnimated(source_key_candidate)) {
    return result;
  }

  (void)comp;
  PathTemporalValidationOptions validation_options;
  validation_options.frame_fit_options.outline_tolerance =
      EffectivePathTolerance(config);
  const PathTemporalValidationResult source_key_validation =
      ValidatePathTemporalCandidate(
          property_samples, source_key_candidate, validation_options);
  if (source_key_validation.samples_checked <= 0 ||
      !source_key_validation.ok) {
    return result;
  }

  if (ShapeFlatSourceKeyScheduleLooksReducible(
          source_key_candidate, EffectivePathTolerance(config)) ||
      ShapeFlatHasCheapVertexReductionSignal(source_key_candidate, config)) {
    return result;
  }

  source_key_candidate.max_err = source_key_validation.max_outline_error;
  source_key_candidate.max_err_screen_px =
      source_key_validation.max_outline_error;
  source_key_candidate.converged = true;
  std::string note =
      "shape_flat_already_near_optimal=true"
      "; source_key_preservation_fast_path=true"
      "; source_key_count=" + std::to_string(result.source_key_count) +
      "; source_vertices=" + std::to_string(result.source_vertices) +
      "; input_samples=" + std::to_string(result.source_samples) +
      "; source_key_schedule=preserved"
      "; key_reduction_upside=negligible"
      "; vertex_reduction_upside=none_obvious"
      "; source_key_validation_error=" +
      std::to_string(source_key_validation.max_outline_error) +
      "; motion_smooth_recommended=true"
      "; operator_note=use Motion Smooth solve mode for trajectory smoothing";
  AppendSampleTimingNote(
      note,
      static_cast<int>(source_key_candidate.keys.size()),
      preserved_timing_count);
  source_key_candidate.notes = note;
  result.keys = std::move(source_key_candidate);
  result.note = note;
  result.applied = true;
  return result;
}

}  // namespace bbsolver
