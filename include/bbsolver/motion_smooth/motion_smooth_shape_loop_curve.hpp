#pragma once

#include <utility>
#include <vector>

namespace bbsolver {

// Catmull-Rom interpolant for a scalar component. Public surface so the
// adaptive sampler in motion_smooth_shape_loop_adaptive.cpp can drive
// evaluation without re-defining the kernel.
double MotionSmoothCatmullRomValue(double p0,
                                   double p1,
                                   double p2,
                                   double p3,
                                   double u);

// Shape-flat vertex point accessor (x, y at the position offset for a
// given vertex index in a single-key value vector).
std::pair<double, double> ShapeFlatVertexPoint(
    const std::vector<double>& value,
    int vertex_index);

// Discrete turn angle in degrees at `cur` given the polyline triplet
// (prev, cur, next). Returns 0 when either leg is degenerate.
double PointTurnDeg(std::pair<double, double> prev,
                    std::pair<double, double> cur,
                    std::pair<double, double> next);

// Evaluate the closed-loop shape value at a continuous parameter `param`
// using Catmull-Rom interpolation across `closed_values` (which must
// include the duplicate wrap entry at the end). Returns the wrapped
// front entry at the boundary param `unique_count - eps`.
std::vector<double> EvaluateClosedLoopShapeAtParam(
    const std::vector<std::vector<double>>& closed_values,
    int dims,
    double param);

}  // namespace bbsolver
