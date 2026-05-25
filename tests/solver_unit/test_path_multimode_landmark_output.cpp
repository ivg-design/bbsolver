#include "bbsolver/path/multimode/path_multimode_landmark_output.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/path/multimode/path_multimode_notes.hpp"
#include "bbsolver/path/multimode/path_multimode_landmark_partition.hpp"
#include "bbsolver/path/multimode/path_multimode_solver.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
  if (condition) {
    return;
  }
  std::cerr << "test_path_multimode_landmark_output: " << message << "\n";
  std::exit(1);
}

std::vector<double> ShapeFlatTwoVertex(double x0, double y0,
                                       double x1, double y1) {
  return {
      0.0, 2.0,
      x0, y0, 0.0, 0.0, 0.0, 0.0,
      x1, y1, 0.0, 0.0, 0.0, 0.0,
  };
}

bbsolver::PropertySamples MakeLinearSamples() {
  bbsolver::PropertySamples samples;
  samples.property.id = "unit/path_multimode/output";
  samples.property.kind = bbsolver::ValueKind::Custom;
  samples.property.units_label = "shape_flat";
  samples.property.dimensions = 14;
  samples.t_start_sec = 0.0;
  samples.t_end_sec = 2.0 / 24.0;
  samples.samples_per_frame = 1;
  samples.samples.push_back({0.0, ShapeFlatTwoVertex(0.0, 0.0, 10.0, 0.0)});
  samples.samples.push_back({1.0 / 24.0,
                             ShapeFlatTwoVertex(1.0, 2.0, 12.0, 4.0)});
  samples.samples.push_back({2.0 / 24.0,
                             ShapeFlatTwoVertex(2.0, 4.0, 14.0, 8.0)});
  return samples;
}

void TestFastSummaryPartition() {
  const bbsolver::PropertySamples samples = MakeLinearSamples();
  bbsolver::ShapeFlatLandmarkSubpathOptions options;
  options.fast_summary_only = true;
  options.region_tolerance = 0.1;
  options.max_region_segment_checks = 128;

  const bbsolver::path_multimode::LandmarkPartitionResult partition =
      bbsolver::path_multimode::BuildFastSummaryPartition(
          samples, {{0, 2}}, 2, options, 2);
  Require(partition.ok, "fast summary partition accepted");
  Require(partition.base_ranges == "0-2", "fast summary base ranges");
  Require(partition.chosen_ranges == "0-2", "fast summary chosen ranges");
  Require(partition.boundary_count == 2, "fast summary boundary count");
  Require(partition.interval_evaluations == 1,
          "fast summary interval evaluations");
  Require(partition.emissions.size() == 1, "fast summary emission count");

  options.cancel_fn = [] { return true; };
  const bbsolver::path_multimode::LandmarkPartitionResult cancelled =
      bbsolver::path_multimode::BuildFastSummaryPartition(
          samples, {{0, 2}}, 2, options, 2);
  Require(!cancelled.ok, "cancelled fast summary partition rejected");
}

void TestLandmarkOutputNotes() {
  const bbsolver::PropertySamples samples = MakeLinearSamples();
  bbsolver::ShapeFlatLandmarkSubpathOptions options;
  options.fast_summary_only = true;
  options.region_tolerance = 0.1;
  options.max_region_segment_checks = 128;

  bbsolver::path_multimode::LandmarkPartitionResult partition =
      bbsolver::path_multimode::BuildFastSummaryPartition(
          samples, {{0, 2}}, 2, options, 2);
  partition.semantic_split_note = "subpath_representation=semantic_split";
  partition.outlier_partition_note =
      "subpath_outlier_partition=not_selected; reason=no_outlier_scores";
  partition.emissions.front().dense_run_note = "0-2:checks=1";
  partition.emissions.front().dense_run_checks = 1;
  partition.emissions.front().segment_gap_note = "2:1";
  partition.emissions.front().segment_gap_max = 2;
  partition.emissions.front().segment_rejection_checks = 1;
  partition.emissions.front().segment_lower_bound_note = "chord:1";
  partition.emissions.front().segment_rejection_note = "validation:1";
  partition.emissions.front().outlier_slot_note = "1:2.000";
  partition.emissions.front().outlier_slot_checks = 3;

  bbsolver::path_multimode::VisibleChannelProbeResult probe;
  probe.attempted = true;
  probe.baseline_keys = 12;
  probe.status = "blocked_baseline_missing";
  probe.reason = "visible_baseline_keys_not_provided";

  const std::vector<bbsolver::PropertyKeys> keys =
      bbsolver::path_multimode::BuildLandmarkSubpathOutputKeys(
          samples, options, partition, probe, 3, 2, 2);
  Require(keys.size() == 1, "one output key bundle emitted");
  const std::string& note = keys.front().notes;
  Require(note.find("landmark_subpath; subpath_index=0") == 0,
          "output note prefix preserved");
  Require(note.find("; subpath_partition=key_count_dp") != std::string::npos,
          "partition note preserved");
  Require(note.find("; subpath_fast_summary=true") != std::string::npos,
          "fast summary note preserved");
  Require(note.find("; visible_channel_probe=done") != std::string::npos,
          "visible probe note appended");
  Require(note.find("; subpath_representation=semantic_split") !=
              std::string::npos,
          "semantic split note appended");
  Require(note.find("; subpath_outlier_partition=not_selected") !=
              std::string::npos,
          "outlier partition note appended");
  Require(note.find("; subpath_dense_run_diagnostic=0-2:checks=1") !=
              std::string::npos,
          "dense run note appended");
  Require(note.find("; subpath_segment_gap_hist=2:1") != std::string::npos,
          "segment gap note appended");
  Require(note.find("; subpath_outlier_vertex_top=1:2.000") !=
              std::string::npos,
          "outlier vertex note appended");
}

void TestLandmarkOutputCancellation() {
  const bbsolver::PropertySamples samples = MakeLinearSamples();
  bbsolver::ShapeFlatLandmarkSubpathOptions options;
  options.region_tolerance = 0.1;
  options.max_region_segment_checks = 128;

  const bbsolver::path_multimode::LandmarkPartitionResult partition =
      bbsolver::path_multimode::BuildFastSummaryPartition(
          samples, {{0, 2}}, 2, options, 2);
  options.cancel_fn = [] { return true; };
  const std::vector<bbsolver::PropertyKeys> keys =
      bbsolver::path_multimode::BuildLandmarkSubpathOutputKeys(
          samples, options, partition, {}, 3, 2, 2);
  Require(keys.empty(), "output assembly preserves cancellation");
}

}  // namespace

int main() {
  TestFastSummaryPartition();
  TestLandmarkOutputNotes();
  TestLandmarkOutputCancellation();
  return 0;
}
