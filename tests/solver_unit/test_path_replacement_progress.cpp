#include "bbsolver/path/replacement/path_replacement_progress.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/progress/progress.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include "bbsolver/dp/dp_placer.hpp"
#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>

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

bbsolver::PropertySamples MakeShapeProperty() {
  bbsolver::PropertySamples ps;
  ps.property.id = "shape/path";
  ps.property.display_name = "Mask Path";
  ps.samples.push_back({0.0, {}});
  ps.samples.push_back({1.0, {}});
  return ps;
}

void TestReplacementRetryStartProgressEventFields() {
  const bbsolver::PropertySamples ps = MakeShapeProperty();
  const nlohmann::json event =
      bbsolver::ReplacementRetryStartProgressEvent(ps, 1, 4, 2, 18);

  Require(event.at("event") == "replacement_retry_start",
          "retry start event name changed");
  Require(event.at("phase") ==
              "Retrying replacement topology 18 vertices for Mask Path",
          "retry start phase changed");
  RequireNear(event.at("progress").get<double>(),
              bbsolver::SolveProgressForPropertyStage(1, 4, 0.82),
              "retry start progress changed");
  Require(event.at("id") == "shape/path", "retry start id changed");
  Require(event.at("display_name") == "Mask Path",
          "retry start display name changed");
  Require(event.at("i") == 1, "retry start property index changed");
  Require(event.at("n") == 4, "retry start property count changed");
  Require(event.at("retry") == 2, "retry start retry number changed");
  Require(event.at("target_vertices") == 18,
          "retry start target vertices changed");
}

void TestReplacementBaselineStartProgressEventFields() {
  const bbsolver::PropertySamples ps = MakeShapeProperty();
  const nlohmann::json event =
      bbsolver::ReplacementBaselineStartProgressEvent(ps, 1, 4);

  Require(event.at("event") == "path_replacement_baseline_start",
          "baseline start event name changed");
  Require(event.at("phase") ==
              "Solving original temporal fallback for Mask Path",
          "baseline start phase changed");
  RequireNear(event.at("progress").get<double>(),
              bbsolver::SolveProgressForPropertyStage(1, 4, 0.71),
              "baseline start progress changed");
  Require(event.at("id") == "shape/path", "baseline start id changed");
  Require(event.at("display_name") == "Mask Path",
          "baseline start display name changed");
  Require(event.at("i") == 1, "baseline start property index changed");
  Require(event.at("n") == 4, "baseline start property count changed");
  Require(event.at("samples") == 2, "baseline start samples changed");
}

void TestReplacementBaselinePlacementProgressEventFields() {
  const bbsolver::PropertySamples ps = MakeShapeProperty();
  bbsolver::PlacementProgress placement;
  placement.stage = "dp_anchor";
  placement.step_index = 1;
  placement.step_total = 2;
  placement.sample_index = 3;
  placement.samples = 5;

  const nlohmann::json event =
      bbsolver::ReplacementBaselinePlacementProgressEvent(ps, 0, 2, placement);

  Require(event.at("event") == "path_replacement_baseline_progress",
          "baseline placement event name changed");
  Require(event.at("phase") ==
              "Solving original fallback placement dp_anchor 1/2 for Mask Path",
          "baseline placement phase changed");
  RequireNear(event.at("progress").get<double>(),
              bbsolver::SolveProgressForPropertyStage(0, 2, 0.72),
              "baseline placement progress changed");
  Require(event.at("placement_stage") == "dp_anchor",
          "baseline placement stage changed");
  Require(event.at("sample_index") == 3,
          "baseline placement sample index changed");
  Require(event.at("samples") == 5, "baseline placement samples changed");
}

void TestReplacementBaselineDoneProgressEventFields() {
  const bbsolver::PropertySamples ps = MakeShapeProperty();
  const nlohmann::json event =
      bbsolver::ReplacementBaselineDoneProgressEvent(
          ps, 2, 5, 17, 0.625, true);

  Require(event.at("event") == "path_replacement_baseline_done",
          "baseline done event name changed");
  Require(event.at("phase") ==
              "Original temporal fallback solved for Mask Path",
          "baseline done phase changed");
  RequireNear(event.at("progress").get<double>(),
              bbsolver::SolveProgressForPropertyStage(2, 5, 0.73),
              "baseline done progress changed");
  Require(event.at("id") == "shape/path", "baseline done id changed");
  Require(event.at("display_name") == "Mask Path",
          "baseline done display name changed");
  Require(event.at("i") == 2, "baseline done property index changed");
  Require(event.at("n") == 5, "baseline done property count changed");
  Require(event.at("K") == 17, "baseline done key count changed");
  RequireNear(event.at("max_err").get<double>(),
              0.625,
              "baseline done max error changed");
  Require(event.at("converged") == true,
          "baseline done converged flag changed");
}

void TestReplacementValidationStartProgressEventFields() {
  const bbsolver::PropertySamples ps = MakeShapeProperty();
  const nlohmann::json event =
      bbsolver::ReplacementValidationStartProgressEvent(ps, 3, 7);

  Require(event.at("event") == "path_validation_start",
          "validation start event name changed");
  Require(event.at("phase") == "Validating replacement outline for Mask Path",
          "validation start phase changed");
  RequireNear(event.at("progress").get<double>(),
              bbsolver::SolveProgressForPropertyStage(3, 7, 0.74),
              "validation start progress changed");
  Require(event.at("id") == "shape/path", "validation start id changed");
  Require(event.at("display_name") == "Mask Path",
          "validation start display name changed");
  Require(event.at("i") == 3, "validation start property index changed");
  Require(event.at("n") == 7, "validation start property count changed");
  Require(event.at("samples") == 2, "validation start samples changed");
}

void TestReplacementRetryDoneProgressEventFields() {
  const bbsolver::PropertySamples ps = MakeShapeProperty();
  const nlohmann::json event =
      bbsolver::ReplacementRetryDoneProgressEvent(
          ps, 2, 5, 3, 20, 19, 0.375, true, false);

  Require(event.at("event") == "replacement_retry_done",
          "retry done event name changed");
  Require(event.at("phase") == "Replacement retry accepted for Mask Path",
          "retry done phase changed");
  RequireNear(event.at("progress").get<double>(),
              bbsolver::SolveProgressForPropertyStage(2, 5, 0.86),
              "retry done progress changed");
  Require(event.at("id") == "shape/path", "retry done id changed");
  Require(event.at("display_name") == "Mask Path",
          "retry done display name changed");
  Require(event.at("i") == 2, "retry done property index changed");
  Require(event.at("n") == 5, "retry done property count changed");
  Require(event.at("retry") == 3, "retry done retry number changed");
  Require(event.at("target_vertices") == 20,
          "retry done target vertices changed");
  Require(event.at("fitted_vertices") == 19,
          "retry done fitted vertices changed");
  RequireNear(event.at("max_outline_error").get<double>(),
              0.375,
              "retry done max outline error changed");
  Require(event.at("accepted") == true, "retry done accepted flag changed");
  Require(event.at("sharp_corners_ok") == false,
          "retry done sharp-corner flag changed");
}

void TestReplacementFastVertexValidationDoneProgressEventFields() {
  const bbsolver::PropertySamples ps = MakeShapeProperty();
  const nlohmann::json event =
      bbsolver::ReplacementFastVertexValidationDoneProgressEvent(
          ps, 0, 3, 0.125, 11, 7, true);

  Require(event.at("event") == "path_validation_done",
          "fast validation event name changed");
  Require(event.at("phase") == "Replacement outline validated for Mask Path",
          "fast validation phase changed");
  RequireNear(event.at("progress").get<double>(),
              bbsolver::SolveProgressForPropertyStage(0, 3, 0.80),
              "fast validation progress changed");
  Require(event.at("id") == "shape/path", "fast validation id changed");
  Require(event.at("display_name") == "Mask Path",
          "fast validation display name changed");
  Require(event.at("i") == 0, "fast validation property index changed");
  Require(event.at("n") == 3, "fast validation property count changed");
  Require(event.at("ok") == true, "fast validation ok flag changed");
  RequireNear(event.at("max_outline_error").get<double>(),
              0.125,
              "fast validation outline error changed");
  Require(event.at("samples_checked") == 11,
          "fast validation samples checked changed");
  Require(event.at("candidate_keys") == 7,
          "fast validation candidate key count changed");
  Require(event.at("source_keys").is_null(),
          "fast validation source keys should stay null");
  Require(event.at("baseline_temporal_skipped") == true,
          "fast validation baseline skip flag changed");
  Require(event.at("sharp_corners_ok") == true,
          "fast validation sharp-corner flag changed");
}

void TestReplacementValidationDoneProgressEventFields() {
  const bbsolver::PropertySamples ps = MakeShapeProperty();
  const nlohmann::json event =
      bbsolver::ReplacementValidationDoneProgressEvent(
          ps, 2, 6, false, 0.875, 13, 8, 21, false);

  Require(event.at("event") == "path_validation_done",
          "validation event name changed");
  Require(event.at("phase") == "Replacement outline rejected for Mask Path",
          "validation rejected phase changed");
  RequireNear(event.at("progress").get<double>(),
              bbsolver::SolveProgressForPropertyStage(2, 6, 0.80),
              "validation progress changed");
  Require(event.at("id") == "shape/path", "validation id changed");
  Require(event.at("display_name") == "Mask Path",
          "validation display name changed");
  Require(event.at("i") == 2, "validation property index changed");
  Require(event.at("n") == 6, "validation property count changed");
  Require(event.at("ok") == false, "validation ok flag changed");
  RequireNear(event.at("max_outline_error").get<double>(),
              0.875,
              "validation outline error changed");
  Require(event.at("samples_checked") == 13,
          "validation samples checked changed");
  Require(event.at("candidate_keys") == 8,
          "validation candidate key count changed");
  Require(event.at("source_keys") == 21,
          "validation source key count changed");
  Require(event.at("sharp_corners_ok") == false,
          "validation sharp-corner flag changed");
}

}  // namespace

int main() {
  TestReplacementBaselineStartProgressEventFields();
  TestReplacementBaselinePlacementProgressEventFields();
  TestReplacementBaselineDoneProgressEventFields();
  TestReplacementValidationStartProgressEventFields();
  TestReplacementRetryStartProgressEventFields();
  TestReplacementRetryDoneProgressEventFields();
  TestReplacementFastVertexValidationDoneProgressEventFields();
  TestReplacementValidationDoneProgressEventFields();
  std::cout << "[PASS] test_path_replacement_progress\n";
  return 0;
}
