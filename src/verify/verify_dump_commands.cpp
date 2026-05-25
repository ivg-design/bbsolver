#include "bbsolver/verify/verify_dump_commands.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
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
  const KeyBundle bundle = ReadKeyBundleJson(bundle_path);
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
  for (const PropertyKeys& property_keys : bundle.property_results) {
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

    const int expected_dimensions =
        std::max(sample_it->property.dimensions, 1);
    if (!KeyVectorDimensionsMatch(property_keys, expected_dimensions)) {
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
  return all_ok ? 0 : kVerifyMismatchExitCode;
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
