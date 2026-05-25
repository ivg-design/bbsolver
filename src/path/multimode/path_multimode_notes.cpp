#include "bbsolver/path/multimode/path_multimode_notes.hpp"
#include "bbsolver/path/multimode/path_multimode_geometry.hpp"

#include "bbsolver/domain.hpp"

#include <ios>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <cstddef>

namespace bbsolver::path_multimode {

namespace {

constexpr int kOutlierDiagnosticTopSlots = 4;
constexpr int kOutlierPartitionCandidateSlots = 2;

}  // namespace

std::string JoinAnchorIndices(const std::vector<int>& anchors) {
  std::string out;
  for (int anchor : anchors) {
    if (!out.empty()) {
      out += ",";
    }
    out += std::to_string(anchor);
  }
  return out;
}

std::string JoinRegionRanges(const std::vector<VertexRegion>& regions) {
  std::string out;
  for (VertexRegion region : regions) {
    if (!out.empty()) {
      out += ",";
    }
    out += std::to_string(region.first_vertex) + "-" +
           std::to_string(region.end_vertex);
  }
  return out;
}

std::string AddReasonCount(std::vector<std::pair<std::string, int>>& counts,
                           const std::string& reason) {
  const std::string normalized =
      reason.empty() ? "unknown" : reason;
  for (auto& entry : counts) {
    if (entry.first == normalized) {
      ++entry.second;
      return normalized;
    }
  }
  counts.push_back({normalized, 1});
  return normalized;
}

std::string DominantReason(
    const std::vector<std::pair<std::string, int>>& counts) {
  if (counts.empty()) {
    return "unknown";
  }
  const auto best =
      std::max_element(counts.begin(), counts.end(),
                       [](const auto& a, const auto& b) {
                         if (a.second != b.second) {
                           return a.second < b.second;
                         }
                         return a.first > b.first;
                       });
  return best->first;
}

std::string FormatReasonCounts(std::vector<std::pair<std::string, int>> counts,
                               int max_reasons) {
  if (counts.empty()) {
    return "";
  }
  std::sort(counts.begin(),
            counts.end(),
            [](const auto& a, const auto& b) {
              if (a.second != b.second) {
                return a.second > b.second;
              }
              return a.first < b.first;
            });
  std::string out;
  const int count = std::min(std::max(0, max_reasons),
                             static_cast<int>(counts.size()));
  for (int idx = 0; idx < count; ++idx) {
    if (!out.empty()) {
      out += ",";
    }
    out += counts[static_cast<std::size_t>(idx)].first + ":" +
           std::to_string(counts[static_cast<std::size_t>(idx)].second);
  }
  return out;
}

std::string NormalizeSegmentDiagnosticReason(const std::string& reason) {
  if (reason.empty()) {
    return "unknown";
  }
  if (reason.find("window_too_large") != std::string::npos) {
    return "gap_cap_exceeded";
  }
  if (reason.find("budget") != std::string::npos) {
    return "budget_exceeded";
  }
  if (reason.find("validation_failed") != std::string::npos ||
      reason.find("exceeds_tolerance") != std::string::npos) {
    return "exact_validation_failed";
  }
  if (reason.find("infeasible_shape_morph_chord") != std::string::npos ||
      reason.find("landmark_subpath_temporal_infeasible_chord") !=
          std::string::npos) {
    return "chord_infeasible";
  }
  if (reason.find("infeasible_shape_morph_band") != std::string::npos) {
    return "band_infeasible";
  }
  if (reason.find("infeasible_shape_morph_timing") != std::string::npos ||
      reason.find("landmark_subpath_temporal_infeasible_timing") !=
          std::string::npos) {
    return "ae_ease_infeasible";
  }
  return reason;
}

std::string FormatGapHistogram(const std::vector<int>& anchors,
                               int* max_gap_out) {
  if (max_gap_out != nullptr) {
    *max_gap_out = 0;
  }
  if (anchors.size() < 2) {
    return "";
  }

  std::vector<std::pair<int, int>> counts;
  for (std::size_t idx = 0; idx + 1 < anchors.size(); ++idx) {
    const int gap = anchors[idx + 1] - anchors[idx];
    if (gap <= 0) {
      continue;
    }
    if (max_gap_out != nullptr) {
      *max_gap_out = std::max(*max_gap_out, gap);
    }
    auto existing =
        std::find_if(counts.begin(),
                     counts.end(),
                     [gap](const auto& entry) {
                       return entry.first == gap;
                     });
    if (existing == counts.end()) {
      counts.push_back({gap, 1});
    } else {
      ++existing->second;
    }
  }
  std::sort(counts.begin(),
            counts.end(),
            [](const auto& a, const auto& b) {
              return a.first < b.first;
            });

  std::string out;
  for (const auto& entry : counts) {
    if (!out.empty()) {
      out += ",";
    }
    out += std::to_string(entry.first) + ":" + std::to_string(entry.second);
  }
  return out;
}

std::string DenseRunInference(int feasible_count,
                              const std::string& dominant_reason) {
  if (feasible_count > 0) {
    return "shared_endpoint_state_or_dp_blocked";
  }
  if (dominant_reason == "infeasible_shape_morph_chord") {
    return "one_shared_progress_chord_infeasible";
  }
  if (dominant_reason == "landmark_subpath_temporal_infeasible_chord") {
    return "one_shared_progress_chord_infeasible";
  }
  if (dominant_reason == "infeasible_shape_morph_timing" ||
      dominant_reason == "landmark_subpath_temporal_infeasible_timing") {
    return "one_shared_progress_timing_infeasible";
  }
  if (dominant_reason == "shape_morph_chord_window_too_large") {
    return "window_budget_limited";
  }
  return "one_shared_progress_infeasible";
}

std::vector<int> SampleIndicesFromKeys(const PropertyKeys& keys) {
  std::vector<int> indices;
  if (keys.keys.empty()) {
    return indices;
  }
  if (keys.segments.empty()) {
    indices.push_back(0);
    return indices;
  }
  indices.reserve(keys.segments.size() + 1);
  indices.push_back(keys.segments.front().start_idx);
  for (const SegmentReport& segment : keys.segments) {
    indices.push_back(segment.end_idx);
  }
  indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
  return indices;
}

std::string FormatScore(double value) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(3) << value;
  return out.str();
}

std::string InterpName(InterpType interp) {
  switch (interp) {
    case InterpType::Hold:
      return "hold";
    case InterpType::Linear:
      return "linear";
    case InterpType::Bezier:
      return "bezier";
  }
  return "unknown";
}

std::string FormatInfluence(const std::vector<TemporalEase>& ease) {
  if (ease.empty()) {
    return "33.300";
  }
  return FormatScore(ease.front().influence);
}

std::string MaskChannelTemporalSignature(const PropertyKeys& keys) {
  if (keys.keys.empty()) {
    return "";
  }
  std::string signature = "K" + std::to_string(keys.keys.size()) + ":";
  signature += JoinAnchorIndices(SampleIndicesFromKeys(keys));
  for (std::size_t segment_idx = 0; segment_idx < keys.segments.size();
       ++segment_idx) {
    const SegmentReport& segment = keys.segments[segment_idx];
    const Key& start_key = keys.keys[segment_idx];
    const Key& end_key = keys.keys[segment_idx + 1];
    signature += "|" + std::to_string(segment.start_idx) + "-" +
                 std::to_string(segment.end_idx) + ":" +
                 InterpName(start_key.interp_out) + "/" +
                 InterpName(end_key.interp_in) + ":" +
                 FormatInfluence(start_key.temporal_ease_out) + "/" +
                 FormatInfluence(end_key.temporal_ease_in);
  }
  return signature;
}

std::string JoinMaskChannelVertices(const std::vector<int>& vertices) {
  std::string out;
  for (int vertex : vertices) {
    if (!out.empty()) {
      out += ",";
    }
    out += std::to_string(vertex);
  }
  return out;
}

std::string FormatMaskChannelSlots(
    const std::vector<MaskChannelSlotPlan>& slots) {
  std::string note;
  for (const MaskChannelSlotPlan& slot : slots) {
    if (!note.empty()) {
      note += ",";
    }
    note += std::to_string(slot.vertex) + ":" +
            (slot.ok ? ("k" + std::to_string(slot.key_count))
                     : ("failed_" + slot.status));
  }
  return note;
}

std::string FormatMaskChannelGroups(
    const std::vector<MaskChannelGroupPlan>& groups) {
  std::string note;
  for (const MaskChannelGroupPlan& group : groups) {
    if (!note.empty()) {
      note += "|";
    }
    note += JoinMaskChannelVertices(group.vertices) +
            ":k=" + std::to_string(group.key_count);
  }
  return note;
}

std::string FormatOutlierSlotNote(const OutlierSlotAnalysis& analysis) {
  if (analysis.slots.empty()) {
    return "";
  }
  std::string note;
  const int count = std::min(kOutlierDiagnosticTopSlots,
                             static_cast<int>(analysis.slots.size()));
  for (int idx = 0; idx < count; ++idx) {
    if (!note.empty()) {
      note += ",";
    }
    const OutlierSlotScore& slot = analysis.slots[static_cast<std::size_t>(idx)];
    note += std::to_string(slot.vertex) + ":" + FormatScore(slot.score);
  }
  return note;
}

std::string VisibleChannelProbeNote(const VisibleChannelProbeResult& probe) {
  if (!probe.attempted) {
    return "";
  }
  std::string note =
      "visible_channel_probe=done"
      "; source_visible_key_baseline=" +
      std::to_string(probe.baseline_keys);
  for (const VisibleChannelProbeCandidate& candidate : probe.candidates) {
    const std::string prefix = "best_contiguous_" +
                               std::to_string(candidate.channel_count) +
                               "_channel_";
    note += "; " + prefix + "status=" + candidate.status +
            "; " + prefix + "candidate_count=" +
            std::to_string(candidate.partition_count) +
            "; " + prefix + "segment_checks=" +
            std::to_string(candidate.segment_checks);
    if (candidate.ok) {
      note += "; " + prefix + "keys=" + std::to_string(candidate.key_count) +
              "; " + prefix + "ranges=" + candidate.ranges +
              "; " + prefix + "max_outline_error=" +
              std::to_string(candidate.max_outline_error) +
              "; " + prefix + "composite_max_outline_error=" +
              std::to_string(candidate.max_outline_error);
    } else {
      note += "; " + prefix + "keys=none";
    }
  }

  note += "; selected_visible_representation=";
  if (probe.selected_channel_count > 0) {
    note += "contiguous_" + std::to_string(probe.selected_channel_count);
  } else {
    note += "none";
  }
  note += "; visible_channel_keys=" + std::to_string(probe.visible_channel_keys) +
          "; visible_improvement_status=" + probe.status +
          "; visible_channel_probe_reason=" + probe.reason +
          "; visible_channel_segment_checks=" +
          std::to_string(probe.segment_checks) +
          "; visible_channel_ranges=" + probe.selected_ranges +
          "; visible_channel_subpath_max_outline_error=" +
          std::to_string(probe.max_outline_error) +
          "; visible_channel_composite_max_outline_error=" +
          std::to_string(probe.max_outline_error);
  return note;
}

std::string VisibleChannelProbeOnlyNote(
    const VisibleChannelProbeResult& probe,
    int source_sample_count,
    int vertex_count,
    int max_gap) {
  std::string note =
      "landmark_subpath; subpath_index=0; subpath_count=1; vertex_range=0-" +
      std::to_string(vertex_count) +
      "; key_count=0"
      "; source_samples=" +
      std::to_string(source_sample_count) +
      "; max_gap_samples=" + std::to_string(max_gap) +
      "; subpath_reconstruction_ok=false"
      "; subpath_reconstruction_max_outline_error=0.000000"
      "; subpath_partition=visible_channel_probe_only"
      "; subpath_diagnostics=fast"
      "; subpath_fast_summary=true";
  const std::string probe_note = VisibleChannelProbeNote(probe);
  if (!probe_note.empty()) {
    note += "; " + probe_note;
  }
  return note;
}

std::string JoinOutlierCandidateSlots(
    const std::vector<OutlierSlotScore>& slots) {
  std::string out;
  const int count = std::min(kOutlierPartitionCandidateSlots,
                             static_cast<int>(slots.size()));
  for (int idx = 0; idx < count; ++idx) {
    if (!out.empty()) {
      out += ",";
    }
    const OutlierSlotScore& slot = slots[static_cast<std::size_t>(idx)];
    out += std::to_string(slot.vertex) + ":" + FormatScore(slot.score);
  }
  return out;
}

}  // namespace bbsolver::path_multimode
