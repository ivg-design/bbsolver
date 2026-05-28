#include "bbsolver/samples/source_key_preservation.hpp"
#include "bbsolver/domain.hpp"

#include <cstddef>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    std::abort();
  }
}

bool EaseEq(const bbsolver::TemporalEase& a,
            const bbsolver::TemporalEase& b) {
  return a.speed == b.speed && a.influence == b.influence;
}

bool EasesEq(const std::vector<bbsolver::TemporalEase>& a,
             const std::vector<bbsolver::TemporalEase>& b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (!EaseEq(a[i], b[i])) {
      return false;
    }
  }
  return true;
}

bool Near(double a, double b) {
  return std::abs(a - b) < 1e-12;
}

bbsolver::Sample MakeSample(double t, std::vector<double> value) {
  bbsolver::Sample sample;
  sample.t_sec = t;
  sample.v = std::move(value);
  return sample;
}

bbsolver::PropertySamples MakeProperty(std::vector<double> times) {
  bbsolver::PropertySamples property;
  property.property.id = "unit/source-key-preservation";
  property.property.dimensions = 2;
  property.property.is_separated = true;
  property.samples.reserve(times.size());
  for (double t: times) {
    property.samples.push_back(MakeSample(t, {t, t + 10.0}));
  }
  return property;
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

void TestSourceKeySampleTimeEpsilonDefaultWhenNoPositiveStep() {
  Require(bbsolver::SourceKeySampleTimeEpsilon(MakeProperty({})) == 1e-6,
          "empty samples must use default epsilon");
  Require(bbsolver::SourceKeySampleTimeEpsilon(MakeProperty({1.0})) == 1e-6,
          "single sample must use default epsilon");
  Require(bbsolver::SourceKeySampleTimeEpsilon(
              MakeProperty({1.0, 1.0, 0.5})) == 1e-6,
          "non-positive adjacent steps must use default epsilon");
}

void TestSourceKeySampleTimeEpsilonUsesSmallestPositiveStep() {
  const bbsolver::PropertySamples property = MakeProperty({0.0, 0.02, 0.07});
  Require(Near(bbsolver::SourceKeySampleTimeEpsilon(property), 0.005),
          "epsilon must be one quarter of the smallest positive step");
}

void TestSourceKeySampleTimeEpsilonKeepsMinimumFloor() {
  const bbsolver::PropertySamples property = MakeProperty({0.0, 1e-7});
  Require(bbsolver::SourceKeySampleTimeEpsilon(property) == 1e-6,
          "epsilon must be floored at 1e-6");
}

void TestFindSampleAtSourceKeyTimeHitsNearestWithinEpsilon() {
  const bbsolver::PropertySamples property = MakeProperty({0.0, 1.0, 2.0});
  const bbsolver::Sample* sample =
      bbsolver::FindSampleAtSourceKeyTime(property, 0.76);
  Require(sample != nullptr, "nearby source key time must find a sample");
  Require(sample->t_sec == 1.0, "nearest sample must be selected");
}

void TestFindSampleAtSourceKeyTimeRejectsOutsideEpsilon() {
  const bbsolver::PropertySamples property = MakeProperty({0.0, 1.0, 2.0});
  const bbsolver::Sample* sample =
      bbsolver::FindSampleAtSourceKeyTime(property, 0.74);
  Require(sample == nullptr, "source key time outside epsilon must reject");
}

void TestBuildPreservationKeysPreservesTimingAndDefaultsMissingTiming() {
  bbsolver::PropertySamples property = MakeProperty({0.0, 1.0, 2.0});
  property.samples[0].key_timing = FullTiming();
  property.samples[2].key_timing = FullTiming();

  int preserved_timing_count = -1;
  bbsolver::PropertyKeys keys =
      bbsolver::BuildShapeFlatSourceKeyPreservationKeys(
          property, {0.0, 1.0, 2.0}, &preserved_timing_count);

  Require(keys.converged, "successful preservation must converge");
  Require(keys.property_id == property.property.id,
          "property id must be copied");
  Require(keys.max_err == 0.0 && keys.max_err_screen_px == 0.0,
          "max errors must start at zero");
  Require(keys.keys.size() == 3, "all source key times must produce keys");
  Require(preserved_timing_count == 2,
          "two samples with key timing must be counted");
  Require(keys.keys[0].interp_in == bbsolver::InterpType::Hold,
          "explicit interp_in must be preserved");
  Require(keys.keys[0].interp_out == bbsolver::InterpType::Bezier,
          "explicit interp_out must be preserved");
  Require(EasesEq(keys.keys[0].temporal_ease_in,
                  property.samples[0].key_timing->temporal_ease_in),
          "explicit temporal ease in must be preserved");
  Require(EasesEq(keys.keys[0].temporal_ease_out,
                  property.samples[0].key_timing->temporal_ease_out),
          "explicit temporal ease out must be preserved");
  Require(keys.keys[0].spatial_in == std::vector<double>({1.0, 2.0}),
          "explicit spatial_in must be preserved");
  Require(keys.keys[0].spatial_out == std::vector<double>({3.0, 4.0}),
          "explicit spatial_out must be preserved");
  Require(keys.keys[0].temporal_continuous,
          "explicit temporal continuous flag must be preserved");
  Require(keys.keys[0].spatial_continuous,
          "explicit spatial continuous flag must be preserved");
  Require(keys.keys[0].temporal_auto_bezier,
          "explicit temporal auto-bezier flag must be preserved");
  Require(keys.keys[0].spatial_auto_bezier,
          "explicit spatial auto-bezier flag must be preserved");
  Require(keys.keys[0].roving, "explicit roving must be preserved");
  Require(keys.keys[1].interp_in == bbsolver::InterpType::Linear,
          "missing timing must default interp_in to linear");
  Require(keys.keys[1].interp_out == bbsolver::InterpType::Linear,
          "missing timing must default interp_out to linear");
  Require(keys.keys[1].temporal_ease_in.size() == 2,
          "missing timing must use separated default in eases");
  Require(keys.keys[1].temporal_ease_out.size() == 2,
          "missing timing must use separated default out eases");
  Require(EaseEq(keys.keys[1].temporal_ease_in[0],
                 bbsolver::TemporalEase{0.0, 33.3}),
          "missing timing default in ease must be neutral");
  Require(EaseEq(keys.keys[2].temporal_ease_out[0],
                 bbsolver::TemporalEase{12.0, 72.0}),
          "explicit temporal ease out must be preserved");
}

void TestBuildPreservationKeysAllowsNullTimingCountOut() {
  bbsolver::PropertySamples property = MakeProperty({0.0, 1.0});
  property.samples[0].key_timing = FullTiming();
  bbsolver::PropertyKeys keys =
      bbsolver::BuildShapeFlatSourceKeyPreservationKeys(
          property, {0.0, 1.0}, nullptr);
  Require(keys.converged, "null timing count output must still converge");
  Require(keys.keys.size() == 2, "null timing count output must not skip keys");
}

void TestBuildPreservationKeysUsesRequestedSourceKeyTime() {
  bbsolver::PropertySamples property = MakeProperty({0.0, 1.0, 2.0});
  int preserved_timing_count = -1;
  bbsolver::PropertyKeys keys =
      bbsolver::BuildShapeFlatSourceKeyPreservationKeys(
          property, {1.24}, &preserved_timing_count);
  Require(keys.converged, "nearby source key time must converge");
  Require(keys.keys.size() == 1, "nearby source key time must produce one key");
  Require(Near(keys.keys[0].t_sec, 1.24),
          "key time must use requested source key time, not sample time");
  Require(keys.keys[0].v == std::vector<double>({1.0, 11.0}),
          "key value must still come from the matched nearest sample");
  Require(preserved_timing_count == 0,
          "missing timing on nearby sample must not increment timing count");
}

void TestBuildPreservationKeysEmptySourceTimesAddsNoSegment() {
  bbsolver::PropertySamples property = MakeProperty({0.0, 1.0});
  int preserved_timing_count = -1;
  bbsolver::PropertyKeys keys =
      bbsolver::BuildShapeFlatSourceKeyPreservationKeys(
          property, {}, &preserved_timing_count);
  Require(keys.converged, "empty source key times must remain converged");
  Require(keys.keys.empty(), "empty source key times must produce no keys");
  Require(keys.segments.empty(),
          "empty source key times must not add a segment");
  Require(preserved_timing_count == 0,
          "empty source key times must report zero preserved timing");
}

void TestBuildPreservationKeysMissClearsKeysAndMarksUnconverged() {
  bbsolver::PropertySamples property = MakeProperty({0.0, 1.0});
  int preserved_timing_count = 99;
  bbsolver::PropertyKeys keys =
      bbsolver::BuildShapeFlatSourceKeyPreservationKeys(
          property, {0.0, 1.4}, &preserved_timing_count);
  Require(!keys.converged, "missed source key time must mark unconverged");
  Require(keys.keys.empty(), "missed source key time must clear keys");
  Require(preserved_timing_count == 99,
          "miss path must preserve output count behavior");
}

void TestBuildPreservationKeysSegmentMetadata() {
  bbsolver::PropertySamples property = MakeProperty({0.0, 1.0, 2.0});
  bbsolver::PropertyKeys keys =
      bbsolver::BuildShapeFlatSourceKeyPreservationKeys(
          property, {0.0, 1.0}, nullptr);
  Require(keys.segments.size() == 1,
          "non-empty preservation must add one segment");
  Require(keys.segments[0].start_idx == 0,
          "segment start must be zero");
  Require(keys.segments[0].end_idx == 2,
          "segment end must cover the source sample range");
  Require(keys.segments[0].reason == "shape_flat_near_optimal_source_keys",
          "segment reason must identify source-key preservation");
}

}  // namespace

int main() {
  TestSourceKeySampleTimeEpsilonDefaultWhenNoPositiveStep();
  TestSourceKeySampleTimeEpsilonUsesSmallestPositiveStep();
  TestSourceKeySampleTimeEpsilonKeepsMinimumFloor();
  TestFindSampleAtSourceKeyTimeHitsNearestWithinEpsilon();
  TestFindSampleAtSourceKeyTimeRejectsOutsideEpsilon();
  TestBuildPreservationKeysPreservesTimingAndDefaultsMissingTiming();
  TestBuildPreservationKeysAllowsNullTimingCountOut();
  TestBuildPreservationKeysUsesRequestedSourceKeyTime();
  TestBuildPreservationKeysEmptySourceTimesAddsNoSegment();
  TestBuildPreservationKeysMissClearsKeysAndMarksUnconverged();
  TestBuildPreservationKeysSegmentMetadata();
  std::cout << "[PASS] test_source_key_preservation\n";
  return 0;
}
