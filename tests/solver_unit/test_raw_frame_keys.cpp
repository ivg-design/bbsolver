#include "bbsolver/samples/raw_frame_keys.hpp"
#include "bbsolver/domain.hpp"

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "bbsolver/shape/shape_flat_topology.hpp"

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    std::abort();
  }
}

bool HasNote(const bbsolver::PropertyKeys& keys, const std::string& needle) {
  return keys.notes.find(needle) != std::string::npos;
}

bbsolver::KeyTiming FullTiming() {
  bbsolver::KeyTiming timing;
  timing.interp_in = bbsolver::InterpType::Hold;
  timing.interp_out = bbsolver::InterpType::Bezier;
  timing.temporal_ease_in = {{9.0, 64.0}};
  timing.temporal_ease_out = {{12.0, 72.0}};
  timing.spatial_in = {1.0, 2.0};
  timing.spatial_out = {3.0, 4.0};
  timing.temporal_continuous = true;
  timing.spatial_continuous = true;
  timing.temporal_auto_bezier = true;
  timing.spatial_auto_bezier = true;
  timing.roving = true;
  return timing;
}

bbsolver::Sample MakeSample(double t, std::vector<double> value) {
  bbsolver::Sample sample;
  sample.t_sec = t;
  sample.v = std::move(value);
  return sample;
}

bbsolver::PropertySamples MakeProperty() {
  bbsolver::PropertySamples property;
  property.property.id = "unit/raw-frame-keys";
  property.property.dimensions = 2;
  property.property.is_separated = true;
  return property;
}

std::vector<double> ShapeValue(int vertices) {
  std::vector<bbsolver::ShapeFlatVertex> shape_vertices;
  shape_vertices.reserve(static_cast<std::size_t>(vertices));
  for (int idx = 0; idx < vertices; ++idx) {
    const double x = static_cast<double>(idx);
    shape_vertices.push_back({x, x + 1.0, x - 0.25, x + 0.75, x + 0.25, x + 1.25});
  }
  return bbsolver::ShapeFlatFromVertices(shape_vertices, false);
}

void TestRawFrameFallbackBuildsOneKeyPerSample() {
  bbsolver::PropertySamples property = MakeProperty();
  property.samples.push_back(MakeSample(0.0, {1.0, 2.0}));
  property.samples.push_back(MakeSample(1.0, {}));
  property.samples.front().key_timing = FullTiming();

  const bbsolver::PropertyKeys keys =
      bbsolver::RawFrameKeyFallback(property, "unit_raw_fallback");

  Require(keys.converged, "raw fallback must converge");
  Require(keys.property_id == property.property.id,
          "property id must be copied");
  Require(keys.keys.size() == 2, "one key per sample must be emitted");
  Require(keys.keys[0].interp_in == bbsolver::InterpType::Hold,
          "explicit timing must be preserved");
  Require(keys.keys[1].v == std::vector<double>({0.0, 0.0}),
          "empty sample vector must fall back to zero dimensions");
  Require(keys.keys[1].temporal_ease_in.size() == 2,
          "missing timing must use separated default eases");
  Require(keys.segments.size() == 1 &&
              keys.segments[0].reason == "raw_frame_keys",
          "raw fallback must add a covering segment");
  Require(HasNote(keys, "unit_raw_fallback"),
          "caller note must be retained");
  Require(HasNote(keys, "raw_frame_keys=2"),
          "raw key count note must be emitted");
  Require(HasNote(keys, "source_key_timing_preserved_partial=1/2"),
          "partial timing preservation must be annotated");
}

void TestShapeFlatFallbackSkipsMalformedAndMarksUnconverged() {
  bbsolver::PropertySamples property = MakeProperty();
  property.samples.push_back(MakeSample(0.0, ShapeValue(2)));
  property.samples.push_back(MakeSample(0.5, {0.0, 3.0}));
  property.samples.push_back(MakeSample(1.0, ShapeValue(2)));

  const bbsolver::PropertyKeys keys =
      bbsolver::ShapeFlatFrameKeyFallback(property, "unit_shape_fallback");

  Require(!keys.converged,
          "malformed shape frame must mark fallback unconverged");
  Require(keys.keys.size() == 2, "malformed frame must be skipped");
  Require(keys.segments.size() == 1 &&
              keys.segments[0].reason == "shape_flat_raw_frame_keys",
          "shape fallback must add a covering segment");
  Require(HasNote(keys, "raw_frame_keys=2"),
          "valid raw shape key count must be annotated");
  Require(HasNote(keys, "skipped_malformed=1"),
          "malformed skip count must be annotated");
}

void TestShapeFlatFallbackNotesVariableTopology() {
  bbsolver::PropertySamples property = MakeProperty();
  property.samples.push_back(MakeSample(0.0, ShapeValue(2)));
  property.samples.push_back(MakeSample(1.0, ShapeValue(3)));

  const bbsolver::PropertyKeys keys =
      bbsolver::ShapeFlatFrameKeyFallback(property, "unit_shape_fallback");

  Require(keys.converged,
          "variable topology without malformed frames remains converged");
  Require(keys.keys.size() == 2, "all valid frames must become keys");
  Require(HasNote(keys, "variable_topology=true"),
          "variable topology must be annotated");
  Require(HasNote(keys, "source_key_timing_missing=true"),
          "missing timing must be annotated");
}

}  // namespace

int main() {
  TestRawFrameFallbackBuildsOneKeyPerSample();
  TestShapeFlatFallbackSkipsMalformedAndMarksUnconverged();
  TestShapeFlatFallbackNotesVariableTopology();
  std::cout << "[PASS] test_raw_frame_keys\n";
  return 0;
}
