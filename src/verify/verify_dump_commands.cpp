#include "bbsolver/verify/verify_dump_commands.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

#include "bbsolver/metrics/error_metrics.hpp"
#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "bbsolver/app/cli_options.hpp"
#include "bbsolver/io/io_json.hpp"
#include "bbsolver/io/key_bundle_io.hpp"
#include "bbsolver/io/sample_bundle_io.hpp"
#include "bbsolver/io/schema_contract.hpp"
#include "bbsolver/verify/verifier.hpp"

namespace bbsolver {
namespace {

bool KeyVectorDimensionsMatch(const PropertyKeys& property_keys,
                              int expected_dimensions) {
  const std::size_t expected =
      static_cast<std::size_t>(std::max(expected_dimensions, 1));
  if (property_keys.dimensions > 0 &&
      property_keys.dimensions != expected_dimensions) {
    return false;
  }
  return std::all_of(
      property_keys.keys.begin(),
      property_keys.keys.end(),
      [&](const Key& key) { return key.v.size() == expected; });
}

bool IsVariableTopologyShapeFlat(const PropertyInfo& info) {
  return info.units_label == "shape_flat" && info.shape_variable_topology;
}

// Per-key shape-flat invariant. Each key's v[] must encode a
// non-negative integer vertex count at index 1 with overall length
// equal to 2 + 6 * vertex_count (the v[0] header plus six floats per
// vertex: in-tangent dx/dy, anchor x/y, out-tangent dx/dy). When the
// source bbsm records shape_max_vertex_count > 0 the per-key vertex
// count must also stay within that ceiling. Returns std::nullopt on
// success, otherwise a JSON object describing the malformed key.
std::optional<nlohmann::json> ValidateShapeFlatKey(const Key& key,
                                                   std::size_t key_index,
                                                   int max_vertex_count) {
  auto fail = [&](const char* reason_detail,
                  int actual_length,
                  int expected_length,
                  int vertex_count) {
    nlohmann::json detail{
        {"key_index", static_cast<std::int64_t>(key_index)},
        {"key_t_sec", key.t_sec},
        {"actual_length", actual_length},
        {"expected_length", expected_length},
        {"vertex_count", vertex_count},
        {"detail", reason_detail},
    };
    if (max_vertex_count > 0) {
      detail["max_vertex_count"] = max_vertex_count;
    }
    return detail;
  };

  const int actual_len = static_cast<int>(key.v.size());
  if (actual_len < 2) {
    return fail("v_length_below_minimum", actual_len, 2, 0);
  }
  const double raw_vertex_count = key.v[1];
  if (!std::isfinite(raw_vertex_count)) {
    return fail("vertex_count_not_finite", actual_len, -1, 0);
  }
  const double rounded = std::round(raw_vertex_count);
  if (std::abs(raw_vertex_count - rounded) > 1e-9 || rounded < 0.0) {
    return fail("vertex_count_not_nonnegative_integer", actual_len, -1,
                static_cast<int>(rounded));
  }
  const int vertex_count = static_cast<int>(rounded);
  const int expected_len = 2 + 6 * vertex_count;
  if (actual_len != expected_len) {
    return fail("v_length_mismatch_2_plus_6_times_vertex_count",
                actual_len, expected_len, vertex_count);
  }
  if (max_vertex_count > 0 && vertex_count > max_vertex_count) {
    return fail("vertex_count_exceeds_shape_max_vertex_count",
                actual_len, expected_len, vertex_count);
  }
  return std::nullopt;
}

// Cross-check the bbsm-declared dimensions field against the
// shape_max_vertex_count metadata. For shape_flat the contract is
// dimensions == 2 + 6 * shape_max_vertex_count. When the max is
// missing we derive it from dimensions only when (dimensions - 2)
// is a non-negative multiple of 6. Returns std::nullopt when the
// contract holds (or there is not enough info to check), otherwise
// a JSON object describing the malformed bbsm metadata. The result
// is purely advisory for verify, but reviewers expect the verifier
// to flag obvious inconsistencies in the input sample bundle.
std::optional<nlohmann::json> ValidateShapeFlatMetadata(
    const PropertyInfo& info) {
  if (info.units_label != "shape_flat") {
    return std::nullopt;
  }
  if (info.shape_max_vertex_count > 0) {
    const int expected = 2 + 6 * info.shape_max_vertex_count;
    if (info.dimensions != expected) {
      return nlohmann::json{
          {"detail",
           "shape_flat dimensions must equal 2 + 6 * shape_max_vertex_count"},
          {"dimensions", info.dimensions},
          {"shape_max_vertex_count", info.shape_max_vertex_count},
          {"expected_dimensions", expected},
      };
    }
    return std::nullopt;
  }
  // shape_max_vertex_count missing: dimensions must be derivable.
  if (info.dimensions >= 2 && ((info.dimensions - 2) % 6) == 0) {
    return std::nullopt;
  }
  return nlohmann::json{
      {"detail",
       "shape_flat dimensions without shape_max_vertex_count must satisfy "
       "(dimensions - 2) mod 6 == 0"},
      {"dimensions", info.dimensions},
  };
}

void RequireDumpBundleJson(const nlohmann::json& root) {
  if (!root.is_object()) {
    throw std::runtime_error(
        "bbsolver dump accepts SampleBundle or KeyBundle JSON only");
  }
  const auto schema_it = root.find("_schema");
  if (schema_it == root.end() || !schema_it->is_string()) {
    throw std::runtime_error(
        "bbsolver dump accepts SampleBundle or KeyBundle JSON only");
  }
  const std::string schema = schema_it->get<std::string>();
  if (schema != "samples" && schema != "keys") {
    throw std::runtime_error(
        "bbsolver dump accepts SampleBundle or KeyBundle JSON only");
  }
  const auto version_it = root.find("schema_version");
  if (version_it == root.end() || !version_it->is_number_integer()) {
    throw std::runtime_error(
        "bbsolver dump accepts SampleBundle or KeyBundle JSON only");
  }
  if (schema == "samples") {
    RequireSupportedBundleSchemaVersion("SampleBundle",
                                        version_it->get<int>());
    RequireSampleBundleJsonRoot(root);
    const SampleBundle samples = ParseSampleBundleJson(root);
    if (samples.properties.empty()) {
      throw std::runtime_error(
          "SampleBundle properties must not be empty");
    }
    return;
  }

  RequireSupportedBundleSchemaVersion("KeyBundle", version_it->get<int>());
  RequireKeyBundleJsonRoot(root);
  const auto results_it = root.find("property_results");
  if (results_it == root.end() || !results_it->is_array()) {
    throw std::runtime_error(
        "KeyBundle property_results must be an array");
  }
  (void)ParseKeyBundleJson(root);
}

}  // namespace

int RunVerifyCommand(int argc, char** argv) {
  if (argc < 4) {
    PrintUsage(std::cerr);
    return 2;
  }
  const std::filesystem::path bundle_path = argv[2];
  const std::filesystem::path sample_path = argv[3];
  if (!HasJsonSuffix(bundle_path)) {
    throw std::runtime_error(
        "bbsolver verify accepts KeyBundle JSON input only");
  }
  if (!HasJsonSuffix(sample_path)) {
    throw std::runtime_error(
        "bbsolver verify accepts SampleBundle JSON input only");
  }
  const KeyBundle bundle = ReadKeyBundleJsonForVerify(bundle_path);
  const SampleBundle samples = ReadSampleBundleJson(sample_path);
  RequireSupportedBundleSchemaVersion("KeyBundle", bundle.schema_version);
  RequireSupportedBundleSchemaVersion("SampleBundle", samples.schema_version);
  if (bundle.property_results.empty()) {
    throw std::runtime_error(
        "KeyBundle property_results must not be empty");
  }

  nlohmann::json property_results = nlohmann::json::array();
  bool all_ok = true;
  int verified_count = 0;
  for (const PropertyKeys& property_keys: bundle.property_results) {
    const auto sample_it =
        std::find_if(samples.properties.begin(),
                     samples.properties.end(),
                     [&](const PropertySamples& property_samples) {
                       return property_samples.property.id ==
                              property_keys.property_id;
                     });
    if (sample_it == samples.properties.end()) {
      all_ok = false;
      property_results.push_back({
          {"property_id", property_keys.property_id},
          {"ok", false},
          {"reason", "missing_samples_for_property_id"},
          {"key_count", property_keys.keys.size()},
      });
      continue;
    }

    const PropertyInfo& info = sample_it->property;
    const int expected_dimensions = std::max(info.dimensions, 1);
    const bool variable_topology_shape_flat = IsVariableTopologyShapeFlat(info);

    if (variable_topology_shape_flat) {
      // Variable-topology shape_flat: the strict bbsm-derived dim does
      // not apply per-key. Validate each key against the shape_flat
      // 2 + 6 * vertex_count rule, and flag any inconsistency in the
      // bbsm-side metadata. property_results.dimensions on the bbky is
      // advisory here — blob v1 demonstrates that per-key v[] length
      // legitimately varies inside a single bundle.
      if (const auto metadata_problem = ValidateShapeFlatMetadata(info)) {
        all_ok = false;
        nlohmann::json entry{
            {"property_id", property_keys.property_id},
            {"ok", false},
            {"reason", "invalid_shape_flat_sample_metadata"},
            {"key_count", property_keys.keys.size()},
        };
        entry.update(*metadata_problem);
        property_results.push_back(entry);
        continue;
      }
      nlohmann::json malformed_keys = nlohmann::json::array();
      for (std::size_t idx = 0; idx < property_keys.keys.size(); ++idx) {
        if (auto problem = ValidateShapeFlatKey(
                property_keys.keys[idx], idx, info.shape_max_vertex_count)) {
          malformed_keys.push_back(*problem);
        }
      }
      if (!malformed_keys.empty()) {
        all_ok = false;
        property_results.push_back({
            {"property_id", property_keys.property_id},
            {"ok", false},
            {"reason", "invalid_shape_flat_key_dimensions"},
            {"shape_max_vertex_count", info.shape_max_vertex_count},
            {"key_count", property_keys.keys.size()},
            {"malformed_keys", malformed_keys},
        });
        continue;
      }
    } else if (!KeyVectorDimensionsMatch(property_keys, expected_dimensions)) {
      all_ok = false;
      property_results.push_back({
          {"property_id", property_keys.property_id},
          {"ok", false},
          {"reason", "key_value_dimension_mismatch"},
          {"expected_dimensions", expected_dimensions},
          {"key_dimensions", property_keys.dimensions},
          {"key_count", property_keys.keys.size()},
      });
      continue;
    }

    const ErrorReport report =
        ValidateKeys(*sample_it,
                     property_keys.keys,
                     samples.config,
                     samples.comp);
    const bool property_ok =
        report.max_err <= samples.config.tolerance + 1e-9 &&
        ((samples.config.tolerance_screen_px <= 0.0 &&
          samples.config.weight_screen <= 0.0) ||
         report.max_err_screen_px <=
             (samples.config.tolerance_screen_px > 0.0
                  ? samples.config.tolerance_screen_px
: samples.config.tolerance) +
                 1e-9);
    all_ok = all_ok && property_ok;
    ++verified_count;
    property_results.push_back({
        {"property_id", property_keys.property_id},
        {"ok", property_ok},
        {"key_count", property_keys.keys.size()},
        {"max_err", report.max_err},
        {"max_err_screen_px", report.max_err_screen_px},
        {"rms_err", report.rms_err},
        {"worst_sample_idx", report.worst_sample_idx},
        {"notes", property_keys.notes},
    });
  }

  const nlohmann::json out{
      {"ok", all_ok},
      {"bundle", bundle_path.string()},
      {"samples", sample_path.string()},
      {"tolerance", samples.config.tolerance},
      {"tolerance_screen_px", samples.config.tolerance_screen_px},
      {"verified_properties", verified_count},
      {"property_results", property_results},
  };
  std::cout << out.dump(2) << '\n';
  return all_ok ? 0: kVerifyMismatchExitCode;
}

int RunDumpCommand(int argc, char** argv) {
  if (argc < 3) {
    PrintUsage(std::cerr);
    return 2;
  }

  const std::filesystem::path path = argv[2];
  if (!HasJsonSuffix(path)) {
    throw std::runtime_error(
        "bbsolver dump accepts JSON bundles only");
  }

  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("Failed to open bundle for dump: " +
                             path.string());
  }
  nlohmann::json root;
  input >> root;
  RequireDumpBundleJson(root);
  std::cout << root.dump(2) << '\n';
  return 0;
}

}  // namespace bbsolver
