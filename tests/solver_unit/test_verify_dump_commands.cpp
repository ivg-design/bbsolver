#include "bbsolver/verify/verify_dump_commands.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/io/io_json.hpp"
#include "bbsolver/io/schema_contract.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>
#include <utility>

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    std::abort();
  }
}

class StreamCapture {
 public:
  explicit StreamCapture(std::ostream& stream)
: stream_(stream) {
    old_ = stream.rdbuf(buffer_.rdbuf());
  }

  ~StreamCapture() {
    stream_.rdbuf(old_);
  }

  std::string str() const {
    return buffer_.str();
  }

 private:
  std::ostream& stream_;
  std::streambuf* old_ = nullptr;
  std::ostringstream buffer_;
};

std::vector<char*> Argv(std::vector<std::string>& args) {
  std::vector<char*> argv;
  argv.reserve(args.size());
  for (std::string& arg: args) {
    argv.push_back(arg.data());
  }
  return argv;
}

std::filesystem::path MakeTempDir() {
  const auto stamp =
      std::chrono::steady_clock::now().time_since_epoch().count();
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() /
      ("bb_verify_dump_test_" + std::to_string(stamp));
  std::filesystem::create_directories(dir);
  return dir;
}

void WriteScalarSampleBundle(const std::filesystem::path& path) {
  std::ofstream out(path);
  out << R"JSON({
  "_schema": "samples",
  "schema_version": 1,
  "request_id": "verify-command-unit",
  "comp": {
    "fps": 24.0,
    "duration_sec": 1.0,
    "width": 1920,
    "height": 1080,
    "pixel_aspect": 1.0,
    "shutter_angle_deg": 180.0,
    "shutter_phase_deg": -90.0,
    "motion_blur_enabled": false,
    "work_area_start_sec": 0.0,
    "work_area_end_sec": 1.0
  },
  "properties": [
    {
      "property": {
        "id": "unit/scalar",
        "match_name": "ADBE Slider Control-0001",
        "display_name": "Slider",
        "kind": "Scalar",
        "dimensions": 1,
        "is_spatial": false,
        "is_separated": false,
        "units_label": "",
        "min_value": [],
        "max_value": []
      },
      "t_start_sec": 0.0,
      "t_end_sec": 1.0,
      "samples_per_frame": 1,
      "samples": [
        {"t_sec": 0.0, "v": [0.0]},
        {"t_sec": 1.0, "v": [1.0]}
      ]
    }
  ],
  "config": {
    "tolerance": 0.0,
    "tolerance_screen_px": 0.0,
    "weight_screen": 0.0,
    "allow_hold": true,
    "allow_linear": true,
    "allow_bezier": true
  }
})JSON";
}

bbsolver::Key MakeLinearScalarKey(double t, double value) {
  bbsolver::Key key;
  key.t_sec = t;
  key.v = {value};
  key.interp_in = bbsolver::InterpType::Linear;
  key.interp_out = bbsolver::InterpType::Linear;
  key.temporal_ease_in = {bbsolver::TemporalEase{}};
  key.temporal_ease_out = {bbsolver::TemporalEase{}};
  return key;
}

void WriteMatchingKeyBundle(const std::filesystem::path& path) {
  bbsolver::PropertyKeys property;
  property.property_id = "unit/scalar";
  property.dimensions = 1;
  property.converged = true;
  property.keys.push_back(MakeLinearScalarKey(0.0, 0.0));
  property.keys.push_back(MakeLinearScalarKey(1.0, 1.0));

  bbsolver::KeyBundle bundle;
  bundle.request_id = "verify-command-unit";
  bundle.solver_version = "unit";
  bundle.total_keys = static_cast<int>(property.keys.size());
  bundle.property_results.push_back(std::move(property));
  bbsolver::WriteKeyBundleJson(path, bundle);
}

void TestVerifyCommandReportsOkJson() {
  const std::filesystem::path dir = MakeTempDir();
  const std::filesystem::path samples = dir / "samples.bbsm.json";
  const std::filesystem::path keys = dir / "keys.bbky.json";
  WriteScalarSampleBundle(samples);
  WriteMatchingKeyBundle(keys);

  std::vector<std::string> args = {
      "bbsolver", "verify", keys.string(), samples.string()};
  std::vector<char*> argv = Argv(args);

  StreamCapture capture(std::cout);
  const int rc = bbsolver::RunVerifyCommand(
      static_cast<int>(argv.size()), argv.data());
  Require(rc == 0, "verify command should accept matching keys");

  const nlohmann::json output = nlohmann::json::parse(capture.str());
  Require(output.at("ok") == true, "verify output ok mismatch");
  Require(output.at("verified_properties") == 1,
          "verify output count mismatch");
  Require(output.at("property_results").at(0).at("property_id") ==
              "unit/scalar",
          "verify property id mismatch");
  Require(output.at("property_results").at(0).at("ok") == true,
          "verify property ok mismatch");
  std::filesystem::remove_all(dir);
}

void TestVerifyCommandReportsMissingProperty() {
  const std::filesystem::path dir = MakeTempDir();
  const std::filesystem::path samples = dir / "samples.bbsm.json";
  const std::filesystem::path keys = dir / "keys.bbky.json";
  WriteScalarSampleBundle(samples);
  WriteMatchingKeyBundle(keys);

  bbsolver::KeyBundle bundle = bbsolver::ReadKeyBundleJson(keys);
  bundle.property_results.front().property_id = "unit/missing";
  bbsolver::WriteKeyBundleJson(keys, bundle);

  std::vector<std::string> args = {
      "bbsolver", "verify", keys.string(), samples.string()};
  std::vector<char*> argv = Argv(args);

  StreamCapture capture(std::cout);
  const int rc = bbsolver::RunVerifyCommand(
      static_cast<int>(argv.size()), argv.data());
  Require(rc == bbsolver::kVerifyMismatchExitCode,
          "verify command should reject missing samples with mismatch exit");

  const nlohmann::json output = nlohmann::json::parse(capture.str());
  Require(output.at("ok") == false, "missing-property verify ok mismatch");
  Require(output.at("verified_properties") == 0,
          "missing-property verified count mismatch");
  Require(output.at("property_results").at(0).at("reason") ==
              "missing_samples_for_property_id",
          "missing-property reason mismatch");
  std::filesystem::remove_all(dir);
}

void TestVerifyCommandReportsKeyValueDimensionMismatch() {
  const std::filesystem::path dir = MakeTempDir();
  const std::filesystem::path samples = dir / "samples.bbsm.json";
  const std::filesystem::path keys = dir / "keys.bbky.json";
  WriteScalarSampleBundle(samples);
  WriteMatchingKeyBundle(keys);

  bbsolver::KeyBundle bundle = bbsolver::ReadKeyBundleJson(keys);
  bbsolver::PropertyKeys& property = bundle.property_results.front();
  property.dimensions = 2;
  for (bbsolver::Key& key: property.keys) {
    key.v = {key.v.front(), key.v.front() + 1.0};
  }
  bbsolver::WriteKeyBundleJson(keys, bundle);

  std::vector<std::string> args = {
      "bbsolver", "verify", keys.string(), samples.string()};
  std::vector<char*> argv = Argv(args);

  StreamCapture capture(std::cout);
  const int rc = bbsolver::RunVerifyCommand(
      static_cast<int>(argv.size()), argv.data());
  Require(rc == bbsolver::kVerifyMismatchExitCode,
          "verify command should reject wrong-length key values");

  const nlohmann::json output = nlohmann::json::parse(capture.str());
  Require(output.at("ok") == false, "dimension-mismatch verify ok mismatch");
  Require(output.at("verified_properties") == 0,
          "dimension-mismatch verified count mismatch");
  Require(output.at("property_results").at(0).at("reason") ==
              "key_value_dimension_mismatch",
          "dimension-mismatch reason mismatch");
  std::filesystem::remove_all(dir);
}

void TestVerifyCommandRejectsNonJsonInputs() {
  bool threw = false;
  try {
    std::vector<std::string> args = {
        "bbsolver", "verify", "keys.bbky", "samples.bbsm.json"};
    std::vector<char*> argv = Argv(args);
    (void)bbsolver::RunVerifyCommand(
        static_cast<int>(argv.size()), argv.data());
  } catch (const std::runtime_error& e) {
    threw = std::string(e.what()).find("KeyBundle JSON input only") !=
            std::string::npos;
  }
  Require(threw, "verify command must reject non-json KeyBundle paths");

  threw = false;
  try {
    std::vector<std::string> args = {
        "bbsolver", "verify", "keys.bbky.json", "samples.bbsm"};
    std::vector<char*> argv = Argv(args);
    (void)bbsolver::RunVerifyCommand(
        static_cast<int>(argv.size()), argv.data());
  } catch (const std::runtime_error& e) {
    threw = std::string(e.what()).find("SampleBundle JSON input only") !=
            std::string::npos;
  }
  Require(threw, "verify command must reject non-json SampleBundle paths");
}

void TestVerifyCommandRejectsUnsupportedSchemaVersion() {
  const std::filesystem::path dir = MakeTempDir();
  const std::filesystem::path samples = dir / "samples.bbsm.json";
  const std::filesystem::path keys = dir / "keys.bbky.json";
  WriteScalarSampleBundle(samples);
  WriteMatchingKeyBundle(keys);

  bbsolver::KeyBundle key_bundle = bbsolver::ReadKeyBundleJson(keys);
  key_bundle.schema_version = 999;
  bbsolver::WriteKeyBundleJson(keys, key_bundle);

  bool threw = false;
  try {
    std::vector<std::string> args = {
        "bbsolver", "verify", keys.string(), samples.string()};
    std::vector<char*> argv = Argv(args);
    (void)bbsolver::RunVerifyCommand(
        static_cast<int>(argv.size()), argv.data());
  } catch (const std::runtime_error& e) {
    threw = std::string(e.what()).find(
                "Unsupported KeyBundle schema_version=999") !=
            std::string::npos;
  }
  Require(threw, "verify command must reject unsupported KeyBundle schema");

  WriteMatchingKeyBundle(keys);
  {
    nlohmann::json sample_root;
    {
      std::ifstream input(samples);
      input >> sample_root;
    }
    sample_root["schema_version"] = 999;
    std::ofstream output(samples);
    output << sample_root.dump(2);
  }

  threw = false;
  try {
    std::vector<std::string> args = {
        "bbsolver", "verify", keys.string(), samples.string()};
    std::vector<char*> argv = Argv(args);
    (void)bbsolver::RunVerifyCommand(
        static_cast<int>(argv.size()), argv.data());
  } catch (const std::runtime_error& e) {
    threw = std::string(e.what()).find(
                "Unsupported SampleBundle schema_version=999") !=
            std::string::npos;
  }
  Require(threw, "verify command must reject unsupported SampleBundle schema");
  std::filesystem::remove_all(dir);
}

void TestVerifyCommandRejectsSwappedBundles() {
  const std::filesystem::path dir = MakeTempDir();
  const std::filesystem::path samples = dir / "samples.bbsm.json";
  const std::filesystem::path keys = dir / "keys.bbky.json";
  WriteScalarSampleBundle(samples);
  WriteMatchingKeyBundle(keys);

  bool threw = false;
  try {
    std::vector<std::string> args = {
        "bbsolver", "verify", samples.string(), keys.string()};
    std::vector<char*> argv = Argv(args);
    (void)bbsolver::RunVerifyCommand(
        static_cast<int>(argv.size()), argv.data());
  } catch (const std::runtime_error& e) {
    threw = std::string(e.what()).find(
                "Expected KeyBundle JSON with _schema=\"keys\"") !=
            std::string::npos;
  }
  Require(threw, "verify command must reject SampleBundle in KeyBundle slot");

  threw = false;
  try {
    std::vector<std::string> args = {
        "bbsolver", "verify", samples.string(), samples.string()};
    std::vector<char*> argv = Argv(args);
    (void)bbsolver::RunVerifyCommand(
        static_cast<int>(argv.size()), argv.data());
  } catch (const std::runtime_error& e) {
    threw = std::string(e.what()).find(
                "Expected KeyBundle JSON with _schema=\"keys\"") !=
            std::string::npos;
  }
  Require(threw, "verify command must reject SampleBundle in both slots");

  threw = false;
  try {
    std::vector<std::string> args = {
        "bbsolver", "verify", keys.string(), keys.string()};
    std::vector<char*> argv = Argv(args);
    (void)bbsolver::RunVerifyCommand(
        static_cast<int>(argv.size()), argv.data());
  } catch (const std::runtime_error& e) {
    threw = std::string(e.what()).find(
                "Expected SampleBundle JSON with _schema=\"samples\"") !=
            std::string::npos;
  }
  Require(threw, "verify command must reject KeyBundle in SampleBundle slot");

  std::filesystem::remove_all(dir);
}

void TestVerifyCommandRejectsEmptyKeyBundle() {
  const std::filesystem::path dir = MakeTempDir();
  const std::filesystem::path samples = dir / "samples.bbsm.json";
  const std::filesystem::path keys = dir / "empty.bbky.json";
  WriteScalarSampleBundle(samples);
  {
    std::ofstream out(keys);
    out << R"JSON({
  "_schema": "keys",
  "schema_version": 1,
  "request_id": "verify-command-unit",
  "property_results": []
})JSON";
  }

  bool threw = false;
  try {
    std::vector<std::string> args = {
        "bbsolver", "verify", keys.string(), samples.string()};
    std::vector<char*> argv = Argv(args);
    (void)bbsolver::RunVerifyCommand(
        static_cast<int>(argv.size()), argv.data());
  } catch (const std::runtime_error& e) {
    threw = std::string(e.what()).find(
                "KeyBundle property_results must not be empty") !=
            std::string::npos;
  }
  Require(threw, "verify command must reject empty KeyBundle results");
  std::filesystem::remove_all(dir);
}

void TestDumpCommandPrettyPrintsBundleJson() {
  const std::filesystem::path dir = MakeTempDir();
  const std::filesystem::path json_path = dir / "bundle.bbsm.json";
  WriteScalarSampleBundle(json_path);

  std::vector<std::string> args = {"bbsolver", "dump", json_path.string()};
  std::vector<char*> argv = Argv(args);

  StreamCapture capture(std::cout);
  const int rc = bbsolver::RunDumpCommand(
      static_cast<int>(argv.size()), argv.data());
  Require(rc == 0, "dump command should accept json input");
  const nlohmann::json output = nlohmann::json::parse(capture.str());
  Require(output.at("_schema") == "samples",
          "dump command must pretty-print a bundle");
  std::filesystem::remove_all(dir);
}

void TestDumpCommandRejectsNonJsonInput() {
  bool threw = false;
  try {
    std::vector<std::string> args = {"bbsolver", "dump", "bundle.bbky"};
    std::vector<char*> argv = Argv(args);
    (void)bbsolver::RunDumpCommand(
        static_cast<int>(argv.size()), argv.data());
  } catch (const std::runtime_error& e) {
    threw = std::string(e.what()).find("JSON bundles only") !=
            std::string::npos;
  }
  Require(threw, "dump command must reject non-json bundle paths");
}

void TestDumpCommandRejectsNonBundleJson() {
  const std::filesystem::path dir = MakeTempDir();
  const std::filesystem::path json_path = dir / "not-a-bundle.json";
  {
    std::ofstream out(json_path);
    out << "{\"ok\":true}\n";
  }

  bool threw = false;
  try {
    std::vector<std::string> args = {"bbsolver", "dump", json_path.string()};
    std::vector<char*> argv = Argv(args);
    (void)bbsolver::RunDumpCommand(
        static_cast<int>(argv.size()), argv.data());
  } catch (const std::runtime_error& e) {
    threw = std::string(e.what()).find(
                "SampleBundle or KeyBundle JSON only") != std::string::npos;
  }
  Require(threw, "dump command must reject non-bundle JSON");

  {
    std::ofstream out(json_path);
    out << "{\"_schema\":\"keys\"}\n";
  }
  threw = false;
  try {
    std::vector<std::string> args = {"bbsolver", "dump", json_path.string()};
    std::vector<char*> argv = Argv(args);
    (void)bbsolver::RunDumpCommand(
        static_cast<int>(argv.size()), argv.data());
  } catch (const std::runtime_error& e) {
    threw = std::string(e.what()).find(
                "SampleBundle or KeyBundle JSON only") != std::string::npos;
  }
  Require(threw, "dump command must reject bundle JSON without schema_version");

  {
    std::ofstream out(json_path);
    out << R"JSON({
  "_schema": "samples",
  "schema_version": 1,
  "comp": {"fps": 24.0, "duration_sec": 1.0},
  "properties": "not an array"
})JSON";
  }
  threw = false;
  try {
    std::vector<std::string> args = {"bbsolver", "dump", json_path.string()};
    std::vector<char*> argv = Argv(args);
    (void)bbsolver::RunDumpCommand(
        static_cast<int>(argv.size()), argv.data());
  } catch (const std::runtime_error& e) {
    threw = std::string(e.what()).find(
                "SampleBundle properties must be an array") !=
            std::string::npos;
  }
  Require(threw, "dump command must reject malformed SampleBundle JSON");

  {
    std::ofstream out(json_path);
    out << R"JSON({
  "_schema": "keys",
  "schema_version": 1,
  "property_results": "not an array"
})JSON";
  }
  threw = false;
  try {
    std::vector<std::string> args = {"bbsolver", "dump", json_path.string()};
    std::vector<char*> argv = Argv(args);
    (void)bbsolver::RunDumpCommand(
        static_cast<int>(argv.size()), argv.data());
  } catch (const std::runtime_error& e) {
    threw = std::string(e.what()).find(
                "KeyBundle property_results must be an array") !=
            std::string::npos;
  }
  Require(threw, "dump command must reject malformed KeyBundle JSON");

  {
    std::ofstream out(json_path);
    out << R"JSON({
  "_schema": "keys",
  "schema_version": 1,
  "property_results": []
})JSON";
  }
  threw = false;
  try {
    std::vector<std::string> args = {"bbsolver", "dump", json_path.string()};
    std::vector<char*> argv = Argv(args);
    (void)bbsolver::RunDumpCommand(
        static_cast<int>(argv.size()), argv.data());
  } catch (const std::runtime_error& e) {
    threw = std::string(e.what()).find(
                "KeyBundle property_results must not be empty") !=
            std::string::npos;
  }
  Require(threw, "dump command must reject empty KeyBundle results");

  {
    std::ofstream out(json_path);
    out << R"JSON({
  "_schema": "keys",
  "schema_version": 1,
  "property_results": [{"property_id": "prop", "dimensions": 1}]
})JSON";
  }
  threw = false;
  try {
    std::vector<std::string> args = {"bbsolver", "dump", json_path.string()};
    std::vector<char*> argv = Argv(args);
    (void)bbsolver::RunDumpCommand(
        static_cast<int>(argv.size()), argv.data());
  } catch (const std::runtime_error& e) {
    threw = std::string(e.what()).find(
                "KeyBundle property_results entry keys must be an array") !=
            std::string::npos;
  }
  Require(threw, "dump command must reject KeyBundle results without keys");

  {
    std::ofstream out(json_path);
    out << R"JSON({
  "_schema": "keys",
  "schema_version": 1,
  "property_results": [{"property_id": "prop", "dimensions": 1, "keys": "not an array"}]
})JSON";
  }
  threw = false;
  try {
    std::vector<std::string> args = {"bbsolver", "dump", json_path.string()};
    std::vector<char*> argv = Argv(args);
    (void)bbsolver::RunDumpCommand(
        static_cast<int>(argv.size()), argv.data());
  } catch (const std::runtime_error& e) {
    threw = std::string(e.what()).find(
                "KeyBundle property_results entry keys must be an array") !=
            std::string::npos;
  }
  Require(threw, "dump command must reject non-array KeyBundle keys");

  {
    std::ofstream out(json_path);
    out << R"JSON({
  "_schema": "keys",
  "schema_version": 1,
  "property_results": [{"property_id": "prop", "dimensions": 1, "keys": []}]
})JSON";
  }
  threw = false;
  try {
    std::vector<std::string> args = {"bbsolver", "dump", json_path.string()};
    std::vector<char*> argv = Argv(args);
    (void)bbsolver::RunDumpCommand(
        static_cast<int>(argv.size()), argv.data());
  } catch (const std::runtime_error& e) {
    threw = std::string(e.what()).find(
                "KeyBundle converged property_results entry keys must not be empty") !=
            std::string::npos;
  }
  Require(threw, "dump command must reject empty converged KeyBundle keys");

  {
    std::ofstream out(json_path);
    out << R"JSON({
  "_schema": "keys",
  "schema_version": 1,
  "property_results": [
    {
      "property_id": "prop",
      "dimensions": 1,
      "converged": false,
      "notes": "cancelled",
      "keys": []
    }
  ]
})JSON";
  }
  {
    std::ofstream out(json_path);
    out << R"JSON({
  "_schema": "keys",
  "schema_version": 1,
  "property_results": [
    {
      "property_id": "prop",
      "dimensions": 1,
      "keys": [
        {"t_sec": 0.0, "v": [0.0, 1.0]}
      ]
    }
  ]
})JSON";
  }
  threw = false;
  try {
    std::vector<std::string> args = {"bbsolver", "dump", json_path.string()};
    std::vector<char*> argv = Argv(args);
    (void)bbsolver::RunDumpCommand(
        static_cast<int>(argv.size()), argv.data());
  } catch (const std::runtime_error& e) {
    threw = std::string(e.what()).find(
                "KeyBundle key v length must equal property_results dimensions") !=
            std::string::npos;
  }
  Require(threw, "dump command must reject wrong-length KeyBundle key values");

  {
    std::ofstream out(json_path);
    out << R"JSON({
  "_schema": "samples",
  "schema_version": 1,
  "comp": {"fps": 24.0, "duration_sec": 1.0},
  "properties": [
    {
      "property": {"id": "unit/scalar", "kind": "Scalar", "dimensions": 1},
      "samples": [
        {"t_sec": 0.0, "v": [0.0, 1.0]}
      ]
    }
  ]
})JSON";
  }
  threw = false;
  try {
    std::vector<std::string> args = {"bbsolver", "dump", json_path.string()};
    std::vector<char*> argv = Argv(args);
    (void)bbsolver::RunDumpCommand(
        static_cast<int>(argv.size()), argv.data());
  } catch (const std::runtime_error& e) {
    threw = std::string(e.what()).find(
                "SampleBundle sample v length must equal dimensions times samples_per_frame") !=
            std::string::npos;
  }
  Require(threw, "dump command must reject wrong-length SampleBundle values");

  {
    std::ofstream out(json_path);
    out << R"JSON({
  "_schema": "keys",
  "schema_version": 1,
  "property_results": [
    {
      "property_id": "prop",
      "dimensions": 1,
      "converged": false,
      "notes": "cancelled",
      "keys": []
    }
  ]
})JSON";
  }
  {
    std::vector<std::string> args = {"bbsolver", "dump", json_path.string()};
    std::vector<char*> argv = Argv(args);
    StreamCapture capture(std::cout);
    const int rc = bbsolver::RunDumpCommand(
        static_cast<int>(argv.size()), argv.data());
    Require(rc == 0, "dump command should accept cancelled empty-key results");
  }

  std::filesystem::remove_all(dir);
}

void TestCommandUsageErrors() {
  std::vector<std::string> verify_args = {"bbsolver", "verify"};
  std::vector<char*> verify_argv = Argv(verify_args);
  StreamCapture verify_err(std::cerr);
  Require(bbsolver::RunVerifyCommand(
              static_cast<int>(verify_argv.size()), verify_argv.data()) == 2,
          "verify usage error should return 2");
  Require(verify_err.str().find("bbsolver verify") != std::string::npos,
          "verify usage error should print usage");

  std::vector<std::string> dump_args = {"bbsolver", "dump"};
  std::vector<char*> dump_argv = Argv(dump_args);
  StreamCapture dump_err(std::cerr);
  Require(bbsolver::RunDumpCommand(
              static_cast<int>(dump_argv.size()), dump_argv.data()) == 2,
          "dump usage error should return 2");
  Require(dump_err.str().find("bbsolver dump") != std::string::npos,
          "dump usage error should print usage");
}

// ============================================================================
// Variable-topology shape_flat verify-rule tests.
//
// Background: variable-topology shape_flat bundles (after-effects mask paths
// whose vertex count changes across keyframes) carry keys with len = 2 + 6
// * vertex_count, where vertex_count is encoded in v[1] and the leading 2
// floats are a header pair. The bbsm-side `dimensions` field reflects the
// max-topology count (2 + 6 * shape_max_vertex_count), so the per-key
// length is legitimately shorter than the bbsm-declared dimension. The
// verifier accepts these via a verifier-scoped relaxed parse path plus a
// per-key rule that gates on units_label==shape_flat && shape_variable_topology.
// Non-shape and fixed-topology paths retain the strict gate.
// ============================================================================

void WriteShapeFlatSampleBundle(const std::filesystem::path& path,
                                int max_vertex_count = 5,
                                int sample_vertex_count = 3,
                                bool variable_topology = true,
                                int dimensions_override = -1) {
  const int dimensions = (dimensions_override >= 0)
      ? dimensions_override
: (2 + 6 * max_vertex_count);
  const int sample_len = 2 + 6 * sample_vertex_count;
  std::ostringstream sample_v0;
  std::ostringstream sample_v1;
  sample_v0 << "[1.0, " << sample_vertex_count;
  sample_v1 << "[1.0, " << sample_vertex_count;
  for (int i = 2; i < sample_len; ++i) {
    sample_v0 << ", 0.0";
    sample_v1 << ", 1.0";
  }
  sample_v0 << "]";
  sample_v1 << "]";

  std::ofstream out(path);
  out << R"JSON({
  "_schema": "samples",
  "schema_version": 1,
  "request_id": "verify-shape-flat-unit",
  "comp": {
    "fps": 24.0,
    "duration_sec": 1.0,
    "width": 1920,
    "height": 1080,
    "pixel_aspect": 1.0,
    "shutter_angle_deg": 180.0,
    "shutter_phase_deg": -90.0,
    "motion_blur_enabled": false,
    "work_area_start_sec": 0.0,
    "work_area_end_sec": 1.0
  },
  "properties": [
    {
      "property": {
        "id": "unit/shape",
        "match_name": "ADBE Vector Shape",
        "display_name": "Shape",
        "kind": "Custom",
        "dimensions": )JSON" << dimensions << R"JSON(,
        "is_spatial": false,
        "is_separated": false,
        "units_label": "shape_flat",
        "min_value": [],
        "max_value": [],
        "shape_variable_topology": )JSON"
      << (variable_topology ? "true": "false")
      << R"JSON(,
        "shape_canonical_method": "shape_flat_raw_variable",
        "shape_canonical_vertex_count": 0,
        "shape_max_vertex_count": )JSON" << max_vertex_count << R"JSON(
      },
      "t_start_sec": 0.0,
      "t_end_sec": 1.0,
      "samples_per_frame": 1,
      "samples": [
        {"t_sec": 0.0, "v": )JSON" << sample_v0.str() << R"JSON(},
        {"t_sec": 1.0, "v": )JSON" << sample_v1.str() << R"JSON(}
      ]
    }
  ],
  "config": {
    "tolerance": 1.0,
    "tolerance_screen_px": 1.0,
    "weight_screen": 0.0,
    "allow_hold": true,
    "allow_linear": true,
    "allow_bezier": true
  }
})JSON";
}

std::vector<double> MakeShapeFlatVector(int vertex_count, double fill = 0.0) {
  std::vector<double> v;
  v.reserve(static_cast<std::size_t>(2 + 6 * vertex_count));
  v.push_back(1.0);
  v.push_back(static_cast<double>(vertex_count));
  for (int i = 0; i < 6 * vertex_count; ++i) {
    v.push_back(fill);
  }
  return v;
}

bbsolver::Key MakeShapeFlatKey(double t, std::vector<double> v) {
  bbsolver::Key key;
  key.t_sec = t;
  key.v = std::move(v);
  key.interp_in = bbsolver::InterpType::Linear;
  key.interp_out = bbsolver::InterpType::Linear;
  key.temporal_ease_in = {bbsolver::TemporalEase{}};
  key.temporal_ease_out = {bbsolver::TemporalEase{}};
  return key;
}

void WriteShapeFlatKeyBundle(const std::filesystem::path& path,
                             std::vector<bbsolver::Key> keys,
                             int dimensions_override = -1) {
  bbsolver::PropertyKeys property;
  property.property_id = "unit/shape";
  // Match the AE harness's behaviour: per-property dimensions reports the
  // first key's length, which for VT is advisory rather than invariant.
  property.dimensions = (dimensions_override >= 0)
      ? dimensions_override
: (keys.empty() ? 0: static_cast<int>(keys.front().v.size()));
  property.converged = true;
  property.keys = std::move(keys);

  bbsolver::KeyBundle bundle;
  bundle.request_id = "verify-shape-flat-unit";
  bundle.solver_version = "unit";
  bundle.total_keys = static_cast<int>(property.keys.size());
  bundle.property_results.push_back(std::move(property));
  bbsolver::WriteKeyBundleJson(path, bundle);
}

// Helper: assert the verifier reached the sample-aware path and did not
// emit either schema-rejection reason. Used by VT shape_flat tests where
// the synthetic error measurement may legitimately exceed ε; what we
// care about is that the relaxed gate didn't reject the structure.
void RequireVerifierAcceptedStructure(const nlohmann::json& output) {
  Require(output.at("verified_properties").get<int>() >= 1,
          "verifier must reach the sample-aware path for VT shape_flat");
  for (const auto& entry: output.at("property_results")) {
    if (entry.contains("reason")) {
      const std::string reason = entry.at("reason").get<std::string>();
      Require(reason != "key_value_dimension_mismatch",
              "VT shape_flat must not be rejected via the strict gate");
      Require(reason != "invalid_shape_flat_key_dimensions",
              "well-formed VT shape_flat keys must not be flagged as malformed");
    }
  }
}

void TestVerifyCommandAcceptsVariableTopologyShapeFlatKeys() {
  const std::filesystem::path dir = MakeTempDir();
  const std::filesystem::path samples = dir / "samples.bbsm.json";
  const std::filesystem::path keys = dir / "keys.bbky.json";
  // shape_max_vertex_count=5, sample_vertex_count=3, so bbsm declares
  // dimensions = 2 + 6 * 5 = 32, but per-key bbky v[] is 2 + 6 * 3 = 20.
  // Pre-fix this triggered key_value_dimension_mismatch; the relaxed
  // gate must now accept it and run the sample-aware verifier.
  WriteShapeFlatSampleBundle(samples, /*max_vertex_count=*/5,
                             /*sample_vertex_count=*/3);
  WriteShapeFlatKeyBundle(keys, {
      MakeShapeFlatKey(0.0, MakeShapeFlatVector(/*vertex_count=*/3, 0.0)),
      MakeShapeFlatKey(1.0, MakeShapeFlatVector(/*vertex_count=*/3, 1.0)),
  });

  std::vector<std::string> args = {
      "bbsolver", "verify", keys.string(), samples.string()};
  std::vector<char*> argv = Argv(args);
  StreamCapture capture(std::cout);
  (void)bbsolver::RunVerifyCommand(
      static_cast<int>(argv.size()), argv.data());
  const nlohmann::json output = nlohmann::json::parse(capture.str());
  RequireVerifierAcceptedStructure(output);
  std::filesystem::remove_all(dir);
}

void TestVerifyCommandAcceptsPerKeyVaryingShapeFlatTopology() {
  const std::filesystem::path dir = MakeTempDir();
  const std::filesystem::path samples = dir / "samples.bbsm.json";
  const std::filesystem::path keys = dir / "keys.bbky.json";
  // shape_max_vertex_count=8 spans the full per-key range we exercise.
  WriteShapeFlatSampleBundle(samples, /*max_vertex_count=*/8,
                             /*sample_vertex_count=*/4);
  // Per-key vertex count varies inside a single bundle (this is the
  // blob-v1 / blob-v6 case from the paper corpus).
  WriteShapeFlatKeyBundle(keys, {
      MakeShapeFlatKey(0.0, MakeShapeFlatVector(/*vertex_count=*/3)),
      MakeShapeFlatKey(0.5, MakeShapeFlatVector(/*vertex_count=*/5)),
      MakeShapeFlatKey(1.0, MakeShapeFlatVector(/*vertex_count=*/4)),
  });

  std::vector<std::string> args = {
      "bbsolver", "verify", keys.string(), samples.string()};
  std::vector<char*> argv = Argv(args);
  StreamCapture capture(std::cout);
  (void)bbsolver::RunVerifyCommand(
      static_cast<int>(argv.size()), argv.data());
  const nlohmann::json output = nlohmann::json::parse(capture.str());
  RequireVerifierAcceptedStructure(output);
  std::filesystem::remove_all(dir);
}

void TestVerifyCommandRejectsMalformedShapeFlatKeyLength() {
  const std::filesystem::path dir = MakeTempDir();
  const std::filesystem::path samples = dir / "samples.bbsm.json";
  const std::filesystem::path keys = dir / "keys.bbky.json";
  WriteShapeFlatSampleBundle(samples, /*max_vertex_count=*/5,
                             /*sample_vertex_count=*/3);
  // Corrupt one key: claim vertex_count=3 but truncate the v[] array so
  // len != 2 + 6 * 3 = 20.
  std::vector<double> truncated = MakeShapeFlatVector(/*vertex_count=*/3);
  truncated.pop_back();
  truncated.pop_back();
  WriteShapeFlatKeyBundle(keys, {
      MakeShapeFlatKey(0.0, MakeShapeFlatVector(/*vertex_count=*/3)),
      MakeShapeFlatKey(1.0, std::move(truncated)),
  });

  std::vector<std::string> args = {
      "bbsolver", "verify", keys.string(), samples.string()};
  std::vector<char*> argv = Argv(args);
  StreamCapture capture(std::cout);
  const int rc = bbsolver::RunVerifyCommand(
      static_cast<int>(argv.size()), argv.data());
  Require(rc == bbsolver::kVerifyMismatchExitCode,
          "verify must reject malformed shape_flat keys");
  const nlohmann::json output = nlohmann::json::parse(capture.str());
  Require(output.at("property_results").at(0).at("reason") ==
              "invalid_shape_flat_key_dimensions",
          "malformed VT key must use the dedicated reason code");
  const nlohmann::json malformed =
      output.at("property_results").at(0).at("malformed_keys");
  Require(malformed.is_array() && !malformed.empty(),
          "malformed_keys must be a non-empty array");
  Require(malformed.at(0).at("key_index") == 1,
          "malformed_keys must record the offending key index");
  Require(malformed.at(0).at("expected_length") == 20,
          "malformed_keys must report the expected length");
  std::filesystem::remove_all(dir);
}

void TestVerifyCommandRejectsOversizedShapeFlatKey() {
  const std::filesystem::path dir = MakeTempDir();
  const std::filesystem::path samples = dir / "samples.bbsm.json";
  const std::filesystem::path keys = dir / "keys.bbky.json";
  WriteShapeFlatSampleBundle(samples, /*max_vertex_count=*/4,
                             /*sample_vertex_count=*/3);
  // A key claiming 10 vertices when shape_max_vertex_count is 4 must be
  // rejected, even though its v[] length is internally consistent.
  WriteShapeFlatKeyBundle(keys, {
      MakeShapeFlatKey(0.0, MakeShapeFlatVector(/*vertex_count=*/3)),
      MakeShapeFlatKey(1.0, MakeShapeFlatVector(/*vertex_count=*/10)),
  });

  std::vector<std::string> args = {
      "bbsolver", "verify", keys.string(), samples.string()};
  std::vector<char*> argv = Argv(args);
  StreamCapture capture(std::cout);
  const int rc = bbsolver::RunVerifyCommand(
      static_cast<int>(argv.size()), argv.data());
  Require(rc == bbsolver::kVerifyMismatchExitCode,
          "verify must reject VT keys whose vertex_count exceeds "
          "shape_max_vertex_count");
  const nlohmann::json output = nlohmann::json::parse(capture.str());
  Require(output.at("property_results").at(0).at("reason") ==
              "invalid_shape_flat_key_dimensions",
          "over-max VT key must use the dedicated reason code");
  std::filesystem::remove_all(dir);
}

void TestVerifyCommandRejectsInconsistentShapeFlatMetadata() {
  const std::filesystem::path dir = MakeTempDir();
  const std::filesystem::path samples = dir / "samples.bbsm.json";
  const std::filesystem::path keys = dir / "keys.bbky.json";
  // dimensions=33 contradicts shape_max_vertex_count=5 (expected = 32).
  WriteShapeFlatSampleBundle(samples, /*max_vertex_count=*/5,
                             /*sample_vertex_count=*/3,
                             /*variable_topology=*/true,
                             /*dimensions_override=*/33);
  WriteShapeFlatKeyBundle(keys, {
      MakeShapeFlatKey(0.0, MakeShapeFlatVector(/*vertex_count=*/3)),
      MakeShapeFlatKey(1.0, MakeShapeFlatVector(/*vertex_count=*/3)),
  });

  std::vector<std::string> args = {
      "bbsolver", "verify", keys.string(), samples.string()};
  std::vector<char*> argv = Argv(args);
  StreamCapture capture(std::cout);
  bool threw = false;
  std::string thrown_msg;
  try {
    (void)bbsolver::RunVerifyCommand(
        static_cast<int>(argv.size()), argv.data());
  } catch (const std::runtime_error& e) {
    threw = true;
    thrown_msg = e.what();
  }
  if (threw) {
    // bbsm-side metadata may be caught at sample-bundle validation; that
    // is acceptable as long as the contract violation is reported.
    Require(thrown_msg.find("shape_flat") != std::string::npos,
            "metadata inconsistency must mention shape_flat");
  } else {
    const nlohmann::json output = nlohmann::json::parse(capture.str());
    Require(output.at("property_results").at(0).at("reason") ==
                "invalid_shape_flat_sample_metadata",
            "metadata-inconsistent bundle must use the dedicated reason");
  }
  std::filesystem::remove_all(dir);
}

void TestVerifyCommandFixedTopologyShapeFlatStaysStrict() {
  const std::filesystem::path dir = MakeTempDir();
  const std::filesystem::path samples = dir / "samples.bbsm.json";
  const std::filesystem::path keys = dir / "keys.bbky.json";
  // shape_variable_topology=false: strict per-key length gate applies.
  WriteShapeFlatSampleBundle(samples, /*max_vertex_count=*/3,
                             /*sample_vertex_count=*/3,
                             /*variable_topology=*/false);
  // Key with the wrong v[] length must be rejected with the legacy reason.
  WriteShapeFlatKeyBundle(keys, {
      MakeShapeFlatKey(0.0, MakeShapeFlatVector(/*vertex_count=*/2)),
  }, /*dimensions_override=*/14);  // 14 != bbsm dimensions of 20.

  std::vector<std::string> args = {
      "bbsolver", "verify", keys.string(), samples.string()};
  std::vector<char*> argv = Argv(args);
  StreamCapture capture(std::cout);
  const int rc = bbsolver::RunVerifyCommand(
      static_cast<int>(argv.size()), argv.data());
  Require(rc == bbsolver::kVerifyMismatchExitCode,
          "fixed-topology shape_flat must keep the strict key-dim gate");
  const nlohmann::json output = nlohmann::json::parse(capture.str());
  Require(output.at("property_results").at(0).at("reason") ==
              "key_value_dimension_mismatch",
          "fixed-topology mismatch must use the legacy reason code");
  std::filesystem::remove_all(dir);
}

void TestVerifyCommandScalarMismatchStaysStrict() {
  // Non-shape properties (units_label != "shape_flat") must keep the
  // strict gate regardless of any spurious shape_variable_topology flag.
  // This guards against future regressions that might leak the relaxed
  // gate into non-shape properties.
  const std::filesystem::path dir = MakeTempDir();
  const std::filesystem::path samples = dir / "samples.bbsm.json";
  const std::filesystem::path keys = dir / "keys.bbky.json";
  WriteScalarSampleBundle(samples);
  WriteMatchingKeyBundle(keys);

  bbsolver::KeyBundle bundle = bbsolver::ReadKeyBundleJson(keys);
  bundle.property_results.front().dimensions = 2;
  for (bbsolver::Key& key: bundle.property_results.front().keys) {
    key.v = {key.v.front(), key.v.front() + 1.0};
  }
  bbsolver::WriteKeyBundleJson(keys, bundle);

  std::vector<std::string> args = {
      "bbsolver", "verify", keys.string(), samples.string()};
  std::vector<char*> argv = Argv(args);
  StreamCapture capture(std::cout);
  const int rc = bbsolver::RunVerifyCommand(
      static_cast<int>(argv.size()), argv.data());
  Require(rc == bbsolver::kVerifyMismatchExitCode,
          "non-shape mismatch must keep the strict gate");
  const nlohmann::json output = nlohmann::json::parse(capture.str());
  Require(output.at("property_results").at(0).at("reason") ==
              "key_value_dimension_mismatch",
          "non-shape mismatch must use the legacy reason code");
  std::filesystem::remove_all(dir);
}

}  // namespace

int main() {
  TestVerifyCommandReportsOkJson();
  TestVerifyCommandReportsMissingProperty();
  TestVerifyCommandReportsKeyValueDimensionMismatch();
  TestVerifyCommandRejectsNonJsonInputs();
  TestVerifyCommandRejectsUnsupportedSchemaVersion();
  TestVerifyCommandRejectsSwappedBundles();
  TestVerifyCommandRejectsEmptyKeyBundle();
  TestDumpCommandPrettyPrintsBundleJson();
  TestDumpCommandRejectsNonJsonInput();
  TestDumpCommandRejectsNonBundleJson();
  TestCommandUsageErrors();
  TestVerifyCommandAcceptsVariableTopologyShapeFlatKeys();
  TestVerifyCommandAcceptsPerKeyVaryingShapeFlatTopology();
  TestVerifyCommandRejectsMalformedShapeFlatKeyLength();
  TestVerifyCommandRejectsOversizedShapeFlatKey();
  TestVerifyCommandRejectsInconsistentShapeFlatMetadata();
  TestVerifyCommandFixedTopologyShapeFlatStaysStrict();
  TestVerifyCommandScalarMismatchStaysStrict();
  std::cout << "verify/dump command tests passed\n";
  return 0;
}
