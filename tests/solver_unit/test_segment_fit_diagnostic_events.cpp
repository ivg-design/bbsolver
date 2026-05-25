#include "bbsolver/fit/segment_fit_diagnostic_events.hpp"
#include "bbsolver/domain.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#include <cstdlib>
#include "bbsolver/metrics/error_metrics.hpp"
#include "bbsolver/dp/dp_placer.hpp"
#include "nlohmann/json_fwd.hpp"
#include "bbsolver/fit/segment_fit_ceres.hpp"
#include <nlohmann/json.hpp>

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    std::abort();
  }
}

bbsolver::SegmentFitResult Result(const std::string& reason) {
  bbsolver::SegmentFitResult result;
  result.feasible = true;
  result.interp = bbsolver::InterpType::Bezier;
  result.reason = reason;
  result.max_err = 0.25;
  result.max_err_screen_px = 0.5;
  result.rms_err = 0.125;
  result.iters = 7;
  result.ease_out_at_i = {{1.0, 33.3}};
  result.ease_in_at_j = {{2.0, 66.6}};
  result.spatial_out_at_i = {3.0, 4.0};
  result.spatial_in_at_j = {-3.0, -4.0};
  return result;
}

void TestPolicyDiagnosticWithAbsentReportUsesNull() {
  const bbsolver::SegmentFitResult result = Result("linear");
  const nlohmann::json event =
      bbsolver::segment_fit::BuildSegmentFitPolicyDiagnostic(
          "req-policy", "linear_acceptance", 1, 5, true, result);

  Require(event["event"] == "segment_fit_policy_result",
          "policy diagnostic event name changed");
  Require(event["schema_version"] ==
              bbsolver::segment_fit::kSegmentFitDiagnosticEventSchemaVersion,
          "policy diagnostic schema version changed");
  Require(event["request_id"] == "req-policy",
          "policy diagnostic must echo request_id");
  Require(event["surface"] == "linear_acceptance",
          "policy diagnostic must echo surface");
  Require(event["sample_i"] == 1 && event["sample_j"] == 5,
          "policy diagnostic must echo sample range");
  Require(event["passed"] == true, "policy diagnostic must echo pass flag");
  Require(event["feasible"] == true, "policy diagnostic must summarize feasibility");
  Require(event["interp"] == "bezier", "policy diagnostic interp name changed");
  Require(event["reason"] == "linear", "policy diagnostic reason changed");
  Require(event["max_err"] == 0.25, "policy diagnostic max_err changed");
  Require(event["max_err_screen_px"] == 0.5,
          "policy diagnostic screen error changed");
  Require(event["rms_err"] == 0.125, "policy diagnostic rms error changed");
  Require(event["iters"] == 7, "policy diagnostic iteration count changed");
  Require(event["ease_out_channels"] == 1,
          "policy diagnostic ease-out channel count changed");
  Require(event["spatial_out_dims"] == 2,
          "policy diagnostic spatial-out dimension count changed");
  Require(event["report"].is_null(),
          "policy diagnostic must use null report when absent");
}

void TestPolicyDiagnosticWithReportLocksReportFieldsAndNullNumbers() {
  bbsolver::SegmentFitResult result = Result("infeasible_linear");
  result.max_err = NAN;
  bbsolver::ErrorReport report;
  report.max_err = 2.0;
  report.max_err_screen_px = INFINITY;
  report.rms_err = 1.0;
  report.worst_sample_idx = 4;
  report.units_evaluated = 12;
  report.fail_fast_exit = true;
  report.shape_outline_wall_ms = 0.75;

  const nlohmann::json event =
      bbsolver::segment_fit::BuildSegmentFitPolicyDiagnostic(
          "req-report", "hold_acceptance", 0, 4, false, result, report);

  Require(event["reason"] == "infeasible_linear",
          "policy diagnostic must preserve stable reason strings");
  Require(event["max_err"].is_null(),
          "policy diagnostic must encode non-finite result numbers as null");
  Require(event["report"]["max_err"] == 2.0,
          "policy diagnostic report max_err changed");
  Require(event["report"]["max_err_screen_px"].is_null(),
          "policy diagnostic report non-finite screen error must be null");
  Require(event["report"]["rms_err"] == 1.0,
          "policy diagnostic report rms_err changed");
  Require(event["report"]["worst_sample_idx"] == 4,
          "policy diagnostic report worst sample changed");
  Require(event["report"]["units_evaluated"] == 12,
          "policy diagnostic report units changed");
  Require(event["report"]["fail_fast_exit"] == true,
          "policy diagnostic report fail-fast flag changed");
  Require(event["report"]["shape_outline_wall_ms"] == 0.75,
          "policy diagnostic report outline timing changed");
}

void TestCeresAdapterDiagnosticLocksDimResultSurface() {
  bbsolver::segment_fit::DimCeresResult result;
  result.ease_out = {8.0, 44.0};
  result.ease_in = {9.0, 55.0};
  result.spatial_out = 3.5;
  result.spatial_in = -4.5;
  result.iters = 6;

  const nlohmann::json event =
      bbsolver::segment_fit::BuildSegmentFitCeresAdapterDiagnostic(
          "req-ceres", "single_dim", 2, 8, 1, result);

  Require(event["event"] == "segment_fit_ceres_adapter_result",
          "Ceres adapter diagnostic event name changed");
  Require(event["schema_version"] == 1,
          "Ceres adapter diagnostic schema version changed");
  Require(event["adapter"] == "single_dim",
          "Ceres adapter diagnostic adapter name changed");
  Require(event["sample_i"] == 2 && event["sample_j"] == 8,
          "Ceres adapter diagnostic sample range changed");
  Require(event["dim"] == 1, "Ceres adapter diagnostic dim changed");
  Require(event["ease_out_speed"] == 8.0,
          "Ceres adapter diagnostic out speed changed");
  Require(event["ease_out_influence"] == 44.0,
          "Ceres adapter diagnostic out influence changed");
  Require(event["ease_in_speed"] == 9.0,
          "Ceres adapter diagnostic in speed changed");
  Require(event["ease_in_influence"] == 55.0,
          "Ceres adapter diagnostic in influence changed");
  Require(event["spatial_out"] == 3.5,
          "Ceres adapter diagnostic spatial_out changed");
  Require(event["spatial_in"] == -4.5,
          "Ceres adapter diagnostic spatial_in changed");
  Require(event["iters"] == 6, "Ceres adapter diagnostic iters changed");
}

void TestShapeTemporalDiagnosticLocksAttemptFields() {
  bbsolver::SegmentFitResult result = Result("shape_temporal_bezier_ok");
  result.fit_shape_temporal_attempts = 1;
  result.fit_shape_temporal_gate_rejections = 0;
  result.fit_shape_temporal_outline_evaluations = 25;
  result.fit_shape_temporal_ceres_wall_ms = 1.5;
  result.fit_shape_temporal_outline_wall_ms = 2.5;
  result.fit_shape_temporal_total_wall_ms = 4.0;

  const nlohmann::json event =
      bbsolver::segment_fit::BuildSegmentFitShapeTemporalDiagnostic(
          "req-shape", 0, 24, result);

  Require(event["event"] == "segment_fit_shape_temporal_result",
          "shape-temporal diagnostic event name changed");
  Require(event["reason"] == "shape_temporal_bezier_ok",
          "shape-temporal diagnostic reason changed");
  Require(event["attempts"] == 1,
          "shape-temporal diagnostic attempts changed");
  Require(event["gate_rejections"] == 0,
          "shape-temporal diagnostic gate rejections changed");
  Require(event["outline_evaluations"] == 25,
          "shape-temporal diagnostic outline evaluations changed");
  Require(event["ceres_wall_ms"] == 1.5,
          "shape-temporal diagnostic Ceres timing changed");
  Require(event["outline_wall_ms"] == 2.5,
          "shape-temporal diagnostic outline timing changed");
  Require(event["total_wall_ms"] == 4.0,
          "shape-temporal diagnostic total timing changed");
}

void TestUnifiedSpatialDiagnosticLocksPathFields() {
  bbsolver::SegmentFitResult result = Result("unified_spatial_speed_ok");
  result.interp = bbsolver::InterpType::Linear;
  const nlohmann::json event =
      bbsolver::segment_fit::BuildSegmentFitUnifiedSpatialDiagnostic(
          "req-spatial", 3, 9, INFINITY, 7, result);

  Require(event["event"] == "segment_fit_unified_spatial_result",
          "unified-spatial diagnostic event name changed");
  Require(event["reason"] == "unified_spatial_speed_ok",
          "unified-spatial diagnostic reason changed");
  Require(event["interp"] == "linear",
          "unified-spatial diagnostic interp name changed");
  Require(event["path_length"].is_null(),
          "unified-spatial diagnostic must null non-finite path length");
  Require(event["target_count"] == 7,
          "unified-spatial diagnostic target count changed");
}

}  // namespace

int main() {
  TestPolicyDiagnosticWithAbsentReportUsesNull();
  TestPolicyDiagnosticWithReportLocksReportFieldsAndNullNumbers();
  TestCeresAdapterDiagnosticLocksDimResultSurface();
  TestShapeTemporalDiagnosticLocksAttemptFields();
  TestUnifiedSpatialDiagnosticLocksPathFields();
  return 0;
}
