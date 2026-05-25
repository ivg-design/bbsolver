// W9/W10 path/diagnostics portability tests.
//
// The existing test_cli_options.cpp / test_solver_diagnostics.cpp
// exercise their surfaces with POSIX-style paths. This file adds focused
// coverage for the Windows path-handling edge cases that those don't
// reach today:
//
//   * HasJsonSuffix called with Windows-native backslash separators and
//     with paths mixing both separators (the common case once
//     std::filesystem::path::make_preferred() normalizes them).
//   * DiagnosticsWriter::ToFile creating a multi-level parent chain
//     under std::filesystem::temp_directory_path() (portable on every
//     platform CMake/MSVC supports) and tolerating the
//     std::filesystem::path::preferred_separator() that
//     make_preferred() inserts.
//
// Pure portability hardening — no solver behavior changes, no new
// diagnostics events, no new public APIs.

#include "bbsolver/app/cli_options.hpp"
#include "bbsolver/diagnostics/solver_diagnostics.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    std::abort();
  }
}

// Build a unique nested directory chain under temp_directory_path() so
// every run uses a fresh path. Counter combined with the steady-clock
// stamp keeps subtests independent even when invoked rapidly back to
// back.
std::filesystem::path UniqueTempBase(const char* tag) {
  static int counter = 0;
  ++counter;
  const auto stamp =
      std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         (std::string("bb_path_portability_") + tag + "_" +
          std::to_string(stamp) + "_" + std::to_string(counter));
}

void TestHasJsonSuffixAcceptsForwardSlashPath() {
  Require(bbsolver::HasJsonSuffix("out.bbky.json"),
          "bare filename with .json suffix must be accepted");
  Require(bbsolver::HasJsonSuffix("nested/dir/out.bbky.json"),
          "forward-slash nested path with .json must be accepted");
}

void TestHasJsonSuffixAcceptsBackslashPath() {
  // std::filesystem::path interprets backslashes as separators on
  // Windows but treats them as literal characters on POSIX. The suffix
  // check must accept the .json terminator regardless: on Windows the
  // filename component is "out.bbky.json"; on POSIX the whole literal
  // is the filename ("dir\\out.bbky.json") which still ends in
  // ".json". Either way HasJsonSuffix returns true.
  Require(bbsolver::HasJsonSuffix("dir\\out.bbky.json"),
          "backslash-only path with .json must be accepted on both platforms");
  Require(bbsolver::HasJsonSuffix("C:\\Users\\bake\\out.bbky.json"),
          "Windows-rooted backslash path with .json must be accepted");
}

void TestHasJsonSuffixAcceptsMixedSeparatorsAndNativePreferred() {
  // Mixed-separator path (post-make_preferred or hand-written by a CLI
  // user on Windows). On Windows the path normalizes; on POSIX the
  // filename still ends in .json.
  Require(bbsolver::HasJsonSuffix("C:/Users\\bake/out.bbky.json"),
          "mixed-separator path with .json must be accepted");

  std::filesystem::path normalized = "nested/dir/out.bbky.json";
  normalized.make_preferred();
  Require(bbsolver::HasJsonSuffix(normalized),
          "path normalized via make_preferred() must still match .json suffix");
}

void TestHasJsonSuffixRejectsNonJsonAndCaseSensitive() {
  Require(!bbsolver::HasJsonSuffix("out.bbky"),
          "non-json suffix must be rejected");
  Require(!bbsolver::HasJsonSuffix("out.JSON"),
          "uppercase .JSON must be rejected (case-sensitive contract)");
  Require(!bbsolver::HasJsonSuffix("out.json.bak"),
          "suffix check must look only at the final component");
  Require(!bbsolver::HasJsonSuffix(""),
          "empty path must be rejected");
  // Note: the existing HasJsonSuffix contract accepts a bare ".json"
  // filename (length == 5, suffix matches). The test asserts the actual
  // observable behavior rather than inventing a stricter "must have a
  // stem" guarantee — W9 is portability hardening, not a contract change.
  Require(bbsolver::HasJsonSuffix(".json"),
          "lone .json (length 5) currently matches the suffix contract");
}

void TestDiagnosticsWriterCreatesDeeplyNestedParentUnderTempDir() {
  // Multi-level missing parent chain under temp_directory_path() — the
  // writer's create_directories call must walk the chain on both
  // platforms. Using temp_directory_path() avoids the POSIX-only
  // assumption baked into older tests that hardcoded "/tmp/".
  const std::filesystem::path root = UniqueTempBase("deep");
  const std::filesystem::path out =
      root / "level_a" / "level_b" / "level_c" / "events.jsonl";
  Require(!std::filesystem::exists(root),
          "test fixture must start from a clean temp root");

  {
    const bbsolver::DiagnosticsWriter writer =
        bbsolver::DiagnosticsWriter::ToFile(out);
    Require(writer.Enabled(),
            "writer must report enabled after successful open");
    writer.Emit({{"event", "deep_nested"}, {"depth", 3}});
  }

  Require(std::filesystem::exists(out),
          "writer must create the output file under the new parent chain");
  Require(std::filesystem::is_directory(root / "level_a" / "level_b"),
          "writer must auto-create all intermediate parent directories");

  std::ifstream in(out);
  std::string line;
  Require(std::getline(in, line) && !line.empty(),
          "writer must write at least one JSONL row");
  const nlohmann::json event = nlohmann::json::parse(line);
  Require(event["event"] == "deep_nested",
          "emitted event must round-trip through the JSONL line");
  Require(event["depth"] == 3,
          "emitted event depth must round-trip through the JSONL line");

  std::filesystem::remove_all(root);
}

void TestDiagnosticsWriterAcceptsNativePreferredPath() {
  // Path normalized via make_preferred() inserts the platform's native
  // separator (backslash on Windows, forward-slash on POSIX). The
  // writer must handle either form when create_directories walks the
  // chain.
  std::filesystem::path out =
      UniqueTempBase("native") / "nested" / "events.jsonl";
  out.make_preferred();
  {
    const bbsolver::DiagnosticsWriter writer =
        bbsolver::DiagnosticsWriter::ToFile(out);
    writer.Emit({{"event", "native_pref"}});
  }
  Require(std::filesystem::exists(out),
          "writer must accept paths normalized via make_preferred()");

  // Tear down by walking up to the unique base segment we created.
  std::filesystem::path root = out;
  while (root.parent_path().filename().string().rfind("bb_path_portability_", 0) == 0) {
    root = root.parent_path();
  }
  // Now root.parent_path() is the bb_path_portability_* base; walk one
  // more level up.
  if (root.parent_path() != root) {
    std::filesystem::remove_all(root.parent_path());
  }
}

void TestDiagnosticsWriterEmptyPathRejected() {
  // Documented contract: ToFile({}) throws with "must not be empty".
  // Re-pinned here under the W9 portability suite so a future regression
  // that drops the empty-path guard is flagged in this focused test
  // (test_solver_diagnostics.cpp already covers the same path).
  bool threw = false;
  try {
    (void)bbsolver::DiagnosticsWriter::ToFile({});
  } catch (const std::runtime_error& e) {
    threw = std::string(e.what()).find("must not be empty") !=
            std::string::npos;
  }
  Require(threw, "empty diagnostics path must throw with the documented message");
}

}  // namespace

int main() {
  TestHasJsonSuffixAcceptsForwardSlashPath();
  TestHasJsonSuffixAcceptsBackslashPath();
  TestHasJsonSuffixAcceptsMixedSeparatorsAndNativePreferred();
  TestHasJsonSuffixRejectsNonJsonAndCaseSensitive();
  TestDiagnosticsWriterCreatesDeeplyNestedParentUnderTempDir();
  TestDiagnosticsWriterAcceptsNativePreferredPath();
  TestDiagnosticsWriterEmptyPathRejected();
  std::cout << "[PASS] test_path_portability\n";
  return 0;
}
