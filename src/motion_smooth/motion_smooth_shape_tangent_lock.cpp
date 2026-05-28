#include "bbsolver/motion_smooth/motion_smooth_shape_tangent_lock.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "bbsolver/shape/shape_flat_topology.hpp"

namespace bbsolver {

ShapeTangentLockStats LockShapeFlatRotationalTangents(
    std::vector<double>* value) {
  ShapeTangentLockStats stats;
  if (value == nullptr) {
    return stats;
  }
  const int vertex_count = ShapeFlatVertexCountFromValues(*value);
  if (vertex_count <= 0) {
    return stats;
  }
  for (int vertex = 0; vertex < vertex_count; ++vertex) {
    const int base = 2 + vertex * 6;
    double& in_x = (*value)[static_cast<std::size_t>(base + 2)];
    double& in_y = (*value)[static_cast<std::size_t>(base + 3)];
    double& out_x = (*value)[static_cast<std::size_t>(base + 4)];
    double& out_y = (*value)[static_cast<std::size_t>(base + 5)];
    const double in_len = std::sqrt(in_x * in_x + in_y * in_y);
    const double out_len = std::sqrt(out_x * out_x + out_y * out_y);
    if (in_len <= 1e-9 || out_len <= 1e-9) {
      continue;
    }
    ++stats.pairs_seen;
    const double cos_angle =
        std::clamp((in_x * out_x + in_y * out_y) / (in_len * out_len),
                   -1.0,
                   1.0);
    const double angle =
        std::acos(cos_angle) * 180.0 / 3.14159265358979323846;
    const double deviation = std::abs(180.0 - angle);
    stats.max_deviation_before_deg =
        std::max(stats.max_deviation_before_deg, deviation);

    double dir_x = out_x / out_len - in_x / in_len;
    double dir_y = out_y / out_len - in_y / in_len;
    double dir_len = std::sqrt(dir_x * dir_x + dir_y * dir_y);
    if (dir_len <= 1e-9) {
      dir_x = out_x / out_len;
      dir_y = out_y / out_len;
      dir_len = 1.0;
    }
    dir_x /= dir_len;
    dir_y /= dir_len;
    in_x = -dir_x * in_len;
    in_y = -dir_y * in_len;
    out_x = dir_x * out_len;
    out_y = dir_y * out_len;
    ++stats.pairs_locked;
  }
  return stats;
}

ShapeTangentLockStats LockShapeFlatRotationalTangents(
    std::vector<std::vector<double>>* values) {
  ShapeTangentLockStats total;
  if (values == nullptr) {
    return total;
  }
  for (std::vector<double>& value: *values) {
    ShapeTangentLockStats stats = LockShapeFlatRotationalTangents(&value);
    total.pairs_seen += stats.pairs_seen;
    total.pairs_locked += stats.pairs_locked;
    total.max_deviation_before_deg =
        std::max(total.max_deviation_before_deg,
                 stats.max_deviation_before_deg);
  }
  return total;
}

ShapeTangentLockStats LockShapeFlatRotationalTangentsExcept(
    std::vector<std::vector<double>>* values,
    const std::vector<bool>& skip_indices) {
  ShapeTangentLockStats total;
  if (values == nullptr) {
    return total;
  }
  for (std::size_t i = 0; i < values->size(); ++i) {
    if (i < skip_indices.size() && skip_indices[i]) {
      continue;
    }
    ShapeTangentLockStats stats =
        LockShapeFlatRotationalTangents(&(*values)[i]);
    total.pairs_seen += stats.pairs_seen;
    total.pairs_locked += stats.pairs_locked;
    total.max_deviation_before_deg =
        std::max(total.max_deviation_before_deg,
                 stats.max_deviation_before_deg);
  }
  return total;
}

}  // namespace bbsolver
