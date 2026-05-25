#pragma once

#include <vector>

#include "bbsolver/path/temporal/path_temporal_validation.hpp"

namespace bbsolver {

struct ShapeTemporalInfluencePair {
  double out_influence = 33.3;
  double in_influence = 33.3;
};

bool ShapeTemporalInfluencesAlmostSame(double a, double b);

std::vector<ShapeTemporalInfluencePair>
BuildInitialShapeTemporalInfluenceCandidates(
    const ShapeMorphProgressBandOptions& options);

}  // namespace bbsolver
