#include "bbsolver/motion_smooth/motion_smooth_shape_trajectory_smooth.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include "bbsolver/domain.hpp"
#include "bbsolver/motion_smooth/motion_smooth_geometry.hpp"
#include "bbsolver/motion_smooth/motion_smooth_sample_points.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_quality.hpp"

namespace bbsolver {

ShapeMotionTrajectorySmoothResult BuildShapeMotionTrajectorySmoothValues(
    const bbsolver::PropertySamples& property_samples,
    const std::vector<double>& source_key_times,
    const std::vector<std::vector<double>>& raw,
    int vertex_count,
    int dims,
    double strength,
    const std::vector<double>* fidelity_times,
    const std::vector<std::vector<double>>* fidelity_values) {
  ShapeMotionTrajectorySmoothResult result;
  result.smoothing_passes = source_key_times.size() > 2 ? 1 : 0;
  result.smoothing_blend = std::clamp(
      strength / (strength + 2.0), 0.0, 0.90);
  result.original_values.reserve(source_key_times.size());
  for (std::size_t i = 0; i < source_key_times.size(); ++i) {
    std::vector<double> value = MotionSmoothInterpolatedVector(
        property_samples, raw, source_key_times[i], dims);
    if (static_cast<int>(value.size()) >= 2) {
      value[0] = property_samples.samples.front().v[0];
      value[1] = property_samples.samples.front().v[1];
    }
    result.original_values.push_back(std::move(value));
  }
  result.smoothed_values = result.original_values;
  result.max_turn_before_deg =
      ShapeFlatSequenceMaxTurnDeg(result.original_values, dims);
  result.max_turn_after_deg = result.max_turn_before_deg;

  if (result.original_values.size() <= 2 || dims <= 2) {
    return result;
  }

  const double extent =
      ShapeFlatSequenceExtent(result.original_values, vertex_count);
  result.displacement_limit = std::max(
      6.0,
      std::min(extent * 0.35,
               std::max(strength * 24.0, extent * 0.04 * strength)));

  std::vector<std::vector<double>> cubic = result.original_values;
  const double start_t = source_key_times.front();
  const double duration =
      std::max(source_key_times.back() - start_t, 1e-9);
  const bool use_source_fidelity =
      fidelity_times && fidelity_values &&
      fidelity_times->size() == fidelity_values->size() &&
      fidelity_times->size() > source_key_times.size();
  result.source_fidelity_enabled = use_source_fidelity;
  result.source_fidelity_samples = use_source_fidelity
      ? static_cast<int>(fidelity_times->size())
      : 0;
  std::vector<double> normalized_times(source_key_times.size(), 0.0);
  for (std::size_t i = 0; i < source_key_times.size(); ++i) {
    normalized_times[i] =
        std::clamp((source_key_times[i] - start_t) / duration, 0.0, 1.0);
  }

  for (int d = 2; d < dims; ++d) {
    const std::size_t sd = static_cast<std::size_t>(d);
    const double p0 = MotionSmoothComponentOrZero(result.original_values.front(), sd);
    const double p3 = MotionSmoothComponentOrZero(result.original_values.back(), sd);
    double s11 = 0.0;
    double s12 = 0.0;
    double s22 = 0.0;
    double r1 = 0.0;
    double r2 = 0.0;
    const std::size_t observation_count = use_source_fidelity
        ? fidelity_times->size()
        : result.original_values.size();
    for (std::size_t i = 0; i < observation_count; ++i) {
      const double obs_t = use_source_fidelity
          ? (*fidelity_times)[i]
          : source_key_times[i];
      const double u = std::clamp((obs_t - start_t) / duration, 0.0, 1.0);
      if (u <= 1e-9 || u >= 1.0 - 1e-9) {
        continue;
      }
      const std::vector<double>& obs_value = use_source_fidelity
          ? (*fidelity_values)[i]
          : result.original_values[i];
      const double omt = 1.0 - u;
      const double a1 = 3.0 * omt * omt * u;
      const double a2 = 3.0 * omt * u * u;
      const double endpoint =
          omt * omt * omt * p0 + u * u * u * p3;
      const double y =
          MotionSmoothComponentOrZero(obs_value, sd) - endpoint;
      s11 += a1 * a1;
      s12 += a1 * a2;
      s22 += a2 * a2;
      r1 += a1 * y;
      r2 += a2 * y;
    }

    const double det = s11 * s22 - s12 * s12;
    double c1 = 0.0;
    double c2 = 0.0;
    const bool solved = std::abs(det) > 1e-12;
    if (solved) {
      c1 = (r1 * s22 - r2 * s12) / det;
      c2 = (s11 * r2 - s12 * r1) / det;
    }
    for (std::size_t i = 1; i + 1 < cubic.size(); ++i) {
      const double u = normalized_times[i];
      if (solved) {
        const double omt = 1.0 - u;
        cubic[i][sd] =
            omt * omt * omt * p0 +
            3.0 * omt * omt * u * c1 +
            3.0 * omt * u * u * c2 +
            u * u * u * p3;
      } else {
        cubic[i][sd] = p0 * (1.0 - u) + p3 * u;
      }
    }
  }

  for (std::size_t i = 1; i + 1 < result.smoothed_values.size(); ++i) {
    for (int control = 0; control < vertex_count * 3; ++control) {
      const int x_idx = 2 + control * 2;
      const int y_idx = x_idx + 1;
      if (y_idx >= dims) {
        continue;
      }
      const double ox = MotionSmoothComponentOrZero(
          result.original_values[i], static_cast<std::size_t>(x_idx));
      const double oy = MotionSmoothComponentOrZero(
          result.original_values[i], static_cast<std::size_t>(y_idx));
      const double tx =
          ox * (1.0 - result.smoothing_blend) +
          MotionSmoothComponentOrZero(cubic[i], static_cast<std::size_t>(x_idx)) *
              result.smoothing_blend;
      const double ty =
          oy * (1.0 - result.smoothing_blend) +
          MotionSmoothComponentOrZero(cubic[i], static_cast<std::size_t>(y_idx)) *
              result.smoothing_blend;
      double dx = tx - ox;
      double dy = ty - oy;
      const double control_displacement = std::sqrt(dx * dx + dy * dy);
      if (control_displacement > result.displacement_limit &&
          control_displacement > 1e-9) {
        const double scale = result.displacement_limit / control_displacement;
        dx *= scale;
        dy *= scale;
      }
      result.smoothed_values[i][static_cast<std::size_t>(x_idx)] = ox + dx;
      result.smoothed_values[i][static_cast<std::size_t>(y_idx)] = oy + dy;
    }
  }

  for (std::size_t i = 0; i < result.original_values.size(); ++i) {
    double max_control = 0.0;
    const double shape_distance = ShapeFlatControlDistance(
        result.original_values[i],
        result.smoothed_values[i],
        vertex_count,
        &max_control);
    result.max_shape_displacement =
        std::max(result.max_shape_displacement, shape_distance);
    result.max_control_displacement =
        std::max(result.max_control_displacement, max_control);
  }
  result.max_turn_after_deg =
      ShapeFlatSequenceMaxTurnDeg(result.smoothed_values, dims);
  return result;
}

}  // namespace bbsolver
