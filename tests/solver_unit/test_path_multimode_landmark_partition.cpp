#include "bbsolver/path/multimode/path_multimode_landmark_partition.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/path/multimode/path_multimode_landmark_partition_alternatives.hpp"
#include "bbsolver/path/multimode/path_multimode_mask_channel_diagnostic.hpp"
#include "bbsolver/path/multimode/path_multimode_solver.hpp"
#include "bbsolver/path/multimode/path_multimode_geometry.hpp"
#include "bbsolver/path/multimode/path_multimode_landmark_emission.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <cstddef>

namespace {

void Require(bool condition, const std::string& message) {
  if (condition) {
    return;
  }
  std::cerr << "test_path_multimode_landmark_partition: " << message << "\n";
  std::exit(1);
}

bbsolver::path_multimode::LandmarkRegionEmissionResult MakeEmission(
    bbsolver::path_multimode::VertexRegion region,
    int key_count,
    double max_error,
    const std::string& dense_note = "") {
  bbsolver::path_multimode::LandmarkRegionEmissionResult emission;
  emission.ok = true;
  emission.region = region;
  emission.keys.keys.resize(static_cast<std::size_t>(key_count));
  emission.reconstruction.ok = true;
  emission.reconstruction.max_outline_error = max_error;
  emission.dense_run_note = dense_note;
  return emission;
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
  samples.property.id = "unit/path_multimode/partition";
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

void TestBoundaryHelpers() {
  const std::vector<bbsolver::path_multimode::VertexRegion> regions = {
      {0, 2}, {2, 4}};
  const std::vector<int> boundaries =
      bbsolver::path_multimode::RegionBoundaryPoints(regions, 4);
  Require(boundaries == std::vector<int>({0, 1, 2, 3, 4}),
          "boundary neighborhood preserved");
  Require(bbsolver::path_multimode::BoundaryIndex(boundaries, 2) == 2,
          "boundary index found");
  Require(bbsolver::path_multimode::BoundaryIndex(boundaries, 5) == -1,
          "missing boundary rejected");
}

void TestEmissionSummaryHelpers() {
  const std::vector<bbsolver::path_multimode::LandmarkRegionEmissionResult>
      emissions = {
          MakeEmission({0, 1}, 2, 0.25),
          MakeEmission({1, 3}, 3, 0.5),
      };
  Require(bbsolver::path_multimode::RegionsCoverFullRange(emissions, 3),
          "contiguous emissions cover full range");
  Require(bbsolver::path_multimode::TotalEmissionKeyCount(emissions) == 5,
          "key counts summed");
  Require(bbsolver::path_multimode::MaxEmissionReconstructionError(emissions) ==
              0.5,
          "max reconstruction error reported");

  std::vector<bbsolver::path_multimode::LandmarkRegionEmissionResult> dense = {
      MakeEmission({0, 3}, 5, 0.0, "one_shared_progress_chord_infeasible")};
  Require(bbsolver::path_multimode::DenseFullRangeNeedsSemanticSplit(dense, 3),
          "dense full-range infeasible note requests semantic split");
  dense.front().region = {1, 3};
  Require(!bbsolver::path_multimode::DenseFullRangeNeedsSemanticSplit(dense, 3),
          "partial dense note does not request full-range split");
}

void TestOutlierSlotRegionsAndMaskEmpty() {
  const std::vector<bbsolver::path_multimode::VertexRegion> regions =
      bbsolver::path_multimode::OutlierSlotRegions(4, 2);
  Require(regions.size() == 3, "middle outlier slot emits three regions");
  Require(regions[0].first_vertex == 0 && regions[0].end_vertex == 2,
          "outlier leading range preserved");
  Require(regions[1].first_vertex == 2 && regions[1].end_vertex == 3,
          "outlier singleton range preserved");
  Require(regions[2].first_vertex == 3 && regions[2].end_vertex == 4,
          "outlier trailing range preserved");
  Require(bbsolver::path_multimode::OutlierSlotRegions(4, -1).empty(),
          "invalid outlier slot rejected");

  bbsolver::path_multimode::LandmarkPartitionResult selected;
  bbsolver::ShapeFlatLandmarkSubpathOptions options;
  const std::string empty =
      bbsolver::path_multimode::DiagnoseNonContiguousMaskChannels(
          MakeLinearSamples(), selected, options, 2);
  Require(empty.empty(), "mask diagnostic empty for unselected partition");
}

void TestPartitionAlternativeHelpers() {
  using bbsolver::path_multimode::LandmarkRegionEmissionResult;
  using bbsolver::path_multimode::LandmarkPartitionResult;

  std::vector<std::vector<int>> prev(
      3, std::vector<int>(3, -1));
  prev[1][1] = 0;
  prev[2][2] = 1;
  std::vector<std::vector<LandmarkRegionEmissionResult>> intervals(
      3, std::vector<LandmarkRegionEmissionResult>(3));
  intervals[0][1] = MakeEmission({0, 1}, 1, 0.1);
  intervals[1][2] = MakeEmission({1, 3}, 2, 0.2);

  const std::vector<LandmarkRegionEmissionResult> reconstructed =
      bbsolver::path_multimode::ReconstructCachedPartition(
          2, 3, prev, intervals);
  Require(reconstructed.size() == 2, "cached partition reconstructs intervals");
  Require(reconstructed[0].region.first_vertex == 0 &&
              reconstructed[1].region.first_vertex == 1,
          "cached partition preserves interval order");

  LandmarkPartitionResult selected;
  selected.ok = true;
  selected.base_ranges = "0-3";
  selected.chosen_ranges = "0-3";
  selected.boundary_count = 3;
  selected.interval_evaluations = 2;
  selected.emissions.push_back(
      MakeEmission({0, 3}, 5, 0.4, "infeasible_shape_morph_chord"));

  std::vector<LandmarkRegionEmissionResult> candidate = {
      MakeEmission({0, 1}, 1, 0.1),
      MakeEmission({1, 3}, 2, 0.2),
  };
  const LandmarkPartitionResult split =
      bbsolver::path_multimode::TrySemanticSplitFromCandidate(
          selected, candidate, 3.05, 3, "unit_candidate");
  Require(split.ok, "lower-key semantic split candidate selected");
  Require(split.chosen_ranges == "0-1,1-3",
          "semantic split candidate ranges preserved");
  Require(split.semantic_split_note.find(
              "subpath_semantic_split_candidate=unit_candidate") !=
              std::string::npos,
          "semantic split candidate note preserved");
}

void TestBuildKeyCountPartition() {
  const bbsolver::PropertySamples samples = MakeLinearSamples();
  bbsolver::ShapeFlatLandmarkSubpathOptions options;
  options.max_regions = 1;
  options.region_tolerance = 0.1;
  options.max_region_segment_checks = 128;

  const bbsolver::path_multimode::LandmarkPartitionResult result =
      bbsolver::path_multimode::BuildKeyCountLandmarkPartition(
          samples, {{0, 2}}, 2, options, 2);
  Require(result.ok, "linear full-range partition accepted");
  Require(result.base_ranges == "0-2", "base ranges preserved");
  Require(result.chosen_ranges == "0-2", "chosen ranges preserved");
  Require(result.boundary_count == 2, "boundary count preserved");
  Require(result.interval_evaluations == 1, "interval evaluations counted");
  Require(result.emissions.size() == 1, "one emission selected");

  options.cancel_fn = [] { return true; };
  const bbsolver::path_multimode::LandmarkPartitionResult cancelled =
      bbsolver::path_multimode::BuildKeyCountLandmarkPartition(
          samples, {{0, 2}}, 2, options, 2);
  Require(!cancelled.ok, "cancelled partition is not accepted");
}

}  // namespace

int main() {
  TestBoundaryHelpers();
  TestEmissionSummaryHelpers();
  TestOutlierSlotRegionsAndMaskEmpty();
  TestPartitionAlternativeHelpers();
  TestBuildKeyCountPartition();
  return 0;
}
