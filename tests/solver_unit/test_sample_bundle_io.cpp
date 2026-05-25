#include "bbsolver/io/sample_bundle_io.hpp"
#include "bbsolver/domain.hpp"

#include <cassert>
#include <cmath>  // IWYU pragma: keep
#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>
#include <vector>

namespace {

using nlohmann::json;

json MinimalCompJson() {
  return {
      {"fps", 24.0},
      {"duration_sec", 1.0},
      {"width", 1280},
      {"height", 720},
  };
}

json MinimalPropertyJson() {
  return {
      {"property",
       {{"id", "prop"},
        {"kind", "Scalar"},
        {"dimensions", 1}}},
      {"samples",
       json::array({
           {{"t_sec", 0.0}, {"v", json::array({1.0})}},
           {{"t_sec", 0.5}, {"v", json::array({2.0})}},
      })},
  };
}

json RichSampleBundleJson() {
  return {
      {"schema_version", 11},
      {"request_id", "rich-sample-request"},
      {"unknown_top_level", "ignored"},
      {"comp",
       {{"fps", 29.97},
        {"duration_sec", 4.5},
        {"width", 1920},
        {"height", 1080},
        {"pixel_aspect", 1.25},
        {"shutter_angle_deg", 144.0},
        {"shutter_phase_deg", -72.0},
        {"motion_blur_enabled", true},
        {"work_area_start_sec", 0.5},
        {"work_area_end_sec", 3.75},
        {"unknown_comp_field", true}}},
      {"properties",
       json::array({
           json{{"property",
                 {{"id", "layer/shape/path"},
                  {"match_name", "ADBE Vector Shape"},
                  {"display_name", "Path 1"},
                  {"layer_path", "Layer/Contents/Shape"},
                  {"kind", "ThreeD_Spatial"},
                  {"dimensions", 3},
                  {"is_spatial", true},
                  {"is_separated", false},
                  {"units_label", "px"},
                  {"min_value", json::array({-10.0, -20.0, -30.0})},
                  {"max_value", json::array({10.0, 20.0, 30.0})},
                  {"source_key_times", json::array({0.0, 1.0, 2.0})},
                  {"unknown_property_field", "ignored"}}},
                {"t_start_sec", 0.5},
                {"t_end_sec", 2.5},
                {"samples_per_frame", 3},
                {"samples",
                 json::array({
                     json{{"t_sec", 0.5},
                          {"v", json::array({1.0, 2.0, 3.0})},
                          {"key_timing",
                           {{"temporal_ease_in",
                             json::array({json::object()})},
                            {"roving", true},
                            {"unknown_timing_field", "ignored"}}}},
                     json{{"t_sec", 1.5},
                          {"v", json::array({4.0, 5.0, 6.0})},
                          {"interp_out", "Linear"}},
                 })},
                {"layer_xform_at_start",
                 {{"anchor_point", json::array({10.0, 20.0, 30.0})},
                  {"position", json::array({100.0, 200.0, 300.0})},
                  {"scale", json::array({95.0, 105.0, 100.0})},
                  {"rotation", json::array({15.0, 30.0, 45.0})},
                  {"opacity", 80.0}}},
                {"hash_of_expression", "expr-hash"},
                {"unknown_property_samples_field", 42}},
       })},
      {"config", {{"tolerance", 0.125}, {"parallel_jobs", 4}}},
  };
}

void TestParseSampleBundleTopLevelAndConfigDelegation() {
  const json root = {
      {"schema_version", 7},
      {"request_id", "sample-request"},
      {"comp", MinimalCompJson()},
      {"properties", json::array({MinimalPropertyJson()})},
      {"config",
       {{"tolerance", 0.25},
        {"allow_path_replacement_fit", true},
        {"parallel_jobs", 6}}},
  };

  const bbsolver::SampleBundle bundle = bbsolver::ParseSampleBundleJson(root);

  assert(bundle.schema_version == 7);
  assert(bundle.request_id == "sample-request");
  assert(std::abs(bundle.comp.fps - 24.0) < 1e-12);
  assert(bundle.comp.width == 1280);
  assert(bundle.properties.size() == 1);
  assert(bundle.properties.front().property.id == "prop");
  assert(bundle.properties.front().samples.size() == 2);
  assert(std::abs(bundle.config.tolerance - 0.25) < 1e-12);
  assert(bundle.config.allow_path_replacement_fit);
  assert(bundle.config.parallel_jobs == 6);
}

void TestMissingPropertiesArrayIsEmpty() {
  const bbsolver::SampleBundle bundle = bbsolver::ParseSampleBundleJson({
      {"comp", MinimalCompJson()},
      {"properties", json{}},
  });
  assert(bundle.properties.empty());
}

void TestNullConfigKeepsDefaults() {
  const bbsolver::SampleBundle bundle = bbsolver::ParseSampleBundleJson({
      {"comp", MinimalCompJson()},
      {"properties", json::array()},
      {"config", json{}},
  });
  assert(bundle.config.tolerance == bbsolver::SolverConfig{}.tolerance);
  assert(!bundle.config.allow_path_replacement_fit);
}

void TestRichSampleBundleBuildParseContract() {
  const bbsolver::SampleBundle bundle =
      bbsolver::ParseSampleBundleJson(RichSampleBundleJson());

  assert(bundle.schema_version == 11);
  assert(bundle.request_id == "rich-sample-request");
  assert(std::abs(bundle.comp.fps - 29.97) < 1e-12);
  assert(std::abs(bundle.comp.duration_sec - 4.5) < 1e-12);
  assert(bundle.comp.width == 1920);
  assert(bundle.comp.height == 1080);
  assert(std::abs(bundle.comp.pixel_aspect - 1.25) < 1e-12);
  assert(std::abs(bundle.comp.shutter_angle_deg - 144.0) < 1e-12);
  assert(std::abs(bundle.comp.shutter_phase_deg + 72.0) < 1e-12);
  assert(bundle.comp.motion_blur_enabled);
  assert(std::abs(bundle.comp.work_area_start_sec - 0.5) < 1e-12);
  assert(std::abs(bundle.comp.work_area_end_sec - 3.75) < 1e-12);

  assert(bundle.properties.size() == 1);
  const bbsolver::PropertySamples& property_samples = bundle.properties.front();
  assert(property_samples.property.id == "layer/shape/path");
  assert(property_samples.property.match_name == "ADBE Vector Shape");
  assert(property_samples.property.display_name == "Path 1");
  assert(property_samples.property.layer_path == "Layer/Contents/Shape");
  assert(property_samples.property.kind == bbsolver::ValueKind::ThreeD_Spatial);
  assert(property_samples.property.dimensions == 3);
  assert(property_samples.property.is_spatial);
  assert(!property_samples.property.is_separated);
  assert(property_samples.property.units_label == "px");
  assert(property_samples.property.min_value ==
         std::vector<double>({-10.0, -20.0, -30.0}));
  assert(property_samples.property.max_value ==
         std::vector<double>({10.0, 20.0, 30.0}));
  assert(property_samples.property.source_key_times ==
         std::vector<double>({0.0, 1.0, 2.0}));
  assert(std::abs(property_samples.t_start_sec - 0.5) < 1e-12);
  assert(std::abs(property_samples.t_end_sec - 2.5) < 1e-12);
  assert(property_samples.samples_per_frame == 3);
  assert(property_samples.hash_of_expression == "expr-hash");

  assert(property_samples.layer_xform_at_start.has_value());
  assert(property_samples.layer_xform_at_start->anchor_point ==
         std::vector<double>({10.0, 20.0, 30.0}));
  assert(property_samples.layer_xform_at_start->position ==
         std::vector<double>({100.0, 200.0, 300.0}));
  assert(property_samples.layer_xform_at_start->scale ==
         std::vector<double>({95.0, 105.0, 100.0}));
  assert(property_samples.layer_xform_at_start->rotation ==
         std::vector<double>({15.0, 30.0, 45.0}));
  assert(std::abs(property_samples.layer_xform_at_start->opacity - 80.0) <
         1e-12);

  assert(property_samples.samples.size() == 2);
  assert(property_samples.samples.front().v ==
         std::vector<double>({1.0, 2.0, 3.0}));
  assert(property_samples.samples.front().key_timing.has_value());
  const bbsolver::KeyTiming& nested_timing =
      *property_samples.samples.front().key_timing;
  assert(nested_timing.interp_in == bbsolver::InterpType::Bezier);
  assert(nested_timing.interp_out == bbsolver::InterpType::Bezier);
  assert(nested_timing.temporal_ease_in.size() == 1);
  assert(std::abs(nested_timing.temporal_ease_in.front().speed - 0.0) <
         1e-12);
  assert(std::abs(nested_timing.temporal_ease_in.front().influence - 33.3) <
         1e-12);
  assert(nested_timing.roving);

  assert(property_samples.samples.back().v ==
         std::vector<double>({4.0, 5.0, 6.0}));
  assert(property_samples.samples.back().key_timing.has_value());
  assert(property_samples.samples.back().key_timing->interp_in ==
         bbsolver::InterpType::Bezier);
  assert(property_samples.samples.back().key_timing->interp_out ==
         bbsolver::InterpType::Linear);

  assert(std::abs(bundle.config.tolerance - 0.125) < 1e-12);
  assert(bundle.config.parallel_jobs == 4);
}

void TestSampleBundleUnknownAndNullDefaultsContract() {
  const bbsolver::SampleBundle bundle = bbsolver::ParseSampleBundleJson({
      {"schema_version", nullptr},
      {"request_id", nullptr},
      {"comp",
       {{"fps", 30.0},
        {"duration_sec", nullptr},
        {"width", nullptr},
        {"height", 600},
        {"pixel_aspect", nullptr},
        {"unknown_comp_field", "ignored"}}},
      {"properties",
       json::array({
           json{{"property",
                 {{"id", "defaults"},
                  {"kind", nullptr},
                  {"dimensions", nullptr},
                  {"is_spatial", nullptr},
                  {"units_label", nullptr},
                  {"unknown_property_field", "ignored"}}},
                {"samples_per_frame", nullptr},
                {"samples",
                 json::array({
                     json{{"t_sec", nullptr},
                          {"v", nullptr},
                          {"key_timing",
                           {{"interp_in", nullptr},
                            {"temporal_ease_in", nullptr},
                            {"spatial_in", nullptr}}}},
                 })},
                {"layer_xform_at_start", nullptr},
                {"hash_of_expression", nullptr}},
       })},
      {"config", nullptr},
  });

  assert(bundle.schema_version == bbsolver::SampleBundle{}.schema_version);
  assert(bundle.request_id.empty());
  assert(std::abs(bundle.comp.fps - 30.0) < 1e-12);
  assert(std::abs(bundle.comp.duration_sec - 0.0) < 1e-12);
  assert(bundle.comp.width == 0);
  assert(bundle.comp.height == 600);
  assert(std::abs(bundle.comp.pixel_aspect - 1.0) < 1e-12);
  assert(bundle.properties.size() == 1);

  const bbsolver::PropertySamples& property_samples = bundle.properties.front();
  assert(property_samples.property.id == "defaults");
  assert(property_samples.property.kind == bbsolver::ValueKind::Scalar);
  assert(property_samples.property.dimensions == 1);
  assert(!property_samples.property.is_spatial);
  assert(property_samples.property.units_label.empty());
  assert(property_samples.samples_per_frame == 1);
  assert(!property_samples.layer_xform_at_start.has_value());
  assert(property_samples.hash_of_expression.empty());
  assert(property_samples.samples.size() == 1);
  assert(std::abs(property_samples.samples.front().t_sec - 0.0) < 1e-12);
  assert(property_samples.samples.front().v.empty());
  assert(property_samples.samples.front().key_timing.has_value());
  assert(property_samples.samples.front().key_timing->interp_in ==
         bbsolver::InterpType::Bezier);
  assert(property_samples.samples.front().key_timing->temporal_ease_in.empty());
  assert(property_samples.samples.front().key_timing->spatial_in.empty());
  assert(bundle.config.tolerance == bbsolver::SolverConfig{}.tolerance);
}

}  // namespace

int main() {
  TestParseSampleBundleTopLevelAndConfigDelegation();
  TestMissingPropertiesArrayIsEmpty();
  TestNullConfigKeepsDefaults();
  TestRichSampleBundleBuildParseContract();
  TestSampleBundleUnknownAndNullDefaultsContract();
  return 0;
}
