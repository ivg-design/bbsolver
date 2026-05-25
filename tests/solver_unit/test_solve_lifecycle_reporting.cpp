#include "bbsolver/solve/solve_lifecycle_reporting.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/app/cli_options.hpp"

#include <chrono>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "nlohmann/json_fwd.hpp"
#include "bbsolver/diagnostics/solver_diagnostics.hpp"
#include "bbsolver/progress/progress.hpp"
#include "bbsolver/io/io_json.hpp"
#include <nlohmann/json.hpp>

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    std::abort();
  }
}

std::filesystem::path TempPath(const std::string& name) {
  const auto stamp =
      std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         ("bbsolver_" + name + "_" + std::to_string(stamp) + ".jsonl");
}

std::vector<nlohmann::json> ReadJsonLines(const std::filesystem::path& path) {
  std::ifstream in(path);
  Require(static_cast<bool>(in), "diagnostics output must be readable");
  std::vector<nlohmann::json> events;
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty()) {
      events.push_back(nlohmann::json::parse(line));
    }
  }
  return events;
}

bbsolver::Key MakeLinearScalarKey(double t, double value) {
  bbsolver::Key key;
  key.t_sec = t;
  key.v = {value};
  key.interp_in = bbsolver::InterpType::Linear;
  key.interp_out = bbsolver::InterpType::Linear;
  return key;
}

void TestBuildSolveStartProgressEvent() {
  bbsolver::SampleBundle samples;
  samples.request_id = "request-abc";
  samples.config.solve_optimization_mode = "full";
  samples.properties.resize(3);

  const nlohmann::json event =
      bbsolver::BuildSolveStartProgressEvent(samples);

  Require(event["event"] == "solve_start", "start progress event name");
  Require(event["phase"] == "Preparing solver input",
          "start progress phase text");
  Require(std::abs(event["progress"].get<double>() - 0.02) < 1e-12,
          "start progress fraction");
  Require(event["request_id"] == "request-abc", "start progress request id");
  Require(event["properties"] == 3, "start progress property count");
  Require(event["solve_optimization_mode"] == "full",
          "start progress optimization mode");
}

void TestBuildParallelConfigProgressEvent() {
  bbsolver::SolveOptions options;
  options.jobs = 2;

  const nlohmann::json event =
      bbsolver::BuildParallelConfigProgressEvent(options, 2);

  Require(event["event"] == "parallel_config",
          "parallel progress event name");
  Require(event["phase"].get<std::string>().find("Parallel runtime:") == 0,
          "parallel progress phase text");
  Require(std::abs(event["progress"].get<double>() - 0.025) < 1e-12,
          "parallel progress fraction");
  Require(event["parallel_jobs_requested"] == 2,
          "parallel requested jobs");
  Require(event["parallel_jobs_resolved"] == 2, "parallel resolved jobs");
  Require(event.contains("parallel_jobs_detected"),
          "parallel detected jobs field");
  Require(event.contains("parallel_jobs_hard_cap"),
          "parallel hard cap field");
  Require(event.contains("tbb_available"), "parallel tbb field");
}

void TestBuildSolveDoneProgressEvent() {
  bbsolver::KeyBundle keys;
  keys.property_results.resize(4);
  keys.total_keys = 12;
  keys.solve_time_ms = 34.5;

  const nlohmann::json event = bbsolver::BuildSolveDoneProgressEvent(keys);

  Require(event["event"] == "done", "done progress event name");
  Require(event["phase"] == "Solver finished", "done progress phase");
  Require(std::abs(event["progress"].get<double>() - 1.0) < 1e-12,
          "done progress fraction");
  Require(event["properties"] == 4, "done progress property count");
  Require(event["total_keys"] == 12, "done progress key count");
  Require(std::abs(event["solve_time_ms"].get<double>() - 34.5) < 1e-12,
          "done progress solve time");
}

void TestEmitSolveStartLifecycleWritesDiagnostics() {
  const std::filesystem::path out = TempPath("solve_lifecycle_start");
  std::filesystem::remove(out);

  bbsolver::SampleBundle samples;
  samples.request_id = "request-start";
  samples.config.tolerance = 0.5;
  samples.config.tolerance_screen_px = 1.25;
  samples.config.solve_optimization_mode = "full";
  samples.properties.resize(2);
  bbsolver::SolveOptions options;
  options.jobs = 1;
  options.fit_canonical_paths = true;
  options.fit_replacement_paths = true;
  const bbsolver::ProgressWriter progress(-1);
  const bbsolver::DiagnosticsWriter diagnostics =
      bbsolver::DiagnosticsWriter::ToFile(out);

  bbsolver::EmitSolveStartLifecycle(samples,
                                    options,
                                    "in.bbsm.json",
                                    "out.bbky.json",
                                    1,
                                    progress,
                                    diagnostics);

  const std::vector<nlohmann::json> events = ReadJsonLines(out);
  Require(events.size() == 4, "start lifecycle diagnostic row count");
  Require(events[0]["event"] == "solve_start",
          "start lifecycle first diagnostic event");
  Require(events[1]["event"] == "parallel_runtime",
          "start lifecycle parallel diagnostic event");
  Require(events[2]["event"] == "solve_mode_capabilities",
          "start lifecycle capabilities diagnostic event");
  Require(events[3]["event"] == "cancellation_status",
          "start lifecycle cancellation diagnostic event");
  for (const nlohmann::json& event : events) {
    Require(event["schema_version"] == 1, "lifecycle schema version");
    Require(event["request_id"] == "request-start", "lifecycle request id");
  }
  Require(events[0]["properties"] == 2, "solve_start property count");
  Require(events[0]["input"] == "in.bbsm.json", "solve_start input path");
  Require(events[0]["output"] == "out.bbky.json", "solve_start output path");
  Require(events[0]["fit_canonical_paths"] == true,
          "solve_start canonical flag");
  Require(events[0]["fit_replacement_paths"] == true,
          "solve_start replacement flag");
  Require(events[1]["resolved_jobs"] == 1, "parallel runtime resolved jobs");
  Require(events[2]["mode"] == "full", "capabilities mode");
  Require(events[3]["cancel_file_set"] == false,
          "cancellation status default");
  std::filesystem::remove(out);
}

void TestEmitSolveDoneLifecycleWritesDiagnosticEvent() {
  const std::filesystem::path out = TempPath("solve_lifecycle_done");
  std::filesystem::remove(out);

  bbsolver::KeyBundle keys;
  keys.property_results.resize(2);
  keys.total_keys = 17;
  keys.total_samples_input = 41;
  keys.solve_time_ms = 88.25;
  const bbsolver::ProgressWriter progress(-1);
  const bbsolver::DiagnosticsWriter diagnostics =
      bbsolver::DiagnosticsWriter::ToFile(out);

  bbsolver::EmitSolveDoneLifecycle("request-done", keys, progress, diagnostics);

  const std::vector<nlohmann::json> events = ReadJsonLines(out);
  Require(events.size() == 1, "done lifecycle diagnostic row count");
  const nlohmann::json& event = events.front();
  Require(event["event"] == "solve_done", "done diagnostic event name");
  Require(event["schema_version"] == 1, "done diagnostic schema");
  Require(event["request_id"] == "request-done", "done diagnostic request id");
  Require(event["properties"] == 2, "done diagnostic property count");
  Require(event["total_keys"] == 17, "done diagnostic key count");
  Require(event["total_samples_input"] == 41,
          "done diagnostic sample count");
  Require(std::abs(event["solve_time_ms"].get<double>() - 88.25) < 1e-12,
          "done diagnostic solve time");
  std::filesystem::remove(out);
}

void TestEmitSolveCancelledLifecycleWritesDiagnosticEvent() {
  const std::filesystem::path out = TempPath("solve_lifecycle_cancelled");
  std::filesystem::remove(out);

  bbsolver::KeyBundle keys;
  keys.property_results.resize(2);
  const bbsolver::DiagnosticsWriter diagnostics =
      bbsolver::DiagnosticsWriter::ToFile(out);

  bbsolver::EmitSolveCancelledLifecycle("request-cancel",
                                        "temporal_refit",
                                        3,
                                        keys,
                                        12.5,
                                        diagnostics);

  const std::vector<nlohmann::json> events = ReadJsonLines(out);
  Require(events.size() == 1, "cancel lifecycle diagnostic row count");
  const nlohmann::json& event = events.front();
  Require(event["event"] == "solve_cancelled",
          "cancel diagnostic event name");
  Require(event["schema_version"] == 1, "cancel diagnostic schema");
  Require(event["request_id"] == "request-cancel",
          "cancel diagnostic request id");
  Require(event["phase"] == "temporal_refit", "cancel diagnostic phase");
  Require(event["property_index"] == 3, "cancel diagnostic property index");
  Require(event["properties_completed"] == 2,
          "cancel diagnostic completed count");
  Require(std::abs(event["solve_time_ms"].get<double>() - 12.5) < 1e-12,
          "cancel diagnostic solve time");
  Require(event["partial_write_exit_code"] == 5,
          "cancel diagnostic partial-write exit code");
  std::filesystem::remove(out);
}

void TestWriteCancelledSolvePartialEmitsDiagnosticAndBundle() {
  const std::filesystem::path output =
      TempPath("solve_lifecycle_cancelled_bundle");
  const std::filesystem::path diagnostics_path =
      TempPath("solve_lifecycle_cancelled_bundle_diag");
  std::filesystem::remove(output);
  std::filesystem::remove(diagnostics_path);

  bbsolver::KeyBundle keys;
  keys.request_id = "request-cancel-output";
  keys.property_results.resize(1);
  keys.property_results[0].property_id = "unit/cancelled";
  keys.property_results[0].converged = true;
  const bbsolver::DiagnosticsWriter diagnostics =
      bbsolver::DiagnosticsWriter::ToFile(diagnostics_path);
  const auto start = std::chrono::steady_clock::now();

  const int rc = bbsolver::WriteCancelledSolvePartial(output,
                                                      "request-cancel-output",
                                                      "property_loop",
                                                      4,
                                                      keys,
                                                      start,
                                                      diagnostics);

  Require(rc == 5, "cancelled solve partial exit code");
  const std::vector<nlohmann::json> events = ReadJsonLines(diagnostics_path);
  Require(events.size() == 1, "cancelled solve partial diagnostic row count");
  Require(events.front()["event"] == "solve_cancelled",
          "cancelled solve partial diagnostic event");
  Require(events.front()["request_id"] == "request-cancel-output",
          "cancelled solve partial request id");
  Require(events.front()["phase"] == "property_loop",
          "cancelled solve partial phase");
  Require(events.front()["property_index"] == 4,
          "cancelled solve partial property index");

  const bbsolver::KeyBundle read_back = bbsolver::ReadKeyBundleJson(output);
  Require(read_back.property_results.size() == 1,
          "cancelled solve partial output property count");
  Require(!read_back.property_results.front().converged,
          "cancelled solve partial marks properties unconverged");
  Require(read_back.property_results.front().notes == "cancelled",
          "cancelled solve partial records cancelled note");
  Require(read_back.solve_time_ms >= 0.0,
          "cancelled solve partial records solve time");
  std::filesystem::remove(output);
  std::filesystem::remove(diagnostics_path);
}

void TestWriteCompletedSolveOutputWritesBundleAndDoneLifecycle() {
  const std::filesystem::path output =
      TempPath("solve_lifecycle_completed_bundle");
  const std::filesystem::path diagnostics_path =
      TempPath("solve_lifecycle_completed_bundle_diag");
  std::filesystem::remove(output);
  std::filesystem::remove(diagnostics_path);

  bbsolver::KeyBundle keys;
  keys.request_id = "request-complete-output";
  keys.total_keys = 7;
  keys.total_samples_input = 15;
  keys.property_results.resize(2);
  keys.property_results[0].property_id = "unit/position/x";
  keys.property_results[0].keys.push_back(MakeLinearScalarKey(0.0, 1.0));
  keys.property_results[0].keys.push_back(MakeLinearScalarKey(1.0, 2.0));
  keys.property_results[1].property_id = "unit/position/y";
  keys.property_results[1].keys.push_back(MakeLinearScalarKey(0.0, 3.0));
  keys.property_results[1].keys.push_back(MakeLinearScalarKey(1.0, 4.0));
  const bbsolver::ProgressWriter progress(-1);
  const bbsolver::DiagnosticsWriter diagnostics =
      bbsolver::DiagnosticsWriter::ToFile(diagnostics_path);
  const auto start = std::chrono::steady_clock::now();

  const int rc = bbsolver::WriteCompletedSolveOutput(output,
                                                     "request-complete-output",
                                                     keys,
                                                     start,
                                                     progress,
                                                     diagnostics);

  Require(rc == 0, "completed solve output exit code");
  const bbsolver::KeyBundle read_back = bbsolver::ReadKeyBundleJson(output);
  Require(read_back.total_keys == 7, "completed output total keys");
  Require(read_back.total_samples_input == 15,
          "completed output total input samples");
  Require(read_back.solve_time_ms >= 0.0,
          "completed output records solve time");

  const std::vector<nlohmann::json> events = ReadJsonLines(diagnostics_path);
  Require(events.size() == 1, "completed solve diagnostic row count");
  Require(events.front()["event"] == "solve_done",
          "completed solve diagnostic event");
  Require(events.front()["request_id"] == "request-complete-output",
          "completed solve diagnostic request id");
  Require(events.front()["properties"] == 2,
          "completed solve diagnostic property count");
  Require(events.front()["total_keys"] == 7,
          "completed solve diagnostic total keys");
  std::filesystem::remove(output);
  std::filesystem::remove(diagnostics_path);
}

}  // namespace

int main() {
  TestBuildSolveStartProgressEvent();
  TestBuildParallelConfigProgressEvent();
  TestBuildSolveDoneProgressEvent();
  TestEmitSolveStartLifecycleWritesDiagnostics();
  TestEmitSolveDoneLifecycleWritesDiagnosticEvent();
  TestEmitSolveCancelledLifecycleWritesDiagnosticEvent();
  TestWriteCancelledSolvePartialEmitsDiagnosticAndBundle();
  TestWriteCompletedSolveOutputWritesBundleAndDoneLifecycle();
  return 0;
}
