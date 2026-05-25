#include "bbsolver/motion_smooth/motion_smooth_shape_loop_adaptive.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include "bbsolver/motion_smooth/motion_smooth_geometry.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_loop_curve.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_quality.hpp"

namespace bbsolver {

AdaptiveClosedLoopShapeSamples BuildAdaptiveClosedLoopShapeSamples(
    const std::vector<std::vector<double>>& closed_values,
    int dims,
    int vertex_count,
    double strength,
    bool source_pose_constraints) {
  AdaptiveClosedLoopShapeSamples result;
  result.values = closed_values;
  if (closed_values.size() < 4 || dims <= 2 || vertex_count <= 0) {
    result.params.reserve(result.values.size());
    for (std::size_t i = 0; i < result.values.size(); ++i) {
      result.params.push_back(static_cast<double>(i));
    }
    result.quality = ShapeMotionQuality(result.values, vertex_count);
    return result;
  }
  const int unique_count = static_cast<int>(closed_values.size()) - 1;
  const double base_target_turn_deg = 48.0 - strength * 3.0;
  result.target_turn_deg = source_pose_constraints
      ? std::clamp(base_target_turn_deg * 0.65, 18.0, 32.0)
      : std::clamp(base_target_turn_deg, 26.0, 42.0);
  result.chord_error_tolerance = source_pose_constraints
      ? std::max(0.25, strength * 0.35)
      : std::max(0.5, strength * 0.55);
  const int max_per_segment =
      source_pose_constraints
          ? std::clamp(static_cast<int>(std::llround(strength * 4.0)) + 10,
                       16,
                       28)
          : std::clamp(static_cast<int>(std::llround(strength * 3.0)) + 4,
                       8,
                       18);
  result.max_keys = unique_count * max_per_segment + 1;

  std::vector<double> params;
  params.reserve(static_cast<std::size_t>(result.max_keys));
  for (int i = 0; i <= unique_count; ++i) {
    params.push_back(static_cast<double>(i));
  }

  auto build_values = [&]() {
    std::vector<std::vector<double>> values;
    values.reserve(params.size());
    for (double param : params) {
      values.push_back(EvaluateClosedLoopShapeAtParam(
          closed_values, dims, param));
    }
    if (!values.empty()) {
      values.back() = values.front();
    }
    return values;
  };

  for (int pass = 0; pass < 16; ++pass) {
    std::vector<std::vector<double>> values = build_values();
    std::vector<bool> split(params.size() > 1 ? params.size() - 1 : 0, false);

    for (std::size_t i = 0; i + 1 < params.size(); ++i) {
      const double mid_param = (params[i] + params[i + 1]) * 0.5;
      const std::vector<double> curve_mid =
          EvaluateClosedLoopShapeAtParam(closed_values, dims, mid_param);
      std::vector<double> chord_mid = values[i];
      if (static_cast<int>(chord_mid.size()) < dims) {
        chord_mid.resize(static_cast<std::size_t>(dims), 0.0);
      }
      for (int d = 2; d < dims; ++d) {
        chord_mid[static_cast<std::size_t>(d)] =
            (MotionSmoothComponentOrZero(values[i], static_cast<std::size_t>(d)) +
             MotionSmoothComponentOrZero(values[i + 1], static_cast<std::size_t>(d))) * 0.5;
      }
      const double chord_error =
          ShapeFlatControlDistance(curve_mid, chord_mid, vertex_count);
      if (chord_error > result.chord_error_tolerance) {
        split[i] = true;
      }
    }

    const ShapeMotionQualityMetrics quality =
        ShapeMotionQuality(values, vertex_count);
    if (quality.max_turn_deg > result.target_turn_deg) {
      for (std::size_t i = 1; i + 1 < values.size(); ++i) {
        double local_max = 0.0;
        const int effective_vertices = std::max(0, quality.effective_vertex_count);
        for (int vertex = 0; vertex < effective_vertices; ++vertex) {
          local_max = std::max(
              local_max,
              PointTurnDeg(ShapeFlatVertexPoint(values[i - 1], vertex),
                           ShapeFlatVertexPoint(values[i], vertex),
                           ShapeFlatVertexPoint(values[i + 1], vertex)));
        }
        if (local_max > result.target_turn_deg) {
          split[i - 1] = true;
          split[i] = true;
        }
      }
      if (quality.boundary_turn_deg > result.target_turn_deg &&
          split.size() >= 2) {
        split.front() = true;
        split.back() = true;
      }
    }

    int split_count = 0;
    for (bool item : split) {
      if (item) {
        ++split_count;
      }
    }
    if (split_count == 0) {
      result.values = std::move(values);
      result.params = params;
      result.quality = quality;
      return result;
    }
    std::vector<double> refined;
    const int split_budget =
        std::max(0, result.max_keys - static_cast<int>(params.size()));
    if (split_count > split_budget) {
      result.budget_hit = true;
    }
    int splits_added_this_pass = 0;
    refined.reserve(
        params.size() +
        static_cast<std::size_t>(std::min(split_count, split_budget)));
    for (std::size_t i = 0; i + 1 < params.size(); ++i) {
      refined.push_back(params[i]);
      if (split[i] && splits_added_this_pass < split_budget) {
        refined.push_back((params[i] + params[i + 1]) * 0.5);
        ++result.splits;
        ++splits_added_this_pass;
      }
    }
    refined.push_back(params.back());
    params.swap(refined);
    ++result.refinement_passes;
    if (result.budget_hit) {
      result.values = build_values();
      result.params = params;
      result.quality = ShapeMotionQuality(result.values, vertex_count);
      return result;
    }
  }
  result.budget_hit = true;
  result.values = build_values();
  result.params = params;
  result.quality = ShapeMotionQuality(result.values, vertex_count);
  return result;
}

}  // namespace bbsolver
