#include "bbsolver/path/multimode/path_multimode_recombined_temporal.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/path/multimode/path_multimode_solver.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
  if (condition) {
    return;
  }
  std::cerr << "test_path_multimode_recombined_temporal: " << message << "\n";
  std::exit(1);
}

std::vector<double> ShapeFlatRelaxedRegionalTiming(double left_x,
                                                   double right_x) {
  const std::vector<std::pair<double, double>> vertices = {
      {left_x, 0.0},
      {left_x, 10.0},
      {right_x, 10.0},
      {right_x, 0.0},
  };
  std::vector<double> out;
  out.push_back(1.0);
  out.push_back(static_cast<double>(vertices.size()));
  for (const auto& vertex : vertices) {
    out.push_back(vertex.first);
    out.push_back(vertex.second);
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
  }
  return out;
}

bbsolver::PropertySamples MakeRelaxedRecombinedRegionalFixture() {
  bbsolver::PropertySamples ps;
  ps.property.id = "unit/path_multimode/recombined_region_relaxed";
  ps.property.kind = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions = 2 + 6 * 4;
  ps.t_start_sec = 0.0;
  ps.t_end_sec = 2.0 / 24.0;
  ps.samples_per_frame = 1;

  ps.samples.push_back({0.0 / 24.0,
                        ShapeFlatRelaxedRegionalTiming(0.0, 80.0)});
  ps.samples.push_back({1.0 / 24.0,
                        ShapeFlatRelaxedRegionalTiming(6.0, 74.0)});
  ps.samples.push_back({2.0 / 24.0,
                        ShapeFlatRelaxedRegionalTiming(20.0, 60.0)});
  return ps;
}

bbsolver::ShapeFlatMultiModeOptions Options() {
  bbsolver::ShapeFlatMultiModeOptions options;
  options.max_regions = 2;
  options.max_gap_samples = 4;
  options.region_tolerance = 2.5;
  options.frame_fit_options.outline_tolerance = 2.5;
  options.frame_fit_options.max_subdivisions_per_segment = 8;
  options.max_region_segment_checks = 10000;
  options.max_candidate_key_ratio = 1.0;
  return options;
}

void TestNoKeyReductionOpportunity() {
  const bbsolver::PropertySamples fixture =
      MakeRelaxedRecombinedRegionalFixture();
  const bbsolver::path_multimode::RecombinedRegionTemporalResult result =
      bbsolver::path_multimode::TryRecombinedRegionTemporalCandidate(
          fixture,
          fixture,
          {{0, 4}},
          Options(),
          4,
          2);
  Require(!result.attempted, "single region should not attempt recombination");
  Require(!result.accepted, "single region should not be accepted");
  Require(result.note ==
              "recombined_region_temporal=not_selected; reason=no_key_reduction_opportunity",
          "single-region rejection note preserved");
}

void TestRejectedCandidateIncludesValidationNotes() {
  const bbsolver::PropertySamples fixture =
      MakeRelaxedRecombinedRegionalFixture();
  const bbsolver::path_multimode::RecombinedRegionTemporalResult result =
      bbsolver::path_multimode::TryRecombinedRegionTemporalCandidate(
          fixture,
          fixture,
          {{0, 2}, {2, 4}},
          Options(),
          2,
          3);
  Require(result.attempted, "two regions should attempt recombination");
  Require(!result.accepted,
          "direct recombined candidate should stay rejected");
  Require(result.note.find(
              "recombined_region_temporal=not_selected; reason=source_outline_validation_failed") !=
              std::string::npos,
          "validation rejection note preserved");
  Require(result.note.find("recombined_region_temporal_keys=2") !=
              std::string::npos,
          "validation rejection note includes candidate key count");
  Require(result.region_key_counts == "2,2",
          "region key count summary preserved");
  Require(result.temporal_segment_checks == 6,
          "temporal segment check count preserved");
}

}  // namespace

int main() {
  TestNoKeyReductionOpportunity();
  TestRejectedCandidateIncludesValidationNotes();
  return 0;
}
