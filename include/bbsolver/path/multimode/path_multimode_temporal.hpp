#pragma once

#include "bbsolver/domain.hpp"

#include <vector>

#include "bbsolver/path/temporal/path_temporal_validation.hpp"

namespace bbsolver::path_multimode {

struct LandmarkInfluencePair {
  double out_influence = 33.3;
  double in_influence = 33.3;
};

std::vector<TemporalEase> NeutralEase();

std::vector<TemporalEase> ShapeEase(double influence);

double ClampInfluence(double influence,
                      const ShapeMorphProgressBandOptions& options);

double ShapeTemporalBezierProgress(
    double alpha,
    TemporalEase ease_out,
    TemporalEase ease_in,
    const ShapeMorphProgressBandOptions& options);

std::vector<double> SegmentProgressValues(
    const PropertySamples& ps,
    int i,
    int j,
    bool bezier,
    TemporalEase ease_out,
    TemporalEase ease_in,
    const ShapeMorphProgressBandOptions& options);

bool SameInfluencePair(const LandmarkInfluencePair& a,
                       const LandmarkInfluencePair& b);

std::vector<LandmarkInfluencePair> BuildLandmarkInfluencePairs(
    const ShapeMorphProgressBandOptions& options,
    const ShapeMorphProgressBandResult& strict_oracle);

bool CanRunExtendedRelaxedBezierSearch(const PropertySamples& region_samples,
                                       int i,
                                       int j);

ShapeMorphProgressBandOptions LandmarkBandOptions(double tolerance,
                                                  int max_gap);

}  // namespace bbsolver::path_multimode
