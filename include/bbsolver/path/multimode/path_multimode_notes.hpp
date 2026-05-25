#pragma once

#include "bbsolver/domain.hpp"

#include <string>
#include <utility>
#include <vector>

#include "bbsolver/path/multimode/path_multimode_geometry.hpp"

namespace bbsolver::path_multimode {

struct OutlierSlotScore {
  int vertex = -1;
  double score = 0.0;
};

struct OutlierSlotAnalysis {
  std::vector<OutlierSlotScore> slots;
  int checks = 0;
};

struct VisibleChannelProbeCandidate {
  int channel_count = 0;
  bool ok = false;
  int key_count = 0;
  double max_outline_error = 0.0;
  int segment_checks = 0;
  int partition_count = 0;
  std::string ranges;
  std::string status;
};

struct VisibleChannelProbeResult {
  bool attempted = false;
  int baseline_keys = 0;
  std::vector<VisibleChannelProbeCandidate> candidates;
  int selected_channel_count = 0;
  int visible_channel_keys = 0;
  double max_outline_error = 0.0;
  int segment_checks = 0;
  std::string selected_ranges;
  std::string status;
  std::string reason;
};

struct MaskChannelSlotPlan {
  bool ok = false;
  int vertex = -1;
  int key_count = 0;
  int segment_checks = 0;
  double score = 0.0;
  std::string signature;
  std::string status;
};

struct MaskChannelGroupPlan {
  std::string signature;
  std::vector<int> vertices;
  int key_count = 0;
  double score = 0.0;
};

std::string JoinAnchorIndices(const std::vector<int>& anchors);

std::string JoinRegionRanges(const std::vector<VertexRegion>& regions);

std::string AddReasonCount(std::vector<std::pair<std::string, int>>& counts,
                           const std::string& reason);

std::string DominantReason(
    const std::vector<std::pair<std::string, int>>& counts);

std::string FormatReasonCounts(std::vector<std::pair<std::string, int>> counts,
                               int max_reasons);

std::string NormalizeSegmentDiagnosticReason(const std::string& reason);

std::string FormatGapHistogram(const std::vector<int>& anchors,
                               int* max_gap_out = nullptr);

std::string DenseRunInference(int feasible_count,
                              const std::string& dominant_reason);

std::vector<int> SampleIndicesFromKeys(const PropertyKeys& keys);

std::string FormatScore(double value);

std::string InterpName(InterpType interp);

std::string FormatInfluence(const std::vector<TemporalEase>& ease);

std::string MaskChannelTemporalSignature(const PropertyKeys& keys);

std::string JoinMaskChannelVertices(const std::vector<int>& vertices);

std::string FormatMaskChannelSlots(const std::vector<MaskChannelSlotPlan>& slots);

std::string FormatMaskChannelGroups(
    const std::vector<MaskChannelGroupPlan>& groups);

std::string FormatOutlierSlotNote(const OutlierSlotAnalysis& analysis);

std::string VisibleChannelProbeNote(const VisibleChannelProbeResult& probe);

std::string VisibleChannelProbeOnlyNote(
    const VisibleChannelProbeResult& probe,
    int source_sample_count,
    int vertex_count,
    int max_gap);

std::string JoinOutlierCandidateSlots(
    const std::vector<OutlierSlotScore>& slots);

}  // namespace bbsolver::path_multimode
