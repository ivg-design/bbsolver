#include "bbsolver/solve/solve_command_config.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/app/cli_options.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

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
      ("bb_solve_command_config_test_" + std::to_string(stamp));
  std::filesystem::create_directories(dir);
  return dir;
}

void WriteMinimalSampleBundle(const std::filesystem::path& path,
                              int schema_version) {
  std::ofstream out(path);
  out << R"JSON({
  "_schema": "samples",
  "schema_version": )JSON"
      << schema_version << R"JSON(,
  "request_id": "solve-command-config-unit",
  "comp": {
    "fps": 24.0,
    "duration_sec": 1.0
  },
  "properties": [
    {
      "property": {
        "id": "unit/scalar",
        "kind": "Scalar",
        "dimensions": 1
      },
      "samples": [
        {"t_sec": 0.0, "v": [0.0]},
        {"t_sec": 1.0, "v": [1.0]}
      ]
    }
  ],
  "config": {}
})JSON";
}

void WriteRawJson(const std::filesystem::path& path,
                  const std::string& text) {
  std::ofstream out(path);
  out << text;
}

void TestParseSolveCommandConfigRequiresJsonInputAndOutput() {
  bool threw = false;
  try {
    std::vector<std::string> args = {
        "bbsolver", "solve", "input.bbsm", "output.bbky.json"};
    std::vector<char*> argv = Argv(args);
    (void)bbsolver::ParseSolveCommandConfig(
        static_cast<int>(argv.size()), argv.data());
  } catch (const std::runtime_error& e) {
    threw = std::string(e.what()).find("SampleBundle JSON input only") !=
            std::string::npos;
  }
  assert(threw);

  threw = false;
  try {
    std::vector<std::string> args = {
        "bbsolver", "solve", "input.bbsm.json", "output.bbky"};
    std::vector<char*> argv = Argv(args);
    (void)bbsolver::ParseSolveCommandConfig(
        static_cast<int>(argv.size()), argv.data());
  } catch (const std::runtime_error& e) {
    threw = std::string(e.what()).find("KeyBundle JSON output only") !=
            std::string::npos;
  }
  assert(threw);
}

void TestPrepareSolveCommandAcceptsSupportedSchemaVersion() {
  const std::filesystem::path dir = MakeTempDir();
  const std::filesystem::path input = dir / "input.bbsm.json";
  const std::filesystem::path output = dir / "output.bbky.json";
  WriteMinimalSampleBundle(input, 1);

  std::vector<std::string> args = {
      "bbsolver", "solve", input.string(), output.string()};
  std::vector<char*> argv = Argv(args);
  const bbsolver::SolveCommandConfig command =
      bbsolver::PrepareSolveCommand(static_cast<int>(argv.size()), argv.data());

  assert(command.samples.schema_version == 1);
  assert(command.keys.schema_version == 1);
  assert(command.keys.request_id == "solve-command-config-unit");
  std::filesystem::remove_all(dir);
}

void TestPrepareSolveCommandRejectsUnsupportedSchemaVersion() {
  const std::filesystem::path dir = MakeTempDir();
  const std::filesystem::path input = dir / "input.bbsm.json";
  const std::filesystem::path output = dir / "output.bbky.json";
  WriteMinimalSampleBundle(input, 999);

  bool threw = false;
  try {
    std::vector<std::string> args = {
        "bbsolver", "solve", input.string(), output.string()};
    std::vector<char*> argv = Argv(args);
    (void)bbsolver::PrepareSolveCommand(
        static_cast<int>(argv.size()), argv.data());
  } catch (const std::runtime_error& e) {
    const std::string message = e.what();
    threw = message.find("Unsupported SampleBundle schema_version=999") !=
                std::string::npos &&
            message.find("schema_version=1") != std::string::npos;
  }
  assert(threw);
  std::filesystem::remove_all(dir);
}

void TestPrepareSolveCommandRejectsMalformedSampleBundleIdentity() {
  const std::filesystem::path dir = MakeTempDir();
  const std::filesystem::path input = dir / "input.bbsm.json";
  const std::filesystem::path output = dir / "output.bbky.json";

  const auto expect_throw = [&](const std::string& json,
                                const std::string& expected_message) {
    WriteRawJson(input, json);
    bool threw = false;
    try {
      std::vector<std::string> args = {
          "bbsolver", "solve", input.string(), output.string()};
      std::vector<char*> argv = Argv(args);
      (void)bbsolver::PrepareSolveCommand(
          static_cast<int>(argv.size()), argv.data());
    } catch (const std::runtime_error& e) {
      threw = std::string(e.what()).find(expected_message) !=
              std::string::npos;
    }
    assert(threw);
  };

  expect_throw(
      R"JSON({"schema_version":1,"comp":{"fps":24.0},"properties":[]})JSON",
      "Expected SampleBundle JSON with _schema=\"samples\"");
  expect_throw(
      R"JSON({"_schema":"keys","schema_version":1,"property_results":[]})JSON",
      "Expected SampleBundle JSON with _schema=\"samples\"");
  expect_throw(
      R"JSON({"_schema":"samples","schema_version":"1","comp":{"fps":24.0},"properties":[]})JSON",
      "Expected SampleBundle JSON with integer schema_version");
  expect_throw(
      R"JSON({"_schema":"samples","schema_version":1,"comp":{"fps":24.0},"properties":[]})JSON",
      "SampleBundle properties must not be empty");
  expect_throw(
      R"JSON({"_schema":"samples","schema_version":1,"comp":{"fps":24.0},"properties":"not an array"})JSON",
      "SampleBundle properties must be an array");
  expect_throw(
      R"JSON({"_schema":"samples","schema_version":1,"comp":{"fps":24.0},"properties":[{"property":{"id":"unit/scalar","kind":"Scalar","dimensions":1}}]})JSON",
      "SampleBundle property entry samples must be an array");
  expect_throw(
      R"JSON({"_schema":"samples","schema_version":1,"comp":{"fps":24.0},"properties":[{"property":{"id":"unit/scalar","kind":"Scalar","dimensions":1},"samples":"not an array"}]})JSON",
      "SampleBundle property entry samples must be an array");
  expect_throw(
      R"JSON({"_schema":"samples","schema_version":1,"comp":{"fps":24.0},"properties":[{"property":{"id":"unit/scalar","kind":"Scalar","dimensions":1},"samples":[]}]})JSON",
      "SampleBundle property entry samples must not be empty");
  expect_throw(
      R"JSON({"_schema":"samples","schema_version":1,"comp":{"fps":24.0},"properties":[{"property":{"id":"unit/scalar","kind":"Scalar","dimensions":1},"samples":[{"t_sec":0.0,"v":[0.0,1.0]}]}]})JSON",
      "SampleBundle sample v length must equal dimensions times samples_per_frame");

  std::filesystem::remove_all(dir);
}

void TestApplySolveOptionsToSampleBundleCopiesCliFields() {
  bbsolver::SampleBundle samples;
  samples.config.solve_optimization_mode = "auto";

  bbsolver::SolveOptions options;
  options.tolerance = 1.25;
  options.screen_px = 2.5;
  options.verbose = true;
  options.fit_canonical_paths = true;
  options.fit_replacement_paths = true;
  options.solve_optimization_mode = "vertex-only";

  bbsolver::ApplySolveOptionsToSampleBundle(samples, options, 7);

  assert(std::abs(samples.config.tolerance - 1.25) < 1e-12);
  assert(std::abs(samples.config.tolerance_screen_px - 2.5) < 1e-12);
  assert(samples.config.parallel_jobs == 7);
  assert(samples.config.verbose == true);
  assert(samples.config.solve_optimization_mode == "vertex_only");
  assert(samples.config.allow_path_spatial_fit == true);
  assert(samples.config.allow_path_replacement_fit == true);
}

void TestApplySolveOptionsToSampleBundleNormalizesExistingMode() {
  bbsolver::SampleBundle samples;
  samples.config.solve_optimization_mode = "default";

  bbsolver::SolveOptions options;

  bbsolver::ApplySolveOptionsToSampleBundle(samples, options, 1);

  assert(samples.config.solve_optimization_mode == "full");
}

void TestMakeInitialKeyBundlePreservesRequestIdentity() {
  bbsolver::SampleBundle samples;
  samples.schema_version = 9;
  samples.request_id = "request-123";

  const bbsolver::KeyBundle keys = bbsolver::MakeInitialKeyBundle(samples);

  assert(keys.schema_version == 9);
  assert(keys.request_id == "request-123");
  assert(keys.solver_version == bbsolver::kBbsolverVersion);
  assert(keys.solver_build == "dev");
  assert(keys.total_keys == 0);
  assert(keys.total_samples_input == 0);
  assert(keys.property_results.empty());
}

}  // namespace

int main() {
  TestParseSolveCommandConfigRequiresJsonInputAndOutput();
  TestPrepareSolveCommandAcceptsSupportedSchemaVersion();
  TestPrepareSolveCommandRejectsUnsupportedSchemaVersion();
  TestPrepareSolveCommandRejectsMalformedSampleBundleIdentity();
  TestApplySolveOptionsToSampleBundleCopiesCliFields();
  TestApplySolveOptionsToSampleBundleNormalizesExistingMode();
  TestMakeInitialKeyBundlePreservesRequestIdentity();
  return 0;
}
