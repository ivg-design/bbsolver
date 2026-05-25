#include "bbsolver/path/multimode/path_multimode_landmark_diagnostics.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/path/multimode/path_multimode_notes.hpp"
#include "bbsolver/dp/dp_placer.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
  if (condition) {
    return;
  }
  std::cerr << "test_path_multimode_landmark_diagnostics: " << message << "\n";
  std::exit(1);
}

std::vector<double> ShapeFlatTwoVertex(double x0, double y0,
                                       double x1, double y1) {
  return {
      0.0, 2.0,
      x0, y0, 0.0, 0.0, 0.0, 0.0,
      x1, y1, 0.0, 0.0, 0.0, 0.0,
  };
}

bbsolver::PropertySamples MakeSamples(bool outlier_second_vertex) {
  bbsolver::PropertySamples samples;
  samples.property.id = "unit/path_multimode/diagnostics";
  samples.property.kind = bbsolver::ValueKind::Custom;
  samples.property.units_label = "shape_flat";
  samples.property.dimensions = 14;
  samples.t_start_sec = 0.0;
  samples.t_end_sec = 3.0 / 24.0;
  samples.samples_per_frame = 1;
  for (int idx = 0; idx <= 3; ++idx) {
    const double x = static_cast<double>(idx);
    double y1 = 2.0 * x;
    if (outlier_second_vertex && (idx == 1 || idx == 2)) {
      y1 += 5.0;
    }
    samples.samples.push_back(
        {x / 24.0, ShapeFlatTwoVertex(x, 0.0, 10.0 + x, y1)});
  }
  return samples;
}

void TestWindowAndDiagnosticCandidate() {
  const bbsolver::PropertySamples samples = MakeSamples(false);
  const bbsolver::PropertySamples window =
      bbsolver::path_multimode::WindowSamples(samples, 1, 2);
  Require(window.samples.size() == 2, "window sample count preserved");
  Require(window.t_start_sec == samples.samples[1].t_sec,
          "window start time preserved");
  Require(window.t_end_sec == samples.samples[2].t_sec,
          "window end time preserved");

  bbsolver::SegmentFitResult fit;
  fit.interp = bbsolver::InterpType::Linear;
  fit.reason = "unit_fit";
  fit.max_err = 0.25;
  fit.max_err_screen_px = 0.25;
  fit.rms_err = 0.1;
  fit.key_value_at_i = samples.samples[0].v;
  fit.key_value_at_j = samples.samples[2].v;
  const bbsolver::PropertyKeys candidate =
      bbsolver::path_multimode::BuildSegmentDiagnosticCandidate(
          samples, 0, 2, fit);
  Require(candidate.converged, "diagnostic candidate is converged");
  Require(candidate.keys.size() == 2, "diagnostic candidate has endpoint keys");
  Require(candidate.segments.size() == 1, "diagnostic candidate has one segment");
  Require(candidate.segments.front().reason == "unit_fit",
          "diagnostic candidate preserves fit reason");
  Require(candidate.segments.front().max_err == 0.25,
          "diagnostic candidate preserves fit error");
}

void TestMergeClassificationAndGapDiagnostics() {
  const bbsolver::PropertySamples samples = MakeSamples(false);
  const bbsolver::path_multimode::SegmentMergeAttemptDiagnostic invalid =
      bbsolver::path_multimode::ClassifySegmentMergeAttempt(
          samples, 2, 2, 0.1, 2);
  Require(invalid.reason == "invalid_window", "invalid window classified");

  const bbsolver::path_multimode::SegmentMergeAttemptDiagnostic gap =
      bbsolver::path_multimode::ClassifySegmentMergeAttempt(
          samples, 0, 3, 0.1, 2);
  Require(gap.reason == "gap_cap_exceeded", "gap cap classified");

  const bbsolver::path_multimode::SegmentGapDiagnostic diagnostic =
      bbsolver::path_multimode::DiagnoseAcceptedSegmentGaps(
          samples, {0, 1, 2, 3}, 0.1, 2);
  Require(diagnostic.gap_histogram == "1:3", "gap histogram preserved");
  Require(diagnostic.max_gap == 1, "max gap preserved");
  Require(diagnostic.rejection_checks == 2,
          "merge rejection checks counted");
}

void TestDenseRunAndOutlierSlots() {
  const bbsolver::PropertySamples samples = MakeSamples(true);
  int checks = 0;
  const std::string note =
      bbsolver::path_multimode::DiagnoseDenseSubpathRuns(
          samples, {0, 1, 2, 3}, 0.1, 2, &checks);
  Require(checks == 2, "dense-run checks counted");
  Require(note.find("0-3:checks=2") != std::string::npos,
          "dense-run note includes run bounds and checks");

  const double deviation =
      bbsolver::path_multimode::SlotChordDeviation(samples, 0, 2, 1, 1);
  Require(deviation > 0.0, "slot chord deviation detects nonlinear slot");

  const bbsolver::path_multimode::OutlierSlotAnalysis outliers =
      bbsolver::path_multimode::AnalyzeDenseRunOutlierSlots(
          samples, {0, 1, 2, 3});
  Require(outliers.checks == 2, "outlier checks counted");
  Require(!outliers.slots.empty(), "outlier slots reported");
  Require(outliers.slots.front().vertex == 1,
          "dominant outlier slot sorted first");
}

}  // namespace

int main() {
  TestWindowAndDiagnosticCandidate();
  TestMergeClassificationAndGapDiagnostics();
  TestDenseRunAndOutlierSlots();
  return 0;
}
