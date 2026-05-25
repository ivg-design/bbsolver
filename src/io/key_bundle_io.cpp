#include "bbsolver/io/key_bundle_io.hpp"
#include "bbsolver/domain.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>
#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>

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

void RequireNonEmptyArrayField(const json& obj,
                               const char* key,
                               const char* owner) {
  const auto it = obj.find(key);
  if (it == obj.end() || !it->is_array()) {
    throw std::runtime_error(std::string(owner) + " " + key +
                             " must be an array");
  }
  if (it->empty()) {
    throw std::runtime_error(std::string(owner) + " " + key +
                             " must not be empty");
  }
}

const json& RequireArrayField(const json& obj,
                              const char* key,
                              const char* owner) {
  const auto it = obj.find(key);
  if (it == obj.end() || !it->is_array()) {
    throw std::runtime_error(std::string(owner) + " " + key +
                             " must be an array");
  }
  return *it;
}

int InferredPropertyKeyDimensions(const PropertyKeys& property) {
  if (property.dimensions > 0) {
    return property.dimensions;
  }
  for (const Key& key : property.keys) {
    if (!key.v.empty()) {
      return static_cast<int>(key.v.size());
    }
  }
  return 1;
}

int RequirePropertyKeyDimensions(const json& property_json) {
  const auto dimensions_it = property_json.find("dimensions");
  if (dimensions_it == property_json.end() ||
      !dimensions_it->is_number_integer()) {
    throw std::runtime_error(
        "KeyBundle property_results dimensions must be an integer");
  }
  const int dimensions = dimensions_it->get<int>();
  if (dimensions <= 0) {
    throw std::runtime_error(
        "KeyBundle property_results dimensions must be positive");
  }
  return dimensions;
}

void RequireKeyJson(const json& key_json, int dimensions) {
  if (!key_json.is_object()) {
    throw std::runtime_error("KeyBundle key entries must be objects");
  }
  const auto time_it = key_json.find("t_sec");
  if (time_it == key_json.end() || !time_it->is_number()) {
    throw std::runtime_error("KeyBundle key t_sec must be a number");
  }
  const auto value_it = key_json.find("v");
  if (value_it == key_json.end() || !value_it->is_array()) {
    throw std::runtime_error("KeyBundle key v must be an array");
  }
  if (value_it->empty()) {
    throw std::runtime_error("KeyBundle key v must not be empty");
  }
  if (value_it->size() != static_cast<std::size_t>(dimensions)) {
    throw std::runtime_error(
        "KeyBundle key v length must equal property_results dimensions");
  }
}

void RequirePropertyKeysJson(const json& property_json) {
  if (!property_json.is_object()) {
    throw std::runtime_error(
        "KeyBundle property_results entries must be objects");
  }
  const auto property_id_it = property_json.find("property_id");
  if (property_id_it == property_json.end() ||
      !property_id_it->is_string() ||
      property_id_it->get<std::string>().empty()) {
    throw std::runtime_error(
        "KeyBundle property_results property_id must be a non-empty string");
  }
  const int dimensions = RequirePropertyKeyDimensions(property_json);
  const json& keys = RequireArrayField(
      property_json, "keys", "KeyBundle property_results entry");
  const bool converged = GetOr<bool>(property_json, "converged", true);
  if (keys.empty() && converged) {
    throw std::runtime_error(
        "KeyBundle converged property_results entry keys must not be empty");
  }
  for (const auto& key_json : keys) {
    RequireKeyJson(key_json, dimensions);
  }
}

InterpType ParseInterpType(const std::string& value) {
  if (value == "Hold") return InterpType::Hold;
  if (value == "Linear") return InterpType::Linear;
  if (value == "Bezier") return InterpType::Bezier;
  throw std::runtime_error("Unknown InterpType: " + value);
}

std::string ToString(InterpType value) {
  switch (value) {
    case InterpType::Hold: return "Hold";
    case InterpType::Linear: return "Linear";
    case InterpType::Bezier: return "Bezier";
  }
  throw std::runtime_error("Unhandled InterpType");
}

std::vector<TemporalEase> ParseTemporalEaseArray(const json& obj,
                                                 const char* key_name) {
  std::vector<TemporalEase> result;
  const auto it = obj.find(key_name);
  if (it == obj.end() || !it->is_array()) {
    return result;
  }
  for (const auto& ease_json : *it) {
    TemporalEase ease;
    ease.speed = GetOr<double>(ease_json, "speed", ease.speed);
    ease.influence = GetOr<double>(ease_json, "influence", ease.influence);
    result.push_back(ease);
  }
  return result;
}

json ToJson(const TemporalEase& ease) {
  return json{
      {"speed", ease.speed},
      {"influence", ease.influence},
  };
}

json ToJson(const Key& key) {
  json temporal_ease_in = json::array();
  for (const auto& ease : key.temporal_ease_in) {
    temporal_ease_in.push_back(ToJson(ease));
  }

  json temporal_ease_out = json::array();
  for (const auto& ease : key.temporal_ease_out) {
    temporal_ease_out.push_back(ToJson(ease));
  }

  return json{
      {"t_sec", key.t_sec},
      {"v", key.v},
      {"interp_in", ToString(key.interp_in)},
      {"interp_out", ToString(key.interp_out)},
      {"temporal_ease_in", temporal_ease_in},
      {"temporal_ease_out", temporal_ease_out},
      {"spatial_in", key.spatial_in},
      {"spatial_out", key.spatial_out},
      {"temporal_continuous", key.temporal_continuous},
      {"spatial_continuous", key.spatial_continuous},
      {"temporal_auto_bezier", key.temporal_auto_bezier},
      {"spatial_auto_bezier", key.spatial_auto_bezier},
      {"roving", key.roving},
  };
}

json ToJson(const SegmentReport& segment) {
  return json{
      {"start_idx", segment.start_idx},
      {"end_idx", segment.end_idx},
      {"max_err", segment.max_err},
      {"max_err_screen_px", segment.max_err_screen_px},
      {"rms_err", segment.rms_err},
      {"iters", segment.iters},
      {"reason", segment.reason},
  };
}

json ToJson(const PropertyKeys& property) {
  json keys = json::array();
  for (const auto& key : property.keys) {
    keys.push_back(ToJson(key));
  }

  json segments = json::array();
  for (const auto& segment : property.segments) {
    segments.push_back(ToJson(segment));
  }

  return json{
      {"property_id", property.property_id},
      {"dimensions", InferredPropertyKeyDimensions(property)},
      {"keys", keys},
      {"max_err", property.max_err},
      {"max_err_screen_px", property.max_err_screen_px},
      {"segments", segments},
      {"converged", property.converged},
      {"notes", property.notes},
  };
}

Key ParseKey(const json& obj) {
  Key key;
  key.t_sec = GetOr<double>(obj, "t_sec", key.t_sec);
  key.v = GetDoubleVector(obj, "v");
  key.interp_in =
      ParseInterpType(GetOr<std::string>(obj, "interp_in", "Bezier"));
  key.interp_out =
      ParseInterpType(GetOr<std::string>(obj, "interp_out", "Bezier"));
  key.temporal_ease_in = ParseTemporalEaseArray(obj, "temporal_ease_in");
  key.temporal_ease_out = ParseTemporalEaseArray(obj, "temporal_ease_out");
  key.spatial_in = GetDoubleVector(obj, "spatial_in");
  key.spatial_out = GetDoubleVector(obj, "spatial_out");
  key.temporal_continuous =
      GetOr<bool>(obj, "temporal_continuous", key.temporal_continuous);
  key.spatial_continuous =
      GetOr<bool>(obj, "spatial_continuous", key.spatial_continuous);
  key.temporal_auto_bezier =
      GetOr<bool>(obj, "temporal_auto_bezier", key.temporal_auto_bezier);
  key.spatial_auto_bezier =
      GetOr<bool>(obj, "spatial_auto_bezier", key.spatial_auto_bezier);
  key.roving = GetOr<bool>(obj, "roving", key.roving);
  return key;
}

SegmentReport ParseSegmentReport(const json& obj) {
  SegmentReport segment;
  segment.start_idx = GetOr<int>(obj, "start_idx", segment.start_idx);
  segment.end_idx = GetOr<int>(obj, "end_idx", segment.end_idx);
  segment.max_err = GetOr<double>(obj, "max_err", segment.max_err);
  segment.max_err_screen_px =
      GetOr<double>(obj, "max_err_screen_px", segment.max_err_screen_px);
  segment.rms_err = GetOr<double>(obj, "rms_err", segment.rms_err);
  segment.iters = GetOr<int>(obj, "iters", segment.iters);
  segment.reason = GetOr<std::string>(obj, "reason", segment.reason);
  return segment;
}

PropertyKeys ParsePropertyKeys(const json& obj) {
  PropertyKeys property;
  property.property_id =
      GetOr<std::string>(obj, "property_id", property.property_id);
  property.dimensions = GetOr<int>(obj, "dimensions", property.dimensions);
  property.max_err = GetOr<double>(obj, "max_err", property.max_err);
  property.max_err_screen_px =
      GetOr<double>(obj, "max_err_screen_px", property.max_err_screen_px);
  property.converged = GetOr<bool>(obj, "converged", property.converged);
  property.notes = GetOr<std::string>(obj, "notes", property.notes);
  if (const auto it = obj.find("keys"); it != obj.end() && it->is_array()) {
    for (const auto& key_json : *it) {
      property.keys.push_back(ParseKey(key_json));
    }
  }
  if (const auto it = obj.find("segments"); it != obj.end() && it->is_array()) {
    for (const auto& segment_json : *it) {
      property.segments.push_back(ParseSegmentReport(segment_json));
    }
  }
  return property;
}

}  // namespace

void RequireKeyBundleJsonRoot(const json& root) {
  if (!root.is_object()) {
    throw std::runtime_error("KeyBundle JSON must be an object");
  }
  const auto schema_it = root.find("_schema");
  if (schema_it == root.end() || !schema_it->is_string() ||
      schema_it->get<std::string>() != "keys") {
    throw std::runtime_error(
        "Expected KeyBundle JSON with _schema=\"keys\"");
  }
  const auto version_it = root.find("schema_version");
  if (version_it == root.end() || !version_it->is_number_integer()) {
    throw std::runtime_error(
        "Expected KeyBundle JSON with integer schema_version");
  }
  RequireNonEmptyArrayField(root, "property_results", "KeyBundle");
  for (const auto& property_json : root.at("property_results")) {
    RequirePropertyKeysJson(property_json);
  }
}

KeyBundle ParseKeyBundleJson(const json& root) {
  KeyBundle bundle;
  bundle.schema_version =
      GetOr<int>(root, "schema_version", bundle.schema_version);
  bundle.request_id =
      GetOr<std::string>(root, "request_id", bundle.request_id);
  bundle.solver_version =
      GetOr<std::string>(root, "solver_version", bundle.solver_version);
  bundle.solver_build =
      GetOr<std::string>(root, "solver_build", bundle.solver_build);
  bundle.solve_time_ms =
      GetOr<double>(root, "solve_time_ms", bundle.solve_time_ms);
  bundle.total_keys = GetOr<int>(root, "total_keys", bundle.total_keys);
  bundle.total_samples_input =
      GetOr<int>(root, "total_samples_input", bundle.total_samples_input);
  if (const auto it = root.find("property_results"); it != root.end() &&
      it->is_array()) {
    for (const auto& property_json : *it) {
      bundle.property_results.push_back(ParsePropertyKeys(property_json));
    }
  }
  if (bundle.total_keys <= 0) {
    for (const auto& property : bundle.property_results) {
      bundle.total_keys += static_cast<int>(property.keys.size());
    }
  }
  return bundle;
}

json BuildKeyBundleJson(const KeyBundle& bundle) {
  json property_results = json::array();
  for (const auto& property : bundle.property_results) {
    property_results.push_back(ToJson(property));
  }

  return json{
      {"_schema", "keys"},
      {"schema_version", bundle.schema_version},
      {"request_id", bundle.request_id},
      {"property_results", property_results},
      {"solver_version", bundle.solver_version},
      {"solver_build", bundle.solver_build},
      {"solve_time_ms", bundle.solve_time_ms},
      {"total_keys", bundle.total_keys},
      {"total_samples_input", bundle.total_samples_input},
  };
}

}  // namespace bbsolver
