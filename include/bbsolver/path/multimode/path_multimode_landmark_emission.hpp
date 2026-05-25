#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/path/multimode/path_multimode_geometry.hpp"
#include "bbsolver/path/multimode/path_multimode_notes.hpp"
#include "bbsolver/path/multimode/path_multimode_reconstruction.hpp"
#include "bbsolver/path/multimode/path_multimode_solver.hpp"

#include <string>
#include <vector>

namespace bbsolver {
namespace path_multimode {

struct LandmarkRegionEmissionResult {
  bool ok = false;
  VertexRegion region;
  PropertyKeys keys;
  std::vector<int> anchors;
  LandmarkSubpathReconstructionResult reconstruction;
  int inserted_samples = 0;
  int temporal_segment_checks = 0;
  int region_segment_checks = 0;
  int dense_run_checks = 0;
  int segment_gap_max = 0;
  int segment_rejection_checks = 0;
  int outlier_slot_checks = 0;
  std::string temporal_status;
  std::string dense_run_note;
  std::string segment_gap_note;
  std::string segment_lower_bound_note;
  std::string segment_rejection_note;
  std::string outlier_slot_note;
  std::vector<OutlierSlotScore> outlier_slots;
};

std::string LandmarkSubpathNote(
    int subpath_index,
    int subpath_count,
    VertexRegion region,
    const std::vector<int>& anchors,
    int source_sample_count,
    int max_gap,
    int region_segment_checks,
    int inserted_samples,
    int temporal_segment_checks,
    const std::string& temporal_status,
    const LandmarkSubpathReconstructionResult& reconstruction,
    int source_vertex_count,
    bool visible_shape_channel);

LandmarkRegionEmissionResult EvaluateLandmarkRegionEmission(
    const PropertySamples& reduced,
    VertexRegion region,
    const ShapeFlatLandmarkSubpathOptions& options,
    int max_gap);

}  // namespace path_multimode
}  // namespace bbsolver
