#include "bbsolver/path/multimode/path_multimode_solver_notes.hpp"

#include <string>
#include <vector>

#include "bbsolver/path/multimode/path_multimode_geometry.hpp"
#include "bbsolver/path/multimode/path_multimode_notes.hpp"

namespace bbsolver::path_multimode {

namespace {

std::string JoinCounts(const std::vector<int>& counts) {
  std::string out;
  for (int count : counts) {
    if (!out.empty()) {
      out += ",";
    }
    out += std::to_string(count);
  }
  return out;
}

}  // namespace

std::string MultiModeRegionBudgetExceededNote(
    int region_segment_checks,
    int max_region_segment_checks,
    const std::vector<VertexRegion>& regions) {
  return "shape_multimode_region_budget_exceeded; region_segment_checks=" +
         std::to_string(region_segment_checks) +
         "; max_region_segment_checks=" +
         std::to_string(max_region_segment_checks) +
         "; regions=" + std::to_string(static_cast<int>(regions.size())) +
         "; region_ranges=" + JoinRegionRanges(regions);
}

std::string MultiModeValidationBudgetExceededNote(
    int union_key_count,
    int source_sample_count,
    int max_validation_samples,
    int validation_work_units,
    int max_validation_work_units,
    const std::vector<VertexRegion>& regions) {
  return "shape_multimode_validation_budget_exceeded; union_keys=" +
         std::to_string(union_key_count) +
         "; source_samples=" + std::to_string(source_sample_count) +
         "; max_validation_samples=" + std::to_string(max_validation_samples) +
         "; validation_work_units=" +
         std::to_string(validation_work_units) +
         "; max_validation_work_units=" +
         std::to_string(max_validation_work_units) +
         "; regions=" + std::to_string(static_cast<int>(regions.size())) +
         "; region_ranges=" + JoinRegionRanges(regions);
}

std::string MultiModeCandidateKeyBudgetExceededNote(
    int union_key_count,
    int source_sample_count,
    double max_candidate_key_ratio,
    const std::vector<VertexRegion>& regions,
    const std::string& recombined_note) {
  std::string note =
      "shape_multimode_candidate_key_budget_exceeded; union_keys=" +
      std::to_string(union_key_count) +
      "; source_samples=" + std::to_string(source_sample_count) +
      "; max_candidate_key_ratio=" +
      std::to_string(max_candidate_key_ratio) +
      "; regions=" + std::to_string(static_cast<int>(regions.size())) +
      "; region_ranges=" + JoinRegionRanges(regions);
  if (!recombined_note.empty()) {
    note += "; " + recombined_note;
  }
  return note;
}

std::string MultiModeCandidateNote(
    const std::vector<VertexRegion>& regions,
    int max_gap,
    const std::vector<int>& per_region_key_counts,
    int region_segment_checks,
    int validation_work_units,
    int union_key_count,
    const std::vector<int>& anchors,
    const std::string& validation_notes,
    const std::string& recombined_note) {
  std::string note =
      "shape_multimode_candidate; regions=" +
      std::to_string(static_cast<int>(regions.size())) +
      "; region_ranges=" + JoinRegionRanges(regions) +
      "; max_gap_samples=" + std::to_string(max_gap) +
      "; region_key_counts=" + JoinCounts(per_region_key_counts) +
      "; region_segment_checks=" + std::to_string(region_segment_checks) +
      "; validation_work_units=" + std::to_string(validation_work_units) +
      "; union_keys=" + std::to_string(union_key_count) +
      "; union_anchors=" + JoinAnchorIndices(anchors) +
      "; source_outline_validation: " + validation_notes;
  if (!recombined_note.empty()) {
    note += "; " + recombined_note;
  }
  return note;
}

}  // namespace bbsolver::path_multimode
