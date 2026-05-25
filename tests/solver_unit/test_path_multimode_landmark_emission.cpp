#include "bbsolver/path/multimode/path_multimode_landmark_emission.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/path/multimode/path_multimode_solver.hpp"
#include "bbsolver/path/multimode/path_multimode_reconstruction.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
  if (condition) {
    return;
  }
  std::cerr << "test_path_multimode_landmark_emission: " << message << "\n";
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
  samples.property.id = "unit/path_multimode/emission";
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

void TestLandmarkSubpathNoteModes() {
  bbsolver::path_multimode::LandmarkSubpathReconstructionResult reconstruction;
  reconstruction.ok = true;
  reconstruction.max_outline_error = 0.25;
  reconstruction.worst_sample_idx = 1;
  reconstruction.worst_t_sec = 1.0 / 24.0;
  reconstruction.samples_checked = 3;

  const std::string landmark_note =
      bbsolver::path_multimode::LandmarkSubpathNote(
          0, 1, {0, 2}, {0, 2}, 3, 2, 4, 1, 2, "accepted",
          reconstruction, 2, false);
  Require(landmark_note.find("landmark_subpath; subpath_index=0") == 0,
          "landmark note prefix preserved");
  Require(landmark_note.find("; vertex_range=0-2") != std::string::npos,
          "landmark note includes vertex range");
  Require(landmark_note.find("; subpath_temporal_solver=accepted") !=
              std::string::npos,
          "landmark note includes temporal status");
  Require(landmark_note.find("; visible_channel=true") == std::string::npos,
          "landmark note does not include visible fields");

  const std::string full_visible_note =
      bbsolver::path_multimode::LandmarkSubpathNote(
          0, 1, {0, 2}, {0, 2}, 3, 2, 4, 1, 2, "accepted",
          reconstruction, 2, true);
  Require(full_visible_note.find("shape_channel_subpath; subpath_index=0") == 0,
          "visible note prefix preserved");
  Require(full_visible_note.find("; visible_channel_mode=shape_group_full") !=
              std::string::npos,
          "full visible note marks renderable shape group");

  const std::string partial_visible_note =
      bbsolver::path_multimode::LandmarkSubpathNote(
          0, 2, {0, 1}, {0, 2}, 3, 2, 4, 1, 2, "accepted",
          reconstruction, 2, true);
  Require(partial_visible_note.find(
              "; reason=partial_shape_channel_not_ae_ready") !=
              std::string::npos,
          "partial visible note preserves probe-only reason");
}

void TestEvaluateLandmarkRegionEmissionAccepted() {
  const bbsolver::PropertySamples samples = MakeLinearSamples();
  bbsolver::ShapeFlatLandmarkSubpathOptions options;
  options.region_tolerance = 0.1;
  options.max_region_segment_checks = 128;

  const bbsolver::path_multimode::LandmarkRegionEmissionResult result =
      bbsolver::path_multimode::EvaluateLandmarkRegionEmission(
          samples, {0, 2}, options, 2);
  Require(result.ok, "linear region emission should be accepted");
  Require(result.keys.converged, "accepted emission converges");
  Require(result.region.first_vertex == 0 && result.region.end_vertex == 2,
          "accepted emission preserves region");
  Require(!result.anchors.empty(), "accepted emission reports anchors");
  Require(result.reconstruction.ok, "accepted emission validates reconstruction");
  Require(!result.temporal_status.empty(),
          "accepted emission reports temporal status");
}

void TestEvaluateLandmarkRegionEmissionCancelled() {
  const bbsolver::PropertySamples samples = MakeLinearSamples();
  bbsolver::ShapeFlatLandmarkSubpathOptions options;
  options.cancel_fn = [] { return true; };

  const bbsolver::path_multimode::LandmarkRegionEmissionResult result =
      bbsolver::path_multimode::EvaluateLandmarkRegionEmission(
          samples, {0, 2}, options, 2);
  Require(!result.ok, "cancelled emission should not be ok");
  Require(result.temporal_status == "cancelled",
          "cancelled emission status preserved");
}

}  // namespace

int main() {
  TestLandmarkSubpathNoteModes();
  TestEvaluateLandmarkRegionEmissionAccepted();
  TestEvaluateLandmarkRegionEmissionCancelled();
  return 0;
}
