#include "bbsolver/diagnostics/solver_diagnostics.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    std::abort();
  }
}

std::filesystem::path TempRoot() {
  const auto stamp =
      std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         ("bbsolver_diagnostics_test_" + std::to_string(stamp));
}

std::vector<nlohmann::json> ReadJsonLines(const std::filesystem::path& path) {
  std::ifstream in(path);
  Require(static_cast<bool>(in), "diagnostics file should be readable");
  std::vector<nlohmann::json> events;
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty()) {
      events.push_back(nlohmann::json::parse(line));
    }
  }
  return events;
}

void TestDefaultWriterIsDisabledNoOp() {
  const bbsolver::DiagnosticsWriter writer;
  Require(!writer.Enabled(), "default diagnostics writer must be disabled");
  writer.Emit({{"event", "ignored"}});
}

void TestWriterCreatesParentAndWritesJsonLines() {
  const std::filesystem::path root = TempRoot();
  const std::filesystem::path out = root / "nested" / "events.jsonl";
  {
    const bbsolver::DiagnosticsWriter writer =
        bbsolver::DiagnosticsWriter::ToFile(out);
    Require(writer.Enabled(), "file diagnostics writer must be enabled");
    writer.Emit({{"event", "first"}, {"value", 1}});
    writer.Emit({{"event", "second"}, {"value", 2}});
  }

  const std::vector<nlohmann::json> events = ReadJsonLines(out);
  Require(events.size() == 2, "diagnostics writer must emit two JSONL rows");
  Require(events[0]["event"] == "first", "first event mismatch");
  Require(events[0]["value"] == 1, "first event value mismatch");
  Require(events[1]["event"] == "second", "second event mismatch");
  Require(events[1]["value"] == 2, "second event value mismatch");
  std::filesystem::remove_all(root);
}

void TestWriterTruncatesExistingFile() {
  const std::filesystem::path root = TempRoot();
  std::filesystem::create_directories(root);
  const std::filesystem::path out = root / "events.jsonl";
  {
    std::ofstream seed(out);
    seed << "{\"event\":\"stale\"}\n";
  }
  {
    const bbsolver::DiagnosticsWriter writer =
        bbsolver::DiagnosticsWriter::ToFile(out);
    writer.Emit({{"event", "fresh"}});
  }
  const std::vector<nlohmann::json> events = ReadJsonLines(out);
  Require(events.size() == 1, "diagnostics file must be truncated on open");
  Require(events[0]["event"] == "fresh", "fresh event mismatch");
  std::filesystem::remove_all(root);
}

void TestEmptyPathThrows() {
  bool threw = false;
  try {
    (void)bbsolver::DiagnosticsWriter::ToFile({});
  } catch (const std::runtime_error& e) {
    threw = std::string(e.what()).find("must not be empty") !=
            std::string::npos;
  }
  Require(threw, "empty diagnostics path must throw");
}

}  // namespace

int main() {
  TestDefaultWriterIsDisabledNoOp();
  TestWriterCreatesParentAndWritesJsonLines();
  TestWriterTruncatesExistingFile();
  TestEmptyPathThrows();
  std::cout << "[PASS] test_solver_diagnostics\n";
  return 0;
}
