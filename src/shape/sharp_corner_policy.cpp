#include "bbsolver/shape/sharp_corner_policy.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/routing/property_classification.hpp"
#include "bbsolver/shape/shape_flat_topology.hpp"
#include "bbsolver/verify/verifier.hpp"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace bbsolver {

double SharpCornerAngleThresholdDeg(const SolverConfig& config) {
  if (!std::isfinite(config.path_sharp_corner_angle_deg)) {
    return 90.0;
  }
  return std::min(175.0, std::max(5.0, config.path_sharp_corner_angle_deg));
}

double SharpCornerLockTolerance(const SolverConfig& config) {
  const double tolerance_scaled_lock =
      std::isfinite(config.tolerance)
          ? std::max(0.0, config.tolerance) * 1.5
: 0.0;
  if (!std::isfinite(config.path_sharp_corner_tolerance)) {
    return std::max(1.5, tolerance_scaled_lock);
  }
  return std::max(
      1e-6,
      std::max(config.path_sharp_corner_tolerance, tolerance_scaled_lock));
}

double ShapeFlatDeflectionAngleDeg(const std::vector<double>& flat,
                                   int vertex_index) {
  const std::vector<ShapeFlatVertex> vertices = ShapeFlatVertices(flat);
  const int n = static_cast<int>(vertices.size());
  if (n < 3 || vertex_index < 0 || vertex_index >= n) {
    return 0.0;
  }
  const bool closed = ShapeFlatClosed(flat);
  if (!closed && (vertex_index == 0 || vertex_index + 1 == n)) {
    return 180.0;
  }
  const int prev_index = closed ? (vertex_index - 1 + n) % n: vertex_index - 1;
  const int next_index = closed ? (vertex_index + 1) % n: vertex_index + 1;
  if (prev_index < 0 || next_index < 0 ||
      prev_index >= n || next_index >= n) {
    return 0.0;
  }

  const ShapeFlatVertex& prev = vertices[static_cast<std::size_t>(prev_index)];
  const ShapeFlatVertex& curr = vertices[static_cast<std::size_t>(vertex_index)];
  const ShapeFlatVertex& next = vertices[static_cast<std::size_t>(next_index)];
  const double in_x = curr.x - prev.x;
  const double in_y = curr.y - prev.y;
  const double out_x = next.x - curr.x;
  const double out_y = next.y - curr.y;
  const double in_len = std::sqrt(in_x * in_x + in_y * in_y);
  const double out_len = std::sqrt(out_x * out_x + out_y * out_y);
  if (in_len <= 1e-9 || out_len <= 1e-9) {
    return 0.0;
  }
  double dot = (in_x * out_x + in_y * out_y) / (in_len * out_len);
  dot = std::max(-1.0, std::min(1.0, dot));
  constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;
  return std::acos(dot) * kRadToDeg;
}

std::vector<int> ShapeFlatSharpCornerIndices(
    const std::vector<double>& flat,
    const SolverConfig& config) {
  std::vector<int> out;
  const int n = ShapeFlatVertexCount(flat);
  if (n <= 0) {
    return out;
  }
  const double angle_threshold = SharpCornerAngleThresholdDeg(config);
  for (int idx = 0; idx < n; ++idx) {
    if (ShapeFlatDeflectionAngleDeg(flat, idx) >= angle_threshold) {
      out.push_back(idx);
    }
  }
  return out;
}

std::vector<ShapeFlatPoint> ShapeFlatSharpCornerPoints(
    const std::vector<double>& flat,
    const SolverConfig& config) {
  std::vector<ShapeFlatPoint> out;
  const std::vector<ShapeFlatVertex> vertices = ShapeFlatVertices(flat);
  for (int idx: ShapeFlatSharpCornerIndices(flat, config)) {
    if (idx >= 0 && idx < static_cast<int>(vertices.size())) {
      const ShapeFlatVertex& v = vertices[static_cast<std::size_t>(idx)];
      out.push_back({v.x, v.y});
    }
  }
  return out;
}

bool ShapeFlatIndexIsSharpCorner(const std::vector<double>& flat,
                                 int vertex_index,
                                 const SolverConfig& config) {
  return ShapeFlatDeflectionAngleDeg(flat, vertex_index) >=
         SharpCornerAngleThresholdDeg(config);
}

std::vector<std::vector<int>> PersistentShapeFlatSharpCornerIndicesByVertexCount(
    const PropertySamples& original,
    const SolverConfig& config) {
  int max_vertices = 0;
  int total_shape_samples = 0;
  for (const Sample& sample: original.samples) {
    const int n = ShapeFlatVertexCount(sample.v);
    if (n > 0) {
      max_vertices = std::max(max_vertices, n);
      ++total_shape_samples;
    }
  }
  std::vector<std::vector<int>> protected_by_count(
      static_cast<std::size_t>(max_vertices + 1));
  if (max_vertices <= 0 || total_shape_samples <= 0) {
    return protected_by_count;
  }

  std::vector<int> frames_by_count(
      static_cast<std::size_t>(max_vertices + 1), 0);
  std::vector<std::vector<int>> corner_hits_by_count(
      static_cast<std::size_t>(max_vertices + 1));
  for (int n = 0; n <= max_vertices; ++n) {
    corner_hits_by_count[static_cast<std::size_t>(n)].assign(
        static_cast<std::size_t>(n), 0);
  }

  for (const Sample& sample: original.samples) {
    const int n = ShapeFlatVertexCount(sample.v);
    if (n <= 0 || n > max_vertices) {
      continue;
    }
    ++frames_by_count[static_cast<std::size_t>(n)];
    for (int idx: ShapeFlatSharpCornerIndices(sample.v, config)) {
      if (idx >= 0 && idx < n) {
        ++corner_hits_by_count[static_cast<std::size_t>(n)]
                              [static_cast<std::size_t>(idx)];
      }
    }
  }

  const int min_topology_frames =
      std::max(3, static_cast<int>(std::ceil(
                      static_cast<double>(total_shape_samples) * 0.05)));
  for (int n = 0; n <= max_vertices; ++n) {
    const int topology_frames = frames_by_count[static_cast<std::size_t>(n)];
    if (topology_frames < min_topology_frames) {
      continue;
    }
    const int min_corner_hits =
        std::max(3, static_cast<int>(std::ceil(
                        static_cast<double>(topology_frames) * 0.25)));
    const std::vector<int>& hits =
        corner_hits_by_count[static_cast<std::size_t>(n)];
    std::vector<int>& protected_indices =
        protected_by_count[static_cast<std::size_t>(n)];
    for (int idx = 0; idx < static_cast<int>(hits.size()); ++idx) {
      if (hits[static_cast<std::size_t>(idx)] >= min_corner_hits) {
        protected_indices.push_back(idx);
      }
    }
  }

  return protected_by_count;
}

bool ShapeFlatKeyIndexIsProtectedCorner(const PropertyKeys& keys,
                                        int target_vertices,
                                        int vertex_index,
                                        const SolverConfig& config) {
  if (!config.path_preserve_sharp_corners) {
    return false;
  }
  for (const Key& key: keys.keys) {
    if (ShapeFlatVertexCount(key.v) != target_vertices) {
      continue;
    }
    if (ShapeFlatIndexIsSharpCorner(key.v, vertex_index, config)) {
      return true;
    }
  }
  return false;
}

SharpCornerValidationResult ValidateSharpCornerPreservation(
    const PropertySamples& original,
    const PropertyKeys& candidate,
    const SolverConfig& config,
    bool source_vertices_are_semantic_anchors) {
  SharpCornerValidationResult result;
  result.enabled = config.path_preserve_sharp_corners &&
                   source_vertices_are_semantic_anchors &&
                   IsShapeFlatPath(original);
  if (!result.enabled) {
    return result;
  }
  const double lock_tolerance = SharpCornerLockTolerance(config);
  const double angle_threshold = SharpCornerAngleThresholdDeg(config);
  const std::vector<std::vector<int>> protected_by_count =
      PersistentShapeFlatSharpCornerIndicesByVertexCount(original, config);
  int worst_missing = 0;
  double worst_distance = 0.0;
  double worst_t = 0.0;

  for (const Sample& sample: original.samples) {
    const int source_vertex_count = ShapeFlatVertexCount(sample.v);
    if (source_vertex_count <= 0 ||
        source_vertex_count >= static_cast<int>(protected_by_count.size())) {
      continue;
    }
    const std::vector<int>& source_corner_indices =
        protected_by_count[static_cast<std::size_t>(source_vertex_count)];
    if (source_corner_indices.empty()) {
      continue;
    }
    const std::vector<ShapeFlatVertex> source_vertices =
        ShapeFlatVertices(sample.v);
    ++result.samples_checked;
    result.source_corners += static_cast<int>(source_corner_indices.size());

    const std::vector<double> candidate_flat =
        EvalKeysAt(candidate.keys, sample.t_sec);
    const std::vector<ShapeFlatVertex> candidate_vertices =
        ShapeFlatVertices(candidate_flat);
    result.candidate_corners += static_cast<int>(
        ShapeFlatSharpCornerPoints(candidate_flat, config).size());

    int missing = 0;
    double sample_worst_distance = 0.0;
    std::vector<bool> used(candidate_vertices.size(), false);
    for (int source_idx: source_corner_indices) {
      if (source_idx < 0 ||
          source_idx >= static_cast<int>(source_vertices.size())) {
        continue;
      }
      const ShapeFlatVertex& source_vertex =
          source_vertices[static_cast<std::size_t>(source_idx)];
      const ShapeFlatPoint source_corner{source_vertex.x, source_vertex.y};
      double best_distance = std::numeric_limits<double>::infinity();
      int best_index = -1;
      for (std::size_t candidate_idx = 0;
           candidate_idx < candidate_vertices.size();
           ++candidate_idx) {
        if (used[candidate_idx]) {
          continue;
        }
        const ShapeFlatVertex& candidate_vertex =
            candidate_vertices[candidate_idx];
        const ShapeFlatPoint candidate_corner{candidate_vertex.x,
                                              candidate_vertex.y};
        const double distance =
            ShapeFlatDistance(source_corner, candidate_corner);
        if (distance < best_distance) {
          best_distance = distance;
          best_index = static_cast<int>(candidate_idx);
        }
      }
      if (best_index >= 0 && best_distance <= lock_tolerance) {
        used[static_cast<std::size_t>(best_index)] = true;
      }
      sample_worst_distance = std::max(sample_worst_distance, best_distance);
      if (!std::isfinite(best_distance) || best_distance > lock_tolerance) {
        ++missing;
      }
    }
    if (missing > 0) {
      result.ok = false;
      if (missing > worst_missing ||
          (missing == worst_missing && sample_worst_distance > worst_distance)) {
        worst_missing = missing;
        worst_distance = sample_worst_distance;
        worst_t = sample.t_sec;
      }
    }
  }

  result.notes =
      std::string(result.ok
                      ? "sharp_corner_preserve=ok"
: "sharp_corner_preserve=rejected") +
      "; sharp_corner_angle_deg=" + std::to_string(angle_threshold) +
      "; sharp_corner_tolerance=" + std::to_string(lock_tolerance) +
      "; sharp_corner_samples_checked=" +
      std::to_string(result.samples_checked) +
      "; source_sharp_landmarks=" + std::to_string(result.source_corners) +
      "; candidate_sharp_corners=" +
      std::to_string(result.candidate_corners);
  if (!result.ok) {
    result.notes +=
        "; missing_sharp_corners=" + std::to_string(worst_missing) +
        "; worst_sharp_corner_distance=" + std::to_string(worst_distance) +
        "; worst_sharp_corner_t=" + std::to_string(worst_t);
  }
  return result;
}

}  // namespace bbsolver
