#include "bbsolver/io/sample_property_io.hpp"
#include "bbsolver/domain.hpp"

#include <cassert>
#include <cmath>  // IWYU pragma: keep
#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>
#include <vector>  // IWYU pragma: keep

namespace {

using nlohmann::json;

void TestParseCompInfo() {
  const bbsolver::CompInfo comp = bbsolver::ParseCompInfoJson({
      {"fps", 24.0},
      {"duration_sec", 2.5},
      {"width", 1920},
      {"height", 1080},
      {"pixel_aspect", 1.2},
      {"shutter_angle_deg", 90.0},
      {"shutter_phase_deg", -45.0},
      {"motion_blur_enabled", true},
      {"work_area_start_sec", 0.25},
      {"work_area_end_sec", 2.0},
  });

  assert(std::abs(comp.fps - 24.0) < 1e-12);
  assert(std::abs(comp.duration_sec - 2.5) < 1e-12);
  assert(comp.width == 1920);
  assert(comp.height == 1080);
  assert(std::abs(comp.pixel_aspect - 1.2) < 1e-12);
  assert(std::abs(comp.shutter_angle_deg - 90.0) < 1e-12);
  assert(std::abs(comp.shutter_phase_deg + 45.0) < 1e-12);
  assert(comp.motion_blur_enabled);
  assert(std::abs(comp.work_area_start_sec - 0.25) < 1e-12);
  assert(std::abs(comp.work_area_end_sec - 2.0) < 1e-12);
}

void TestParsePropertyInfo() {
  const bbsolver::PropertyInfo property =
      bbsolver::ParsePropertyInfoJson({
          {"id", "layer/path"},
          {"match_name", "ADBE Vector Shape"},
          {"display_name", "Path 1"},
          {"layer_path", "Layer/Contents"},
          {"kind", "Custom"},
          {"dimensions", 14},
          {"is_spatial", true},
          {"is_separated", true},
          {"units_label", "shape_flat"},
          {"min_value", json::array({0.0, 1.0})},
          {"max_value", json::array({10.0, 11.0})},
          {"source_key_times", json::array({0.0, 0.5, 1.0})},
      });

  assert(property.id == "layer/path");
  assert(property.match_name == "ADBE Vector Shape");
  assert(property.display_name == "Path 1");
  assert(property.layer_path == "Layer/Contents");
  assert(property.kind == bbsolver::ValueKind::Custom);
  assert(property.dimensions == 14);
  assert(property.is_spatial);
  assert(property.is_separated);
  assert(property.units_label == "shape_flat");
  assert(property.min_value == std::vector<double>({0.0, 1.0}));
  assert(property.max_value == std::vector<double>({10.0, 11.0}));
  assert(property.source_key_times == std::vector<double>({0.0, 0.5, 1.0}));
}

void TestPropertyInfoUnknownFieldsAndNullsKeepDefaults() {
  const bbsolver::PropertyInfo property =
      bbsolver::ParsePropertyInfoJson({
          {"id", "prop"},
          {"kind", nullptr},
          {"dimensions", nullptr},
          {"is_spatial", nullptr},
          {"units_label", nullptr},
          {"unknown_field", "ignored"},
      });

  assert(property.id == "prop");
  assert(property.kind == bbsolver::ValueKind::Scalar);
  assert(property.dimensions == 1);
  assert(!property.is_spatial);
  assert(property.units_label.empty());
  assert(property.min_value.empty());
  assert(property.max_value.empty());
}

void TestParsePropertySamples() {
  const json property_json = {
      {"property",
       {{"id", "prop"},
        {"kind", "TwoD"},
        {"dimensions", 2},
        {"is_spatial", true}}},
      {"t_start_sec", 0.25},
      {"t_end_sec", 1.25},
      {"samples_per_frame", 2},
      {"samples",
       json::array({
           {{"t_sec", 0.25}, {"v", json::array({1.0, 2.0})}},
           {{"t_sec", 0.75},
            {"v", json::array({3.0, 4.0})},
            {"interp_out", "Linear"}},
       })},
      {"layer_xform_at_start",
       {{"anchor_point", json::array({1.0, 2.0})},
        {"position", json::array({3.0, 4.0})},
        {"scale", json::array({100.0, 100.0})},
        {"rotation", json::array({45.0})},
        {"opacity", 75.0}}},
      {"hash_of_expression", "hash"},
  };
  const bbsolver::PropertySamples samples =
      bbsolver::ParsePropertySamplesJson(property_json);

  assert(samples.property.kind == bbsolver::ValueKind::TwoD);
  assert(samples.property.dimensions == 2);
  assert(samples.property.is_spatial);
  assert(std::abs(samples.t_start_sec - 0.25) < 1e-12);
  assert(std::abs(samples.t_end_sec - 1.25) < 1e-12);
  assert(samples.samples_per_frame == 2);
  assert(samples.samples.size() == 2);
  assert(samples.samples.front().v == std::vector<double>({1.0, 2.0}));
  assert(!samples.samples.front().key_timing.has_value());
  assert(samples.samples.back().key_timing.has_value());
  assert(samples.samples.back().key_timing->interp_out ==
         bbsolver::InterpType::Linear);
  assert(samples.layer_xform_at_start.has_value());
  assert(samples.layer_xform_at_start->anchor_point ==
         std::vector<double>({1.0, 2.0}));
  assert(std::abs(samples.layer_xform_at_start->opacity - 75.0) < 1e-12);
  assert(samples.hash_of_expression == "hash");
}

void TestNullLayerXformStaysEmpty() {
  const bbsolver::PropertySamples samples =
      bbsolver::ParsePropertySamplesJson({
          {"property", {{"id", "prop"}, {"kind", "Scalar"}}},
          {"layer_xform_at_start", json{}},
      });
  assert(!samples.layer_xform_at_start.has_value());
}

}  // namespace

int main() {
  TestParseCompInfo();
  TestParsePropertyInfo();
  TestPropertyInfoUnknownFieldsAndNullsKeepDefaults();
  TestParsePropertySamples();
  TestNullLayerXformStaysEmpty();
  return 0;
}
