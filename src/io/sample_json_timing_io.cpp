#include "bbsolver/io/sample_json_timing_io.hpp"

#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>

#include "bbsolver/domain.hpp"
#include <stdexcept>
#include <string>
#include <vector>

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

InterpType ParseInterpType(const std::string& value) {
  if (value == "Hold") return InterpType::Hold;
  if (value == "Linear") return InterpType::Linear;
  if (value == "Bezier") return InterpType::Bezier;
  throw std::runtime_error("Unknown InterpType: " + value);
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

}  // namespace

bool HasSampleKeyTimingJsonFields(const json& obj) {
  return obj.contains("interp_in") ||
         obj.contains("interp_out") ||
         obj.contains("temporal_ease_in") ||
         obj.contains("temporal_ease_out") ||
         obj.contains("spatial_in") ||
         obj.contains("spatial_out") ||
         obj.contains("temporal_continuous") ||
         obj.contains("spatial_continuous") ||
         obj.contains("temporal_auto_bezier") ||
         obj.contains("spatial_auto_bezier") ||
         obj.contains("roving");
}

KeyTiming ParseSampleKeyTimingJson(const json& obj) {
  KeyTiming timing;
  timing.interp_in =
      ParseInterpType(GetOr<std::string>(obj, "interp_in", "Bezier"));
  timing.interp_out =
      ParseInterpType(GetOr<std::string>(obj, "interp_out", "Bezier"));
  timing.temporal_ease_in = ParseTemporalEaseArray(obj, "temporal_ease_in");
  timing.temporal_ease_out = ParseTemporalEaseArray(obj, "temporal_ease_out");
  timing.spatial_in = GetDoubleVector(obj, "spatial_in");
  timing.spatial_out = GetDoubleVector(obj, "spatial_out");
  timing.temporal_continuous =
      GetOr<bool>(obj, "temporal_continuous", timing.temporal_continuous);
  timing.spatial_continuous =
      GetOr<bool>(obj, "spatial_continuous", timing.spatial_continuous);
  timing.temporal_auto_bezier =
      GetOr<bool>(obj, "temporal_auto_bezier", timing.temporal_auto_bezier);
  timing.spatial_auto_bezier =
      GetOr<bool>(obj, "spatial_auto_bezier", timing.spatial_auto_bezier);
  timing.roving = GetOr<bool>(obj, "roving", timing.roving);
  return timing;
}

Sample ParseSampleJson(const json& obj) {
  Sample sample;
  sample.t_sec = GetOr<double>(obj, "t_sec", sample.t_sec);
  sample.v = GetDoubleVector(obj, "v");
  if (const auto it = obj.find("key_timing");
      it != obj.end() && it->is_object()) {
    sample.key_timing = ParseSampleKeyTimingJson(*it);
  } else if (HasSampleKeyTimingJsonFields(obj)) {
    sample.key_timing = ParseSampleKeyTimingJson(obj);
  }
  return sample;
}

}  // namespace bbsolver
