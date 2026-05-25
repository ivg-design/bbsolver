#include "bbsolver/motion_smooth/motion_smooth_bezier_ease.hpp"
#include "bbsolver/motion_smooth/motion_smooth_endpoint_keys.hpp"
#include "bbsolver/motion_smooth/motion_smooth_sample_points.hpp"
#include "bbsolver/motion_smooth/motion_smooth_spatial_trajectory.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/shape/shape_flat_topology.hpp"
#include "bbsolver/dp/dp_placer.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>
#include <cstddef>

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    std::abort();
  }
}

bool AlmostEqual(double a, double b, double eps = 1e-9) {
  return std::abs(a - b) <= eps;
}

bool Contains(const std::string& text, const std::string& needle) {
  return text.find(needle) != std::string::npos;
}

bbsolver::Sample Sample(double t, std::vector<double> value) {
  bbsolver::Sample sample;
  sample.t_sec = t;
  sample.v = std::move(value);
  return sample;
}

bbsolver::PropertySamples BaseProperty(int dimensions = 2) {
  bbsolver::PropertySamples property;
  property.property.id = "position";
  property.property.dimensions = dimensions;
  property.t_start_sec = 0.0;
  property.t_end_sec = 1.0;
  return property;
}

bbsolver::PropertySamples SpatialProperty() {
  bbsolver::PropertySamples property = BaseProperty(2);
  property.property.kind = bbsolver::ValueKind::TwoD_Spatial;
  property.property.is_spatial = true;
  property.samples.push_back(Sample(0.0, {0.0, 0.0}));
  property.samples.push_back(Sample(0.5, {10.0, 20.0}));
  property.samples.push_back(Sample(1.0, {20.0, 0.0}));
  return property;
}

bbsolver::PropertySamples ShapeFlatProperty(
    const std::vector<double>& first,
    const std::vector<double>& last) {
  bbsolver::PropertySamples property = BaseProperty(
      static_cast<int>(std::max(first.size(), last.size())));
  property.property.id = "shape";
  property.property.kind = bbsolver::ValueKind::Custom;
  property.property.units_label = "shape_flat";
  property.samples.push_back(Sample(0.0, first));
  property.samples.push_back(Sample(1.0, last));
  return property;
}

std::vector<double> ShapeFlat(int count, bool closed = true) {
  std::vector<bbsolver::ShapeFlatVertex> vertices;
  vertices.reserve(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    vertices.push_back({static_cast<double>(i), static_cast<double>(i % 2),
                        0.0, 0.0, 0.0, 0.0});
  }
  return bbsolver::ShapeFlatFromVertices(vertices, closed);
}

void TestMotionSmoothSpatialClassification() {
  bbsolver::PropertySamples spatial = SpatialProperty();
  Require(bbsolver::IsMotionSmoothSpatialProperty(spatial),
          "unseparated spatial property must use spatial motion smoothing");

  spatial.property.is_separated = true;
  Require(!bbsolver::IsMotionSmoothSpatialProperty(spatial),
          "separated spatial property must not use spatial motion smoothing");

  bbsolver::PropertySamples shape =
      ShapeFlatProperty(ShapeFlat(2), ShapeFlat(2));
  shape.property.is_spatial = true;
  Require(!bbsolver::IsMotionSmoothSpatialProperty(shape),
          "shape_flat custom property must not use spatial motion smoothing");
}

void TestSourceKeyTimesFilterSortAndDeduplicate() {
  bbsolver::PropertySamples property = BaseProperty();
  property.t_start_sec = 1.0;
  property.t_end_sec = 3.0;
  property.property.source_key_times = {
      3.5,
      std::numeric_limits<double>::infinity(),
      2.0,
      1.0 - 2e-6,
      1.0,
      2.0 + 5e-7,
      2.5,
  };

  const std::vector<double> times =
      bbsolver::MotionSmoothSourceKeyTimes(property);
  Require(times.size() == 3, "source key times must filter and deduplicate");
  Require(AlmostEqual(times[0], 1.0), "source key times must include start");
  Require(AlmostEqual(times[1], 2.0), "source key times must keep first duplicate");
  Require(AlmostEqual(times[2], 2.5), "source key times must sort ascending");
}

void TestSegmentEndpointValueOrSample() {
  bbsolver::PropertySamples property = BaseProperty(3);
  property.samples.push_back(Sample(0.0, {1.0, 2.0, 3.0}));
  property.samples.push_back(Sample(1.0, {}));

  bbsolver::SegmentFitResult fit;
  fit.key_value_at_i = {7.0, 8.0};
  fit.key_value_at_j = {9.0, 10.0};
  Require(bbsolver::SegmentEndpointValueOrSample(property, fit, true) ==
              fit.key_value_at_i,
          "fitted start endpoint value must win over sample");
  Require(bbsolver::SegmentEndpointValueOrSample(property, fit, false) ==
              fit.key_value_at_j,
          "fitted end endpoint value must win over sample");

  fit.key_value_at_i.clear();
  fit.key_value_at_j.clear();
  Require(bbsolver::SegmentEndpointValueOrSample(property, fit, true) ==
              std::vector<double>({1.0, 2.0, 3.0}),
          "missing fitted start endpoint must fall back to first sample");
  Require(bbsolver::SegmentEndpointValueOrSample(property, fit, false) ==
              std::vector<double>({0.0, 0.0, 0.0}),
          "missing fitted end endpoint must fall back to last sample zeros");
}

void TestEndpointKeysAndTopologyFallback() {
  bbsolver::SolverConfig config;
  config.motion_smooth_use_ease = true;

  bbsolver::PropertySamples property = SpatialProperty();
  const bbsolver::PropertyKeys keys =
      bbsolver::MotionSmoothEndpointKeys(property, config);
  Require(keys.converged, "endpoint keys must converge for sampled property");
  Require(keys.keys.size() == 2, "endpoint keys must emit first and last sample");
  Require(keys.keys.front().interp_in == bbsolver::InterpType::Linear,
          "first endpoint key must have linear incoming interpolation");
  Require(keys.keys.back().interp_out == bbsolver::InterpType::Linear,
          "last endpoint key must have linear outgoing interpolation");
  Require(keys.keys.front().spatial_continuous &&
              keys.keys.front().spatial_auto_bezier,
          "spatial endpoint keys must preserve spatial auto-bezier flags");
  Require(keys.segments.size() == 1 &&
              keys.segments.front().reason ==
                  "motion_smooth_endpoint_interpolation",
          "endpoint keys must keep segment report reason");
  Require(Contains(keys.notes, "endpoint_keys=2") &&
              Contains(keys.notes, "motion_smooth_ease=on") &&
              Contains(keys.notes, "source_error_not_evaluated=true"),
          "endpoint notes must preserve motion-smooth tokens");

  bbsolver::PropertySamples empty = BaseProperty();
  const bbsolver::PropertyKeys no_samples =
      bbsolver::MotionSmoothEndpointKeys(empty, config);
  Require(!no_samples.converged &&
              no_samples.notes == "solve_mode_motion_smooth; no_samples",
          "endpoint keys must preserve empty sample note");

  const bbsolver::PropertyKeys fallback =
      bbsolver::MotionSmoothEndpointKeys(
          ShapeFlatProperty(ShapeFlat(2), ShapeFlat(3)), config);
  Require(Contains(fallback.notes,
                   "solve_mode_motion_smooth_skipped: "
                   "endpoint_topology_mismatch"),
          "shape endpoint topology mismatch must use fallback note");
}

void TestRawAndInterpolatedVectors() {
  bbsolver::PropertySamples property = BaseProperty(3);
  property.samples.push_back(Sample(0.0, {0.0, 0.0}));
  property.samples.push_back(Sample(1.0, {10.0, 20.0, 30.0}));

  const std::vector<std::vector<double>> raw =
      bbsolver::MotionSmoothRawPoints(property, 3);
  Require(raw.size() == 2, "raw points must mirror samples");
  Require(raw[0] == std::vector<double>({0.0, 0.0, 0.0}),
          "raw points must zero-fill short sample vectors");

  const std::vector<double> mid =
      bbsolver::MotionSmoothInterpolatedVector(property, raw, 0.5, 3);
  Require(mid == std::vector<double>({5.0, 10.0, 15.0}),
          "interpolated vector must linearly interpolate between samples");
}

void TestBezierEaseApplication() {
  bbsolver::PropertySamples property = BaseProperty(2);
  property.samples.push_back(Sample(0.0, {0.0, 0.0}));
  property.samples.push_back(Sample(1.0, {3.0, 4.0}));

  bbsolver::SolverConfig config;
  config.motion_smooth_use_ease = true;
  config.motion_smooth_bezier_x1 = 0.25;
  config.motion_smooth_bezier_y1 = 0.50;
  config.motion_smooth_bezier_x2 = 0.75;
  config.motion_smooth_bezier_y2 = 0.50;

  std::vector<bbsolver::Key> keys(2);
  keys[0].t_sec = 0.0;
  keys[0].v = {0.0, 0.0};
  keys[1].t_sec = 1.0;
  keys[1].v = {3.0, 4.0};
  bbsolver::ApplyMotionSmoothBezierEase(property, config, 2, &keys);

  Require(keys[0].interp_out == bbsolver::InterpType::Bezier &&
              keys[1].interp_in == bbsolver::InterpType::Bezier,
          "ease application must set Bezier interpolation");
  Require(keys[0].temporal_continuous && keys[1].temporal_continuous,
          "ease application must set temporal continuity");
  Require(!keys[0].temporal_ease_out.empty() &&
              AlmostEqual(keys[0].temporal_ease_out[0].speed, 10.0),
          "outgoing ease speed must preserve average-speed slope behavior");
  Require(!keys[1].temporal_ease_in.empty() &&
              AlmostEqual(keys[1].temporal_ease_in[0].influence, 25.0),
          "incoming ease influence must preserve x2 behavior");
}

void TestSpatialTrajectoryUsesSourceKeySchedule() {
  bbsolver::PropertySamples property = SpatialProperty();
  property.property.source_key_times = {0.0, 0.5, 1.0};
  bbsolver::SolverConfig config;
  config.motion_smooth_use_ease = true;
  config.allow_bezier = true;
  config.motion_smooth_tolerance = 3.0;

  const bbsolver::PropertyKeys keys =
      bbsolver::MotionSmoothSpatialTrajectoryKeys(property, config);
  Require(keys.converged, "spatial trajectory keys must converge");
  Require(keys.keys.size() == 3,
          "source key schedule must drive spatial trajectory key count");
  Require(keys.keys.front().v == property.samples.front().v,
          "source-key spatial trajectory must preserve raw first endpoint");
  Require(keys.keys.back().v == property.samples.back().v,
          "source-key spatial trajectory must preserve raw last endpoint");
  Require(keys.keys[1].roving,
          "interior spatial trajectory keys must keep roving enabled");
  Require(keys.keys[1].spatial_continuous &&
              !keys.keys[1].spatial_auto_bezier,
          "spatial trajectory keys must preserve spatial flags");
  Require(keys.keys[1].spatial_in.size() == 2 &&
              keys.keys[1].spatial_out.size() == 2,
          "spatial trajectory keys must size spatial tangents to dimensions");
  Require(keys.segments.size() == 1 &&
              keys.segments.front().reason ==
                  "motion_smooth_spatial_trajectory_filter",
          "spatial trajectory segment reason must be stable");
  Require(Contains(keys.notes, "motion_smooth_spatial_trajectory_filter=true") &&
              Contains(keys.notes, "motion_smooth_source_key_times=true") &&
              Contains(keys.notes, "key_schedule=source_keys") &&
              Contains(keys.notes, "motion_smooth_ease=on") &&
              Contains(keys.notes, "source_error_not_constrained=true"),
          "spatial trajectory notes must preserve diagnostic tokens");
}

void TestSpatialTrajectoryFallsBackToSampleRdpSchedule() {
  bbsolver::PropertySamples property = SpatialProperty();
  property.property.source_key_times.clear();
  bbsolver::SolverConfig config;
  config.motion_smooth_use_ease = false;
  config.motion_smooth_tolerance = 1.0;

  const bbsolver::PropertyKeys keys =
      bbsolver::MotionSmoothSpatialTrajectoryKeys(property, config);
  Require(keys.keys.size() >= 2,
          "sample RDP schedule must keep at least endpoint keys");
  Require(Contains(keys.notes, "motion_smooth_source_key_times=false") &&
              Contains(keys.notes, "key_schedule=sample_rdp") &&
              Contains(keys.notes, "motion_smooth_ease=off"),
          "sample RDP schedule notes must preserve fallback tokens");
}

}  // namespace

int main() {
  TestMotionSmoothSpatialClassification();
  TestSourceKeyTimesFilterSortAndDeduplicate();
  TestSegmentEndpointValueOrSample();
  TestEndpointKeysAndTopologyFallback();
  TestRawAndInterpolatedVectors();
  TestBezierEaseApplication();
  TestSpatialTrajectoryUsesSourceKeySchedule();
  TestSpatialTrajectoryFallsBackToSampleRdpSchedule();
  std::cout << "[PASS] test_motion_smooth_solver\n";
  return 0;
}
