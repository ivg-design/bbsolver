// Implements the sharp-feature classifier declared in path_sharp_feature.hpp.
// Behavior is byte-faithful with the prior path_frame_fit.cpp definitions:
// same constants, same turn-angle formula, same zero-tangent threshold,
// same closed/open endpoint handling.
//
// Diagnostics decision: **none / pure layout**. Pure geometric classifier
// that returns booleans + a small decision struct. No DiagnosticsWriter, no
// progress, no cancellation, no operator state. Diagnostics ownership stays
// with callers (the per-frame validator / fitter / canonical-layout builder
// each decide whether a sharp or zero-tangent verdict deserves a result note).

#include "bbsolver/path/geometry/path_sharp_feature.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit_geometry.hpp"

namespace bbsolver {
namespace pff_sharp {

double TurnAngleAtSourceVertex(const std::vector<double>& flat,
                               int vertex_index,
                               const pff_geom::DecodedShape& decoded) {
  if (decoded.vertex_count < 3) {
    return 0.0;
  }
  if (!decoded.closed && (vertex_index == 0 || vertex_index == decoded.vertex_count - 1)) {
    return std::numeric_limits<double>::infinity();
  }
  const int prev = vertex_index == 0 ? decoded.vertex_count - 1 : vertex_index - 1;
  const int next = (vertex_index + 1) % decoded.vertex_count;
  const pff_geom::Point p = pff_geom::FlatPoint(flat, vertex_index, 0);
  const pff_geom::Point a = pff_geom::FlatPoint(flat, prev, 0);
  const pff_geom::Point b = pff_geom::FlatPoint(flat, next, 0);
  const pff_geom::Point va = pff_geom::Sub(a, p);
  const pff_geom::Point vb = pff_geom::Sub(b, p);
  const double la = pff_geom::Length(va);
  const double lb = pff_geom::Length(vb);
  if (!(la > 1e-9) || !(lb > 1e-9)) {
    return std::numeric_limits<double>::infinity();
  }
  const double cos_theta = std::clamp((va.x * vb.x + va.y * vb.y) / (la * lb), -1.0, 1.0);
  const double interior = std::acos(cos_theta);
  return std::abs(pff_geom::kPi - interior);
}

bool HasZeroTangentsAtSourceVertex(const std::vector<double>& flat,
                                   int vertex_index,
                                   const pff_geom::DecodedShape& decoded) {
  if (vertex_index < 0 || vertex_index >= decoded.vertex_count) {
    return false;
  }
  const pff_geom::Point in = pff_geom::FlatPoint(flat, vertex_index, 2);
  const pff_geom::Point out = pff_geom::FlatPoint(flat, vertex_index, 4);
  return pff_geom::Length(in) <= kZeroTangentEpsilon &&
         pff_geom::Length(out) <= kZeroTangentEpsilon;
}

SharpFeatureDecision DetectSharpFeatureAtSourceVertex(
    const std::vector<double>& flat,
    int vertex_index,
    const pff_geom::DecodedShape& decoded,
    const PathFrameFitOptions& options) {
  SharpFeatureDecision decision;
  if (!options.source_vertices_are_semantic_anchors) {
    return decision;
  }
  if (vertex_index < 0 || vertex_index >= decoded.vertex_count) {
    return decision;
  }
  if (!decoded.closed && (vertex_index == 0 || vertex_index == decoded.vertex_count - 1)) {
    return decision;
  }
  const double turn_angle = TurnAngleAtSourceVertex(flat, vertex_index, decoded);
  if (!std::isfinite(turn_angle)) {
    return decision;
  }
  decision.turn_radians = turn_angle;
  const bool geometric_corner = turn_angle >= kSharpTurnRadians;
  decision.zero_tangent_cue =
      HasZeroTangentsAtSourceVertex(flat, vertex_index, decoded) &&
      turn_angle >= kSemanticSharpTurnRadians;
  decision.required = geometric_corner || decision.zero_tangent_cue;
  return decision;
}

std::vector<bool> DetectSharpSourceVertices(
    const std::vector<double>& flat,
    const pff_geom::DecodedShape& decoded,
    const PathFrameFitOptions& options) {
  std::vector<bool> sharp(static_cast<std::size_t>(std::max(decoded.vertex_count, 0)), false);
  if (decoded.vertex_count <= 0) {
    return sharp;
  }
  if (!options.source_vertices_are_semantic_anchors) {
    return sharp;
  }
  if (!decoded.closed && decoded.vertex_count > 1) {
    sharp[0] = true;
    sharp[static_cast<std::size_t>(decoded.vertex_count - 1)] = true;
  }
  for (int i = 0; i < decoded.vertex_count; ++i) {
    if (DetectSharpFeatureAtSourceVertex(flat, i, decoded, options).required) {
      sharp[static_cast<std::size_t>(i)] = true;
    }
  }
  return sharp;
}

std::vector<bool> DetectTangentLockedSourceVertices(
    const std::vector<double>& flat,
    const pff_geom::DecodedShape& decoded,
    const PathFrameFitOptions& options) {
  std::vector<bool> locked(static_cast<std::size_t>(std::max(decoded.vertex_count, 0)), false);
  if (decoded.vertex_count <= 0) {
    return locked;
  }
  if (!options.source_vertices_are_semantic_anchors) {
    return locked;
  }
  if (!decoded.closed && decoded.vertex_count > 1) {
    locked[0] = true;
    locked[static_cast<std::size_t>(decoded.vertex_count - 1)] = true;
  }
  for (int i = 0; i < decoded.vertex_count; ++i) {
    const SharpFeatureDecision decision =
        DetectSharpFeatureAtSourceVertex(flat, i, decoded, options);
    if (decision.zero_tangent_cue ||
        decision.turn_radians >= kHardTangentLockTurnRadians) {
      locked[static_cast<std::size_t>(i)] = true;
    }
  }
  return locked;
}

}  // namespace pff_sharp
}  // namespace bbsolver
