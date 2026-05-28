#include "bbsolver/samples/sample_key_timing.hpp"
#include "bbsolver/domain.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <cstddef>

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

bbsolver::PropertySamples Property(int dimensions, bool separated) {
  bbsolver::PropertySamples samples;
  samples.property.dimensions = dimensions;
  samples.property.is_separated = separated;
  return samples;
}

bbsolver::Key SentinelKey() {
  bbsolver::Key key;
  key.interp_in = bbsolver::InterpType::Hold;
  key.interp_out = bbsolver::InterpType::Linear;
  key.temporal_ease_in = {{1.0, 2.0}};
  key.temporal_ease_out = {{3.0, 4.0}};
  key.spatial_in = {5.0, 6.0};
  key.spatial_out = {7.0, 8.0};
  key.temporal_continuous = true;
  key.spatial_continuous = true;
  key.temporal_auto_bezier = true;
  key.spatial_auto_bezier = true;
  key.roving = true;
  return key;
}

bool KeysEqual(const bbsolver::Key& a, const bbsolver::Key& b) {
  return a.interp_in == b.interp_in &&
         a.interp_out == b.interp_out &&
         EasesEq(a.temporal_ease_in, b.temporal_ease_in) &&
         EasesEq(a.temporal_ease_out, b.temporal_ease_out) &&
         a.spatial_in == b.spatial_in &&
         a.spatial_out == b.spatial_out &&
         a.temporal_continuous == b.temporal_continuous &&
         a.spatial_continuous == b.spatial_continuous &&
         a.temporal_auto_bezier == b.temporal_auto_bezier &&
         a.spatial_auto_bezier == b.spatial_auto_bezier &&
         a.roving == b.roving;
}

void TestDefaultEasesNonSeparatedUsesOneEase() {
  const auto eases = bbsolver::DefaultEasesForProperty(Property(4, false));
  Require(eases.size() == 1, "non-separated property must use one ease");
  Require(EaseEq(eases[0], bbsolver::TemporalEase{0.0, 33.3}),
          "non-separated default ease must be neutral");
}

void TestDefaultEasesSeparatedUsesDimensions() {
  const auto eases = bbsolver::DefaultEasesForProperty(Property(3, true));
  Require(eases.size() == 3, "separated property must use dimensions eases");
  for (const bbsolver::TemporalEase& ease: eases) {
    Require(EaseEq(ease, bbsolver::TemporalEase{0.0, 33.3}),
            "separated default ease must be neutral");
  }
}

void TestDefaultEasesSeparatedClampsDimensionFloor() {
  const auto eases = bbsolver::DefaultEasesForProperty(Property(0, true));
  Require(eases.size() == 1, "separated dimensions must clamp to at least one");
  Require(EaseEq(eases[0], bbsolver::TemporalEase{0.0, 33.3}),
          "dimension-floor ease must be neutral");
}

void TestAbsentTimingDoesNotMutateKey() {
  const bbsolver::PropertySamples property = Property(2, true);
  bbsolver::Sample sample;
  bbsolver::Key key = SentinelKey();
  const bbsolver::Key before = key;
  Require(!bbsolver::ApplySampleKeyTiming(property, sample, key),
          "missing key_timing must return false");
  Require(KeysEqual(key, before), "missing key_timing must not mutate key");
}

void TestEmptyEasesFallBackToDefaults() {
  const bbsolver::PropertySamples property = Property(2, true);
  bbsolver::Sample sample;
  bbsolver::KeyTiming timing;
  timing.interp_in = bbsolver::InterpType::Hold;
  timing.interp_out = bbsolver::InterpType::Linear;
  sample.key_timing = timing;

  bbsolver::Key key;
  Require(bbsolver::ApplySampleKeyTiming(property, sample, key),
          "present key_timing must return true");
  Require(key.temporal_ease_in.size() == 2,
          "empty in-ease timing must fall back to separated defaults");
  Require(key.temporal_ease_out.size() == 2,
          "empty out-ease timing must fall back to separated defaults");
  Require(EaseEq(key.temporal_ease_in[0], bbsolver::TemporalEase{0.0, 33.3}),
          "fallback in ease must be neutral");
  Require(EaseEq(key.temporal_ease_out[1], bbsolver::TemporalEase{0.0, 33.3}),
          "fallback out ease must be neutral");
}

void TestExplicitEasesArePreserved() {
  const bbsolver::PropertySamples property = Property(3, true);
  bbsolver::Sample sample;
  bbsolver::KeyTiming timing;
  timing.temporal_ease_in = {{9.0, 64.0}};
  timing.temporal_ease_out = {{12.0, 72.0}, {13.0, 73.0}};
  sample.key_timing = timing;

  bbsolver::Key key;
  Require(bbsolver::ApplySampleKeyTiming(property, sample, key),
          "present explicit key_timing must return true");
  Require(EasesEq(key.temporal_ease_in, timing.temporal_ease_in),
          "explicit in eases must be preserved exactly");
  Require(EasesEq(key.temporal_ease_out, timing.temporal_ease_out),
          "explicit out eases must be preserved exactly");
}

void TestCopiesTimingFields() {
  const bbsolver::PropertySamples property = Property(2, false);
  bbsolver::Sample sample;
  bbsolver::KeyTiming timing;
  timing.interp_in = bbsolver::InterpType::Hold;
  timing.interp_out = bbsolver::InterpType::Linear;
  timing.temporal_ease_in = {{5.0, 44.0}};
  timing.temporal_ease_out = {{6.0, 55.0}};
  timing.spatial_in = {1.0, 2.0};
  timing.spatial_out = {3.0, 4.0};
  timing.temporal_continuous = true;
  timing.spatial_continuous = true;
  timing.temporal_auto_bezier = true;
  timing.spatial_auto_bezier = true;
  timing.roving = true;
  sample.key_timing = timing;

  bbsolver::Key key;
  Require(bbsolver::ApplySampleKeyTiming(property, sample, key),
          "present complete key_timing must return true");
  Require(key.interp_in == timing.interp_in, "interp_in must copy");
  Require(key.interp_out == timing.interp_out, "interp_out must copy");
  Require(EasesEq(key.temporal_ease_in, timing.temporal_ease_in),
          "temporal_ease_in must copy");
  Require(EasesEq(key.temporal_ease_out, timing.temporal_ease_out),
          "temporal_ease_out must copy");
  Require(key.spatial_in == timing.spatial_in, "spatial_in must copy");
  Require(key.spatial_out == timing.spatial_out, "spatial_out must copy");
  Require(key.temporal_continuous == timing.temporal_continuous,
          "temporal_continuous must copy");
  Require(key.spatial_continuous == timing.spatial_continuous,
          "spatial_continuous must copy");
  Require(key.temporal_auto_bezier == timing.temporal_auto_bezier,
          "temporal_auto_bezier must copy");
  Require(key.spatial_auto_bezier == timing.spatial_auto_bezier,
          "spatial_auto_bezier must copy");
  Require(key.roving == timing.roving, "roving must copy");
}

}  // namespace

int main() {
  TestDefaultEasesNonSeparatedUsesOneEase();
  TestDefaultEasesSeparatedUsesDimensions();
  TestDefaultEasesSeparatedClampsDimensionFloor();
  TestAbsentTimingDoesNotMutateKey();
  TestEmptyEasesFallBackToDefaults();
  TestExplicitEasesArePreserved();
  TestCopiesTimingFields();
  std::cout << "[PASS] test_sample_key_timing\n";
  return 0;
}
