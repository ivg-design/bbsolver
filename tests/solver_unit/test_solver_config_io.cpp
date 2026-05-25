#include "bbsolver/io/solver_config_io.hpp"
#include "bbsolver/domain.hpp"

#include <cassert>
#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>

namespace {

using nlohmann::json;

void TestEmptyObjectYieldsAllDefaults() {
  const bbsolver::SolverConfig parsed = bbsolver::ParseSolverConfigJson(json::object());
  const bbsolver::SolverConfig defaults;

  assert(parsed.tolerance == defaults.tolerance);
  assert(parsed.tolerance_screen_px == defaults.tolerance_screen_px);
  assert(parsed.weight_pos == defaults.weight_pos);
  assert(parsed.allow_hold == defaults.allow_hold);
  assert(parsed.allow_linear == defaults.allow_linear);
  assert(parsed.allow_bezier == defaults.allow_bezier);
  assert(parsed.allow_shape_temporal_bezier == defaults.allow_shape_temporal_bezier);
  assert(parsed.allow_path_spatial_fit == defaults.allow_path_spatial_fit);
  assert(parsed.allow_path_replacement_fit == defaults.allow_path_replacement_fit);
  assert(parsed.motion_smooth_tolerance == defaults.motion_smooth_tolerance);
  assert(parsed.motion_smooth_use_ease == defaults.motion_smooth_use_ease);
  assert(parsed.motion_smooth_source_fidelity == defaults.motion_smooth_source_fidelity);
  assert(parsed.motion_smooth_bezier_x1 == defaults.motion_smooth_bezier_x1);
  assert(parsed.motion_smooth_bezier_y1 == defaults.motion_smooth_bezier_y1);
  assert(parsed.motion_smooth_bezier_x2 == defaults.motion_smooth_bezier_x2);
  assert(parsed.motion_smooth_bezier_y2 == defaults.motion_smooth_bezier_y2);
  assert(parsed.motion_path_smoothing_tolerance ==
         defaults.motion_path_smoothing_tolerance);
  assert(parsed.motion_path_accuracy_tolerance ==
         defaults.motion_path_accuracy_tolerance);
  assert(parsed.motion_path_preserve_bounds ==
         defaults.motion_path_preserve_bounds);
  assert(parsed.motion_path_bounds_tolerance ==
         defaults.motion_path_bounds_tolerance);
  assert(parsed.motion_path_preserve_sharp_points ==
         defaults.motion_path_preserve_sharp_points);
  assert(parsed.motion_path_sharp_angle_deg ==
         defaults.motion_path_sharp_angle_deg);
  assert(parsed.motion_path_respect_keyed_frames ==
         defaults.motion_path_respect_keyed_frames);
  assert(parsed.path_preserve_sharp_corners == defaults.path_preserve_sharp_corners);
  assert(parsed.path_sharp_corner_angle_deg == defaults.path_sharp_corner_angle_deg);
  assert(parsed.path_sharp_corner_tolerance == defaults.path_sharp_corner_tolerance);
  assert(parsed.path_specific_max_gap == defaults.path_specific_max_gap);
  assert(parsed.shape_temporal_bezier_attempt_threshold_ratio ==
         defaults.shape_temporal_bezier_attempt_threshold_ratio);
  assert(parsed.min_influence == defaults.min_influence);
  assert(parsed.max_influence == defaults.max_influence);
  assert(parsed.max_iters_per_segment == defaults.max_iters_per_segment);
  assert(parsed.min_segment_frames == defaults.min_segment_frames);
  assert(parsed.max_keys_hint == defaults.max_keys_hint);
  assert(parsed.parallel_jobs == defaults.parallel_jobs);
  assert(parsed.placement_strategy == defaults.placement_strategy);
  assert(parsed.verbose == defaults.verbose);
}

void TestCanonicalSnakeCaseFieldsAreParsed() {
  const json obj = {
      {"tolerance", 0.25},
      {"tolerance_screen_px", 0.75},
      {"weight_pos", 1.5},
      {"weight_vel", 2.5},
      {"weight_acc", 3.5},
      {"weight_curv", 4.5},
      {"weight_screen", 5.5},
      {"allow_hold", false},
      {"allow_linear", false},
      {"allow_bezier", true},
      {"allow_shape_temporal_bezier", true},
      {"allow_path_spatial_fit", true},
      {"allow_path_replacement_fit", true},
      {"path_replacement_prefer_vertices", true},
      {"solve_optimization_mode", "vertex_only"},
      {"motion_smooth_use_ease", true},
      {"motion_smooth_source_fidelity", true},
      {"motion_smooth_tolerance", 7.25},
      {"motion_smooth_bezier_x1", 0.10},
      {"motion_smooth_bezier_y1", 0.20},
      {"motion_smooth_bezier_x2", 0.30},
      {"motion_smooth_bezier_y2", 0.40},
      {"motion_path_smoothing_tolerance", 8.5},
      {"motion_path_accuracy_tolerance", 1.25},
      {"motion_path_preserve_bounds", true},
      {"motion_path_bounds_tolerance", 12.5},
      {"motion_path_preserve_sharp_points", false},
      {"motion_path_sharp_angle_deg", 120.0},
      {"motion_path_respect_keyed_frames", true},
      {"path_preserve_sharp_corners", false},
      {"path_sharp_corner_angle_deg", 42.5},
      {"path_sharp_corner_tolerance", 0.125},
      {"path_replacement_min_vertices", 4},
      {"path_replacement_max_vertices", 32},
      {"path_replacement_max_key_growth_ratio", 1.5},
      {"path_replacement_min_vertex_reduction_ratio", 0.5},
      {"path_specific_max_gap", 6},
      {"shape_temporal_bezier_attempt_threshold_ratio", 0.875},
      {"min_influence", 0.05},
      {"max_influence", 95.0},
      {"max_iters_per_segment", 12},
      {"min_segment_frames", 3},
      {"max_keys_hint", 64},
      {"parallel_jobs", 8},
      {"placement_strategy", "greedy"},
      {"verbose", true},
  };

  const bbsolver::SolverConfig parsed = bbsolver::ParseSolverConfigJson(obj);

  assert(std::abs(parsed.tolerance - 0.25) < 1e-12);
  assert(std::abs(parsed.tolerance_screen_px - 0.75) < 1e-12);
  assert(std::abs(parsed.weight_pos - 1.5) < 1e-12);
  assert(std::abs(parsed.weight_vel - 2.5) < 1e-12);
  assert(std::abs(parsed.weight_acc - 3.5) < 1e-12);
  assert(std::abs(parsed.weight_curv - 4.5) < 1e-12);
  assert(std::abs(parsed.weight_screen - 5.5) < 1e-12);
  assert(parsed.allow_hold == false);
  assert(parsed.allow_linear == false);
  assert(parsed.allow_bezier == true);
  assert(parsed.allow_shape_temporal_bezier == true);
  assert(parsed.allow_path_spatial_fit == true);
  assert(parsed.allow_path_replacement_fit == true);
  assert(parsed.path_replacement_prefer_vertices == true);
  assert(parsed.solve_optimization_mode == "vertex_only");
  assert(parsed.motion_smooth_use_ease == true);
  assert(parsed.motion_smooth_source_fidelity == true);
  assert(std::abs(parsed.motion_smooth_tolerance - 7.25) < 1e-12);
  assert(std::abs(parsed.motion_smooth_bezier_x1 - 0.10) < 1e-12);
  assert(std::abs(parsed.motion_smooth_bezier_y1 - 0.20) < 1e-12);
  assert(std::abs(parsed.motion_smooth_bezier_x2 - 0.30) < 1e-12);
  assert(std::abs(parsed.motion_smooth_bezier_y2 - 0.40) < 1e-12);
  assert(std::abs(parsed.motion_path_smoothing_tolerance - 8.5) < 1e-12);
  assert(std::abs(parsed.motion_path_accuracy_tolerance - 1.25) < 1e-12);
  assert(parsed.motion_path_preserve_bounds == true);
  assert(std::abs(parsed.motion_path_bounds_tolerance - 12.5) < 1e-12);
  assert(parsed.motion_path_preserve_sharp_points == false);
  assert(std::abs(parsed.motion_path_sharp_angle_deg - 120.0) < 1e-12);
  assert(parsed.motion_path_respect_keyed_frames == true);
  assert(parsed.path_preserve_sharp_corners == false);
  assert(std::abs(parsed.path_sharp_corner_angle_deg - 42.5) < 1e-12);
  assert(std::abs(parsed.path_sharp_corner_tolerance - 0.125) < 1e-12);
  assert(parsed.path_replacement_min_vertices == 4);
  assert(parsed.path_replacement_max_vertices == 32);
  assert(std::abs(parsed.path_replacement_max_key_growth_ratio - 1.5) < 1e-12);
  assert(std::abs(parsed.path_replacement_min_vertex_reduction_ratio - 0.5) < 1e-12);
  assert(parsed.path_specific_max_gap == 6);
  assert(std::abs(parsed.shape_temporal_bezier_attempt_threshold_ratio - 0.875) < 1e-12);
  assert(std::abs(parsed.min_influence - 0.05) < 1e-12);
  assert(std::abs(parsed.max_influence - 95.0) < 1e-12);
  assert(parsed.max_iters_per_segment == 12);
  assert(parsed.min_segment_frames == 3);
  assert(parsed.max_keys_hint == 64);
  assert(parsed.parallel_jobs == 8);
  assert(parsed.placement_strategy == "greedy");
  assert(parsed.verbose == true);
}

void TestLegacyAliasesAreHonoredWhenCanonicalAbsent() {
  // The pre-extraction io_json.cpp accepted camelCase aliases from older
  // panel writers. The leaf module must preserve every one of these alias
  // fallbacks; this test pins the alias contract symbol-by-symbol.
  const json obj = {
      {"motionSmoothUseEase", true},
      {"motionSmoothSourceFidelity", true},
      {"motionSmoothTolerance", 11.0},
      {"motionSmoothBezierX1", 0.11},
      {"motionSmoothBezierY1", 0.22},
      {"motionSmoothBezierX2", 0.33},
      {"motionSmoothBezierY2", 0.44},
      {"motionPathSmoothingTolerance", 6.0},
      {"motionPathAccuracyTolerance", 0.75},
      {"motionPathPreserveBounds", true},
      {"motionPathBoundsTolerance", 9.5},
      {"motionPathPreserveSharpPoints", false},
      {"motionPathSharpAngleDeg", 95.0},
      {"motionPathRespectKeyedFrames", true},
      {"solveOptimizationMode", "temporal_only"},
      {"shape_temporal_bezier", true},
      {"path_spatial_fit", true},
      {"path_replacement_fit", true},
      {"shape_temporal_bezier_gate_ratio", 0.625},
  };

  const bbsolver::SolverConfig parsed = bbsolver::ParseSolverConfigJson(obj);

  assert(parsed.motion_smooth_use_ease == true);
  assert(parsed.motion_smooth_source_fidelity == true);
  assert(std::abs(parsed.motion_smooth_tolerance - 11.0) < 1e-12);
  assert(std::abs(parsed.motion_smooth_bezier_x1 - 0.11) < 1e-12);
  assert(std::abs(parsed.motion_smooth_bezier_y1 - 0.22) < 1e-12);
  assert(std::abs(parsed.motion_smooth_bezier_x2 - 0.33) < 1e-12);
  assert(std::abs(parsed.motion_smooth_bezier_y2 - 0.44) < 1e-12);
  assert(std::abs(parsed.motion_path_smoothing_tolerance - 6.0) < 1e-12);
  assert(std::abs(parsed.motion_path_accuracy_tolerance - 0.75) < 1e-12);
  assert(parsed.motion_path_preserve_bounds == true);
  assert(std::abs(parsed.motion_path_bounds_tolerance - 9.5) < 1e-12);
  assert(parsed.motion_path_preserve_sharp_points == false);
  assert(std::abs(parsed.motion_path_sharp_angle_deg - 95.0) < 1e-12);
  assert(parsed.motion_path_respect_keyed_frames == true);
  assert(parsed.solve_optimization_mode == "temporal_only");
  assert(parsed.allow_shape_temporal_bezier == true);
  assert(parsed.allow_path_spatial_fit == true);
  assert(parsed.allow_path_replacement_fit == true);
  assert(std::abs(parsed.shape_temporal_bezier_attempt_threshold_ratio - 0.625) < 1e-12);
}

void TestLegacyAliasMotionSmoothSourceKeyFidelityChain() {
  // motion_smooth_source_fidelity has a 4-key alias chain:
  //   motion_smooth_source_fidelity (canonical)
  //   motionSmoothSourceFidelity
  //   motion_smooth_source_key_fidelity (older snake_case alias)
  //   motionSmoothSourceKeyFidelity (oldest camelCase alias)
  // Verify each tier wins when shallower keys are absent.
  const json older_snake = {{"motion_smooth_source_key_fidelity", true}};
  assert(bbsolver::ParseSolverConfigJson(older_snake).motion_smooth_source_fidelity == true);

  const json oldest_camel = {{"motionSmoothSourceKeyFidelity", true}};
  assert(bbsolver::ParseSolverConfigJson(oldest_camel).motion_smooth_source_fidelity == true);

  // Default is false; verify the alias actually flipped it.
  assert(bbsolver::SolverConfig{}.motion_smooth_source_fidelity == false);
}

void TestCanonicalWinsOverLegacyAlias() {
  // When both the canonical and the legacy alias are present, canonical
  // must win. This is the pre-extraction precedence and must be preserved
  // byte-for-byte through the move.
  const json obj = {
      {"motion_smooth_tolerance", 2.0},
      {"motionSmoothTolerance", 99.0},
      {"allow_shape_temporal_bezier", false},
      {"shape_temporal_bezier", true},
      {"shape_temporal_bezier_attempt_threshold_ratio", 0.5},
      {"shape_temporal_bezier_gate_ratio", 0.9},
      {"solve_optimization_mode", "full"},
      {"solveOptimizationMode", "vertex_only"},
  };

  const bbsolver::SolverConfig parsed = bbsolver::ParseSolverConfigJson(obj);

  assert(std::abs(parsed.motion_smooth_tolerance - 2.0) < 1e-12);
  assert(parsed.allow_shape_temporal_bezier == false);
  assert(std::abs(parsed.shape_temporal_bezier_attempt_threshold_ratio - 0.5) < 1e-12);
  assert(parsed.solve_optimization_mode == "full");
}

void TestNullValuesFallBackToDefaults() {
  // GetOr treats explicit-null and missing identically; both must yield
  // the default-constructed value. Without this guarantee panel writers
  // that emit { "tolerance": null } would silently silently coerce to 0.
  const json obj = {
      {"tolerance", json{}},
      {"motion_smooth_tolerance", json{}},
      {"motion_path_smoothing_tolerance", json{}},
      {"motion_path_accuracy_tolerance", json{}},
      {"motion_path_preserve_bounds", json{}},
      {"motion_path_bounds_tolerance", json{}},
      {"placement_strategy", json{}},
  };
  const bbsolver::SolverConfig parsed = bbsolver::ParseSolverConfigJson(obj);
  const bbsolver::SolverConfig defaults;
  assert(parsed.tolerance == defaults.tolerance);
  assert(parsed.motion_smooth_tolerance == defaults.motion_smooth_tolerance);
  assert(parsed.motion_path_smoothing_tolerance ==
         defaults.motion_path_smoothing_tolerance);
  assert(parsed.motion_path_accuracy_tolerance ==
         defaults.motion_path_accuracy_tolerance);
  assert(parsed.motion_path_preserve_bounds ==
         defaults.motion_path_preserve_bounds);
  assert(parsed.motion_path_bounds_tolerance ==
         defaults.motion_path_bounds_tolerance);
  assert(parsed.placement_strategy == defaults.placement_strategy);
}

}  // namespace

int main() {
  TestEmptyObjectYieldsAllDefaults();
  TestCanonicalSnakeCaseFieldsAreParsed();
  TestLegacyAliasesAreHonoredWhenCanonicalAbsent();
  TestLegacyAliasMotionSmoothSourceKeyFidelityChain();
  TestCanonicalWinsOverLegacyAlias();
  TestNullValuesFallBackToDefaults();
  return 0;
}
