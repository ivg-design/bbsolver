#include "bbsolver/app/cli_options.hpp"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    std::abort();
  }
}

void RequireNear(double actual, double expected, const std::string& message) {
  if (std::abs(actual - expected) > 1e-12) {
    std::cerr << message << ": expected " << expected << ", got " << actual
              << "\n";
    std::abort();
  }
}

bbsolver::SolveOptions Parse(std::vector<std::string> args) {
  std::vector<char*> argv;
  argv.reserve(args.size());
  for (std::string& arg : args) {
    argv.push_back(arg.data());
  }
  return bbsolver::ParseSolveOptions(
      static_cast<int>(argv.size()), argv.data(), 4);
}

void TestParseDefaults() {
  const bbsolver::SolveOptions options =
      Parse({"bbsolver", "solve", "in.json", "out.json"});
  RequireNear(options.tolerance, 0.5, "default tolerance mismatch");
  RequireNear(options.screen_px, 0.0, "default screen px mismatch");
  Require(options.jobs == 0, "default jobs mismatch");
  Require(options.progress_fd == -1, "default progress fd mismatch");
  Require(!options.diagnostics_file.has_value(),
          "default diagnostics file mismatch");
  Require(!options.cancel_file.has_value(), "default cancel file mismatch");
  Require(!options.decompose_paths, "default decompose flag mismatch");
  Require(!options.fit_canonical_paths, "default canonical flag mismatch");
  Require(!options.fit_replacement_paths, "default replacement flag mismatch");
  Require(!options.emit_landmark_subpaths, "default landmark flag mismatch");
  Require(!options.solve_optimization_mode.has_value(),
          "default solve mode mismatch");
  Require(!options.verbose, "default verbose mismatch");
}

void TestParseAllOptions() {
  // W12 portability: derive the diagnostics + cancel argument strings
  // from std::filesystem::temp_directory_path() rather than hardcoding
  // `/tmp/`. The parser must round-trip whatever path the operator
  // passed — the contract under test is "argv string -> options path
  // -> .string()" equality, not the specific prefix.
  const std::filesystem::path tmp_root =
      std::filesystem::temp_directory_path();
  const std::string diagnostics_arg =
      (tmp_root / "bb.diagnostics.jsonl").string();
  const std::string cancel_arg = (tmp_root / "bb.cancel").string();

  const bbsolver::SolveOptions options = Parse({
      "bbsolver",
      "solve",
      "in.json",
      "out.json",
      "--tolerance",
      "2.5",
      "--screen-px",
      "3.25",
      "--jobs",
      "8",
      "--progress-fd",
      "42",
      "--diagnostics",
      diagnostics_arg,
      "--cancel-file",
      cancel_arg,
      "--decompose-paths",
      "--fit-canonical-paths",
      "--fit-replacement-paths",
      "--emit-landmark-subpaths",
      "--solve-mode",
      "motion-smooth",
      "--verbose",
  });

  RequireNear(options.tolerance, 2.5, "parsed tolerance mismatch");
  RequireNear(options.screen_px, 3.25, "parsed screen px mismatch");
  Require(options.jobs == 8, "parsed jobs mismatch");
  Require(options.progress_fd == 42, "parsed progress fd mismatch");
  Require(options.diagnostics_file.has_value(),
          "parsed diagnostics file missing");
  Require(options.diagnostics_file->string() == diagnostics_arg,
          "parsed diagnostics file mismatch");
  Require(options.cancel_file.has_value(), "parsed cancel file missing");
  Require(options.cancel_file->string() == cancel_arg,
          "parsed cancel file mismatch");
  Require(options.decompose_paths, "parsed decompose flag missing");
  Require(options.fit_canonical_paths, "parsed canonical flag missing");
  Require(options.fit_replacement_paths, "parsed replacement flag missing");
  Require(options.emit_landmark_subpaths, "parsed landmark flag missing");
  Require(options.solve_optimization_mode.has_value(),
          "parsed solve mode missing");
  Require(*options.solve_optimization_mode == "motion-smooth",
          "parsed solve mode mismatch");
  Require(options.verbose, "parsed verbose missing");
}

void TestParseLegacySolveModeAlias() {
  const bbsolver::SolveOptions options = Parse({
      "bbsolver",
      "solve",
      "in.json",
      "out.json",
      "--solve-optimization-mode",
      "vertex-only",
  });
  Require(options.solve_optimization_mode.has_value(),
          "legacy solve mode alias missing");
  Require(*options.solve_optimization_mode == "vertex-only",
          "legacy solve mode alias mismatch");
}

void TestParseRejectsUnknownOrIncompleteOptions() {
  bool threw = false;
  try {
    (void)Parse({"bbsolver", "solve", "in.json", "out.json", "--bad"});
  } catch (const std::runtime_error& e) {
    threw = std::string(e.what()).find("--bad") != std::string::npos;
  }
  Require(threw, "unknown option should throw with option name");

  threw = false;
  try {
    (void)Parse({"bbsolver", "solve", "in.json", "out.json", "--jobs"});
  } catch (const std::runtime_error& e) {
    threw = std::string(e.what()).find("--jobs") != std::string::npos;
  }
  Require(threw, "incomplete option should throw with option name");
}

void TestJsonSuffixAndUsage() {
  Require(bbsolver::HasJsonSuffix("sample.bbsm.json"),
          "json suffix should be accepted");
  // W12 portability: nested-path acceptance uses temp_directory_path()
  // so the prefix is platform-native, not hardcoded `/tmp/`.
  const std::string nested_json =
      (std::filesystem::temp_directory_path() / "out.bbky.json").string();
  Require(bbsolver::HasJsonSuffix(nested_json),
          "nested json suffix should be accepted");
  Require(!bbsolver::HasJsonSuffix("sample.bbsm"),
          "non-json suffix should be rejected");
  Require(!bbsolver::HasJsonSuffix("sample.JSON"),
          "json suffix check should remain case-sensitive");

  std::ostringstream out;
  bbsolver::PrintUsage(out);
  const std::string usage = out.str();
  Require(usage.find("bbsolver solve") != std::string::npos,
          "usage must include solve command");
  Require(usage.find("<in.bbsm.json> <out.bbky.json>") !=
              std::string::npos,
          "usage must document JSON-only solve paths");
  Require(usage.find("bbsolver verify <bundle.bbky.json> "
                     "<samples.bbsm.json>") != std::string::npos,
          "usage must document JSON-only verify paths");
  Require(usage.find("|.bbsm") == std::string::npos,
          "usage must not advertise binary SampleBundle input");
  Require(usage.find("|.bbky") == std::string::npos,
          "usage must not advertise binary KeyBundle output");
  Require(usage.find("--solve-mode") != std::string::npos,
          "usage must include solve mode flag");
  Require(usage.find("motion-path-smooth") != std::string::npos,
          "usage must include motion path smooth mode");
  Require(usage.find("--diagnostics PATH") != std::string::npos,
          "usage must include diagnostics flag");
  Require(usage.find("bbsolver --version") != std::string::npos,
          "usage must include version command");
  Require(std::string(bbsolver::kBbsolverVersion) == "bbsolver 1.0.1",
          "version string mismatch");
}

}  // namespace

int main() {
  TestParseDefaults();
  TestParseAllOptions();
  TestParseLegacySolveModeAlias();
  TestParseRejectsUnknownOrIncompleteOptions();
  TestJsonSuffixAndUsage();
  std::cout << "cli option tests passed\n";
  return 0;
}
