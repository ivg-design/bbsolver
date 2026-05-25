#include "bbsolver/progress/solve_cancellation.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/io/io_json.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <utility>

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    std::abort();
  }
}

std::filesystem::path MakeTempDir() {
  const auto stamp =
      std::chrono::steady_clock::now().time_since_epoch().count();
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() /
      ("bb_solve_cancellation_test_" + std::to_string(stamp));
  std::filesystem::create_directories(dir);
  return dir;
}

bbsolver::PropertyKeys MakeProperty(std::string id,
                                    bool converged,
                                    std::string notes) {
  bbsolver::PropertyKeys property;
  property.property_id = std::move(id);
  property.converged = converged;
  property.notes = std::move(notes);
  return property;
}

bbsolver::KeyBundle MakeBundle() {
  bbsolver::KeyBundle bundle;
  bundle.schema_version = 1;
  bundle.request_id = "solve-cancellation-unit";
  bundle.solver_version = "unit";
  bundle.solver_build = "test";
  bundle.property_results.push_back(MakeProperty("unit/empty-note", true, ""));
  bundle.property_results.push_back(
      MakeProperty("unit/existing-note", true, "kept note"));
  bundle.property_results.push_back(
      MakeProperty("unit/already-unconverged", false, ""));
  return bundle;
}

void TestCancelFileExistsOptionalAndMissingPaths() {
  Require(!bbsolver::CancelFileExists(std::nullopt),
          "empty cancel path must be false");

  const std::filesystem::path dir = MakeTempDir();
  const std::filesystem::path missing = dir / "missing.cancel";
  Require(!bbsolver::CancelFileExists(missing),
          "missing cancel path must be false");
  std::filesystem::remove_all(dir);
}

void TestCancelFileExistsExistingPath() {
  const std::filesystem::path dir = MakeTempDir();
  const std::filesystem::path cancel_file = dir / "stop.cancel";
  {
    std::ofstream out(cancel_file);
    out << "cancel\n";
  }
  Require(bbsolver::CancelFileExists(cancel_file),
          "existing cancel path must be true");
  std::filesystem::remove_all(dir);
}

void TestMarkCancelledPartialNotesAndConverged() {
  bbsolver::KeyBundle bundle = MakeBundle();
  bbsolver::MarkCancelledPartial(bundle);

  Require(bundle.property_results.size() == 3,
          "test bundle property count mismatch");
  Require(!bundle.property_results[0].converged,
          "empty-note property must be marked unconverged");
  Require(bundle.property_results[0].notes == "cancelled",
          "empty notes must become the cancelled sentinel");
  Require(!bundle.property_results[1].converged,
          "existing-note property must be marked unconverged");
  Require(bundle.property_results[1].notes == "kept note; cancelled",
          "existing notes must append the cancelled sentinel");
  Require(!bundle.property_results[2].converged,
          "already-unconverged property must stay unconverged");
  Require(bundle.property_results[2].notes == "cancelled",
          "empty notes on unconverged property must become cancelled");
}

void TestWriteCancelledPartialWritesReadableKeyBundle() {
  const std::filesystem::path dir = MakeTempDir();
  const std::filesystem::path output = dir / "partial.bbky.json";
  bbsolver::KeyBundle bundle = MakeBundle();
  const auto start =
      std::chrono::steady_clock::now() - std::chrono::milliseconds(5);

  const int rc = bbsolver::WriteCancelledPartial(output, bundle, start);
  Require(rc == 5, "cancelled partial write must return exit code 5");

  const bbsolver::KeyBundle read_back = bbsolver::ReadKeyBundleJson(output);
  Require(read_back.request_id == "solve-cancellation-unit",
          "request id must round-trip");
  Require(read_back.solve_time_ms >= 0.0,
          "solve_time_ms must be populated");
  Require(read_back.property_results.size() == 3,
          "read-back property count mismatch");
  Require(!read_back.property_results[0].converged,
          "read-back empty-note property must be unconverged");
  Require(read_back.property_results[0].notes == "cancelled",
          "read-back empty notes must be cancelled");
  Require(!read_back.property_results[1].converged,
          "read-back existing-note property must be unconverged");
  Require(read_back.property_results[1].notes == "kept note; cancelled",
          "read-back existing notes must append cancelled");
  Require(!read_back.property_results[2].converged,
          "read-back already-unconverged property must stay unconverged");
  Require(read_back.property_results[2].notes == "cancelled",
          "read-back unconverged empty notes must be cancelled");
  std::filesystem::remove_all(dir);
}

}  // namespace

int main() {
  TestCancelFileExistsOptionalAndMissingPaths();
  TestCancelFileExistsExistingPath();
  TestMarkCancelledPartialNotesAndConverged();
  TestWriteCancelledPartialWritesReadableKeyBundle();
  std::cout << "[PASS] test_solve_cancellation\n";
  return 0;
}
