#pragma once

#include <cstddef>
#include <vector>

namespace bbsolver {

struct ShapeFlatPoint {
  double x = 0.0;
  double y = 0.0;
};

struct ShapeFlatVertex {
  double x = 0.0;
  double y = 0.0;
  double in_x = 0.0;
  double in_y = 0.0;
  double out_x = 0.0;
  double out_y = 0.0;
};

int ShapeFlatVertexCountFromValues(const std::vector<double>& values);

int ShapeFlatVertexCount(const std::vector<double>& flat);

bool ShapeFlatClosed(const std::vector<double>& flat);

double ShapeFlatDistance(ShapeFlatPoint a, ShapeFlatPoint b);

std::size_t ShapeFlatVertexOffset(int vertex_index);

double ShapeFlatComponent(const std::vector<double>& flat,
                          int vertex_index,
                          int component_offset);

std::vector<ShapeFlatVertex> ShapeFlatVertices(
    const std::vector<double>& flat);

std::vector<double> ShapeFlatFromVertices(
    const std::vector<ShapeFlatVertex>& vertices,
    bool closed);

}  // namespace bbsolver
