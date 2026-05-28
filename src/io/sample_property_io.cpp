#include "bbsolver/io/sample_property_io.hpp"

#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>

#include "bbsolver/domain.hpp"
#include <string>
#include <vector>

#include "bbsolver/io/sample_json_timing_io.hpp"
#include "bbsolver/io/sample_json_value_io.hpp"

namespace bbsolver {
namespace {

using nlohmann::json;

template <typename T>
T GetOr(const json& obj, const char* key, T fallback) {
  const auto it = obj.find(key);
  if (it == obj.end() || it->is_null()) {
    return fallback;
  }
  return it->get<T>();
}

std::vector<double> GetDoubleVector(const json& obj, const char* key) {
  const auto it = obj.find(key);
  if (it == obj.end() || it->is_null()) {
    return {};
  }
  return it->get<std::vector<double>>();
}

}  // namespace

CompInfo ParseCompInfoJson(const json& obj) {
  CompInfo comp;
  comp.fps = GetOr<double>(obj, "fps", comp.fps);
  comp.duration_sec = GetOr<double>(obj, "duration_sec", comp.duration_sec);
  comp.width = GetOr<int>(obj, "width", comp.width);
  comp.height = GetOr<int>(obj, "height", comp.height);
  comp.pixel_aspect = GetOr<double>(obj, "pixel_aspect", comp.pixel_aspect);
  comp.shutter_angle_deg =
      GetOr<double>(obj, "shutter_angle_deg", comp.shutter_angle_deg);
  comp.shutter_phase_deg =
      GetOr<double>(obj, "shutter_phase_deg", comp.shutter_phase_deg);
  comp.motion_blur_enabled =
      GetOr<bool>(obj, "motion_blur_enabled", comp.motion_blur_enabled);
  comp.work_area_start_sec =
      GetOr<double>(obj, "work_area_start_sec", comp.work_area_start_sec);
  comp.work_area_end_sec =
      GetOr<double>(obj, "work_area_end_sec", comp.work_area_end_sec);
  return comp;
}

LayerXform ParseLayerXformJson(const json& obj) {
  LayerXform xform;
  xform.anchor_point = GetDoubleVector(obj, "anchor_point");
  xform.position = GetDoubleVector(obj, "position");
  xform.scale = GetDoubleVector(obj, "scale");
  xform.rotation = GetDoubleVector(obj, "rotation");
  xform.opacity = GetOr<double>(obj, "opacity", xform.opacity);
  return xform;
}

PropertyInfo ParsePropertyInfoJson(const json& obj) {
  PropertyInfo property;
  property.id = GetOr<std::string>(obj, "id", property.id);
  property.match_name =
      GetOr<std::string>(obj, "match_name", property.match_name);
  property.display_name =
      GetOr<std::string>(obj, "display_name", property.display_name);
  property.layer_path =
      GetOr<std::string>(obj, "layer_path", property.layer_path);
  property.kind =
      ParseSampleValueKindJson(GetOr<std::string>(obj, "kind", "Scalar"));
  property.dimensions = GetOr<int>(obj, "dimensions", property.dimensions);
  property.is_spatial = GetOr<bool>(obj, "is_spatial", property.is_spatial);
  property.is_separated =
      GetOr<bool>(obj, "is_separated", property.is_separated);
  property.units_label =
      GetOr<std::string>(obj, "units_label", property.units_label);
  property.min_value = GetDoubleVector(obj, "min_value");
  property.max_value = GetDoubleVector(obj, "max_value");
  property.source_key_times = GetDoubleVector(obj, "source_key_times");
  property.shape_variable_topology =
      GetOr<bool>(obj, "shape_variable_topology",
                  property.shape_variable_topology);
  property.shape_canonical_method =
      GetOr<std::string>(obj, "shape_canonical_method",
                         property.shape_canonical_method);
  property.shape_canonical_vertex_count =
      GetOr<int>(obj, "shape_canonical_vertex_count",
                 property.shape_canonical_vertex_count);
  property.shape_max_vertex_count =
      GetOr<int>(obj, "shape_max_vertex_count",
                 property.shape_max_vertex_count);
  return property;
}

PropertySamples ParsePropertySamplesJson(const json& obj) {
  PropertySamples property_samples;
  property_samples.property = ParsePropertyInfoJson(obj.at("property"));
  property_samples.t_start_sec =
      GetOr<double>(obj, "t_start_sec", property_samples.t_start_sec);
  property_samples.t_end_sec =
      GetOr<double>(obj, "t_end_sec", property_samples.t_end_sec);
  property_samples.samples_per_frame =
      GetOr<int>(obj, "samples_per_frame", property_samples.samples_per_frame);
  if (const auto it = obj.find("samples"); it != obj.end() && it->is_array()) {
    for (const auto& sample_json : *it) {
      property_samples.samples.push_back(ParseSampleJson(sample_json));
    }
  }
  if (const auto it = obj.find("layer_xform_at_start");
      it != obj.end() && !it->is_null()) {
    property_samples.layer_xform_at_start = ParseLayerXformJson(*it);
  }
  property_samples.hash_of_expression =
      GetOr<std::string>(
          obj, "hash_of_expression", property_samples.hash_of_expression);
  return property_samples;
}

}  // namespace bbsolver
