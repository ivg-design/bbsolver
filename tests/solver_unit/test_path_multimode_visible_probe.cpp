#include "bbsolver/path/multimode/path_multimode_visible_probe.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/path/multimode/path_multimode_notes.hpp"
#include "bbsolver/path/multimode/path_multimode_solver.hpp"
#include "bbsolver/path/multimode/path_multimode_geometry.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
  if (condition) {
    return;
  }
  std::cerr << "test_path_multimode_visible_probe: " << message << "\n";
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
  samples.property.id = "unit/path_multimode/visible_probe";
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

void TestPartitionDeduplicationAndValidation() {
  std::vector<std::vector<bbsolver::path_multimode::VertexRegion>> partitions;
  bbsolver::path_multimode::AddVisibleProbePartition(
      {{0, 1}, {1, 2}}, 2, &partitions);
  bbsolver::path_multimode::AddVisibleProbePartition(
      {{0, 1}, {1, 2}}, 2, &partitions);
  Require(partitions.size() == 1, "duplicate visible partitions ignored");

  bbsolver::path_multimode::AddVisibleProbePartition(
      {{0, 1}}, 2, &partitions);
  Require(partitions.size() == 1, "non-covering visible partition rejected");

  Require(bbsolver::path_multimode::SameRegionList(
              partitions.front(), {{0, 1}, {1, 2}}),
          "region list equality preserved");
}

void TestVisibleProbePartitionsAndCandidate() {
  const bbsolver::PropertySamples samples = MakeLinearSamples();
  const std::vector<std::vector<bbsolver::path_multimode::VertexRegion>>
      partitions =
          bbsolver::path_multimode::VisibleProbePartitions(samples, 2, 2);
  Require(!partitions.empty(), "visible probe partitions produced");
  Require(partitions.front().front().first_vertex == 0,
          "visible probe partition starts at zero");

  bbsolver::ShapeFlatLandmarkSubpathOptions options;
  options.region_tolerance = 0.1;
  options.max_region_segment_checks = 128;
  const bbsolver::path_multimode::VisibleChannelProbeCandidate empty =
      bbsolver::path_multimode::EvaluateVisibleProbePartition(
          samples, {}, 2, options, 2);
  Require(empty.status == "empty_partition",
          "empty visible probe partition status preserved");

  const bbsolver::path_multimode::VisibleChannelProbeCandidate candidate =
      bbsolver::path_multimode::EvaluateVisibleProbePartition(
          samples, partitions.front(), 2, options, 2);
  Require(candidate.ok, "linear visible probe candidate accepted");
  Require(candidate.status == "ok", "visible probe candidate ok status");
  Require(candidate.key_count > 0, "visible probe candidate key count reported");
  Require(candidate.ranges == "0-1,1-2",
          "visible probe candidate range note preserved");
}

void TestRunVisibleChannelProbeStatus() {
  const bbsolver::PropertySamples samples = MakeLinearSamples();
  bbsolver::ShapeFlatLandmarkSubpathOptions options;
  options.region_tolerance = 0.1;
  options.max_region_segment_checks = 128;

  bbsolver::path_multimode::VisibleChannelProbeResult missing_baseline =
      bbsolver::path_multimode::RunVisibleChannelProbe(samples, options, 2, 2);
  Require(missing_baseline.attempted, "visible probe attempted");
  Require(missing_baseline.status == "blocked_baseline_missing",
          "missing baseline status preserved");
  Require(missing_baseline.reason == "visible_baseline_keys_not_provided",
          "missing baseline reason preserved");

  options.visible_baseline_keys = 100;
  const bbsolver::path_multimode::VisibleChannelProbeResult accepted =
      bbsolver::path_multimode::RunVisibleChannelProbe(samples, options, 2, 2);
  Require(accepted.status == "accepted_key_reduction",
          "visible probe accepts lower key count");
  Require(accepted.reason == "best_contiguous_lower_than_baseline",
          "visible probe accepted reason preserved");
  Require(accepted.selected_channel_count == 2,
          "visible probe selected channel count preserved");

  options.cancel_fn = [] { return true; };
  const bbsolver::path_multimode::VisibleChannelProbeResult cancelled =
      bbsolver::path_multimode::RunVisibleChannelProbe(samples, options, 2, 2);
  Require(cancelled.status == "cancelled", "visible probe cancellation status");
  Require(cancelled.reason == "cancelled", "visible probe cancellation reason");
}

}  // namespace

int main() {
  TestPartitionDeduplicationAndValidation();
  TestVisibleProbePartitionsAndCandidate();
  TestRunVisibleChannelProbeStatus();
  return 0;
}
