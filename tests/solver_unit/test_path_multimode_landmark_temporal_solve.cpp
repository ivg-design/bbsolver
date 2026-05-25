#include "bbsolver/path/multimode/path_multimode_landmark_temporal_solve.hpp"
#include "bbsolver/domain.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
  if (condition) {
    return;
  }
  std::cerr << "test_path_multimode_landmark_temporal_solve: " << message << "\n";
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
  samples.property.id = "unit/path_multimode/temporal_solve";
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

void TestTemporalSolveAccepted() {
  const bbsolver::PropertySamples samples = MakeLinearSamples();
  const bbsolver::path_multimode::LandmarkSubpathTemporalResult result =
      bbsolver::path_multimode::SolveLandmarkRegionTemporal(
          samples, 0.1, 2, 128, {});
  Require(result.ok, "linear temporal solve should be accepted");
  Require(result.keys.converged, "accepted temporal solve converges");
  Require(!result.keys.keys.empty(), "accepted temporal solve emits keys");
  Require(result.reconstruction.ok, "accepted temporal solve validates reconstruction");
  Require(result.notes == "landmark_subpath_temporal_accepted" ||
              result.notes == "landmark_subpath_temporal_relaxed_accepted",
          "accepted temporal note preserved");
}

void TestTemporalSolveCancelled() {
  const bbsolver::PropertySamples samples = MakeLinearSamples();
  const bbsolver::path_multimode::LandmarkSubpathTemporalResult result =
      bbsolver::path_multimode::SolveLandmarkRegionTemporal(
          samples, 0.1, 2, 128, [] { return true; });
  Require(!result.ok, "cancelled temporal solve should not be ok");
  Require(result.notes == "cancelled", "cancelled temporal note preserved");
}

}  // namespace

int main() {
  TestTemporalSolveAccepted();
  TestTemporalSolveCancelled();
  return 0;
}
