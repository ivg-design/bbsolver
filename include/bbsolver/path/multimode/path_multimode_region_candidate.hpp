#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/path/multimode/path_multimode_geometry.hpp"
#include "bbsolver/path/multimode/path_multimode_solver.hpp"

#include <vector>

namespace bbsolver {
namespace path_multimode {

struct RegionSolveResult {
  std::vector<int> anchors;
  bool budget_exceeded = false;
};

bool RegionSegmentFeasible(const PropertySamples& reduced,
                           int i,
                           int j,
                           VertexRegion region,
                           double tolerance);

RegionSolveResult SolveRegionAnchors(const PropertySamples& reduced,
                                      VertexRegion region,
                                      const ShapeFlatMultiModeOptions& options,
                                      int max_gap,
                                      int* segment_checks);

Key MakeLinearShapeKey(const PropertySamples& reduced,
                       int sample_idx,
                       bool first,
                       bool last);

Key MakeShapeKeyFromValue(const PropertySamples& reduced,
                          int sample_idx,
                          std::vector<double> value,
                          bool first,
                          bool last);

PropertyKeys BuildCandidate(const PropertySamples& reduced,
                            const std::vector<int>& anchors);

bool CandidateKeyBudgetExceeded(int key_count,
                                int sample_count,
                                double max_candidate_key_ratio);

int ValidationWorkUnits(const PropertySamples& original,
                        int candidate_vertex_count,
                        const PathFrameFitOptions& options);

std::vector<double> LinearInterpolateShapeFlat(const std::vector<double>& a,
                                               const std::vector<double>& b,
                                               double u);

}  // namespace path_multimode
}  // namespace bbsolver
