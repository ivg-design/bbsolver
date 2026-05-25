#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace bbsolver {

enum class ValueKind : std::uint8_t {
  Scalar = 0,
  TwoD = 1,
  ThreeD = 2,
  TwoD_Spatial = 3,
  ThreeD_Spatial = 4,
  Color = 5,
  Custom = 6,
};

enum class InterpType : std::uint8_t {
  Hold = 0,
  Linear = 1,
  Bezier = 2,
};

struct TemporalEase {
  double speed = 0.0;
  double influence = 33.3;
};

struct KeyTiming {
  InterpType interp_in = InterpType::Bezier;
  InterpType interp_out = InterpType::Bezier;
  std::vector<TemporalEase> temporal_ease_in;
  std::vector<TemporalEase> temporal_ease_out;
  std::vector<double> spatial_in;
  std::vector<double> spatial_out;
  bool temporal_continuous = false;
  bool spatial_continuous = false;
  bool temporal_auto_bezier = false;
  bool spatial_auto_bezier = false;
  bool roving = false;
};

struct CompInfo {
  double fps = 0.0;
  double duration_sec = 0.0;
  int width = 0;
  int height = 0;
  double pixel_aspect = 1.0;
  double shutter_angle_deg = 180.0;
  double shutter_phase_deg = 0.0;
  bool motion_blur_enabled = false;
  double work_area_start_sec = 0.0;
  double work_area_end_sec = 0.0;
};

struct LayerXform {
  std::vector<double> anchor_point;
  std::vector<double> position;
  std::vector<double> scale;
  std::vector<double> rotation;
  double opacity = 100.0;
};

struct PropertyInfo {
  std::string id;
  std::string match_name;
  std::string display_name;
  std::string layer_path;
  ValueKind kind = ValueKind::Scalar;
  int dimensions = 1;
  bool is_spatial = false;
  bool is_separated = false;
  std::string units_label;
  std::vector<double> min_value;
  std::vector<double> max_value;
  std::vector<double> source_key_times;
};

struct Sample {
  double t_sec = 0.0;
  std::vector<double> v;
  std::optional<KeyTiming> key_timing;
};

struct PropertySamples {
  PropertyInfo property;
  double t_start_sec = 0.0;
  double t_end_sec = 0.0;
  int samples_per_frame = 1;
  std::vector<Sample> samples;
  std::optional<LayerXform> layer_xform_at_start;
  std::string hash_of_expression;
};

struct SolverConfig {
  double tolerance = 0.5;
  double tolerance_screen_px = 0.0;
  double weight_pos = 1.0;
  double weight_vel = 0.1;
  double weight_acc = 0.01;
  double weight_curv = 0.0;
  double weight_screen = 0.0;
  bool allow_hold = true;
  bool allow_linear = true;
  bool allow_bezier = true;
  bool allow_shape_temporal_bezier = false;
  bool allow_path_spatial_fit = false;
  bool allow_path_replacement_fit = false;
  bool path_replacement_prefer_vertices = false;
  std::string solve_optimization_mode = "full";
  bool motion_smooth_use_ease = false;
  bool motion_smooth_source_fidelity = false;
  double motion_smooth_tolerance = 3.0;
  double motion_smooth_bezier_x1 = 0.33;
  double motion_smooth_bezier_y1 = 0.0;
  double motion_smooth_bezier_x2 = 0.67;
  double motion_smooth_bezier_y2 = 1.0;
  double motion_path_smoothing_tolerance = 3.0;
  double motion_path_accuracy_tolerance = 1.5;
  bool motion_path_preserve_bounds = false;
  double motion_path_bounds_tolerance = 0.0;
  bool motion_path_preserve_sharp_points = true;
  double motion_path_sharp_angle_deg = 75.0;
  bool motion_path_respect_keyed_frames = false;
  bool path_preserve_sharp_corners = true;
  double path_sharp_corner_angle_deg = 90.0;
  double path_sharp_corner_tolerance = 1.5;
  int path_replacement_min_vertices = 4;
  int path_replacement_max_vertices = 0;
  double path_replacement_max_key_growth_ratio = 4.0;
  double path_replacement_min_vertex_reduction_ratio = 0.20;
  int path_specific_max_gap = 0;
  double shape_temporal_bezier_attempt_threshold_ratio = -1.0;
  double min_influence = 0.1;
  double max_influence = 100.0;
  int max_iters_per_segment = 100;
  int min_segment_frames = 2;
  int max_keys_hint = 0;
  int parallel_jobs = 0;
  std::string placement_strategy = "dp";
  bool verbose = false;
};

struct SampleBundle {
  int schema_version = 1;
  std::string request_id;
  CompInfo comp;
  std::vector<PropertySamples> properties;
  SolverConfig config;
};

struct Key {
  double t_sec = 0.0;
  std::vector<double> v;
  InterpType interp_in = InterpType::Bezier;
  InterpType interp_out = InterpType::Bezier;
  std::vector<TemporalEase> temporal_ease_in;
  std::vector<TemporalEase> temporal_ease_out;
  std::vector<double> spatial_in;
  std::vector<double> spatial_out;
  bool temporal_continuous = false;
  bool spatial_continuous = false;
  bool temporal_auto_bezier = false;
  bool spatial_auto_bezier = false;
  bool roving = false;
};

struct SegmentReport {
  int start_idx = 0;
  int end_idx = 0;
  double max_err = 0.0;
  double max_err_screen_px = 0.0;
  double rms_err = 0.0;
  int iters = 0;
  std::string reason;
};

struct PropertyKeys {
  std::string property_id;
  int dimensions = 0;
  std::vector<Key> keys;
  double max_err = 0.0;
  double max_err_screen_px = 0.0;
  std::vector<SegmentReport> segments;
  bool converged = true;
  std::string notes;
};

struct KeyBundle {
  int schema_version = 1;
  std::string request_id;
  std::vector<PropertyKeys> property_results;
  std::string solver_version;
  std::string solver_build;
  double solve_time_ms = 0.0;
  int total_keys = 0;
  int total_samples_input = 0;
};

}  // namespace bbsolver
