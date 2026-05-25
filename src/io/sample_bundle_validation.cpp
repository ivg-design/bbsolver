#include "bbsolver/io/sample_bundle_validation.hpp"

#include "nlohmann/json_fwd.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>

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

int RequirePropertyDimensions(const json& property_json) {
  const auto dimensions_it = property_json.find("dimensions");
  if (dimensions_it == property_json.end() ||
      !dimensions_it->is_number_integer()) {
    throw std::runtime_error(
        "SampleBundle property dimensions must be an integer");
  }
  const int dimensions = dimensions_it->get<int>();
  if (dimensions <= 0) {
    throw std::runtime_error(
        "SampleBundle property dimensions must be positive");
  }
  return dimensions;
}

const json& RequireSampleValueJson(const json& sample_json) {
  if (!sample_json.is_object()) {
    throw std::runtime_error("SampleBundle sample entries must be objects");
  }
  const auto t_it = sample_json.find("t_sec");
  if (t_it == sample_json.end() || !t_it->is_number()) {
    throw std::runtime_error("SampleBundle sample t_sec must be a number");
  }
  const auto value_it = sample_json.find("v");
  if (value_it == sample_json.end() || !value_it->is_array()) {
    throw std::runtime_error("SampleBundle sample v must be an array");
  }
  if (value_it->empty()) {
    throw std::runtime_error("SampleBundle sample v must not be empty");
  }
  return *value_it;
}

bool IsVariableTopologyShapeFlat(const json& property_json) {
  return GetOr<std::string>(property_json, "units_label", "") == "shape_flat" &&
         GetOr<bool>(property_json, "shape_variable_topology", false);
}

void RequireShapeFlatSampleVector(const json& values, int max_dimensions) {
  if (values.size() < 8) {
    throw std::runtime_error(
        "SampleBundle variable shape_flat sample v is too short");
  }
  if (!values.at(1).is_number()) {
    throw std::runtime_error(
        "SampleBundle variable shape_flat vertex count must be numeric");
  }
  const double vertex_count_value = values.at(1).get<double>();
  const double rounded_vertex_count = std::round(vertex_count_value);
  if (std::abs(vertex_count_value - rounded_vertex_count) > 1e-9 ||
      rounded_vertex_count <= 0.0) {
    throw std::runtime_error(
        "SampleBundle variable shape_flat vertex count must be positive");
  }
  const int expected_count =
      2 + static_cast<int>(rounded_vertex_count) * 6;
  if (values.size() != static_cast<std::size_t>(expected_count)) {
    throw std::runtime_error(
        "SampleBundle variable shape_flat sample v length must match its "
        "vertex count");
  }
  if (expected_count > max_dimensions) {
    throw std::runtime_error(
        "SampleBundle variable shape_flat sample v length must not exceed "
        "property dimensions");
  }
}

void RequireSampleJson(const json& sample_json,
                       int expected_value_count,
                       bool variable_topology_shape_flat) {
  const json& values = RequireSampleValueJson(sample_json);
  if (variable_topology_shape_flat) {
    RequireShapeFlatSampleVector(values, expected_value_count);
    return;
  }
  if (values.size() != static_cast<std::size_t>(expected_value_count)) {
    throw std::runtime_error(
        "SampleBundle sample v length must equal dimensions times "
        "samples_per_frame");
  }
}

int RequireSamplesPerFrame(const json& property_json) {
  const auto samples_per_frame_it = property_json.find("samples_per_frame");
  if (samples_per_frame_it == property_json.end() ||
      samples_per_frame_it->is_null()) {
    return 1;
  }
  if (!samples_per_frame_it->is_number_integer()) {
    throw std::runtime_error(
        "SampleBundle property entry samples_per_frame must be an integer");
  }
  const int samples_per_frame = samples_per_frame_it->get<int>();
  if (samples_per_frame <= 0) {
    throw std::runtime_error(
        "SampleBundle property entry samples_per_frame must be positive");
  }
  return samples_per_frame;
}

}  // namespace

void RequirePropertySamplesJson(const json& property_json) {
  if (!property_json.is_object()) {
    throw std::runtime_error("SampleBundle properties entries must be objects");
  }
  RequireObjectField(property_json, "property", "SampleBundle property entry");
  const int dimensions =
      RequirePropertyDimensions(property_json.at("property"));
  const int expected_value_count =
      dimensions * RequireSamplesPerFrame(property_json);
  const bool variable_topology_shape_flat =
      IsVariableTopologyShapeFlat(property_json.at("property"));
  RequireNonEmptyArrayField(
      property_json, "samples", "SampleBundle property entry");
  for (const auto& sample_json : property_json.at("samples")) {
    RequireSampleJson(
        sample_json, expected_value_count, variable_topology_shape_flat);
  }
}

}  // namespace bbsolver
