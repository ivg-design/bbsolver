#include "bbsolver/path/multimode/path_multimode_notes.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/path/multimode/path_multimode_geometry.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    std::abort();
  }
}

bool Contains(const std::string& text, const std::string& needle) {
  return text.find(needle) != std::string::npos;
}

bbsolver::PropertyKeys SampleKeys() {
  bbsolver::PropertyKeys keys;
  keys.keys.resize(3);
  keys.keys[0].interp_out = bbsolver::InterpType::Bezier;
  keys.keys[0].temporal_ease_out = {{0.0, 80.0}};
  keys.keys[1].interp_in = bbsolver::InterpType::Bezier;
  keys.keys[1].interp_out = bbsolver::InterpType::Linear;
  keys.keys[1].temporal_ease_in = {{0.0, 20.0}};
  keys.keys[2].interp_in = bbsolver::InterpType::Linear;

  bbsolver::SegmentReport first;
  first.start_idx = 0;
  first.end_idx = 2;
  bbsolver::SegmentReport second;
  second.start_idx = 2;
  second.end_idx = 5;
  keys.segments.push_back(first);
  keys.segments.push_back(second);
  return keys;
}

void TestJoinHelpersAndGapHistogram() {
  const std::vector<bbsolver::path_multimode::VertexRegion> regions = {
      {0, 2},
      {2, 5},
  };
  Require(bbsolver::path_multimode::JoinAnchorIndices({0, 2, 5}) == "0,2,5",
          "anchor join must use comma-separated integers");
  Require(bbsolver::path_multimode::JoinRegionRanges(regions) == "0-2,2-5",
          "region join must use first-end ranges");

  int max_gap = -1;
  Require(bbsolver::path_multimode::FormatGapHistogram(
              {0, 2, 5, 7}, &max_gap) == "2:2,3:1",
          "gap histogram must count sorted positive gaps");
  Require(max_gap == 3, "gap histogram must report max gap");
}

void TestReasonCountsAndNormalization() {
  std::vector<std::pair<std::string, int>> counts;
  Require(bbsolver::path_multimode::AddReasonCount(counts, "") == "unknown",
          "empty reasons must normalize to unknown");
  bbsolver::path_multimode::AddReasonCount(counts, "budget_limit");
  bbsolver::path_multimode::AddReasonCount(counts, "budget_limit");
  bbsolver::path_multimode::AddReasonCount(counts, "alpha");
  Require(bbsolver::path_multimode::DominantReason(counts) == "budget_limit",
          "dominant reason must pick highest count");
  Require(bbsolver::path_multimode::FormatReasonCounts(counts, 2) ==
              "budget_limit:2,alpha:1",
          "reason count formatter must sort count desc then reason asc");

  Require(bbsolver::path_multimode::NormalizeSegmentDiagnosticReason(
              "landmark_subpath_temporal_infeasible_chord") ==
              "chord_infeasible",
          "chord failure must normalize to chord_infeasible");
  Require(bbsolver::path_multimode::NormalizeSegmentDiagnosticReason(
              "shape_morph_chord_window_too_large") ==
              "gap_cap_exceeded",
          "window cap failure must normalize to gap_cap_exceeded");
  Require(bbsolver::path_multimode::DenseRunInference(
              0, "landmark_subpath_temporal_infeasible_timing") ==
              "one_shared_progress_timing_infeasible",
          "dense-run inference must preserve timing-infeasible reason");
}

void TestTemporalSignatureAndMaskFormatting() {
  const bbsolver::PropertyKeys keys = SampleKeys();
  Require(bbsolver::path_multimode::SampleIndicesFromKeys(keys) ==
              std::vector<int>({0, 2, 5}),
          "sample indices must read segment boundaries");
  Require(bbsolver::path_multimode::MaskChannelTemporalSignature(keys) ==
              "K3:0,2,5|0-2:bezier/bezier:80.000/20.000|"
              "2-5:linear/linear:33.300/33.300",
          "mask-channel signature must include key count, anchors, interp and influence");

  const std::vector<bbsolver::path_multimode::MaskChannelSlotPlan> slots = {
      {true, 0, 2, 0, 0.0, "sig", ""},
      {false, 1, 0, 0, 0.0, "", "blocked"},
  };
  Require(bbsolver::path_multimode::FormatMaskChannelSlots(slots) ==
              "0:k2,1:failed_blocked",
          "slot formatter must spell ok and failed slots");
  const std::vector<bbsolver::path_multimode::MaskChannelGroupPlan> groups = {
      {"a", {0, 2}, 2, 0.0},
      {"b", {1, 3}, 3, 0.0},
  };
  Require(bbsolver::path_multimode::FormatMaskChannelGroups(groups) ==
              "0,2:k=2|1,3:k=3",
          "group formatter must join grouped vertices with pipe separators");
}

void TestOutlierAndVisibleProbeNotes() {
  bbsolver::path_multimode::OutlierSlotAnalysis analysis;
  analysis.slots = {
      {2, 1.23456},
      {1, 9.0},
      {3, 0.5},
      {4, 0.25},
      {5, 0.125},
  };
  Require(bbsolver::path_multimode::FormatOutlierSlotNote(analysis) ==
              "2:1.235,1:9.000,3:0.500,4:0.250",
          "outlier slot note must format the first four supplied scores");
  Require(bbsolver::path_multimode::JoinOutlierCandidateSlots(
              analysis.slots) == "2:1.235,1:9.000",
          "outlier candidate slot join must include only the top two slots");

  bbsolver::path_multimode::VisibleChannelProbeResult probe;
  probe.attempted = true;
  probe.baseline_keys = 12;
  probe.selected_channel_count = 2;
  probe.visible_channel_keys = 8;
  probe.max_outline_error = 0.25;
  probe.segment_checks = 33;
  probe.selected_ranges = "0-2,2-4";
  probe.status = "accepted_key_reduction";
  probe.reason = "best_contiguous_lower_than_baseline";
  probe.candidates.push_back(
      {2, true, 8, 0.25, 33, 2, "0-2,2-4", "ok"});
  const std::string note =
      bbsolver::path_multimode::VisibleChannelProbeNote(probe);
  Require(Contains(note, "visible_channel_probe=done"),
          "visible probe note must include done marker");
  Require(Contains(note, "best_contiguous_2_channel_keys=8"),
          "visible probe note must include candidate key count");
  Require(Contains(note, "selected_visible_representation=contiguous_2"),
          "visible probe note must include selected representation");

  const std::string probe_only =
      bbsolver::path_multimode::VisibleChannelProbeOnlyNote(probe, 5, 4, 3);
  Require(Contains(probe_only, "subpath_partition=visible_channel_probe_only"),
          "probe-only note must include diagnostic partition marker");
  Require(Contains(probe_only, "source_samples=5"),
          "probe-only note must include source sample count");
}

}  // namespace

int main() {
  TestJoinHelpersAndGapHistogram();
  TestReasonCountsAndNormalization();
  TestTemporalSignatureAndMaskFormatting();
  TestOutlierAndVisibleProbeNotes();
  std::cout << "[PASS] test_path_multimode_notes\n";
  return 0;
}
