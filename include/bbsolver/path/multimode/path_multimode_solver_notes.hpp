#pragma once

#include <string>
#include <vector>

#include "bbsolver/path/multimode/path_multimode_geometry.hpp"

namespace bbsolver::path_multimode {

std::string MultiModeRegionBudgetExceededNote(
    int region_segment_checks,
    int max_region_segment_checks,
    const std::vector<VertexRegion>& regions);

std::string MultiModeValidationBudgetExceededNote(
    int union_key_count,
    int source_sample_count,
    int max_validation_samples,
    int validation_work_units,
    int max_validation_work_units,
    const std::vector<VertexRegion>& regions);

std::string MultiModeCandidateKeyBudgetExceededNote(
    int union_key_count,
    int source_sample_count,
    double max_candidate_key_ratio,
    const std::vector<VertexRegion>& regions,
    const std::string& recombined_note);

std::string MultiModeCandidateNote(
    const std::vector<VertexRegion>& regions,
    int max_gap,
    const std::vector<int>& per_region_key_counts,
    int region_segment_checks,
    int validation_work_units,
    int union_key_count,
    const std::vector<int>& anchors,
    const std::string& validation_notes,
    const std::string& recombined_note);

}  // namespace bbsolver::path_multimode
