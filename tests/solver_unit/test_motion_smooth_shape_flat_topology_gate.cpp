//  focused test: the -extracted topology gate.
//
// ValidateMotionSmoothShapeFlatTopology has five return paths that the
// shape-flat orchestrator dispatches on:
//
//   1. success                       → ok=true, vertex_count/dims/key_times populated
//   2. no_shape_motion_span          → samples.size() < 2; direct keys.notes set
//   3. invalid_shape_topology        → vertex_count <= 0 or dims < 8
//   4. variable_shape_topology       → per-sample topology mismatch
//   5. no_source_key_schedule        → MotionSmoothSourceKeyTimes returns < 2
//
// The policy locks the four fallback note strings via raw-text
// scan; this file complements that by exercising the actual branches
// at runtime with synthetic PropertySamples and verifying each
// produces the expected note plus an ok=false flag.

#include "bbsolver/motion_smooth/motion_smooth_shape_flat_topology_gate.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "bbsolver/domain.hpp"

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    std::abort();
  }
}

bool Contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

// Shape-flat value vector layout (per shape_flat_topology.cpp):
//   value[0] = closed flag (0 or 1)
//   value[1] = vertex count N (read via std::llround)
//   value[2 + i*6 + 0..5] = vertex_i {pos_x, pos_y, in_x, in_y, out_x, out_y}
// Total size for N vertices: 2 + N * 6.
std::vector<double> SingleVertexValue(double pos_x, double pos_y) {
  return {0.0, 1.0, pos_x, pos_y, 0.0, 0.0, 0.0, 0.0};
}

std::vector<double> TwoVertexValue(double v0x, double v0y,
                                   double v1x, double v1y) {
  return {0.0, 2.0,
          v0x, v0y, 0.0, 0.0, 0.0, 0.0,
          v1x, v1y, 0.0, 0.0, 0.0, 0.0};
}

bbsolver::PropertySamples MakeProperty(double t_start, double t_end) {
  bbsolver::PropertySamples ps;
  ps.property.id = "test_motion_smooth_topology_gate";
  ps.t_start_sec = t_start;
  ps.t_end_sec = t_end;
  return ps;
}

bbsolver::Sample MakeSample(double t_sec, std::vector<double> v) {
  bbsolver::Sample s;
  s.t_sec = t_sec;
  s.v = std::move(v);
  return s;
}

void TestGateRejectsZeroOrOneSampleAsNoShapeMotionSpan() {
  // Path 2: samples.size() < 2. Direct keys.notes set (not
  // ShapeFlatFrameKeyFallback). Note format: literal
  // "solve_mode_motion_smooth; no_shape_motion_span".
  bbsolver::PropertySamples zero_samples = MakeProperty(0.0, 1.0);
  bbsolver::MotionSmoothShapeFlatTopologyGateResult r0 =
      bbsolver::ValidateMotionSmoothShapeFlatTopology(zero_samples);
  Require(!r0.ok, "zero-sample input must reject");
  Require(Contains(r0.fallback_keys.notes,
                   "solve_mode_motion_smooth; no_shape_motion_span"),
          "zero-sample fallback must publish the exact no_shape_motion_span note");

  bbsolver::PropertySamples one_sample = MakeProperty(0.0, 1.0);
  one_sample.samples.push_back(MakeSample(0.5, SingleVertexValue(0.0, 0.0)));
  bbsolver::MotionSmoothShapeFlatTopologyGateResult r1 =
      bbsolver::ValidateMotionSmoothShapeFlatTopology(one_sample);
  Require(!r1.ok, "single-sample input must reject");
  Require(Contains(r1.fallback_keys.notes, "no_shape_motion_span"),
          "single-sample fallback must publish no_shape_motion_span");
}

void TestGateRejectsInvalidShapeTopology() {
  // Path 3: vertex_count <= 0 or dims < 8. The minimum valid shape-flat
  // value vector has dims = 2 + 1*6 = 8. A shorter value yields
  // dims < 8; a value where value[1] doesn't match the actual vertex
  // count yields vertex_count = -1 via ShapeFlatVertexCountFromValues.

  // Short value (only 4 dims) → dims < 8 fails the gate.
  bbsolver::PropertySamples short_dims = MakeProperty(0.0, 1.0);
  short_dims.samples.push_back(MakeSample(0.0, {0.0, 0.0, 0.0, 0.0}));
  short_dims.samples.push_back(MakeSample(1.0, {0.0, 0.0, 0.0, 0.0}));
  bbsolver::MotionSmoothShapeFlatTopologyGateResult r_short =
      bbsolver::ValidateMotionSmoothShapeFlatTopology(short_dims);
  Require(!r_short.ok, "dims<8 input must reject");
  Require(Contains(r_short.fallback_keys.notes,
                   "solve_mode_motion_smooth_skipped: invalid_shape_topology"),
          "dims<8 fallback must publish invalid_shape_topology note");

  // Mismatched vertex-count slot: value has size 8 but value[1]=0
  // (claiming zero vertices) → ShapeFlatVertexCountFromValues returns
  // -1 → vertex_count <= 0 fails the gate.
  bbsolver::PropertySamples bad_vertex_count = MakeProperty(0.0, 1.0);
  std::vector<double> mismatched =
      {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};  // value[1]=0
  bad_vertex_count.samples.push_back(MakeSample(0.0, mismatched));
  bad_vertex_count.samples.push_back(MakeSample(1.0, mismatched));
  bbsolver::MotionSmoothShapeFlatTopologyGateResult r_bad =
      bbsolver::ValidateMotionSmoothShapeFlatTopology(bad_vertex_count);
  Require(!r_bad.ok, "vertex_count<=0 input must reject");
  Require(Contains(r_bad.fallback_keys.notes, "invalid_shape_topology"),
          "vertex_count<=0 fallback must publish invalid_shape_topology");
}

void TestGateRejectsVariableShapeTopology() {
  // Path 4: per-sample topology mismatch. First sample has 1 vertex;
  // second sample has 2 vertices. Both have size>=8 individually so
  // the first sample passes the per-front gate, then the per-sample
  // loop catches the mismatch on the second.
  bbsolver::PropertySamples mixed = MakeProperty(0.0, 1.0);
  mixed.samples.push_back(MakeSample(0.0, SingleVertexValue(0.0, 0.0)));
  mixed.samples.push_back(
      MakeSample(1.0, TwoVertexValue(0.0, 0.0, 1.0, 1.0)));
  bbsolver::MotionSmoothShapeFlatTopologyGateResult r =
      bbsolver::ValidateMotionSmoothShapeFlatTopology(mixed);
  Require(!r.ok, "variable-topology input must reject");
  Require(Contains(r.fallback_keys.notes,
                   "solve_mode_motion_smooth_skipped: variable_shape_topology"),
          "variable-topology fallback must publish the exact variable_shape_topology note");
}

void TestGateRejectsNoSourceKeySchedule() {
  // Path 5: valid topology but property.source_key_times is empty
  // (or none fall in window). MotionSmoothSourceKeyTimes returns < 2
  // entries → fallback "no_source_key_schedule".
  bbsolver::PropertySamples no_keys = MakeProperty(0.0, 1.0);
  no_keys.samples.push_back(MakeSample(0.0, SingleVertexValue(0.0, 0.0)));
  no_keys.samples.push_back(MakeSample(1.0, SingleVertexValue(1.0, 0.0)));
  // property.source_key_times is default-empty.
  bbsolver::MotionSmoothShapeFlatTopologyGateResult r =
      bbsolver::ValidateMotionSmoothShapeFlatTopology(no_keys);
  Require(!r.ok, "no-source-key-times input must reject");
  Require(Contains(r.fallback_keys.notes,
                   "solve_mode_motion_smooth_skipped: no_source_key_schedule"),
          "no-source-keys fallback must publish no_source_key_schedule note");

  // Edge case: a single source key in-window. MotionSmoothSourceKeyTimes
  // returns one entry; gate requires >=2.
  bbsolver::PropertySamples single_key = MakeProperty(0.0, 1.0);
  single_key.samples.push_back(MakeSample(0.0, SingleVertexValue(0.0, 0.0)));
  single_key.samples.push_back(MakeSample(1.0, SingleVertexValue(1.0, 0.0)));
  single_key.property.source_key_times = {0.5};
  bbsolver::MotionSmoothShapeFlatTopologyGateResult r_single =
      bbsolver::ValidateMotionSmoothShapeFlatTopology(single_key);
  Require(!r_single.ok, "single-source-key input must reject");
  Require(Contains(r_single.fallback_keys.notes, "no_source_key_schedule"),
          "single-source-key fallback must publish no_source_key_schedule");
}

void TestGateAcceptsValidInputAndPopulatesOutputs() {
  // Path 1: success. Valid topology (1 vertex, 8 dims, consistent
  // across samples) + >=2 source key times in window. Verify
  // ok=true and the output fields are populated correctly.
  bbsolver::PropertySamples valid = MakeProperty(0.0, 1.0);
  valid.samples.push_back(MakeSample(0.0, SingleVertexValue(0.0, 0.0)));
  valid.samples.push_back(MakeSample(0.5, SingleVertexValue(0.5, 0.5)));
  valid.samples.push_back(MakeSample(1.0, SingleVertexValue(1.0, 1.0)));
  valid.property.source_key_times = {0.0, 0.5, 1.0};
  bbsolver::MotionSmoothShapeFlatTopologyGateResult r =
      bbsolver::ValidateMotionSmoothShapeFlatTopology(valid);
  Require(r.ok, "valid input must accept");
  Require(r.vertex_count == 1, "vertex_count must reflect value[1]");
  Require(r.dims == 8, "dims must reflect value vector size");
  Require(r.key_times.size() == 3,
          "key_times must carry all three in-window source keys");
  Require(r.fallback_keys.notes.empty(),
          "fallback_keys.notes must be empty on success");
}

void TestGateAcceptsTwoSourceKeyMinimum() {
  // The gate requires >=2 source key times. Two exactly should
  // accept.
  bbsolver::PropertySamples two_keys = MakeProperty(0.0, 1.0);
  two_keys.samples.push_back(MakeSample(0.0, SingleVertexValue(0.0, 0.0)));
  two_keys.samples.push_back(MakeSample(1.0, SingleVertexValue(1.0, 0.0)));
  two_keys.property.source_key_times = {0.25, 0.75};
  bbsolver::MotionSmoothShapeFlatTopologyGateResult r =
      bbsolver::ValidateMotionSmoothShapeFlatTopology(two_keys);
  Require(r.ok, "two source keys must accept (minimum boundary)");
  Require(r.key_times.size() == 2,
          "two source keys must round-trip through MotionSmoothSourceKeyTimes");
}

}  // namespace

int main() {
  TestGateRejectsZeroOrOneSampleAsNoShapeMotionSpan();
  TestGateRejectsInvalidShapeTopology();
  TestGateRejectsVariableShapeTopology();
  TestGateRejectsNoSourceKeySchedule();
  TestGateAcceptsValidInputAndPopulatesOutputs();
  TestGateAcceptsTwoSourceKeyMinimum();
  std::cout << "[PASS] test_motion_smooth_shape_flat_topology_gate\n";
  return 0;
}
