#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/path/multimode/path_multimode_geometry.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"

#include <vector>

namespace bbsolver {
namespace path_multimode {

struct LandmarkSubpathReconstructionResult {
  bool ok = false;
  double max_outline_error = 0.0;
  int worst_sample_idx = -1;
  double worst_t_sec = 0.0;
  int samples_checked = 0;
};

struct LandmarkSubpathRefinementResult {
  bool ok = false;
  std::vector<int> anchors;
  LandmarkSubpathReconstructionResult reconstruction;
  int inserted_samples = 0;
};

LandmarkSubpathReconstructionResult EvaluateLandmarkSubpathReconstruction(
    const PropertySamples& reduced,
    VertexRegion region,
    const std::vector<int>& anchors,
    double tolerance);

LandmarkSubpathReconstructionResult EvaluateLandmarkSubpathCandidate(
    const PropertySamples& region_samples,
    const PropertyKeys& candidate,
    double tolerance);

LandmarkSubpathRefinementResult RefineLandmarkSubpathAnchors(
    const PropertySamples& reduced,
    VertexRegion region,
    const std::vector<int>& initial_anchors,
    double tolerance,
    const CancelFn& cancel_fn);

std::vector<double> EvaluateTemporalShapeAtSample(
    const PropertySamples& samples,
    const PropertyKeys& keys,
    int sample_idx,
    const ShapeMorphProgressBandOptions& band_options);

}  // namespace path_multimode
}  // namespace bbsolver
