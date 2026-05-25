#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/path/multimode/path_multimode_geometry.hpp"
#include "bbsolver/path/multimode/path_multimode_solver.hpp"

#include <string>
#include <vector>

namespace bbsolver {
namespace path_multimode {

struct RecombinedRegionTemporalResult {
  bool attempted = false;
  bool accepted = false;
  PropertyKeys keys;
  std::string note;
  std::vector<int> anchors;
  int temporal_segment_checks = 0;
  std::string region_key_counts;
};

RecombinedRegionTemporalResult TryRecombinedRegionTemporalCandidate(
    const PropertySamples& original,
    const PropertySamples& reduced,
    const std::vector<VertexRegion>& regions,
    const ShapeFlatMultiModeOptions& options,
    int max_gap,
    int union_key_count);

}  // namespace path_multimode
}  // namespace bbsolver
