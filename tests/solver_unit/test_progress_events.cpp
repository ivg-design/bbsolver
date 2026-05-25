#include "bbsolver/progress/progress.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/dp/dp_placer.hpp"

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>

#ifndef _WIN32
#include <unistd.h>
#include <sys/types.h>
#endif

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

bbsolver::PropertySamples MakePropertySamples() {
  bbsolver::PropertySamples ps;
  ps.property.id = "layer/path/property";
  ps.property.match_name = "ADBE Position";
  ps.property.display_name = "Position";
  ps.property.layer_path = "Comp/Layer";
  ps.property.units_label = "px";
  ps.property.kind = bbsolver::ValueKind::TwoD;
  ps.property.dimensions = 2;
  ps.samples.push_back({0.0, {0.0, 0.0}, std::nullopt});
  ps.samples.push_back({1.0, {10.0, 20.0}, std::nullopt});
  return ps;
}

void TestPropertyStageProgressMath() {
  RequireNear(bbsolver::SolveProgressForPropertyStage(0, 0, -1.0),
              0.0,
              "zero-property progress must clamp low");
  RequireNear(bbsolver::SolveProgressForPropertyStage(0, 0, 2.0),
              1.0,
              "zero-property progress must clamp high");

  RequireNear(bbsolver::SolveProgressForPropertyStage(0, 2, 0.0),
              0.02,
              "first property starts at the reserved lower band");
  RequireNear(bbsolver::SolveProgressForPropertyStage(0, 2, 1.0),
              0.50,
              "first property ends halfway through the 0.02..0.98 band");
  RequireNear(bbsolver::SolveProgressForPropertyStage(1, 2, 0.0),
              0.50,
              "second property starts where first property ends");
  RequireNear(bbsolver::SolveProgressForPropertyStage(1, 2, 1.0),
              0.98,
              "last property ends at the reserved upper band");
  RequireNear(bbsolver::SolveProgressForPropertyStage(50, 2, 1.0),
              0.98,
              "property progress must clamp overflowing indices");
}

void TestPropertyLabelFallback() {
  bbsolver::PropertySamples ps = MakePropertySamples();
  Require(bbsolver::ProgressPropertyLabel(ps) == "Position",
          "display name should be preferred");
  ps.property.display_name.clear();
  Require(bbsolver::ProgressPropertyLabel(ps) == "ADBE Position",
          "match name should be second fallback");
  ps.property.match_name.clear();
  Require(bbsolver::ProgressPropertyLabel(ps) == "layer/path/property",
          "property id should be third fallback");
  ps.property.id.clear();
  Require(bbsolver::ProgressPropertyLabel(ps) == "<unnamed>",
          "empty property metadata should use unnamed fallback");
}

void TestPropertyProgressEventFields() {
  const bbsolver::PropertySamples ps = MakePropertySamples();
  const nlohmann::json event = bbsolver::PropertyProgressEvent(
      "property_prepare", "Preparing Position", 1, 3, 0.25, ps);

  Require(event.at("event") == "property_prepare", "event name mismatch");
  Require(event.at("phase") == "Preparing Position", "phase mismatch");
  RequireNear(event.at("progress").get<double>(),
              bbsolver::SolveProgressForPropertyStage(1, 3, 0.25),
              "progress value mismatch");
  Require(event.at("id") == ps.property.id, "property id mismatch");
  Require(event.at("display_name") == "Position", "display label mismatch");
  Require(event.at("i") == 1, "property index mismatch");
  Require(event.at("n") == 3, "property count mismatch");
  Require(event.at("samples") == 2, "sample count mismatch");
  Require(event.at("units_label") == "px", "units label mismatch");
  Require(event.at("layer_path") == "Comp/Layer", "layer path mismatch");
}

void TestPlacementProgressEventAttributionFields() {
  const bbsolver::PropertySamples ps = MakePropertySamples();
  bbsolver::PlacementProgress placement;
  placement.stage = "dp_anchor";
  placement.step_index = 2;
  placement.step_total = 4;
  placement.sample_index = 7;
  placement.samples = 9;
  placement.segments_tried = 11;
  placement.segments_feasible = 5;
  placement.dp_candidate_slots = 13;
  placement.dp_unreachable_candidates = 2;
  placement.dp_incompatible_candidates = 3;
  placement.dp_final_anchor_candidate_slots = 4;
  placement.dp_fit_wall_ms = 5.5;
  placement.dp_reduction_wall_ms = 0.25;
  placement.dp_final_anchor_fit_wall_ms = 1.25;
  placement.dp_final_anchor_reduction_wall_ms = 0.125;

  placement.fit_segment_hold_attempts = 17;
  placement.fit_segment_linear_attempts = 19;
  placement.fit_segment_hold_units_evaluated = 23;
  placement.fit_segment_linear_units_evaluated = 29;
  placement.fit_segment_hold_fail_fast_exits = 31;
  placement.fit_segment_linear_fail_fast_exits = 37;
  placement.fit_shape_temporal_attempts = 41;
  placement.fit_shape_temporal_gate_rejections = 43;
  placement.fit_shape_temporal_outline_evaluations = 47;
  placement.fit_segment_hold_wall_ms = 1.5;
  placement.fit_segment_linear_wall_ms = 2.5;
  placement.fit_segment_hold_shape_outline_wall_ms = 3.5;
  placement.fit_segment_linear_shape_outline_wall_ms = 4.5;
  placement.fit_shape_temporal_ceres_wall_ms = 5.5;
  placement.fit_shape_temporal_outline_wall_ms = 6.5;
  placement.fit_shape_temporal_total_wall_ms = 7.5;

  placement.fit_replacement_oracle_calls = 53;
  placement.fit_replacement_oracle_evaluations = 59;
  placement.fit_replacement_relaxed_attempts = 61;
  placement.fit_replacement_relaxed_validations = 67;
  placement.fit_replacement_oracle_wall_ms = 8.5;
  placement.fit_replacement_outline_wall_ms = 9.5;
  placement.fit_replacement_relaxed_wall_ms = 10.5;

  const nlohmann::json event = bbsolver::PlacementProgressEvent(
      "temporal_solve_progress", "Solving temporal placement", 0, 2,
      0.20, 0.80, ps, placement);

  Require(event.at("event") == "temporal_solve_progress",
          "placement event name mismatch");
  Require(event.at("placement_stage") == "dp_anchor",
          "placement stage mismatch");
  Require(event.at("placement_step") == 2, "placement step mismatch");
  Require(event.at("placement_total") == 4, "placement total mismatch");
  Require(event.at("sample_index") == 7, "sample index mismatch");
  Require(event.at("samples") == 9, "placement sample count mismatch");
  Require(event.at("segments_tried") == 11, "segments tried mismatch");
  Require(event.at("segment_checks") == 11, "segment checks mismatch");
  Require(event.at("segments_feasible") == 5, "segments feasible mismatch");
  RequireNear(event.at("progress").get<double>(),
              bbsolver::SolveProgressForPropertyStage(0, 2, 0.50),
              "placement local progress mapping mismatch");

  Require(event.at("dp_candidate_slots") == 13, "DP slots missing");
  Require(event.at("dp_unreachable_candidates") == 2,
          "DP unreachable missing");
  Require(event.at("dp_incompatible_candidates") == 3,
          "DP incompatible missing");
  Require(event.at("dp_final_anchor_candidate_slots") == 4,
          "DP final-anchor slots missing");
  RequireNear(event.at("dp_fit_wall_ms").get<double>(), 5.5,
              "DP fit timing missing");
  RequireNear(event.at("dp_reduction_wall_ms").get<double>(), 0.25,
              "DP reduction timing missing");
  RequireNear(event.at("dp_final_anchor_fit_wall_ms").get<double>(), 1.25,
              "DP final-anchor timing missing");
  RequireNear(event.at("dp_final_anchor_reduction_wall_ms").get<double>(),
              0.125,
              "DP final-anchor reduction timing missing");

  Require(event.at("fit_segment_hold_attempts") == 17,
          "hold attempts missing");
  Require(event.at("fit_segment_linear_attempts") == 19,
          "linear attempts missing");
  Require(event.at("fit_segment_hold_units_evaluated") == 23,
          "hold units missing");
  Require(event.at("fit_segment_linear_fail_fast_exits") == 37,
          "linear fail-fast exits missing");
  Require(event.at("fit_shape_temporal_attempts") == 41,
          "shape-temporal attempts missing");
  Require(event.at("fit_shape_temporal_gate_rejections") == 43,
          "shape-temporal gate rejections missing");
  Require(event.at("fit_shape_temporal_outline_evaluations") == 47,
          "shape-temporal outline evaluations missing");
  RequireNear(event.at("fit_segment_hold_wall_ms").get<double>(), 1.5,
              "hold fit timing missing");
  RequireNear(event.at("fit_segment_linear_wall_ms").get<double>(), 2.5,
              "linear fit timing missing");
  RequireNear(event.at("fit_segment_hold_shape_outline_wall_ms").get<double>(),
              3.5,
              "hold outline timing missing");
  RequireNear(event.at("fit_segment_linear_shape_outline_wall_ms").get<double>(),
              4.5,
              "linear outline timing missing");
  RequireNear(event.at("fit_shape_temporal_ceres_wall_ms").get<double>(), 5.5,
              "shape-temporal Ceres timing missing");
  RequireNear(event.at("fit_shape_temporal_outline_wall_ms").get<double>(), 6.5,
              "shape-temporal outline timing missing");
  RequireNear(event.at("fit_shape_temporal_total_wall_ms").get<double>(), 7.5,
              "shape-temporal total timing missing");

  Require(event.at("fit_replacement_oracle_calls") == 53,
          "replacement oracle calls missing");
  Require(event.at("fit_replacement_oracle_evaluations") == 59,
          "replacement oracle evaluations missing");
  Require(event.at("fit_replacement_relaxed_attempts") == 61,
          "replacement relaxed attempts missing");
  Require(event.at("fit_replacement_relaxed_validations") == 67,
          "replacement relaxed validations missing");
  RequireNear(event.at("fit_replacement_oracle_wall_ms").get<double>(), 8.5,
              "replacement oracle timing missing");
  RequireNear(event.at("fit_replacement_outline_wall_ms").get<double>(), 9.5,
              "replacement outline timing missing");
  RequireNear(event.at("fit_replacement_relaxed_wall_ms").get<double>(), 10.5,
              "replacement relaxed timing missing");
}

void TestPlacementProgressEventOmitsZeroAttributionFields() {
  const bbsolver::PropertySamples ps = MakePropertySamples();
  bbsolver::PlacementProgress placement;
  placement.stage = "dp_anchor";
  placement.step_index = 0;
  placement.step_total = 1;

  const nlohmann::json event = bbsolver::PlacementProgressEvent(
      "temporal_solve_progress", "Solving temporal placement", 0, 1,
      0.0, 1.0, ps, placement);

  Require(event.contains("placement_stage"),
          "base placement fields must still be emitted");
  Require(!event.contains("dp_candidate_slots"),
          "zero DP attribution fields should be omitted");
  Require(!event.contains("dp_fit_wall_ms"),
          "zero DP timing fields should be omitted");
  Require(!event.contains("fit_segment_hold_attempts"),
          "zero ordinary fit attribution fields should be omitted");
  Require(!event.contains("fit_replacement_oracle_calls"),
          "zero replacement attribution fields should be omitted");
}

void TestProgressWriterNoOpAndJsonLine() {
  bbsolver::ProgressWriter disabled(-1);
  disabled.Emit({{"event", "noop"}, {"phase", "noop"}, {"progress", 0.0}});

#ifndef _WIN32
  int fds[2] = {-1, -1};
  Require(::pipe(fds) == 0, "pipe creation failed");
  bbsolver::ProgressWriter writer(fds[1]);
  writer.Emit({{"event", "unit"}, {"phase", "phase"}, {"progress", 0.5}});

  char buffer[256] = {};
  const std::ptrdiff_t read_count = ::read(fds[0], buffer, sizeof(buffer) - 1);
  ::close(fds[0]);
  ::close(fds[1]);
  Require(read_count > 0, "progress writer did not emit bytes");
  const std::string line(buffer, static_cast<std::size_t>(read_count));
  Require(!line.empty() && line.back() == '\n',
          "progress writer must newline-delimit JSON");
  const nlohmann::json parsed = nlohmann::json::parse(line);
  Require(parsed.at("event") == "unit", "progress writer JSON event mismatch");
  RequireNear(parsed.at("progress").get<double>(), 0.5,
              "progress writer JSON progress mismatch");
#endif
}

}  // namespace

int main() {
  TestPropertyStageProgressMath();
  TestPropertyLabelFallback();
  TestPropertyProgressEventFields();
  TestPlacementProgressEventAttributionFields();
  TestPlacementProgressEventOmitsZeroAttributionFields();
  TestProgressWriterNoOpAndJsonLine();
  std::cout << "progress event tests passed\n";
  return 0;
}
