#include "bbsolver/io/sample_json_timing_io.hpp"
#include "bbsolver/domain.hpp"

#include <cassert>
#include <cmath>  // IWYU pragma: keep
#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>  // IWYU pragma: keep

namespace {

using nlohmann::json;

void TestKeyTimingFieldsDetection() {
  assert(!bbsolver::HasSampleKeyTimingJsonFields(json::object()));
  assert(bbsolver::HasSampleKeyTimingJsonFields({{"interp_in", "Hold"}}));
  assert(bbsolver::HasSampleKeyTimingJsonFields(
      {{"temporal_auto_bezier", true}}));
  assert(bbsolver::HasSampleKeyTimingJsonFields({{"roving", true}}));
}

void TestParseKeyTimingObject() {
  const json obj = {
      {"interp_in", "Hold"},
      {"interp_out", "Linear"},
      {"temporal_ease_in",
       json::array({{{"speed", 12.0}, {"influence", 45.0}}})},
      {"temporal_ease_out",
       json::array({{{"speed", 13.0}, {"influence", 55.0}}})},
      {"spatial_in", json::array({1.0, 2.0})},
      {"spatial_out", json::array({3.0, 4.0})},
      {"temporal_continuous", true},
      {"spatial_continuous", true},
      {"temporal_auto_bezier", true},
      {"spatial_auto_bezier", true},
      {"roving", true},
  };
  const bbsolver::KeyTiming timing =
      bbsolver::ParseSampleKeyTimingJson(obj);

  assert(timing.interp_in == bbsolver::InterpType::Hold);
  assert(timing.interp_out == bbsolver::InterpType::Linear);
  assert(timing.temporal_ease_in.size() == 1);
  assert(std::abs(timing.temporal_ease_in.front().speed - 12.0) < 1e-12);
  assert(std::abs(timing.temporal_ease_in.front().influence - 45.0) < 1e-12);
  assert(timing.temporal_ease_out.size() == 1);
  assert(std::abs(timing.temporal_ease_out.front().speed - 13.0) < 1e-12);
  assert(std::abs(timing.temporal_ease_out.front().influence - 55.0) < 1e-12);
  assert(timing.spatial_in == std::vector<double>({1.0, 2.0}));
  assert(timing.spatial_out == std::vector<double>({3.0, 4.0}));
  assert(timing.temporal_continuous);
  assert(timing.spatial_continuous);
  assert(timing.temporal_auto_bezier);
  assert(timing.spatial_auto_bezier);
  assert(timing.roving);
}

void TestParseSampleUsesNestedKeyTimingBeforeRootLegacyFields() {
  const json sample_json = {
      {"t_sec", 1.0},
      {"v", json::array({10.0})},
      {"interp_in", "Hold"},
      {"key_timing",
       {{"interp_in", "Linear"},
        {"interp_out", "Hold"},
        {"temporal_continuous", true}}},
  };
  const bbsolver::Sample sample = bbsolver::ParseSampleJson(sample_json);

  assert(std::abs(sample.t_sec - 1.0) < 1e-12);
  assert(sample.v == std::vector<double>({10.0}));
  assert(sample.key_timing.has_value());
  assert(sample.key_timing->interp_in == bbsolver::InterpType::Linear);
  assert(sample.key_timing->interp_out == bbsolver::InterpType::Hold);
  assert(sample.key_timing->temporal_continuous);
}

void TestParseSampleUsesLegacyRootTimingFields() {
  const json sample_json = {
      {"t_sec", 2.0},
      {"v", json::array({20.0})},
      {"interp_out", "Linear"},
      {"roving", true},
  };
  const bbsolver::Sample sample = bbsolver::ParseSampleJson(sample_json);

  assert(sample.key_timing.has_value());
  assert(sample.key_timing->interp_in == bbsolver::InterpType::Bezier);
  assert(sample.key_timing->interp_out == bbsolver::InterpType::Linear);
  assert(sample.key_timing->roving);
}

void TestParseSampleWithoutTimingFieldsHasNoTiming() {
  const bbsolver::Sample sample =
      bbsolver::ParseSampleJson({{"t_sec", 3.0}, {"v", json::array({30.0})}});
  assert(!sample.key_timing.has_value());
}

void TestTemporalEaseNullFieldsUseDefaults() {
  const bbsolver::KeyTiming timing =
      bbsolver::ParseSampleKeyTimingJson({
          {"temporal_ease_in",
           json::array({{{"speed", nullptr}, {"influence", nullptr}}})},
          {"temporal_ease_out", nullptr},
      });

  assert(timing.temporal_ease_in.size() == 1);
  assert(std::abs(timing.temporal_ease_in.front().speed - 0.0) < 1e-12);
  assert(std::abs(timing.temporal_ease_in.front().influence - 33.3) < 1e-12);
  assert(timing.temporal_ease_out.empty());
}

void TestMalformedInterpTypeThrows() {
  bool threw = false;
  try {
    (void)bbsolver::ParseSampleKeyTimingJson({{"interp_in", "Snap"}});
  } catch (const std::runtime_error& err) {
    threw = std::string(err.what()) == "Unknown InterpType: Snap";
  }
  assert(threw);
}

}  // namespace

int main() {
  TestKeyTimingFieldsDetection();
  TestParseKeyTimingObject();
  TestParseSampleUsesNestedKeyTimingBeforeRootLegacyFields();
  TestParseSampleUsesLegacyRootTimingFields();
  TestParseSampleWithoutTimingFieldsHasNoTiming();
  TestTemporalEaseNullFieldsUseDefaults();
  TestMalformedInterpTypeThrows();
  return 0;
}
