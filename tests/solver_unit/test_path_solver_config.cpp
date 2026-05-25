#include "bbsolver/path/config/path_solver_config.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

#include <cassert>
#include <iostream>
#include <limits>

namespace {

void TestPathChildConfigTightensTolerances() {
  bbsolver::SolverConfig config;
  config.tolerance = 8.0;
  config.tolerance_screen_px = 4.0;

  const bbsolver::SolverConfig child = bbsolver::PathChildConfig(config);
  assert(std::abs(child.tolerance - 0.8) < 1e-12);
  assert(std::abs(child.tolerance_screen_px - 0.4) < 1e-12);
}

void TestPathChildConfigKeepsMinimumPositiveTolerance() {
  bbsolver::SolverConfig config;
  config.tolerance = 1e-9;
  config.tolerance_screen_px = 0.0;

  const bbsolver::SolverConfig child = bbsolver::PathChildConfig(config);
  assert(child.tolerance == 1e-6);
  assert(child.tolerance_screen_px == 0.0);
}

void TestEffectivePathToleranceUsesLargestFiniteTolerance() {
  bbsolver::SolverConfig config;
  config.tolerance = 1.25;
  config.tolerance_screen_px = 3.5;
  assert(bbsolver::EffectivePathTolerance(config) == 3.5);

  config.tolerance = std::numeric_limits<double>::quiet_NaN();
  config.tolerance_screen_px = 2.0;
  assert(bbsolver::EffectivePathTolerance(config) == 2.0);
}

void TestFrameFitOptionsReserveTemporalBudget() {
  bbsolver::SolverConfig config;
  config.tolerance = 4.0;
  config.tolerance_screen_px = 10.0;

  const bbsolver::PathFrameFitOptions replacement =
      bbsolver::ReplacementFrameFitOptions(config);
  assert(replacement.outline_tolerance == 5.0);
  assert(replacement.source_vertices_are_semantic_anchors);

  const bbsolver::PathFrameFitOptions visible =
      bbsolver::VisibleOutlineFrameFitOptions(config);
  assert(visible.outline_tolerance == 5.0);
  assert(!visible.source_vertices_are_semantic_anchors);
}

void TestReplacementPathTemporalValidationOptionsUseFullTolerance() {
  bbsolver::SolverConfig config;
  config.tolerance = 4.0;
  config.tolerance_screen_px = 10.0;

  const bbsolver::PathTemporalValidationOptions authored =
      bbsolver::ReplacementPathTemporalValidationOptions(
          config, false);
  assert(authored.frame_fit_options.outline_tolerance == 10.0);
  assert(authored.frame_fit_options.source_vertices_are_semantic_anchors);

  const bbsolver::PathTemporalValidationOptions visible =
      bbsolver::ReplacementPathTemporalValidationOptions(
          config, true);
  assert(visible.frame_fit_options.outline_tolerance == 10.0);
  assert(!visible.frame_fit_options.source_vertices_are_semantic_anchors);
}

void TestReplacementTemporalConfigPreservesBudgetAndEnablesBezier() {
  bbsolver::SolverConfig config;
  config.allow_shape_temporal_bezier = false;
  config.tolerance = 6.0;

  const bbsolver::SolverConfig replacement =
      bbsolver::ReplacementTemporalConfig(config, 99.0);
  assert(replacement.allow_shape_temporal_bezier);
  assert(replacement.tolerance == 6.0);
}

void TestPathChildMaxGapDelegatesInteractivePolicy() {
  bbsolver::CompInfo comp;
  comp.fps = 120.0;
  assert(bbsolver::PathChildMaxGap(comp) == 40);
}

}  // namespace

int main() {
  TestPathChildConfigTightensTolerances();
  TestPathChildConfigKeepsMinimumPositiveTolerance();
  TestEffectivePathToleranceUsesLargestFiniteTolerance();
  TestFrameFitOptionsReserveTemporalBudget();
  TestReplacementPathTemporalValidationOptionsUseFullTolerance();
  TestReplacementTemporalConfigPreservesBudgetAndEnablesBezier();
  TestPathChildMaxGapDelegatesInteractivePolicy();
  std::cout << "[PASS] test_path_solver_config\n";
  return 0;
}
