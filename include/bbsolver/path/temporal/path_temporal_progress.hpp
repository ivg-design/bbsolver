#pragma once

#include <vector>

namespace bbsolver {

bool PathTemporalShapeFlatIsValid(const std::vector<double>& values);

std::vector<double> LerpShapeFlatChord(const std::vector<double>& a,
                                       const std::vector<double>& b,
                                       double u);

double ClampShapeTemporalInfluencePercent(double influence,
                                          double min_influence,
                                          double max_influence);

double ShapeTemporalBezierProgress(double alpha,
                                   double out_influence_percent,
                                   double in_influence_percent,
                                   double min_influence_percent = 0.1,
                                   double max_influence_percent = 100.0);

double DefaultShapeTemporalBezierProgress(double alpha);

int ProgressStepForLinear(double alpha, int progress_steps);

int ProgressStepForDefaultBezier(double alpha, int progress_steps);

}  // namespace bbsolver
