#pragma once

#include <string>
#include <vector>

namespace bbsolver {

double ShapeFlatControlDistance(const std::vector<double>& a,
                                const std::vector<double>& b,
                                int vertex_count,
                                double* max_control_distance_out = nullptr);

double ShapeFlatSequenceMaxTurnDeg(
    const std::vector<std::vector<double>>& values,
    int dims);

double ShapeFlatClosedDuplicateMaxTurnDeg(
    const std::vector<std::vector<double>>& values,
    int dims);

double ShapeFlatSequenceExtent(const std::vector<std::vector<double>>& values,
                               int vertex_count);

double ShapeFlatVectorDistanceToLinear(const std::vector<double>& left,
                                       const std::vector<double>& right,
                                       const std::vector<double>& value,
                                       double u);

struct ShapeMotionQualityMetrics {
  bool valid = false;
  int vertex_count = 0;
  int effective_vertex_count = 0;
  int turn_count = 0;
  double max_turn_deg = 0.0;
  double p95_turn_deg = 0.0;
  double avg_turn_deg = 0.0;
  double boundary_turn_deg = 0.0;
  double max_speed_ratio = 0.0;
};

ShapeMotionQualityMetrics ShapeMotionQuality(
    const std::vector<std::vector<double>>& values,
    int vertex_count,
    const std::vector<double>* times = nullptr);

std::string ShapeMotionQualityNote(
    const ShapeMotionQualityMetrics& metrics,
    const std::string& prefix);

}  // namespace bbsolver
