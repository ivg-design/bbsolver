#include "bbsolver/path/multimode/path_multimode_geometry.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <cstddef>
#include <utility>

namespace bbsolver::path_multimode {

namespace {

constexpr int kDefaultMaxRegions = 4;

}  // namespace

bool IsShapeFlatPath(const PropertySamples& ps) {
  return ps.property.kind == ValueKind::Custom &&
         ps.property.units_label == "shape_flat";
}

int ShapeFlatVertexCount(const std::vector<double>& flat) {
  if (flat.size() < 2) {
    return 0;
  }
  const int vertex_count = static_cast<int>(std::llround(flat[1]));
  const int expected = 2 + vertex_count * 6;
  if (vertex_count <= 0 || static_cast<int>(flat.size()) != expected) {
    return 0;
  }
  return vertex_count;
}

int MaxShapeFlatVertexCount(const PropertySamples& ps) {
  int max_count = 0;
  for (const Sample& sample: ps.samples) {
    max_count = std::max(max_count, ShapeFlatVertexCount(sample.v));
  }
  return max_count;
}

bool SameShapeFlatTopology(const std::vector<double>& a,
                           const std::vector<double>& b) {
  const int count_a = ShapeFlatVertexCount(a);
  const int count_b = ShapeFlatVertexCount(b);
  return count_a > 0 &&
         count_a == count_b &&
         std::llround(a[0]) == std::llround(b[0]) &&
         a.size() == b.size();
}

bool SameSampleTimes(const PropertySamples& a, const PropertySamples& b) {
  if (a.samples.size() != b.samples.size()) {
    return false;
  }
  for (std::size_t idx = 0; idx < a.samples.size(); ++idx) {
    if (std::abs(a.samples[idx].t_sec - b.samples[idx].t_sec) > 1e-9) {
      return false;
    }
  }
  return true;
}

double ShapeComponent(const std::vector<double>& flat,
                      int vertex,
                      int component) {
  const std::size_t idx =
      static_cast<std::size_t>(2 + vertex * 6 + component);
  return idx < flat.size() ? flat[idx]: 0.0;
}

double VertexMotionBoundaryScore(const PropertySamples& reduced,
                                 int left_vertex,
                                 int right_vertex) {
  if (reduced.samples.size() < 2) {
    return 0.0;
  }
  const std::vector<double>& first = reduced.samples.front().v;
  if (left_vertex < 0 || right_vertex < 0) {
    return 0.0;
  }

  double score = 0.0;
  int count = 0;
  const double left_x0 = ShapeComponent(first, left_vertex, 0);
  const double left_y0 = ShapeComponent(first, left_vertex, 1);
  const double right_x0 = ShapeComponent(first, right_vertex, 0);
  const double right_y0 = ShapeComponent(first, right_vertex, 1);
  for (const Sample& sample: reduced.samples) {
    const double left_dx = ShapeComponent(sample.v, left_vertex, 0) - left_x0;
    const double left_dy = ShapeComponent(sample.v, left_vertex, 1) - left_y0;
    const double right_dx = ShapeComponent(sample.v, right_vertex, 0) - right_x0;
    const double right_dy = ShapeComponent(sample.v, right_vertex, 1) - right_y0;
    const double diff_x = left_dx - right_dx;
    const double diff_y = left_dy - right_dy;
    score += std::sqrt(diff_x * diff_x + diff_y * diff_y);
    ++count;
  }
  return count > 0 ? score / static_cast<double>(count): 0.0;
}

std::vector<VertexRegion> BuildVertexRegions(int vertex_count,
                                             int requested_regions) {
  const int region_count =
      std::max(1, std::min(vertex_count,
                           requested_regions > 0 ? requested_regions
: kDefaultMaxRegions));
  std::vector<VertexRegion> regions;
  regions.reserve(static_cast<std::size_t>(region_count));
  for (int region = 0; region < region_count; ++region) {
    const int first = (vertex_count * region) / region_count;
    const int end = (vertex_count * (region + 1)) / region_count;
    regions.push_back({first, std::max(first + 1, end)});
  }
  return regions;
}

std::vector<VertexRegion> BuildMotionAwareVertexRegions(
    const PropertySamples& reduced,
    int vertex_count,
    int requested_regions) {
  const int region_count =
      std::max(1, std::min(vertex_count,
                           requested_regions > 0 ? requested_regions
: kDefaultMaxRegions));
  if (region_count <= 1 || vertex_count <= 2) {
    return BuildVertexRegions(vertex_count, region_count);
  }

  std::vector<std::pair<double, int>> boundaries;
  boundaries.reserve(static_cast<std::size_t>(std::max(0, vertex_count - 1)));
  double max_score = 0.0;
  for (int boundary = 1; boundary < vertex_count; ++boundary) {
    const double score =
        VertexMotionBoundaryScore(reduced, boundary - 1, boundary);
    boundaries.push_back({score, boundary});
    max_score = std::max(max_score, score);
  }
  if (!(max_score > 1e-9)) {
    return BuildVertexRegions(vertex_count, region_count);
  }

  std::sort(boundaries.begin(), boundaries.end(),
            [](const auto& a, const auto& b) {
              if (std::abs(a.first - b.first) > 1e-12) {
                return a.first > b.first;
              }
              return a.second < b.second;
            });

  std::vector<int> splits;
  splits.reserve(static_cast<std::size_t>(region_count - 1));
  for (const auto& scored_boundary: boundaries) {
    if (static_cast<int>(splits.size()) >= region_count - 1) {
      break;
    }
    splits.push_back(scored_boundary.second);
  }
  std::sort(splits.begin(), splits.end());

  std::vector<VertexRegion> regions;
  regions.reserve(static_cast<std::size_t>(region_count));
  int first = 0;
  for (int split: splits) {
    if (split > first && split < vertex_count) {
      regions.push_back({first, split});
      first = split;
    }
  }
  if (first < vertex_count) {
    regions.push_back({first, vertex_count});
  }
  if (regions.empty()) {
    return BuildVertexRegions(vertex_count, region_count);
  }
  return regions;
}

std::vector<double> ShapeFlatRegion(const std::vector<double>& flat,
                                    VertexRegion region) {
  const int source_count = ShapeFlatVertexCount(flat);
  if (source_count <= 0 ||
      region.first_vertex < 0 ||
      region.end_vertex <= region.first_vertex ||
      region.end_vertex > source_count) {
    return {};
  }
  const int region_count = region.end_vertex - region.first_vertex;
  std::vector<double> out;
  out.reserve(static_cast<std::size_t>(2 + region_count * 6));
  const bool preserves_full_path =
      region.first_vertex == 0 && region.end_vertex == source_count;
  out.push_back(preserves_full_path ? flat[0]: 0.0);
  out.push_back(static_cast<double>(region_count));
  for (int vertex = region.first_vertex; vertex < region.end_vertex; ++vertex) {
    const std::size_t base = static_cast<std::size_t>(2 + vertex * 6);
    for (int component = 0; component < 6; ++component) {
      double value = flat[base + static_cast<std::size_t>(component)];
      if (!preserves_full_path) {
        const int local_vertex = vertex - region.first_vertex;
        if (local_vertex == 0 && (component == 2 || component == 3)) {
          value = 0.0;
        }
        if (local_vertex == region_count - 1 &&
            (component == 4 || component == 5)) {
          value = 0.0;
        }
      }
      out.push_back(value);
    }
  }
  return out;
}

bool InsertShapeFlatRegion(std::vector<double>* full,
                           VertexRegion region,
                           const std::vector<double>& region_shape) {
  if (full == nullptr) {
    return false;
  }
  const int full_count = ShapeFlatVertexCount(*full);
  const int region_count = ShapeFlatVertexCount(region_shape);
  if (full_count <= 0 ||
      region_count != region.end_vertex - region.first_vertex ||
      region.first_vertex < 0 ||
      region.end_vertex > full_count) {
    return false;
  }
  for (int local_vertex = 0; local_vertex < region_count; ++local_vertex) {
    const int full_vertex = region.first_vertex + local_vertex;
    const std::size_t full_base =
        static_cast<std::size_t>(2 + full_vertex * 6);
    const std::size_t region_base =
        static_cast<std::size_t>(2 + local_vertex * 6);
    for (int component = 0; component < 6; ++component) {
      (*full)[full_base + static_cast<std::size_t>(component)] =
          region_shape[region_base + static_cast<std::size_t>(component)];
    }
  }
  return true;
}

PropertySamples BuildLandmarkRegionSamples(const PropertySamples& reduced,
                                           VertexRegion region) {
  PropertySamples out = reduced;
  const int region_count = region.end_vertex - region.first_vertex;
  out.property.dimensions = 2 + region_count * 6;
  out.samples.clear();
  out.samples.reserve(reduced.samples.size());
  for (const Sample& sample: reduced.samples) {
    Sample region_sample;
    region_sample.t_sec = sample.t_sec;
    region_sample.v = ShapeFlatRegion(sample.v, region);
    if (region_sample.v.empty()) {
      out.samples.clear();
      return out;
    }
    out.samples.push_back(std::move(region_sample));
  }
  return out;
}

}  // namespace bbsolver::path_multimode
