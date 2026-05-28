#include "bbsolver/progress/progress.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/dp/dp_placer.hpp"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <mutex>
#include <stdexcept>
#include <string>

#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <io.h>
#include <limits>
#else
#include <unistd.h>
#endif

namespace bbsolver {
namespace {

std::ptrdiff_t WriteProgressFd(int fd, const char* data, std::size_t size) {
#ifdef _WIN32
  const std::size_t chunk =
      std::min<std::size_t>(size, std::numeric_limits<unsigned int>::max());
  return static_cast<std::ptrdiff_t>(
::_write(fd, data, static_cast<unsigned int>(chunk)));
#else
  return static_cast<std::ptrdiff_t>(::write(fd, data, size));
#endif
}

}  // namespace

ProgressWriter::ProgressWriter(int fd): fd_(fd) {}

void ProgressWriter::Emit(const nlohmann::json& event) const {
  if (fd_ < 0) {
    return;
  }
  const std::lock_guard<std::mutex> lock(mutex_);
  std::string line = event.dump();
  line.push_back('\n');
  const char* data = line.data();
  std::size_t remaining = line.size();
  while (remaining > 0) {
    const std::ptrdiff_t written = WriteProgressFd(fd_, data, remaining);
    if (written < 0) {
      if (errno != EINTR) {
        throw std::runtime_error("Failed to write progress event");
      }
    } else if (written == 0) {
      throw std::runtime_error("Failed to write progress event");
    } else {
      data += written;
      remaining -= static_cast<std::size_t>(written);
    }
  }
}

double SolveProgressForPropertyStage(std::size_t property_idx,
                                     std::size_t property_count,
                                     double stage_fraction) {
  const double stage = std::clamp(stage_fraction, 0.0, 1.0);
  if (property_count == 0) {
    return stage;
  }
  const double property_span = 0.96 / static_cast<double>(property_count);
  return std::clamp(
      0.02 + (static_cast<double>(property_idx) + stage) * property_span,
      0.0,
      0.98);
}

std::string ProgressPropertyLabel(const PropertySamples& ps) {
  if (!ps.property.display_name.empty()) {
    return ps.property.display_name;
  }
  if (!ps.property.match_name.empty()) {
    return ps.property.match_name;
  }
  if (!ps.property.id.empty()) {
    return ps.property.id;
  }
  return "<unnamed>";
}

nlohmann::json PropertyProgressEvent(const char* event,
                                     const std::string& phase,
                                     std::size_t property_idx,
                                     std::size_t property_count,
                                     double stage_fraction,
                                     const PropertySamples& ps) {
  const std::string label = ProgressPropertyLabel(ps);
  nlohmann::json out = {
      {"event", event},
      {"phase", phase},
      {"progress", SolveProgressForPropertyStage(property_idx,
                                                  property_count,
                                                  stage_fraction)},
      {"id", ps.property.id},
      {"display_name", label},
      {"i", property_idx},
      {"n", property_count},
      {"samples", ps.samples.size()},
  };
  if (!ps.property.units_label.empty()) {
    out["units_label"] = ps.property.units_label;
  }
  if (!ps.property.layer_path.empty()) {
    out["layer_path"] = ps.property.layer_path;
  }
  return out;
}

nlohmann::json PlacementProgressEvent(const char* event,
                                      const std::string& phase_prefix,
                                      std::size_t property_idx,
                                      std::size_t property_count,
                                      double stage_start,
                                      double stage_end,
                                      const PropertySamples& ps,
                                      const PlacementProgress& placement) {
  const int total = std::max(1, placement.step_total);
  const int step = std::clamp(placement.step_index, 0, total);
  const double local =
      std::clamp(static_cast<double>(step) / static_cast<double>(total),
                 0.0,
                 1.0);
  const double stage_fraction =
      stage_start + (stage_end - stage_start) * local;
  nlohmann::json out = PropertyProgressEvent(
      event,
      phase_prefix + " " + placement.stage + " " +
          std::to_string(step) + "/" + std::to_string(total) +
          " for " + ProgressPropertyLabel(ps),
      property_idx,
      property_count,
      stage_fraction,
      ps);
  out["placement_stage"] = placement.stage;
  out["placement_step"] = step;
  out["placement_total"] = total;
  out["sample_index"] = placement.sample_index;
  out["samples"] = placement.samples;
  out["segments_tried"] = placement.segments_tried;
  out["segment_checks"] = placement.segments_tried;
  out["segments_feasible"] = placement.segments_feasible;
  if (placement.dp_candidate_slots > 0) {
    out["dp_candidate_slots"] = placement.dp_candidate_slots;
    out["dp_unreachable_candidates"] = placement.dp_unreachable_candidates;
    out["dp_incompatible_candidates"] =
        placement.dp_incompatible_candidates;
    out["dp_final_anchor_candidate_slots"] =
        placement.dp_final_anchor_candidate_slots;
  }
  if (placement.dp_fit_wall_ms > 0.0 ||
      placement.dp_reduction_wall_ms > 0.0) {
    out["dp_fit_wall_ms"] = placement.dp_fit_wall_ms;
    out["dp_reduction_wall_ms"] = placement.dp_reduction_wall_ms;
    out["dp_final_anchor_fit_wall_ms"] =
        placement.dp_final_anchor_fit_wall_ms;
    out["dp_final_anchor_reduction_wall_ms"] =
        placement.dp_final_anchor_reduction_wall_ms;
  }
  if (placement.fit_segment_hold_attempts > 0 ||
      placement.fit_segment_linear_attempts > 0 ||
      placement.fit_shape_temporal_attempts > 0 ||
      placement.fit_shape_temporal_gate_rejections > 0 ||
      placement.fit_shape_temporal_outline_evaluations > 0 ||
      placement.fit_segment_hold_units_evaluated > 0 ||
      placement.fit_segment_linear_units_evaluated > 0 ||
      placement.fit_segment_hold_fail_fast_exits > 0 ||
      placement.fit_segment_linear_fail_fast_exits > 0 ||
      placement.fit_segment_hold_wall_ms > 0.0 ||
      placement.fit_segment_linear_wall_ms > 0.0 ||
      placement.fit_segment_hold_shape_outline_wall_ms > 0.0 ||
      placement.fit_segment_linear_shape_outline_wall_ms > 0.0 ||
      placement.fit_shape_temporal_ceres_wall_ms > 0.0 ||
      placement.fit_shape_temporal_outline_wall_ms > 0.0 ||
      placement.fit_shape_temporal_total_wall_ms > 0.0) {
    out["fit_segment_hold_attempts"] =
        placement.fit_segment_hold_attempts;
    out["fit_segment_linear_attempts"] =
        placement.fit_segment_linear_attempts;
    out["fit_segment_hold_units_evaluated"] =
        placement.fit_segment_hold_units_evaluated;
    out["fit_segment_linear_units_evaluated"] =
        placement.fit_segment_linear_units_evaluated;
    out["fit_segment_hold_fail_fast_exits"] =
        placement.fit_segment_hold_fail_fast_exits;
    out["fit_segment_linear_fail_fast_exits"] =
        placement.fit_segment_linear_fail_fast_exits;
    out["fit_shape_temporal_attempts"] =
        placement.fit_shape_temporal_attempts;
    out["fit_shape_temporal_gate_rejections"] =
        placement.fit_shape_temporal_gate_rejections;
    out["fit_shape_temporal_outline_evaluations"] =
        placement.fit_shape_temporal_outline_evaluations;
    out["fit_segment_hold_wall_ms"] =
        placement.fit_segment_hold_wall_ms;
    out["fit_segment_linear_wall_ms"] =
        placement.fit_segment_linear_wall_ms;
    out["fit_segment_hold_shape_outline_wall_ms"] =
        placement.fit_segment_hold_shape_outline_wall_ms;
    out["fit_segment_linear_shape_outline_wall_ms"] =
        placement.fit_segment_linear_shape_outline_wall_ms;
    out["fit_shape_temporal_ceres_wall_ms"] =
        placement.fit_shape_temporal_ceres_wall_ms;
    out["fit_shape_temporal_outline_wall_ms"] =
        placement.fit_shape_temporal_outline_wall_ms;
    out["fit_shape_temporal_total_wall_ms"] =
        placement.fit_shape_temporal_total_wall_ms;
  }
  if (placement.fit_replacement_oracle_calls > 0 ||
      placement.fit_replacement_relaxed_attempts > 0) {
    out["fit_replacement_oracle_calls"] =
        placement.fit_replacement_oracle_calls;
    out["fit_replacement_oracle_evaluations"] =
        placement.fit_replacement_oracle_evaluations;
    out["fit_replacement_relaxed_attempts"] =
        placement.fit_replacement_relaxed_attempts;
    out["fit_replacement_relaxed_validations"] =
        placement.fit_replacement_relaxed_validations;
    out["fit_replacement_oracle_wall_ms"] =
        placement.fit_replacement_oracle_wall_ms;
    out["fit_replacement_outline_wall_ms"] =
        placement.fit_replacement_outline_wall_ms;
    out["fit_replacement_relaxed_wall_ms"] =
        placement.fit_replacement_relaxed_wall_ms;
  }
  return out;
}

}  // namespace bbsolver
