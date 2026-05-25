#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/path/multimode/path_multimode_reconstruction.hpp"

#include <string>

namespace bbsolver {
namespace path_multimode {

struct LandmarkSubpathTemporalResult {
  bool ok = false;
  PropertyKeys keys;
  LandmarkSubpathReconstructionResult reconstruction;
  int segment_checks = 0;
  bool budget_exceeded = false;
  std::string notes;
};

LandmarkSubpathTemporalResult SolveLandmarkRegionTemporal(
    const PropertySamples& region_samples,
    double tolerance,
    int max_gap,
    int max_segment_checks,
    const CancelFn& cancel_fn);

}  // namespace path_multimode
}  // namespace bbsolver
