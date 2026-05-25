#include "bbsolver/fit/segment_fit_diagnostic_events.hpp"
#include "bbsolver/domain.hpp"

#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include "bbsolver/metrics/error_metrics.hpp"
#include "bbsolver/dp/dp_placer.hpp"
#include "nlohmann/json_fwd.hpp"
#include "bbsolver/fit/segment_fit_ceres.hpp"
#include <nlohmann/json.hpp>

namespace bbsolver::segment_fit {
namespace {

nlohmann::json FiniteOrNull(double value) {
  if (!std::isfinite(value)) {
    return nullptr;
  }
  return value;
}

const char* InterpName(InterpType interp) {
  switch (interp) {
    case InterpType::Hold:
      return "hold";
    case InterpType::Linear:
      return "linear";
    case InterpType::Bezier:
      return "bezier";
  }
  return "unknown";
}

nlohmann::json BaseEvent(const char* event_name, const std::string& request_id) {
  return {
      {"event", event_name},
      {"schema_version", kSegmentFitDiagnosticEventSchemaVersion},
      {"request_id", request_id},
  };
}

void AddSampleRange(nlohmann::json& event, int sample_i, int sample_j) {
  event["sample_i"] = sample_i;
  event["sample_j"] = sample_j;
}

void AddResultSummary(nlohmann::json& event, const SegmentFitResult& result) {
  event["feasible"] = result.feasible;
  event["interp"] = InterpName(result.interp);
  event["reason"] = result.reason;
  event["max_err"] = FiniteOrNull(result.max_err);
  event["max_err_screen_px"] = FiniteOrNull(result.max_err_screen_px);
  event["rms_err"] = FiniteOrNull(result.rms_err);
  event["iters"] = result.iters;
  event["ease_out_channels"] = result.ease_out_at_i.size();
  event["ease_in_channels"] = result.ease_in_at_j.size();
  event["spatial_out_dims"] = result.spatial_out_at_i.size();
  event["spatial_in_dims"] = result.spatial_in_at_j.size();
}

void AddOptionalReport(nlohmann::json& event,
                       const std::optional<ErrorReport>& report) {
  if (!report.has_value()) {
    event["report"] = nullptr;
    return;
  }
  event["report"] = {
      {"max_err", FiniteOrNull(report->max_err)},
      {"max_err_screen_px", FiniteOrNull(report->max_err_screen_px)},
      {"rms_err", FiniteOrNull(report->rms_err)},
      {"worst_sample_idx", report->worst_sample_idx},
      {"units_evaluated", report->units_evaluated},
      {"fail_fast_exit", report->fail_fast_exit},
      {"shape_outline_wall_ms", FiniteOrNull(report->shape_outline_wall_ms)},
  };
}

}  // namespace

nlohmann::json BuildSegmentFitPolicyDiagnostic(
    const std::string& request_id,
    const std::string& surface,
    int sample_i,
    int sample_j,
    bool passed,
    const SegmentFitResult& result,
    const std::optional<ErrorReport>& report) {
  nlohmann::json event =
      BaseEvent("segment_fit_policy_result", request_id);
  event["surface"] = surface;
  AddSampleRange(event, sample_i, sample_j);
  event["passed"] = passed;
  AddResultSummary(event, result);
  AddOptionalReport(event, report);
  return event;
}

nlohmann::json BuildSegmentFitCeresAdapterDiagnostic(
    const std::string& request_id,
    const std::string& adapter,
    int sample_i,
    int sample_j,
    int dim,
    const DimCeresResult& result) {
  nlohmann::json event =
      BaseEvent("segment_fit_ceres_adapter_result", request_id);
  event["adapter"] = adapter;
  AddSampleRange(event, sample_i, sample_j);
  event["dim"] = dim;
  event["ease_out_speed"] = FiniteOrNull(result.ease_out.speed);
  event["ease_out_influence"] = FiniteOrNull(result.ease_out.influence);
  event["ease_in_speed"] = FiniteOrNull(result.ease_in.speed);
  event["ease_in_influence"] = FiniteOrNull(result.ease_in.influence);
  event["spatial_out"] = FiniteOrNull(result.spatial_out);
  event["spatial_in"] = FiniteOrNull(result.spatial_in);
  event["iters"] = result.iters;
  return event;
}

nlohmann::json BuildSegmentFitShapeTemporalDiagnostic(
    const std::string& request_id,
    int sample_i,
    int sample_j,
    const SegmentFitResult& result) {
  nlohmann::json event =
      BaseEvent("segment_fit_shape_temporal_result", request_id);
  AddSampleRange(event, sample_i, sample_j);
  AddResultSummary(event, result);
  event["attempts"] = result.fit_shape_temporal_attempts;
  event["gate_rejections"] = result.fit_shape_temporal_gate_rejections;
  event["outline_evaluations"] =
      result.fit_shape_temporal_outline_evaluations;
  event["ceres_wall_ms"] =
      FiniteOrNull(result.fit_shape_temporal_ceres_wall_ms);
  event["outline_wall_ms"] =
      FiniteOrNull(result.fit_shape_temporal_outline_wall_ms);
  event["total_wall_ms"] =
      FiniteOrNull(result.fit_shape_temporal_total_wall_ms);
  return event;
}

nlohmann::json BuildSegmentFitUnifiedSpatialDiagnostic(
    const std::string& request_id,
    int sample_i,
    int sample_j,
    double path_length,
    int target_count,
    const SegmentFitResult& result) {
  nlohmann::json event =
      BaseEvent("segment_fit_unified_spatial_result", request_id);
  AddSampleRange(event, sample_i, sample_j);
  AddResultSummary(event, result);
  event["path_length"] = FiniteOrNull(path_length);
  event["target_count"] = target_count;
  return event;
}

}  // namespace bbsolver::segment_fit
