#include "bbsolver/path/temporal/path_temporal_validation.hpp"
#include "bbsolver/domain.hpp"

#include <cassert>
#include <string>
#include <utility>
#include <vector>
#include <cstddef>

namespace {

std::vector<double> ShapeFlatRect(double x, double y, double w, double h) {
  const std::vector<std::pair<double, double>> vertices = {
      {x, y},
      {x + w, y},
      {x + w, y + h},
      {x, y + h},
  };
  std::vector<double> out;
  out.push_back(1.0);
  out.push_back(static_cast<double>(vertices.size()));
  for (const auto& vertex: vertices) {
    out.push_back(vertex.first);
    out.push_back(vertex.second);
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
  }
  return out;
}

bbsolver::PropertySamples MakeMorphChordFixture() {
  bbsolver::PropertySamples ps;
  ps.property.id = "unit/path/progress_band";
  ps.property.kind = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions = 2 + 6 * 4;
  ps.t_start_sec = 0.0;
  ps.t_end_sec = 8.0 / 24.0;
  ps.samples_per_frame = 1;

  for (int idx = 0; idx <= 8; ++idx) {
    const double y = (idx == 0 || idx == 8) ? 0.0: 5.0;
    ps.samples.push_back({static_cast<double>(idx) / 24.0,
                          ShapeFlatRect(static_cast<double>(idx), y, 10.0, 10.0)});
  }
  return ps;
}

bbsolver::PropertySamples MakeMonotoneButNonBezierProgressFixture() {
  bbsolver::PropertySamples ps;
  ps.property.id = "unit/path/progress_band/monotone_non_bezier";
  ps.property.kind = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions = 2 + 6 * 4;
  ps.t_start_sec = 0.0;
  ps.t_end_sec = 4.0 / 24.0;
  ps.samples_per_frame = 1;

  const std::vector<double> progress = {0.0, 0.45, 0.46, 0.47, 1.0};
  for (int idx = 0; idx < static_cast<int>(progress.size()); ++idx) {
    ps.samples.push_back({static_cast<double>(idx) / 24.0,
                          ShapeFlatRect(100.0 * progress[idx],
                                        0.0,
                                        10.0,
                                        10.0)});
  }
  return ps;
}

bbsolver::ShapeMorphProgressBandOptions Options() {
  bbsolver::ShapeMorphProgressBandOptions options;
  options.frame_fit_options.outline_tolerance = 0.25;
  options.frame_fit_options.max_subdivisions_per_segment = 2;
  options.progress_steps = 40;
  options.max_window_samples = 16;
  options.max_evaluations = 1024;
  options.compute_progress_bands = true;
  return options;
}

void TestLongChordReportsInfeasible() {
  const bbsolver::PropertySamples source = MakeMorphChordFixture();
  const bbsolver::ShapeMorphProgressBandOptions options = Options();

  const bbsolver::ShapeMorphProgressBandResult result =
      bbsolver::EvaluateShapeFlatMorphProgressBands(
          source,
          0,
          8,
          source.samples[0].v,
          source.samples[8].v,
          options);

  assert(result.ok);
  assert(result.reason == "infeasible_shape_morph_chord");
  assert(result.progress_bands_computed);
  assert(!result.progress_band_possible);
  assert(!result.monotone_band_progress_possible);
  assert(!result.monotone_progress_possible);
  assert(!result.ae_ease_progress_possible);
  assert(!result.linear_progress_possible);
  assert(!result.default_bezier_progress_possible);
  assert(bbsolver::ShapeMorphProgressFeasibilityClass(
             result, options.frame_fit_options.outline_tolerance) ==
         "chord_infeasible");
  assert(result.samples_checked == 9);
  assert(result.evaluations == 9 * (options.progress_steps + 3));
  assert(result.max_best_error > options.frame_fit_options.outline_tolerance);
  assert(result.samples.size() == 9);

  bool found_empty_inner_band = false;
  for (std::size_t idx = 1; idx + 1 < result.samples.size(); ++idx) {
    found_empty_inner_band =
        found_empty_inner_band || result.samples[idx].intervals.empty();
  }
  assert(found_empty_inner_band);
}

void TestShortChordReportsFeasible() {
  const bbsolver::PropertySamples source = MakeMorphChordFixture();
  const bbsolver::ShapeMorphProgressBandOptions options = Options();

  const bbsolver::ShapeMorphProgressBandResult result =
      bbsolver::EvaluateShapeFlatMorphProgressBands(
          source,
          4,
          6,
          source.samples[4].v,
          source.samples[6].v,
          options);

  assert(result.ok);
  assert(result.reason == "shape_morph_chord_ok");
  assert(result.progress_bands_computed);
  assert(result.progress_band_possible);
  assert(result.monotone_band_progress_possible);
  assert(result.monotone_progress_possible);
  assert(result.ae_ease_progress_possible);
  assert(result.linear_progress_possible);
  assert(result.default_bezier_progress_possible);
  assert(bbsolver::ShapeMorphProgressFeasibilityClass(
             result, options.frame_fit_options.outline_tolerance) ==
         "ae_ease_feasible");
  assert(result.samples_checked == 3);
  assert(result.evaluations == 3 * (options.progress_steps + 3));
  assert(result.max_best_error <= 1e-9);
  assert(result.max_linear_error <= options.frame_fit_options.outline_tolerance);
  assert(result.samples.size() == 3);
  assert(!result.samples[1].intervals.empty());
  assert(result.samples[1].best_u > 0.45);
  assert(result.samples[1].best_u < 0.55);
}

void TestMonotoneChordCanBeAeEaseInfeasible() {
  const bbsolver::PropertySamples source =
      MakeMonotoneButNonBezierProgressFixture();
  bbsolver::ShapeMorphProgressBandOptions options = Options();
  options.frame_fit_options.outline_tolerance = 1.0;
  options.progress_steps = 100;
  options.max_evaluations = 20000;
  options.fit_bezier_influence_pairs = true;
  options.bezier_influence_grid_steps = 7;
  options.bezier_influence_refinement_steps = 2;
  options.max_bezier_influence_pairs = 64;

  const bbsolver::ShapeMorphProgressBandResult result =
      bbsolver::EvaluateShapeFlatMorphProgressBands(
          source,
          0,
          4,
          source.samples[0].v,
          source.samples[4].v,
          options);

  assert(result.ok);
  assert(result.reason == "shape_morph_chord_ok");
  assert(result.progress_band_possible);
  assert(result.monotone_band_progress_possible);
  assert(result.monotone_progress_possible);
  assert(!result.linear_progress_possible);
  assert(!result.default_bezier_progress_possible);
  assert(!result.fitted_bezier_progress_possible);
  assert(!result.ae_ease_progress_possible);
  assert(bbsolver::ShapeMorphProgressFeasibilityClass(
             result, options.frame_fit_options.outline_tolerance) ==
         "ae_ease_infeasible");
  const int sample_count = 5;
  const int full_scan_evaluations =
      sample_count * (options.progress_steps + 3 +
                      options.max_bezier_influence_pairs);
  const int pre_bezier_evaluations =
      sample_count * (options.progress_steps + 3);
  assert(result.fitted_bezier_pairs_tried > 0);
  assert(result.evaluations < full_scan_evaluations);
  assert(result.evaluations >=
         pre_bezier_evaluations + result.fitted_bezier_pairs_tried);
}

void TestFittedBezierBudgetUsesWorstCaseChecks() {
  const bbsolver::PropertySamples source =
      MakeMonotoneButNonBezierProgressFixture();
  bbsolver::ShapeMorphProgressBandOptions options = Options();
  options.compute_progress_bands = false;
  options.fit_bezier_influence_pairs = true;
  options.max_bezier_influence_pairs = 8;
  const int sample_count = 5;
  options.max_evaluations =
      sample_count * (2 + options.max_bezier_influence_pairs) - 1;

  const bbsolver::ShapeMorphProgressBandResult result =
      bbsolver::EvaluateShapeFlatMorphProgressBands(
          source,
          0,
          4,
          source.samples[0].v,
          source.samples[4].v,
          options);

  assert(!result.ok);
  assert(result.reason == "shape_morph_chord_evaluation_budget_exceeded");
  assert(result.fitted_bezier_pairs_tried == 0);
  assert(result.evaluations == 0);
}

void TestEvaluationBudgetFailsClosed() {
  const bbsolver::PropertySamples source = MakeMorphChordFixture();
  bbsolver::ShapeMorphProgressBandOptions options = Options();
  options.max_evaluations = 4;

  const bbsolver::ShapeMorphProgressBandResult result =
      bbsolver::EvaluateShapeFlatMorphProgressBands(
          source,
          4,
          6,
          source.samples[4].v,
          source.samples[6].v,
          options);

  assert(!result.ok);
  assert(result.reason == "shape_morph_chord_evaluation_budget_exceeded");
}

}  // namespace

int main() {
  TestLongChordReportsInfeasible();
  TestShortChordReportsFeasible();
  TestMonotoneChordCanBeAeEaseInfeasible();
  TestFittedBezierBudgetUsesWorstCaseChecks();
  TestEvaluationBudgetFailsClosed();
  return 0;
}
