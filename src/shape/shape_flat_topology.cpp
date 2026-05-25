#include "bbsolver/shape/shape_flat_topology.hpp"

#include <cstddef>
#include <cmath>
#include <vector>

namespace bbsolver {

int ShapeFlatVertexCountFromValues(const std::vector<double>& values) {
  if (values.size() < 2) {
    return -1;
  }
  const int n = static_cast<int>(std::llround(values[1]));
  const int expected_size = 2 + n * 6;
  if (n <= 0 || static_cast<int>(values.size()) != expected_size) {
    return -1;
  }
  return n;
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

bool ShapeFlatClosed(const std::vector<double>& flat) {
  return !flat.empty() && static_cast<int>(std::llround(flat[0])) != 0;
}

double ShapeFlatDistance(ShapeFlatPoint a, ShapeFlatPoint b) {
  const double dx = a.x - b.x;
  const double dy = a.y - b.y;
  return std::sqrt(dx * dx + dy * dy);
}

std::size_t ShapeFlatVertexOffset(int vertex_index) {
  return static_cast<std::size_t>(2 + vertex_index * 6);
}

double ShapeFlatComponent(const std::vector<double>& flat,
                          int vertex_index,
                          int component_offset) {
  const std::size_t idx =
      ShapeFlatVertexOffset(vertex_index) +
      static_cast<std::size_t>(component_offset);
  return idx < flat.size() ? flat[idx] : 0.0;
}

std::vector<ShapeFlatVertex> ShapeFlatVertices(
    const std::vector<double>& flat) {
  const int n = ShapeFlatVertexCount(flat);
  std::vector<ShapeFlatVertex> vertices;
  if (n <= 0) {
    return vertices;
  }
  vertices.reserve(static_cast<std::size_t>(n));
  for (int idx = 0; idx < n; ++idx) {
    const std::size_t offset = ShapeFlatVertexOffset(idx);
    vertices.push_back({
        flat[offset],
        flat[offset + 1],
        flat[offset + 2],
        flat[offset + 3],
        flat[offset + 4],
        flat[offset + 5],
    });
  }
  return vertices;
}

std::vector<double> ShapeFlatFromVertices(
    const std::vector<ShapeFlatVertex>& vertices,
    bool closed) {
  std::vector<double> out;
  out.reserve(2 + vertices.size() * 6);
  out.push_back(closed ? 1.0 : 0.0);
  out.push_back(static_cast<double>(vertices.size()));
  for (const ShapeFlatVertex& vertex : vertices) {
    out.push_back(vertex.x);
    out.push_back(vertex.y);
    out.push_back(vertex.in_x);
    out.push_back(vertex.in_y);
    out.push_back(vertex.out_x);
    out.push_back(vertex.out_y);
  }
  return out;
}

}  // namespace bbsolver
