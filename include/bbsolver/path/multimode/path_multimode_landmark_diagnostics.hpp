#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/path/multimode/path_multimode_notes.hpp"

#include <string>
#include <vector>

namespace bbsolver {
namespace path_multimode {

struct SegmentGapDiagnostic {
  std::string gap_histogram;
  int max_gap = 0;
  std::string lower_bound_top;
  std::string rejection_top;
  int rejection_checks = 0;
};

struct SegmentMergeAttemptDiagnostic {
  std::string lower_bound_class;
  std::string reason;
};

PropertySamples WindowSamples(const PropertySamples& samples,
                              int i,
                              int j);

PropertyKeys BuildSegmentDiagnosticCandidate(
    const PropertySamples& region_samples,
    int i,
    int j,
    const SegmentFitResult& fit);

SegmentMergeAttemptDiagnostic ClassifySegmentMergeAttempt(
    const PropertySamples& region_samples,
    int i,
    int j,
    double tolerance,
    int max_gap);

SegmentGapDiagnostic DiagnoseAcceptedSegmentGaps(
    const PropertySamples& region_samples,
    const std::vector<int>& anchors,
    double tolerance,
    int max_gap);

std::string DiagnoseDenseSubpathRuns(const PropertySamples& region_samples,
                                     const std::vector<int>& anchors,
                                     double tolerance,
                                     int max_gap,
                                     int* checks_out);

double SlotChordDeviation(const PropertySamples& region_samples,
                          int i,
                          int j,
                          int sample_idx,
                          int vertex);

OutlierSlotAnalysis AnalyzeDenseRunOutlierSlots(
    const PropertySamples& region_samples,
    const std::vector<int>& anchors);

}  // namespace path_multimode
}  // namespace bbsolver
