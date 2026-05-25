#pragma once

#include "bbsolver/domain.hpp"

#include <vector>

namespace bbsolver {
namespace path_fit_geometry {

constexpr int kPathHeaderScalars = 2;
constexpr int kScalarsPerVertex = 6;
constexpr double kZeroTangentEps = 1e-6;

struct Point {
  double x = 0.0;
  double y = 0.0;
};

struct DecodedShape {
  bool ok = false;
  bool closed = false;
  int vertex_count = 0;
};

bool IsShapeFlatPath(const PropertySamples& ps);

DecodedShape DecodeHeader(const std::vector<double>& flat);

Point VertexAt(const std::vector<double>& flat, int vertex_index);

double Length(Point p);

Point Sub(Point a, Point b);

double Distance(Point a, Point b);

double PointSegmentDistance(Point p, Point a, Point b);

std::vector<Point> FlatToDensePolyline(const std::vector<double>& flat);

double DirectedPolylineDistance(const std::vector<Point>& a_points,
                                const std::vector<Point>& b_points,
                                bool closed);

bool HasZeroTangentFeature(const std::vector<double>& flat, int vertex_index);

}  // namespace path_fit_geometry
}  // namespace bbsolver
