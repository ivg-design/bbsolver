#include "bbsolver/io/key_bundle_io.hpp"
#include "bbsolver/domain.hpp"

#include <cassert>
#include <string>
#include <vector>
#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>

namespace {

using nlohmann::json;

void TestParseKeyBundlePreservesNestedTimingAndFallbackTotalKeys() {
  const json root = {
      {"schema_version", 3},
      {"request_id", "request-123"},
      {"solver_version", "unit-version"},
      {"solver_build", "unit-build"},
      {"solve_time_ms", 12.5},
      {"total_keys", 0},
      {"total_samples_input", 99},
      {"property_results",
       json::array(
           {{
               {"property_id", "prop/a"},
               {"max_err", 0.25},
               {"max_err_screen_px", 1.5},
               {"converged", false},
               {"notes", "unit-note"},
               {"keys",
                json::array(
                    {{
                        {"t_sec", 1.25},
                        {"v", json::array({10.0, 20.0})},
                        {"interp_in", "Hold"},
                        {"interp_out", "Linear"},
                        {"temporal_ease_in",
                         json::array({{{"speed", 2.0},
                                       {"influence", 45.0}}})},
                        {"temporal_ease_out",
                         json::array({{{"speed", 3.0},
                                       {"influence", 55.0}}})},
                        {"spatial_in", json::array({1.0, 2.0})},
                        {"spatial_out", json::array({3.0, 4.0})},
                        {"temporal_continuous", true},
                        {"spatial_continuous", true},
                        {"temporal_auto_bezier", true},
                        {"spatial_auto_bezier", true},
                        {"roving", true},
                    }})},
               {"segments",
                json::array(
                    {{
                        {"start_idx", 2},
                        {"end_idx", 5},
                        {"max_err", 0.125},
                        {"max_err_screen_px", 0.5},
                        {"rms_err", 0.0625},
                        {"iters", 7},
                        {"reason", "unit-segment"},
                    }})},
           }})},
  };

  const bbsolver::KeyBundle bundle = bbsolver::ParseKeyBundleJson(root);

  assert(bundle.schema_version == 3);
  assert(bundle.request_id == "request-123");
  assert(bundle.solver_version == "unit-version");
  assert(bundle.solver_build == "unit-build");
  assert(std::abs(bundle.solve_time_ms - 12.5) < 1e-12);
  assert(bundle.total_keys == 1);
  assert(bundle.total_samples_input == 99);
  assert(bundle.property_results.size() == 1);

  const bbsolver::PropertyKeys& property = bundle.property_results.front();
  assert(property.property_id == "prop/a");
  assert(std::abs(property.max_err - 0.25) < 1e-12);
  assert(std::abs(property.max_err_screen_px - 1.5) < 1e-12);
  assert(property.converged == false);
  assert(property.notes == "unit-note");
  assert(property.keys.size() == 1);
  assert(property.segments.size() == 1);

  const bbsolver::Key& key = property.keys.front();
  assert(std::abs(key.t_sec - 1.25) < 1e-12);
  assert(key.v == std::vector<double>({10.0, 20.0}));
  assert(key.interp_in == bbsolver::InterpType::Hold);
  assert(key.interp_out == bbsolver::InterpType::Linear);
  assert(key.temporal_ease_in.size() == 1);
  assert(std::abs(key.temporal_ease_in.front().speed - 2.0) < 1e-12);
  assert(std::abs(key.temporal_ease_in.front().influence - 45.0) < 1e-12);
  assert(key.temporal_ease_out.size() == 1);
  assert(std::abs(key.temporal_ease_out.front().speed - 3.0) < 1e-12);
  assert(std::abs(key.temporal_ease_out.front().influence - 55.0) < 1e-12);
  assert(key.spatial_in == std::vector<double>({1.0, 2.0}));
  assert(key.spatial_out == std::vector<double>({3.0, 4.0}));
  assert(key.temporal_continuous);
  assert(key.spatial_continuous);
  assert(key.temporal_auto_bezier);
  assert(key.spatial_auto_bezier);
  assert(key.roving);

  const bbsolver::SegmentReport& segment = property.segments.front();
  assert(segment.start_idx == 2);
  assert(segment.end_idx == 5);
  assert(std::abs(segment.max_err - 0.125) < 1e-12);
  assert(std::abs(segment.max_err_screen_px - 0.5) < 1e-12);
  assert(std::abs(segment.rms_err - 0.0625) < 1e-12);
  assert(segment.iters == 7);
  assert(segment.reason == "unit-segment");
}

void TestBuildKeyBundleJsonPreservesWrittenShape() {
  bbsolver::Key key;
  key.t_sec = 2.0;
  key.v = {1.0, 2.0, 3.0};
  key.interp_in = bbsolver::InterpType::Linear;
  key.interp_out = bbsolver::InterpType::Hold;
  key.temporal_ease_in.push_back({4.0, 40.0});
  key.temporal_ease_out.push_back({5.0, 50.0});
  key.spatial_in = {-1.0, -2.0};
  key.spatial_out = {1.0, 2.0};
  key.temporal_continuous = true;
  key.spatial_auto_bezier = true;

  bbsolver::SegmentReport segment;
  segment.start_idx = 0;
  segment.end_idx = 1;
  segment.max_err = 0.1;
  segment.max_err_screen_px = 0.2;
  segment.rms_err = 0.05;
  segment.iters = 3;
  segment.reason = "write-test";

  bbsolver::PropertyKeys property;
  property.property_id = "prop/write";
  property.keys.push_back(key);
  property.max_err = 0.1;
  property.max_err_screen_px = 0.2;
  property.segments.push_back(segment);
  property.converged = true;
  property.notes = "written";

  bbsolver::KeyBundle bundle;
  bundle.schema_version = 4;
  bundle.request_id = "write-request";
  bundle.property_results.push_back(property);
  bundle.solver_version = "write-version";
  bundle.solver_build = "write-build";
  bundle.solve_time_ms = 1.25;
  bundle.total_keys = 9;
  bundle.total_samples_input = 10;

  const json root = bbsolver::BuildKeyBundleJson(bundle);

  assert(root.at("_schema").get<std::string>() == "keys");
  assert(root.at("schema_version").get<int>() == 4);
  assert(root.at("request_id").get<std::string>() == "write-request");
  assert(root.at("solver_version").get<std::string>() == "write-version");
  assert(root.at("solver_build").get<std::string>() == "write-build");
  assert(std::abs(root.at("solve_time_ms").get<double>() - 1.25) < 1e-12);
  assert(root.at("total_keys").get<int>() == 9);
  assert(root.at("total_samples_input").get<int>() == 10);

  const json key_json =
      root.at("property_results").at(0).at("keys").at(0);
  assert(key_json.at("interp_in").get<std::string>() == "Linear");
  assert(key_json.at("interp_out").get<std::string>() == "Hold");
  assert(key_json.at("temporal_ease_in").at(0).at("speed").get<double>() ==
         4.0);
  assert(key_json.at("temporal_ease_out").at(0).at("influence").get<double>() ==
         50.0);
  assert(key_json.at("temporal_continuous").get<bool>() == true);
  assert(key_json.at("spatial_auto_bezier").get<bool>() == true);

  const bbsolver::KeyBundle parsed = bbsolver::ParseKeyBundleJson(root);
  assert(parsed.total_keys == 9);
  assert(parsed.property_results.front().keys.front().interp_in ==
         bbsolver::InterpType::Linear);
  assert(parsed.property_results.front().keys.front().interp_out ==
         bbsolver::InterpType::Hold);
}

void TestMissingNestedArraysAndTimingDefaultToBezier() {
  const json root = {
      {"property_results",
       json::array(
           {{
               {"property_id", "prop/defaults"},
               {"keys", json::array({{{"t_sec", 0.0}, {"v", json::array()}}})},
           }})},
  };

  const bbsolver::KeyBundle bundle = bbsolver::ParseKeyBundleJson(root);
  assert(bundle.total_keys == 1);
  assert(bundle.property_results.size() == 1);
  assert(bundle.property_results.front().converged == true);
  assert(bundle.property_results.front().segments.empty());
  assert(bundle.property_results.front().keys.front().interp_in ==
         bbsolver::InterpType::Bezier);
  assert(bundle.property_results.front().keys.front().interp_out ==
         bbsolver::InterpType::Bezier);
}

}  // namespace

int main() {
  TestParseKeyBundlePreservesNestedTimingAndFallbackTotalKeys();
  TestBuildKeyBundleJsonPreservesWrittenShape();
  TestMissingNestedArraysAndTimingDefaultToBezier();
  return 0;
}
