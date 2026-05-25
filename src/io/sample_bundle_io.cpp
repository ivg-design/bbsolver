#include "bbsolver/io/sample_bundle_io.hpp"

#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>

#include "bbsolver/domain.hpp"
#include <stdexcept>
#include <string>

#include "bbsolver/io/sample_bundle_validation.hpp"
#include "bbsolver/io/sample_property_io.hpp"
#include "bbsolver/io/solver_config_io.hpp"

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

void RequireObjectField(const json& obj,
                        const char* key,
                        const char* owner) {
  const auto it = obj.find(key);
  if (it == obj.end() || !it->is_object()) {
    throw std::runtime_error(std::string(owner) + " " + key +
                             " must be an object");
  }
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

}  // namespace

void RequireSampleBundleJsonRoot(const json& root) {
  if (!root.is_object()) {
    throw std::runtime_error("SampleBundle JSON must be an object");
  }
  const auto schema_it = root.find("_schema");
  if (schema_it == root.end() || !schema_it->is_string() ||
      schema_it->get<std::string>() != "samples") {
    throw std::runtime_error(
        "Expected SampleBundle JSON with _schema=\"samples\"");
  }
  const auto version_it = root.find("schema_version");
  if (version_it == root.end() || !version_it->is_number_integer()) {
    throw std::runtime_error(
        "Expected SampleBundle JSON with integer schema_version");
  }
  RequireObjectField(root, "comp", "SampleBundle");
  RequireNonEmptyArrayField(root, "properties", "SampleBundle");
  for (const auto& property_json : root.at("properties")) {
    RequirePropertySamplesJson(property_json);
  }
}

SampleBundle ParseSampleBundleJson(const json& root) {
  SampleBundle bundle;
  bundle.schema_version =
      GetOr<int>(root, "schema_version", bundle.schema_version);
  bundle.request_id =
      GetOr<std::string>(root, "request_id", bundle.request_id);
  bundle.comp = ParseCompInfoJson(root.at("comp"));
  if (const auto it = root.find("properties");
      it != root.end() && it->is_array()) {
    for (const auto& property_json : *it) {
      bundle.properties.push_back(ParsePropertySamplesJson(property_json));
    }
  }
  if (const auto it = root.find("config"); it != root.end() && !it->is_null()) {
    bundle.config = ParseSolverConfigJson(*it);
  }
  return bundle;
}

}  // namespace bbsolver
