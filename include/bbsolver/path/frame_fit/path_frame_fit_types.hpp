#pragma once

#include <string>
#include <vector>

namespace bbsolver {

struct PathFrameFitOptions {
  double outline_tolerance = 0.5;
  int max_subdivisions_per_segment = 64;
  int max_refine_iterations = 256;
  // 0 = automatic minimum vertex count under outline_tolerance.
  //
  // When > 0, the fitter first finds the automatic minimum, then inserts
  // geometrically meaningful outline points until this count is reached. This
  // lets a caller fit every frame to a common K for temporal solving. If the
  // target is lower than required by tolerance/sharp corners, the automatic
  // larger count is kept and target_met is false. Extra vertices are inserted
  // by splitting the accepted fitted cubic segments, so target growth should
  // preserve the automatic fit's outline error.
  int target_vertex_count = 0;
  bool use_catmull_tangents = true;
  // True for authored source paths. Visible-outline extraction can emit a
  // synthetic boundary polygon whose vertices are split/sample points, not
  // semantic source anchors; callers should set this false for that mode.
  bool source_vertices_are_semantic_anchors = true;
};

struct PathFeatureAnchor {
  int source_vertex_index = -1;
  double outline_fraction = 0.0;
  double turn_radians = 0.0;
  bool zero_tangent_cue = false;
};

struct PathFrameFitResult {
  bool ok = false;
  bool applied = false;
  bool closed = false;
  int source_vertex_count = 0;
  int fitted_vertex_count = 0;
  int target_vertex_count = 0;
  // False when target_vertex_count was requested but the automatic fit needed
  // more vertices than the target. In that case fitted still contains the
  // tolerance-preserving automatic result.
  bool target_met = true;
  double max_outline_error = 0.0;
  std::string warning;
  std::vector<double> fitted;
  std::vector<int> kept_dense_indices;
  std::vector<int> source_vertex_indices;
  // Normalized source-outline fraction for each fitted vertex in seam order.
  // Automatic fits populate this so another frame can reuse the same stable
  // vertex-slot layout via FitShapeFlatFrameAtFractions.
  std::vector<double> outline_fractions;
};

struct VisibleShapeFlatOutlineResult {
  bool ok = false;
  bool applied = false;
  bool closed = false;
  int source_vertex_count = 0;
  int outline_vertex_count = 0;
  std::string warning;
  std::vector<double> outline;
};

struct PathFractionExpansionOptions {
  // Maximum resulting slot count. 0 uses the seed count plus max_insertions,
  // still capped below the minimum source vertex count across the supplied
  // frames so expansion cannot erase the replacement's vertex-count benefit.
  int max_fraction_count = 0;
  // Maximum accepted midpoint insertions. <= 0 means no insertion-count cap;
  // max_fraction_count and source topology still bound the search.
  int max_insertions = 8;
  // Per pass, try the N largest fraction gaps. <= 0 tries every gap.
  int max_candidate_gaps_per_pass = 0;
  // Required max-error reduction before accepting a denser layout. The default
  // accepts any non-worsening replayable insertion, which lets later midpoint
  // splits unlock improvement while the search remains bounded above.
  double min_error_improvement = 0.0;
};

struct PathFractionExpansionResult {
  bool ok = false;
  bool applied = false;
  bool closed = false;
  bool tolerance_met = false;
  std::string warning;
  std::vector<double> outline_fractions;
  int initial_fraction_count = 0;
  int final_fraction_count = 0;
  int max_fraction_count = 0;
  int insertions = 0;
  int candidate_evaluations = 0;
  double initial_max_outline_error = 0.0;
  double final_max_outline_error = 0.0;
};

struct PathFeatureFractionLayoutResult {
  bool ok = false;
  bool closed = false;
  std::string warning;
  std::string notes;
  std::vector<double> outline_fractions;
  int target_count = 0;
  int frame_count = 0;
  int feature_count = 0;
};

struct PathReplacementTargetLadderOptions {
  // Optional lower bound, typically the configured path_replacement_min_vertices.
  int min_target_vertices = 0;
  // Optional upper bound, typically path_replacement_max_vertices. The ladder
  // is always additionally capped to source_min_vertices - 1 so every target
  // still represents a stable-topology reduction for all source frames.
  int max_target_vertices = 0;
  int step_vertices = 2;
  // Hard cap on returned targets. Includes the initial target.
  int max_candidate_targets = 4;
  // When there is room in the budget, try the highest legal reduction target
  // as the final fallback candidate.
  bool include_source_min_minus_one = true;
};

struct PathFrameGeometryRefineOptions {
  // Hard bound on local coordinate-descent passes over the fitted landmarks.
  int max_iterations = 16;
  // 0 = derive from source bounds/tolerance.
  double initial_step_px = 0.0;
  // 0 = derive from source bounds. This limits how far non-feature landmarks
  // can move away from their fixed-fraction starting positions.
  double max_vertex_move_px = 0.0;
  double min_error_improvement = 1e-5;
};

struct PathFrameGeometryRefineResult {
  bool ok = false;
  // True when the final fit improves over the input fixed-fraction fit.
  bool improved = false;
  std::string warning;
  PathFrameFitResult fit;
  double initial_max_outline_error = 0.0;
  int iterations = 0;
  int candidate_evaluations = 0;
};

struct ShapeFlatOutlinePoint {
  double x = 0.0;
  double y = 0.0;
};

struct ShapeFlatOutlinePolyline {
  bool ok = false;
  bool closed = false;
  std::vector<ShapeFlatOutlinePoint> points;
};

}  // namespace bbsolver
