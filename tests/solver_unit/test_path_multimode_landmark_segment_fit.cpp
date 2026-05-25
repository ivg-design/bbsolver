#include "bbsolver/path/multimode/path_multimode_landmark_segment_fit.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/path/multimode/path_multimode_temporal.hpp"
#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
  if (condition) {
    return;
  }
  std::cerr << "test_path_multimode_landmark_segment_fit: " << message << "\n";
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
  samples.property.id = "unit/path_multimode/segment_fit";
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

void TestLinearSegmentFit() {
  const bbsolver::PropertySamples samples = MakeLinearSamples();
  bbsolver::SolverConfig config;
  config.tolerance = 0.1;
  config.allow_hold = false;
  config.allow_linear = true;
  config.allow_bezier = true;
  config.allow_shape_temporal_bezier = true;
  const bbsolver::ShapeMorphProgressBandOptions band_options =
      bbsolver::path_multimode::LandmarkBandOptions(config.tolerance, 2);
  const bbsolver::SegmentFitResult fit =
      bbsolver::path_multimode::FitLandmarkRegionShapeSegment(
          0, 2, samples, config, band_options, false);
  Require(fit.feasible, "linear region segment should fit");
  Require(fit.interp == bbsolver::InterpType::Linear,
          "linear region segment should use linear interp");
  Require(fit.reason == "landmark_subpath_temporal_linear_ok",
          "linear fit reason preserved");
}

void TestInvalidSegmentReason() {
  const bbsolver::PropertySamples samples = MakeLinearSamples();
  bbsolver::SolverConfig config;
  const bbsolver::ShapeMorphProgressBandOptions band_options =
      bbsolver::path_multimode::LandmarkBandOptions(0.1, 2);
  const bbsolver::SegmentFitResult fit =
      bbsolver::path_multimode::FitLandmarkRegionShapeSegment(
          1, 1, samples, config, band_options, true);
  Require(!fit.feasible, "invalid segment should not fit");
  Require(fit.reason == "landmark_subpath_temporal_invalid_segment",
          "invalid segment reason preserved");
}

}  // namespace

int main() {
  TestLinearSegmentFit();
  TestInvalidSegmentReason();
  return 0;
}
