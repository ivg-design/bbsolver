#include "bbsolver/motion_smooth/motion_smooth_shape_flat.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/motion_smooth/motion_smooth_shape_loop.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_quality.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_rove_schedule.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_source_key_schedule.hpp"
#include "bbsolver/shape/shape_flat_topology.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
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

std::vector<double> Flat(
    const std::vector<bbsolver::ShapeFlatVertex>& vertices,
    bool closed = false) {
  return bbsolver::ShapeFlatFromVertices(vertices, closed);
}

std::vector<double> Triangle(double x_offset = 0.0, bool closed = false) {
  return Flat({
      {0.0 + x_offset, 0.0, 0.0, 0.0, 0.0, 0.0},
      {10.0 + x_offset, 0.0, 0.0, 0.0, 0.0, 0.0},
      {0.0 + x_offset, 10.0, 0.0, 0.0, 0.0, 0.0},
  }, closed);
}

bbsolver::Sample Sample(double t, std::vector<double> value) {
  bbsolver::Sample sample;
  sample.t_sec = t;
  sample.v = std::move(value);
  return sample;
}

bbsolver::PropertySamples ShapeProperty(
    const std::vector<double>& times,
    const std::vector<std::vector<double>>& values) {
  bbsolver::PropertySamples property;
  property.property.id = "shape";
  property.property.kind = bbsolver::ValueKind::Custom;
  property.property.units_label = "shape_flat";
  property.property.dimensions =
      values.empty() ? 1: static_cast<int>(values.front().size());
  property.t_start_sec = times.empty() ? 0.0: times.front();
  property.t_end_sec = times.empty() ? 0.0: times.back();
  property.property.source_key_times = times;
  for (std::size_t i = 0; i < times.size() && i < values.size(); ++i) {
    property.samples.push_back(Sample(times[i], values[i]));
  }
  return property;
}

void TestControlDistanceAndMalformedInputs() {
  const std::vector<double> a = Flat({
      {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
  });
  const std::vector<double> b = Flat({
      {3.0, 4.0, 0.0, 0.0, 0.0, 0.0},
  });
  double max_control = -1.0;
  Require(AlmostEqual(
              bbsolver::ShapeFlatControlDistance(a, b, 1, &max_control), 5.0),
          "control distance must use euclidean control deltas");
  Require(AlmostEqual(max_control, 5.0),
          "control distance must report max control displacement");

  max_control = -1.0;
  Require(bbsolver::ShapeFlatControlDistance({1.0}, b, 1, &max_control) ==
              0.0,
          "malformed control distance input must return zero");
  Require(max_control == 0.0,
          "malformed control distance input must clear max control output");
}

void TestVectorDistanceToLinearGuardsTopologyAndClosedFlag() {
  const std::vector<double> left = Triangle(0.0);
  const std::vector<double> right = Triangle(10.0);
  const std::vector<double> mid = Triangle(5.0);
  Require(bbsolver::ShapeFlatVectorDistanceToLinear(left, right, mid, 0.5) ==
              0.0,
          "shape vector distance must be zero for linear midpoint");

  const std::vector<double> closed_mid = Triangle(5.0, true);
  Require(std::isinf(bbsolver::ShapeFlatVectorDistanceToLinear(
              left, right, closed_mid, 0.5)),
          "shape vector distance must reject closed-flag mismatch");
}

void TestMotionQualityAndNoteTokens() {
  const std::vector<double> first = Flat({
      {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      {10.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
  }, true);
  const std::vector<double> second = Flat({
      {1.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      {11.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      {1.0, 0.0, 0.0, 0.0, 0.0, 0.0},
  }, true);
  const bbsolver::ShapeMotionQualityMetrics metrics =
      bbsolver::ShapeMotionQuality({first, second}, 3);
  Require(metrics.valid, "valid duplicate-terminal shapes must score");
  Require(metrics.vertex_count == 3,
          "quality metrics must preserve declared vertex count");
  Require(metrics.effective_vertex_count == 2,
          "quality metrics must discount closed duplicate terminal vertices");

  const std::string note =
      bbsolver::ShapeMotionQualityNote(metrics, "shape_quality");
  Require(Contains(note, "shape_quality_valid=true"),
          "quality note must include valid token");
  Require(Contains(note, "shape_quality_effective_vertices=2"),
          "quality note must include effective vertex token");

  const bbsolver::ShapeMotionQualityMetrics invalid =
      bbsolver::ShapeMotionQuality({first}, 3);
  Require(bbsolver::ShapeMotionQualityNote(invalid, "shape_quality") ==
              "shape_quality_valid=false",
          "invalid quality note must preserve compact false token");
}

void TestEvenTimesAndRoveSchedule() {
  const std::vector<double> even = bbsolver::EvenTimesForValueCount(1.0, 3.0, 3);
  Require(even.size() == 3 && even[0] == 1.0 && even[1] == 2.0 &&
              even[2] == 3.0,
          "even time helper must include endpoints and midpoint");
  const std::vector<double> single =
      bbsolver::EvenTimesForValueCount(5.0, 9.0, 1);
  Require(single.size() == 1 && single[0] == 5.0,
          "single even time must return the start time");

  const std::vector<double> a = Triangle(0.0);
  const std::vector<double> b = Triangle(0.0);
  const std::vector<double> c = Triangle(10.0);
  const bbsolver::ShapeMotionRoveSchedule static_removed =
      bbsolver::BuildShapeMotionRoveScheduleFromValues(
          {0.0, 0.5, 1.0}, {a, b, c}, 3);
  Require(static_removed.static_keys_removed == 1,
          "rove schedule must drop redundant interior static source keys");
  Require(static_removed.times.size() == 2 &&
              static_removed.values.size() == 2,
          "rove schedule must keep endpoints after redundant removal");

  const bbsolver::ShapeMotionRoveSchedule unroved =
      bbsolver::BuildShapeMotionRoveScheduleFromValues(
          {0.0, 0.5, 1.0}, {Triangle(0.0), Triangle(4.0), Triangle(10.0)}, 3,
          false);
  Require(!unroved.rove_applied,
          "disabled rove schedule must preserve source timing");
  Require(unroved.times.size() == 3 && unroved.times[1] == 0.5,
          "disabled rove schedule must not shift interior source time");
}

void TestSourceKeyScheduleSimplifiesLinearSourceKeys() {
  const std::vector<double> times = {0.0, 0.5, 1.0};
  const std::vector<std::vector<double>> values = {
      Triangle(0.0),
      Triangle(5.0),
      Triangle(10.0),
  };
  const bbsolver::PropertySamples property = ShapeProperty(times, values);
  const bbsolver::ShapeMotionSourceKeySchedule schedule =
      bbsolver::BuildShapeMotionSourceKeySchedule(
          property,
          times,
          values,
          static_cast<int>(values.front().size()),
          3.0);
  Require(schedule.raw_count == 3,
          "source key schedule must report raw source key count");
  Require(schedule.simplification_enabled,
          "source key schedule must enable simplification with more than two keys");
  Require(schedule.simplified_count == 2,
          "linear source key schedule must simplify to endpoints");
  Require(schedule.redundant_removed == 1,
          "source key schedule must count redundant removed source keys");
}

void TestShapeFlatTrajectoryFallbacksAndSuccessfulNotes() {
  bbsolver::SolverConfig config;

  bbsolver::PropertySamples too_short;
  too_short.property.id = "shape";
  const bbsolver::PropertyKeys no_span =
      bbsolver::MotionSmoothShapeFlatTrajectoryKeys(too_short, config);
  Require(no_span.converged && no_span.keys.empty() &&
              no_span.notes == "solve_mode_motion_smooth; no_shape_motion_span",
          "shape-flat trajectory must preserve no-span note");

  const bbsolver::PropertySamples invalid =
      ShapeProperty({0.0, 1.0}, {{1.0}, {1.0}});
  const bbsolver::PropertyKeys invalid_keys =
      bbsolver::MotionSmoothShapeFlatTrajectoryKeys(invalid, config);
  Require(Contains(invalid_keys.notes,
                   "solve_mode_motion_smooth_skipped: invalid_shape_topology"),
          "invalid shape topology must fall back to raw shape keys");

  const std::vector<double> times = {0.0, 0.5, 1.0};
  const std::vector<std::vector<double>> values = {
      Triangle(0.0),
      Triangle(4.0),
      Triangle(10.0),
  };
  const bbsolver::PropertySamples property = ShapeProperty(times, values);
  const bbsolver::PropertyKeys keys =
      bbsolver::MotionSmoothShapeFlatTrajectoryKeys(property, config);
  Require(keys.converged, "shape-flat trajectory keys must converge");
  Require(keys.keys.size() >= 2,
          "shape-flat trajectory keys must emit at least two keys");
  Require(keys.segments.size() == 1 &&
              keys.segments.front().reason ==
                  "motion_smooth_shape_trajectory_filter",
          "shape-flat trajectory must keep segment report reason");
  Require(Contains(keys.notes, "motion_smooth_shape_rove_time=true") &&
              Contains(keys.notes,
                       "motion_smooth_shape_trajectory_filter=true") &&
              Contains(keys.notes, "motion_smooth_stable_topology=true") &&
              Contains(keys.notes, "key_schedule=source_keys_roved") &&
              Contains(keys.notes, "motion_smooth_ease=off") &&
              Contains(keys.notes, "source_error_not_constrained=true"),
          "shape-flat trajectory notes must preserve motion-smooth tokens");
  Require(keys.keys.front().interp_in == bbsolver::InterpType::Linear &&
              keys.keys.back().interp_out == bbsolver::InterpType::Linear,
          "shape-flat trajectory endpoints must keep linear outer interpolation");
}

void TestShapeFlatTrajectoryEaseAndSourceFidelity() {
  const std::vector<double> times = {0.0, 0.25, 0.5, 0.75, 1.0};
  const std::vector<std::vector<double>> values = {
      Triangle(0.0),
      Triangle(2.0),
      Triangle(6.0),
      Triangle(8.0),
      Triangle(10.0),
  };
  const bbsolver::PropertySamples property = ShapeProperty(times, values);

  bbsolver::SolverConfig eased_config;
  eased_config.motion_smooth_use_ease = true;
  eased_config.allow_bezier = true;
  const bbsolver::PropertyKeys eased =
      bbsolver::MotionSmoothShapeFlatTrajectoryKeys(property, eased_config);
  Require(Contains(eased.notes, "motion_smooth_ease=on"),
          "shape-flat trajectory must report enabled temporal ease");
  Require(eased.keys.size() >= 2 &&
              eased.keys.front().interp_out == bbsolver::InterpType::Bezier &&
              eased.keys.back().interp_in == bbsolver::InterpType::Bezier,
          "enabled temporal ease must set inner key interpolation to bezier");
  Require(eased.keys.front().temporal_continuous &&
              !eased.keys.front().temporal_auto_bezier,
          "enabled temporal ease must preserve motion-smooth ease flags");

  bbsolver::SolverConfig fidelity_config;
  fidelity_config.motion_smooth_source_fidelity = true;
  const bbsolver::PropertyKeys fidelity =
      bbsolver::MotionSmoothShapeFlatTrajectoryKeys(property, fidelity_config);
  Require(Contains(fidelity.notes, "motion_smooth_source_fidelity=true") &&
              Contains(fidelity.notes, "key_schedule=source_key_times_spline") &&
              Contains(fidelity.notes, "source_error_not_constrained=false"),
          "source fidelity mode must preserve schedule and error note tokens");
}

}  // namespace

int main() {
  TestControlDistanceAndMalformedInputs();
  TestVectorDistanceToLinearGuardsTopologyAndClosedFlag();
  TestMotionQualityAndNoteTokens();
  TestEvenTimesAndRoveSchedule();
  TestSourceKeyScheduleSimplifiesLinearSourceKeys();
  TestShapeFlatTrajectoryFallbacksAndSuccessfulNotes();
  TestShapeFlatTrajectoryEaseAndSourceFidelity();
  std::cout << "[PASS] test_motion_smooth_shape_flat\n";
  return 0;
}
