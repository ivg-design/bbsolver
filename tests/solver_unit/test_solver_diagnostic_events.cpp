#include "bbsolver/diagnostics/solver_diagnostic_events.hpp"
#include "bbsolver/domain.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>

#include "bbsolver/path/bridge_prune/path_bridge_prune.hpp"
#include "bbsolver/runtime/runtime_env.hpp"

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
      ("bb_solver_diagnostic_events_test_" + std::to_string(stamp));
  std::filesystem::create_directories(dir);
  return dir;
}

void TestSolveStartEventShape() {
  // W11 portability: derive both endpoints from
  // std::filesystem::temp_directory_path() so the test does not bake in
  // a POSIX `/tmp/` prefix. The event builder serializes the path via
  // .string(), so what matters is that the JSON value round-trips
  // through that .string() — the actual prefix is the platform's temp
  // root (`/tmp` on POSIX, `%TEMP%` on Windows).
  const std::filesystem::path tmp_root =
      std::filesystem::temp_directory_path();
  const std::filesystem::path input_path = tmp_root / "in.bbsm.json";
  const std::filesystem::path output_path = tmp_root / "out.bbky.json";

  bbsolver::SolveStartDiagnosticInput input;
  input.request_id = "req-start";
  input.input_path = input_path;
  input.output_path = output_path;
  input.property_count = 4;
  input.tolerance = 0.75;
  input.screen_px = 2.5;
  input.decompose_paths = true;
  input.fit_replacement_paths = true;

  const nlohmann::json event = bbsolver::BuildSolveStartEvent(input);
  Require(event["event"] == "solve_start",
          "solve-start event must carry the solve_start name");
  Require(event["schema_version"] ==
              bbsolver::kSolverDiagnosticEventSchemaVersion,
          "solve-start event must stamp the schema version constant");
  Require(event["request_id"] == "req-start",
          "solve-start event must echo request id");
  Require(event["input"].get<std::string>() == input_path.string(),
          "solve-start event must echo input path .string() verbatim");
  Require(event["output"].get<std::string>() == output_path.string(),
          "solve-start event must echo output path .string() verbatim");
  Require(event["properties"] == 4,
          "solve-start event must echo property count");
  Require(event["tolerance"] == 0.75,
          "solve-start event must echo tolerance");
  Require(event["screen_px"] == 2.5,
          "solve-start event must echo screen tolerance");
  Require(event["decompose_paths"] == true,
          "solve-start event must echo decompose flag");
  Require(event["fit_canonical_paths"] == false,
          "solve-start event must echo canonical flag");
  Require(event["fit_replacement_paths"] == true,
          "solve-start event must echo replacement flag");
  Require(event["emit_landmark_subpaths"] == false,
          "solve-start event must echo landmark flag");
}

void TestParallelRuntimeEventShapeForAutoRequest() {
  const nlohmann::json event = bbsolver::BuildParallelRuntimeEvent(0);
  Require(event["event"] == "parallel_runtime",
          "auto runtime event must carry the parallel_runtime name");
  Require(event["schema_version"] ==
              bbsolver::kSolverDiagnosticEventSchemaVersion,
          "auto runtime event must stamp the schema version constant");
  Require(event["requested_jobs"] == 0,
          "auto runtime event must echo the requested jobs verbatim");
  Require(event["hard_cap"] == bbsolver::kParallelJobsHardCap,
          "auto runtime event must publish the hard cap constant");
  Require(event["tbb_available"] == bbsolver::TbbRuntimeAvailable(),
          "auto runtime event must publish the TBB availability flag");
  Require(event["detected_jobs"] == bbsolver::DetectedParallelJobs(),
          "auto runtime event must publish the detected-jobs result");
  Require(event["resolved_jobs"] == bbsolver::ResolveParallelJobs(0),
          "auto runtime event must publish the resolved-jobs result");
  Require(event["phase"].get<std::string>() ==
              bbsolver::ParallelRuntimePhase(
                  0, bbsolver::ResolveParallelJobs(0)),
          "auto runtime event must publish the phase string");
  Require(!event.contains("error"),
          "auto runtime event must not set an error field");
}

void TestParallelRuntimeEventPositiveRequest() {
  const int requested = 1;
  const nlohmann::json event =
      bbsolver::BuildParallelRuntimeEvent(requested);
  Require(event["requested_jobs"] == requested,
          "positive runtime event must echo the requested jobs");
  Require(event["resolved_jobs"] ==
              bbsolver::ResolveParallelJobs(requested),
          "positive runtime event must publish the resolved-jobs result");
  Require(event["phase"].get<std::string>() ==
              bbsolver::ParallelRuntimePhase(
                  requested, bbsolver::ResolveParallelJobs(requested)),
          "positive runtime event must publish the phase string");
}

void TestParallelRuntimeEventNegativeRequestReportsError() {
  const nlohmann::json event = bbsolver::BuildParallelRuntimeEvent(-1);
  Require(event["event"] == "parallel_runtime",
          "negative runtime event must keep the parallel_runtime name");
  Require(event["requested_jobs"] == -1,
          "negative runtime event must echo the requested jobs");
  Require(event["resolved_jobs"].is_null(),
          "negative runtime event must null the resolved-jobs field");
  Require(event["phase"].is_null(),
          "negative runtime event must null the phase field");
  Require(event["error"].get<std::string>() == "requested_jobs_negative",
          "negative runtime event must report the negative-requested error");
  Require(event["detected_jobs"] == bbsolver::DetectedParallelJobs(),
          "negative runtime event must still publish detected jobs");
  Require(event["hard_cap"] == bbsolver::kParallelJobsHardCap,
          "negative runtime event must still publish the hard cap");
}

void TestSolveModeCapabilitiesEventForFull() {
  bbsolver::SolverConfig config;
  config.solve_optimization_mode = "full";
  const nlohmann::json event =
      bbsolver::BuildSolveModeCapabilitiesEvent(config);
  Require(event["event"] == "solve_mode_capabilities",
          "solve mode event must carry the solve_mode_capabilities name");
  Require(event["schema_version"] ==
              bbsolver::kSolverDiagnosticEventSchemaVersion,
          "solve mode event must stamp the schema version constant");
  Require(event["mode"] == "full",
          "full solve mode must normalize verbatim");
  Require(event["allows_temporal"] == true,
          "full mode must allow temporal route");
  Require(event["allows_vertex"] == true,
          "full mode must allow vertex route");
  Require(event["allows_spatial_topology"] == true,
          "full mode must allow spatial topology route");
  Require(event["is_motion_smooth"] == false,
          "full mode must not be motion smooth");
  Require(event["is_motion_path_smooth"] == false,
          "full mode must not be motion path smooth");
  Require(event["uses_motion_smoothing"] == false,
          "full mode must not use motion smoothing");
}

void TestSolveModeCapabilitiesEventForTemporalOnly() {
  bbsolver::SolverConfig config;
  config.solve_optimization_mode = "temporal_only";
  const nlohmann::json event =
      bbsolver::BuildSolveModeCapabilitiesEvent(config);
  Require(event["mode"] == "temporal_only",
          "temporal-only solve mode must normalize verbatim");
  Require(event["allows_temporal"] == true,
          "temporal-only must allow temporal route");
  Require(event["allows_vertex"] == false,
          "temporal-only must not allow vertex route");
  Require(event["allows_spatial_topology"] == false,
          "temporal-only must not allow spatial topology route");
  Require(event["is_motion_smooth"] == false,
          "temporal-only must not be motion smooth");
  Require(event["is_motion_path_smooth"] == false,
          "temporal-only must not be motion path smooth");
  Require(event["uses_motion_smoothing"] == false,
          "temporal-only must not use motion smoothing");
}

void TestSolveModeCapabilitiesEventForMotionSmoothAndAliases() {
  bbsolver::SolverConfig motion_smooth;
  motion_smooth.solve_optimization_mode = "motion_smooth";
  const nlohmann::json motion_event =
      bbsolver::BuildSolveModeCapabilitiesEvent(motion_smooth);
  Require(motion_event["mode"] == "motion_smooth",
          "motion-smooth mode must normalize verbatim");
  Require(motion_event["is_motion_smooth"] == true,
          "motion-smooth mode must report is_motion_smooth=true");
  Require(motion_event["is_motion_path_smooth"] == false,
          "motion-smooth mode must not report motion path smooth");
  Require(motion_event["uses_motion_smoothing"] == true,
          "motion-smooth mode must report uses_motion_smoothing=true");
  Require(motion_event["allows_temporal"] == false &&
              motion_event["allows_vertex"] == false &&
              motion_event["allows_spatial_topology"] == false,
          "motion-smooth must disallow temporal/vertex/spatial routes");

  bbsolver::SolverConfig motion_path_smooth;
  motion_path_smooth.solve_optimization_mode = "motion_path_smooth";
  const nlohmann::json motion_path_event =
      bbsolver::BuildSolveModeCapabilitiesEvent(motion_path_smooth);
  Require(motion_path_event["mode"] == "motion_path_smooth",
          "motion-path-smooth mode must normalize verbatim");
  Require(motion_path_event["is_motion_smooth"] == false,
          "motion-path-smooth mode must not report legacy motion smooth");
  Require(motion_path_event["is_motion_path_smooth"] == true,
          "motion-path-smooth mode must report is_motion_path_smooth=true");
  Require(motion_path_event["uses_motion_smoothing"] == true,
          "motion-path-smooth mode must report uses_motion_smoothing=true");
  Require(motion_path_event["allows_temporal"] == false &&
              motion_path_event["allows_vertex"] == false &&
              motion_path_event["allows_spatial_topology"] == false,
          "motion-path-smooth must disallow temporal/vertex/spatial routes");

  bbsolver::SolverConfig aliased;
  aliased.solve_optimization_mode = "AUTO";  // alias for "full"
  const nlohmann::json aliased_event =
      bbsolver::BuildSolveModeCapabilitiesEvent(aliased);
  Require(aliased_event["mode"] == "full",
          "AUTO alias must normalize to full");
}

void TestCancellationStatusEventWithoutCancelFile() {
  const nlohmann::json event =
      bbsolver::BuildCancellationStatusEvent(std::nullopt);
  Require(event["event"] == "cancellation_status",
          "cancellation event must carry the cancellation_status name");
  Require(event["schema_version"] ==
              bbsolver::kSolverDiagnosticEventSchemaVersion,
          "cancellation event must stamp the schema version constant");
  Require(event["cancel_file_set"] == false,
          "absent cancel path must report cancel_file_set=false");
  Require(event["cancel_file_path"].get<std::string>().empty(),
          "absent cancel path must publish an empty path string");
  Require(event["cancel_file_exists"] == false,
          "absent cancel path must report cancel_file_exists=false");
  Require(event["partial_write_exit_code"] == 5,
          "cancellation event must publish the documented exit code 5");
}

void TestCancellationStatusEventWithMissingCancelFile() {
  const std::filesystem::path dir = MakeTempDir();
  const std::filesystem::path missing = dir / "does-not-exist.cancel";
  const nlohmann::json event =
      bbsolver::BuildCancellationStatusEvent(missing);
  Require(event["cancel_file_set"] == true,
          "set cancel path must report cancel_file_set=true even if missing");
  Require(event["cancel_file_path"].get<std::string>() == missing.string(),
          "set cancel path must publish the normalized path string");
  Require(event["cancel_file_exists"] == false,
          "missing cancel file must report cancel_file_exists=false");
  Require(event["partial_write_exit_code"] == 5,
          "cancellation event must publish the documented exit code 5");
  std::filesystem::remove_all(dir);
}

void TestCancellationStatusEventWithExistingCancelFile() {
  const std::filesystem::path dir = MakeTempDir();
  const std::filesystem::path present = dir / "stop.cancel";
  {
    std::ofstream out(present);
    out << "cancel\n";
  }
  const nlohmann::json event =
      bbsolver::BuildCancellationStatusEvent(present);
  Require(event["cancel_file_set"] == true,
          "set cancel path must report cancel_file_set=true");
  Require(event["cancel_file_path"].get<std::string>() == present.string(),
          "set cancel path must publish the normalized path string");
  Require(event["cancel_file_exists"] == true,
          "present cancel file must report cancel_file_exists=true");
  std::filesystem::remove_all(dir);
}

void TestSolveCancelledEventShape() {
  const nlohmann::json event = bbsolver::BuildSolveCancelledEvent(
      "req-123", "temporal_refit", 7, 3, 12.5);
  Require(event["event"] == "solve_cancelled",
          "cancelled event must carry the solve_cancelled name");
  Require(event["schema_version"] ==
              bbsolver::kSolverDiagnosticEventSchemaVersion,
          "cancelled event must stamp the schema version constant");
  Require(event["request_id"] == "req-123",
          "cancelled event must echo request id");
  Require(event["phase"] == "temporal_refit",
          "cancelled event must echo the cancellation phase");
  Require(event["property_index"] == 7,
          "cancelled event must echo property index");
  Require(event["properties_completed"] == 3,
          "cancelled event must echo completed count");
  Require(event["solve_time_ms"] == 12.5,
          "cancelled event must echo solve time");
  Require(event["partial_write_exit_code"] == 5,
          "cancelled event must publish partial-write exit code");
}

void TestSolveDoneEventShape() {
  const nlohmann::json event =
      bbsolver::BuildSolveDoneEvent("req-done", 6, 42, 99, 123.5);
  Require(event["event"] == "solve_done",
          "solve-done event must carry the solve_done name");
  Require(event["schema_version"] ==
              bbsolver::kSolverDiagnosticEventSchemaVersion,
          "solve-done event must stamp the schema version constant");
  Require(event["request_id"] == "req-done",
          "solve-done event must echo request id");
  Require(event["properties"] == 6,
          "solve-done event must echo property count");
  Require(event["total_keys"] == 42,
          "solve-done event must echo key count");
  Require(event["total_samples_input"] == 99,
          "solve-done event must echo sample count");
  Require(event["solve_time_ms"] == 123.5,
          "solve-done event must echo solve time");
}

void TestBridgePruneResultEventAcceptedShape() {
  bbsolver::PropertySamples property_samples;
  property_samples.property.id = "Layer/Path";
  property_samples.property.match_name = "ADBE Vector Shape";
  property_samples.property.display_name = "Bezier Path";

  bbsolver::PostSolvePathVertexReductionResult result;
  result.accepted = true;
  result.attempted = true;
  result.source_vertices = 16;
  result.fitted_vertices = 9;
  result.max_outline_error = 0.125;
  result.notes =
      "post_solve_vertex_reduction_accepted; "
      "mode=post_temporal_bridge_prune";

  const nlohmann::json event = bbsolver::BuildBridgePruneResultEvent(
      "req-bridge", property_samples, 2, 7, result);
  Require(event["event"] == "post_temporal_bridge_prune_result",
          "bridge-prune result event must carry the stable event name");
  Require(event["schema_version"] ==
              bbsolver::kSolverDiagnosticEventSchemaVersion,
          "bridge-prune result event must stamp the schema version constant");
  Require(event["request_id"] == "req-bridge",
          "bridge-prune result event must echo request id");
  Require(event["property_id"] == "Layer/Path",
          "bridge-prune result event must echo property id");
  Require(event["property_name"] == "Bezier Path",
          "bridge-prune result event must prefer display name");
  Require(event["property_index"] == 2,
          "bridge-prune result event must echo property index");
  Require(event["property_count"] == 7,
          "bridge-prune result event must echo property count");
  Require(event["accepted"] == true,
          "bridge-prune result event must echo accepted flag");
  Require(event["attempted"] == true,
          "bridge-prune result event must echo attempted flag");
  Require(event["source_vertices"] == 16,
          "bridge-prune result event must echo source vertices");
  Require(event["fitted_vertices"] == 9,
          "bridge-prune result event must echo fitted vertices");
  Require(event["max_outline_error"] == 0.125,
          "bridge-prune result event must echo outline error");
  Require(event["notes"].get<std::string>() == result.notes,
          "bridge-prune result event must preserve notes exactly");
}

void TestBridgePruneResultEventRejectedFallbackNames() {
  bbsolver::PropertySamples match_name_property;
  match_name_property.property.id = "match-only";
  match_name_property.property.match_name = "ADBE Match Only";

  bbsolver::PostSolvePathVertexReductionResult result;
  result.notes = "post_solve_vertex_reduction_rejected";

  const nlohmann::json match_event = bbsolver::BuildBridgePruneResultEvent(
      "req-rejected", match_name_property, 0, 1, result);
  Require(match_event["accepted"] == false,
          "bridge-prune rejected result must echo accepted=false");
  Require(match_event["attempted"] == false,
          "bridge-prune default result must echo attempted=false");
  Require(match_event["property_name"] == "ADBE Match Only",
          "bridge-prune result event must fall back to match name");
  Require(match_event["notes"] == "post_solve_vertex_reduction_rejected",
          "bridge-prune rejected result must preserve notes exactly");

  bbsolver::PropertySamples id_property;
  id_property.property.id = "id-only";
  const nlohmann::json id_event = bbsolver::BuildBridgePruneResultEvent(
      "req-id", id_property, 0, 1, result);
  Require(id_event["property_name"] == "id-only",
          "bridge-prune result event must fall back to property id");

  bbsolver::PropertySamples unnamed_property;
  const nlohmann::json unnamed_event = bbsolver::BuildBridgePruneResultEvent(
      "req-unnamed", unnamed_property, 0, 1, result);
  Require(unnamed_event["property_name"] == "<unnamed>",
          "bridge-prune result event must use the unnamed sentinel");
}

void TestBridgePruneResultEventDefaultSummaryOnly() {
  bbsolver::PropertySamples property_samples;
  property_samples.property.id = "shape-default";

  bbsolver::PostSolvePathVertexReductionResult result;
  result.keys.keys.push_back(bbsolver::Key{});

  const nlohmann::json event = bbsolver::BuildBridgePruneResultEvent(
      "req-default", property_samples, 1, 3, result);
  Require(event["accepted"] == false,
          "bridge-prune default event must echo accepted=false");
  Require(event["attempted"] == false,
          "bridge-prune default event must echo attempted=false");
  Require(event["source_vertices"] == 0,
          "bridge-prune default event must echo zero source vertices");
  Require(event["fitted_vertices"] == 0,
          "bridge-prune default event must echo zero fitted vertices");
  Require(event["max_outline_error"] == 0.0,
          "bridge-prune default event must echo zero outline error");
  Require(event["notes"] == "",
          "bridge-prune default event must preserve empty notes");
  Require(!event.contains("keys"),
          "bridge-prune diagnostic event must not serialize output keys");
  Require(!event.contains("progress"),
          "bridge-prune diagnostic event must not duplicate progress JSON");
  Require(!event.contains("cancelled"),
          "bridge-prune diagnostic event must not add cancellation fields");
}

void TestBridgePrunePhaseEventShape() {
  bbsolver::PropertySamples property_samples;
  property_samples.property.id = "Layer/Path";
  property_samples.property.display_name = "Bezier Path";

  bbsolver::BridgePrunePhaseDiagnosticInput input;
  input.request_id = "req-phase";
  input.phase = "candidate_progress";
  input.property_index = 3;
  input.property_count = 8;
  input.target_vertices = 12;
  input.removed_index = 4;
  input.candidate_count = 10;
  input.candidates_checked = 6;
  input.attempt = 14;
  input.accepted = true;
  input.batch = true;

  const nlohmann::json event =
      bbsolver::BuildBridgePrunePhaseEvent(property_samples, input);
  Require(event["event"] == "post_temporal_bridge_prune_phase",
          "bridge-prune phase event must carry the stable event name");
  Require(event["schema_version"] ==
              bbsolver::kSolverDiagnosticEventSchemaVersion,
          "bridge-prune phase event must stamp the schema version constant");
  Require(event["request_id"] == "req-phase",
          "bridge-prune phase event must echo request id");
  Require(event["property_id"] == "Layer/Path",
          "bridge-prune phase event must echo property id");
  Require(event["property_name"] == "Bezier Path",
          "bridge-prune phase event must use diagnostic property name");
  Require(event["property_index"] == 3 && event["property_count"] == 8,
          "bridge-prune phase event must echo property position");
  Require(event["phase"] == "candidate_progress",
          "bridge-prune phase event must echo phase");
  Require(event["target_vertices"] == 12 && event["removed_index"] == 4,
          "bridge-prune phase event must echo candidate identity");
  Require(event["candidate_count"] == 10 &&
              event["candidates_checked"] == 6,
          "bridge-prune phase event must echo candidate counters");
  Require(event["attempt"] == 14,
          "bridge-prune phase event must echo attempt count");
  Require(event["accepted"] == true && event["batch"] == true,
          "bridge-prune phase event must echo accepted/batch flags");
  Require(!event.contains("progress"),
          "bridge-prune phase diagnostic event must not duplicate progress");
  Require(!event.contains("notes"),
          "bridge-prune phase diagnostic event must not duplicate result notes");
}

void TestEventBuildersAreSideEffectFree() {
  // Run each builder twice with identical inputs and assert byte-equal
  // results. A builder that accidentally mutated global state, wrote to
  // disk, or recorded telemetry would diverge across calls.
  const nlohmann::json a1 = bbsolver::BuildParallelRuntimeEvent(0);
  const nlohmann::json a2 = bbsolver::BuildParallelRuntimeEvent(0);
  Require(a1 == a2,
          "parallel runtime builder must be deterministic across calls");

  bbsolver::SolverConfig config;
  config.solve_optimization_mode = "vertex_only";
  const nlohmann::json b1 = bbsolver::BuildSolveModeCapabilitiesEvent(config);
  const nlohmann::json b2 = bbsolver::BuildSolveModeCapabilitiesEvent(config);
  Require(b1 == b2,
          "solve mode capabilities builder must be deterministic");

  const nlohmann::json c1 =
      bbsolver::BuildCancellationStatusEvent(std::nullopt);
  const nlohmann::json c2 =
      bbsolver::BuildCancellationStatusEvent(std::nullopt);
  Require(c1 == c2,
          "cancellation status builder must be deterministic");

  bbsolver::PropertySamples property_samples;
  property_samples.property.id = "shape";
  bbsolver::PostSolvePathVertexReductionResult result;
  result.notes = "unchanged notes";
  const nlohmann::json d1 = bbsolver::BuildBridgePruneResultEvent(
      "req-bridge", property_samples, 0, 1, result);
  const nlohmann::json d2 = bbsolver::BuildBridgePruneResultEvent(
      "req-bridge", property_samples, 0, 1, result);
  Require(d1 == d2,
          "bridge-prune result builder must be deterministic");

  bbsolver::BridgePrunePhaseDiagnosticInput phase_input;
  phase_input.request_id = "req-phase";
  phase_input.phase = "candidate_start";
  const nlohmann::json e1 =
      bbsolver::BuildBridgePrunePhaseEvent(property_samples, phase_input);
  const nlohmann::json e2 =
      bbsolver::BuildBridgePrunePhaseEvent(property_samples, phase_input);
  Require(e1 == e2,
          "bridge-prune phase builder must be deterministic");
}

}  // namespace

int main() {
  TestSolveStartEventShape();
  TestParallelRuntimeEventShapeForAutoRequest();
  TestParallelRuntimeEventPositiveRequest();
  TestParallelRuntimeEventNegativeRequestReportsError();
  TestSolveModeCapabilitiesEventForFull();
  TestSolveModeCapabilitiesEventForTemporalOnly();
  TestSolveModeCapabilitiesEventForMotionSmoothAndAliases();
  TestCancellationStatusEventWithoutCancelFile();
  TestCancellationStatusEventWithMissingCancelFile();
  TestCancellationStatusEventWithExistingCancelFile();
  TestSolveCancelledEventShape();
  TestSolveDoneEventShape();
  TestBridgePruneResultEventAcceptedShape();
  TestBridgePruneResultEventRejectedFallbackNames();
  TestBridgePruneResultEventDefaultSummaryOnly();
  TestBridgePrunePhaseEventShape();
  TestEventBuildersAreSideEffectFree();
  std::cout << "[PASS] test_solver_diagnostic_events\n";
  return 0;
}
