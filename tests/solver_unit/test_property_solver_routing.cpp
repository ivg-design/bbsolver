#include "bbsolver/routing/property_solver_routing.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void RequireRoute(bbsolver::PropertySolveRoute actual,
                  bbsolver::PropertySolveRoute expected,
                  const std::string& message) {
  if (actual != expected) {
    std::cerr << message << "\n";
    std::abort();
  }
}

void TestDefaultChoosesPlainTemporal() {
  bbsolver::PropertySolveRouteInput input;
  RequireRoute(
      bbsolver::ChoosePropertySolveRoute(input),
      bbsolver::PropertySolveRoute::PlainTemporal,
      "default route should use plain temporal solve");
}

void TestPreserveSourceKeysHasHighestPrecedence() {
  bbsolver::PropertySolveRouteInput input;
  input.preserve_source_keys = true;
  input.motion_smooth_enabled = true;
  input.temporal_optimization_enabled = false;
  input.path_temporal_reduced_by_fit = true;
  input.decompose_paths = true;
  input.decompose_candidate_is_shape_flat = true;

  RequireRoute(
      bbsolver::ChoosePropertySolveRoute(input),
      bbsolver::PropertySolveRoute::PreserveSourceKeys,
      "source-key preservation must win over every later solve route");
}

void TestMotionSmoothPrecedesFrameFallback() {
  bbsolver::PropertySolveRouteInput input;
  input.motion_smooth_enabled = true;
  input.temporal_optimization_enabled = false;
  input.path_temporal_reduced_by_fit = true;

  RequireRoute(
      bbsolver::ChoosePropertySolveRoute(input),
      bbsolver::PropertySolveRoute::MotionSmooth,
      "motion-smooth mode must route before frame-key fallback");
}

void TestFrameFallbackPrecedesPathSpecificRoutes() {
  bbsolver::PropertySolveRouteInput input;
  input.temporal_optimization_enabled = false;
  input.path_temporal_reduced_by_fit = true;
  input.decompose_paths = true;
  input.decompose_candidate_is_shape_flat = true;

  RequireRoute(
      bbsolver::ChoosePropertySolveRoute(input),
      bbsolver::PropertySolveRoute::FrameKeyFallback,
      "disabled temporal optimization must route to frame-key fallback");
}

void TestReplacementTemporalPrecedesDecompose() {
  bbsolver::PropertySolveRouteInput input;
  input.path_temporal_reduced_by_fit = true;
  input.decompose_paths = true;
  input.decompose_candidate_is_shape_flat = true;

  RequireRoute(
      bbsolver::ChoosePropertySolveRoute(input),
      bbsolver::PropertySolveRoute::ReplacementShapeFlatTemporal,
      "fit-reduced temporal route must precede path decomposition");
}

void TestPathDecomposeRequiresShapeFlatCandidate() {
  bbsolver::PropertySolveRouteInput input;
  input.decompose_paths = true;

  RequireRoute(
      bbsolver::ChoosePropertySolveRoute(input),
      bbsolver::PropertySolveRoute::PlainTemporal,
      "decompose flag alone should not route non-shape properties");

  input.decompose_candidate_is_shape_flat = true;
  RequireRoute(
      bbsolver::ChoosePropertySolveRoute(input),
      bbsolver::PropertySolveRoute::PathDecomposed,
      "shape-flat decompose candidate should use path decomposition");
}

}  // namespace

int main() {
  TestDefaultChoosesPlainTemporal();
  TestPreserveSourceKeysHasHighestPrecedence();
  TestMotionSmoothPrecedesFrameFallback();
  TestFrameFallbackPrecedesPathSpecificRoutes();
  TestReplacementTemporalPrecedesDecompose();
  TestPathDecomposeRequiresShapeFlatCandidate();
  std::cout << "property solver routing tests passed\n";
  return 0;
}
