#include "bbsolver/io/solver_config_io.hpp"
#include "bbsolver/domain.hpp"

#include <string>
#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>

namespace bbsolver {
namespace {

using nlohmann::json;

template <typename T>
T GetOr(const json& obj, const char* key, T fallback) {
  const auto it = obj.find(key);
  if (it == obj.end() || it->is_null()) {
    return fallback;
  }
  return it->get<T>();
}

}  // namespace

SolverConfig ParseSolverConfigJson(const json& obj) {
  SolverConfig config;
  config.tolerance = GetOr<double>(obj, "tolerance", config.tolerance);
  config.tolerance_screen_px = GetOr<double>(obj, "tolerance_screen_px", config.tolerance_screen_px);
  config.weight_pos = GetOr<double>(obj, "weight_pos", config.weight_pos);
  config.weight_vel = GetOr<double>(obj, "weight_vel", config.weight_vel);
  config.weight_acc = GetOr<double>(obj, "weight_acc", config.weight_acc);
  config.weight_curv = GetOr<double>(obj, "weight_curv", config.weight_curv);
  config.weight_screen = GetOr<double>(obj, "weight_screen", config.weight_screen);
  config.allow_hold = GetOr<bool>(obj, "allow_hold", config.allow_hold);
  config.allow_linear = GetOr<bool>(obj, "allow_linear", config.allow_linear);
  config.allow_bezier = GetOr<bool>(obj, "allow_bezier", config.allow_bezier);
  config.allow_shape_temporal_bezier =
      GetOr<bool>(obj,
                  "allow_shape_temporal_bezier",
                  GetOr<bool>(obj, "shape_temporal_bezier", config.allow_shape_temporal_bezier));
  config.allow_path_spatial_fit =
      GetOr<bool>(obj,
                  "allow_path_spatial_fit",
                  GetOr<bool>(obj, "path_spatial_fit", config.allow_path_spatial_fit));
  config.allow_path_replacement_fit =
      GetOr<bool>(obj,
                  "allow_path_replacement_fit",
                  GetOr<bool>(obj, "path_replacement_fit", config.allow_path_replacement_fit));
  config.path_replacement_prefer_vertices =
      GetOr<bool>(obj,
                  "path_replacement_prefer_vertices",
                  config.path_replacement_prefer_vertices);
  config.solve_optimization_mode =
      GetOr<std::string>(
          obj,
          "solve_optimization_mode",
          GetOr<std::string>(
              obj,
              "solveOptimizationMode",
              config.solve_optimization_mode));
  config.motion_smooth_use_ease =
      GetOr<bool>(
          obj,
          "motion_smooth_use_ease",
          GetOr<bool>(
              obj,
              "motionSmoothUseEase",
              config.motion_smooth_use_ease));
  config.motion_smooth_source_fidelity =
      GetOr<bool>(
          obj,
          "motion_smooth_source_fidelity",
          GetOr<bool>(
              obj,
              "motionSmoothSourceFidelity",
              GetOr<bool>(
                  obj,
                  "motion_smooth_source_key_fidelity",
                  GetOr<bool>(
                      obj,
                      "motionSmoothSourceKeyFidelity",
                      config.motion_smooth_source_fidelity))));
  config.motion_smooth_tolerance =
      GetOr<double>(
          obj,
          "motion_smooth_tolerance",
          GetOr<double>(
              obj,
              "motionSmoothTolerance",
              config.motion_smooth_tolerance));
  config.motion_smooth_bezier_x1 =
      GetOr<double>(
          obj,
          "motion_smooth_bezier_x1",
          GetOr<double>(
              obj,
              "motionSmoothBezierX1",
              config.motion_smooth_bezier_x1));
  config.motion_smooth_bezier_y1 =
      GetOr<double>(
          obj,
          "motion_smooth_bezier_y1",
          GetOr<double>(
              obj,
              "motionSmoothBezierY1",
              config.motion_smooth_bezier_y1));
  config.motion_smooth_bezier_x2 =
      GetOr<double>(
          obj,
          "motion_smooth_bezier_x2",
          GetOr<double>(
              obj,
              "motionSmoothBezierX2",
              config.motion_smooth_bezier_x2));
  config.motion_smooth_bezier_y2 =
      GetOr<double>(
          obj,
          "motion_smooth_bezier_y2",
          GetOr<double>(
              obj,
              "motionSmoothBezierY2",
              config.motion_smooth_bezier_y2));
  config.motion_path_smoothing_tolerance =
      GetOr<double>(
          obj,
          "motion_path_smoothing_tolerance",
          GetOr<double>(
              obj,
              "motionPathSmoothingTolerance",
              config.motion_path_smoothing_tolerance));
  config.motion_path_accuracy_tolerance =
      GetOr<double>(
          obj,
          "motion_path_accuracy_tolerance",
          GetOr<double>(
              obj,
              "motionPathAccuracyTolerance",
              config.motion_path_accuracy_tolerance));
  config.motion_path_preserve_bounds =
      GetOr<bool>(
          obj,
          "motion_path_preserve_bounds",
          GetOr<bool>(
              obj,
              "motionPathPreserveBounds",
              config.motion_path_preserve_bounds));
  config.motion_path_bounds_tolerance =
      GetOr<double>(
          obj,
          "motion_path_bounds_tolerance",
          GetOr<double>(
              obj,
              "motionPathBoundsTolerance",
              config.motion_path_bounds_tolerance));
  config.motion_path_preserve_sharp_points =
      GetOr<bool>(
          obj,
          "motion_path_preserve_sharp_points",
          GetOr<bool>(
              obj,
              "motionPathPreserveSharpPoints",
              config.motion_path_preserve_sharp_points));
  config.motion_path_sharp_angle_deg =
      GetOr<double>(
          obj,
          "motion_path_sharp_angle_deg",
          GetOr<double>(
              obj,
              "motionPathSharpAngleDeg",
              config.motion_path_sharp_angle_deg));
  config.motion_path_respect_keyed_frames =
      GetOr<bool>(
          obj,
          "motion_path_respect_keyed_frames",
          GetOr<bool>(
              obj,
              "motionPathRespectKeyedFrames",
              config.motion_path_respect_keyed_frames));
  config.path_preserve_sharp_corners =
      GetOr<bool>(obj,
                  "path_preserve_sharp_corners",
                  config.path_preserve_sharp_corners);
  config.path_sharp_corner_angle_deg =
      GetOr<double>(obj,
                    "path_sharp_corner_angle_deg",
                    config.path_sharp_corner_angle_deg);
  config.path_sharp_corner_tolerance =
      GetOr<double>(obj,
                    "path_sharp_corner_tolerance",
                    config.path_sharp_corner_tolerance);
  config.path_replacement_min_vertices =
      GetOr<int>(obj, "path_replacement_min_vertices", config.path_replacement_min_vertices);
  config.path_replacement_max_vertices =
      GetOr<int>(obj, "path_replacement_max_vertices", config.path_replacement_max_vertices);
  config.path_replacement_max_key_growth_ratio =
      GetOr<double>(obj,
                    "path_replacement_max_key_growth_ratio",
                    config.path_replacement_max_key_growth_ratio);
  config.path_replacement_min_vertex_reduction_ratio =
      GetOr<double>(obj,
                    "path_replacement_min_vertex_reduction_ratio",
                    config.path_replacement_min_vertex_reduction_ratio);
  config.path_specific_max_gap =
      GetOr<int>(obj, "path_specific_max_gap", config.path_specific_max_gap);
  config.shape_temporal_bezier_attempt_threshold_ratio =
      GetOr<double>(obj,
                    "shape_temporal_bezier_attempt_threshold_ratio",
                    GetOr<double>(
                        obj,
                        "shape_temporal_bezier_gate_ratio",
                        config.shape_temporal_bezier_attempt_threshold_ratio));
  config.min_influence = GetOr<double>(obj, "min_influence", config.min_influence);
  config.max_influence = GetOr<double>(obj, "max_influence", config.max_influence);
  config.max_iters_per_segment = GetOr<int>(obj, "max_iters_per_segment", config.max_iters_per_segment);
  config.min_segment_frames = GetOr<int>(obj, "min_segment_frames", config.min_segment_frames);
  config.max_keys_hint = GetOr<int>(obj, "max_keys_hint", config.max_keys_hint);
  config.parallel_jobs = GetOr<int>(obj, "parallel_jobs", config.parallel_jobs);
  config.placement_strategy =
      GetOr<std::string>(obj, "placement_strategy", config.placement_strategy);
  config.verbose = GetOr<bool>(obj, "verbose", config.verbose);
  return config;
}

}  // namespace bbsolver
