#include "bbsolver/path/multimode/path_multimode_region_candidate.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/path/multimode/path_multimode_solver.hpp"
#include "bbsolver/path/multimode/path_multimode_geometry.hpp"
#include "bbsolver/path/multimode/path_multimode_temporal.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <cstddef>
#include <vector>

namespace bbsolver {
namespace path_multimode {

bool RegionSegmentFeasible(const PropertySamples& reduced,
                           int i,
                           int j,
                           VertexRegion region,
                           double tolerance) {
  if (i < 0 || j <= i || j >= static_cast<int>(reduced.samples.size())) {
    return false;
  }
  const Sample& start = reduced.samples[static_cast<std::size_t>(i)];
  const Sample& end = reduced.samples[static_cast<std::size_t>(j)];
  const double t0 = start.t_sec;
  const double t1 = end.t_sec;
  if (!(t1 > t0)) {
    return false;
  }
  for (int sample_idx = i + 1; sample_idx < j; ++sample_idx) {
    const Sample& sample =
        reduced.samples[static_cast<std::size_t>(sample_idx)];
    const double u = std::clamp((sample.t_sec - t0) / (t1 - t0), 0.0, 1.0);
    for (int vertex = region.first_vertex; vertex < region.end_vertex; ++vertex) {
      for (int component = 0; component < 2; ++component) {
        const double a = ShapeComponent(start.v, vertex, component);
        const double b = ShapeComponent(end.v, vertex, component);
        const double actual = ShapeComponent(sample.v, vertex, component);
        const double predicted = a + (b - a) * u;
        if (std::abs(actual - predicted) > tolerance + 1e-9) {
          return false;
        }
      }
    }
  }
  return true;
}

RegionSolveResult SolveRegionAnchors(const PropertySamples& reduced,
                                      VertexRegion region,
                                      const ShapeFlatMultiModeOptions& options,
                                      int max_gap,
                                      int* segment_checks) {
  RegionSolveResult result;
  const int n = static_cast<int>(reduced.samples.size());
  if (n <= 1) {
    result.anchors = n == 1 ? std::vector<int>{0} : std::vector<int>{};
    return result;
  }
  constexpr int kInf = std::numeric_limits<int>::max() / 4;
  std::vector<int> dp(static_cast<std::size_t>(n), kInf);
  std::vector<int> prev(static_cast<std::size_t>(n), -1);
  dp[0] = 0;

  for (int j = 1; j < n; ++j) {
    if (options.cancel_fn && options.cancel_fn()) {
      return {};
    }
    const int i_lo = std::max(0, j - max_gap);
    for (int i = i_lo; i < j; ++i) {
      if (segment_checks != nullptr) {
        ++(*segment_checks);
        if (options.max_region_segment_checks > 0 &&
            *segment_checks > options.max_region_segment_checks) {
          result.budget_exceeded = true;
          return result;
        }
      }
      if (dp[static_cast<std::size_t>(i)] >= kInf) {
        continue;
      }
      if (!RegionSegmentFeasible(reduced, i, j, region,
                                 std::max(options.region_tolerance, 0.0))) {
        continue;
      }
      const int candidate = dp[static_cast<std::size_t>(i)] + 1;
      if (candidate < dp[static_cast<std::size_t>(j)]) {
        dp[static_cast<std::size_t>(j)] = candidate;
        prev[static_cast<std::size_t>(j)] = i;
      }
    }
  }

  if (dp[static_cast<std::size_t>(n - 1)] >= kInf) {
    std::vector<int> fallback;
    fallback.reserve(static_cast<std::size_t>(n));
    for (int idx = 0; idx < n; ++idx) {
      fallback.push_back(idx);
    }
    result.anchors = std::move(fallback);
    return result;
  }

  std::vector<int> anchors;
  for (int idx = n - 1; idx >= 0;) {
    anchors.push_back(idx);
    if (idx == 0) {
      break;
    }
    idx = prev[static_cast<std::size_t>(idx)];
    if (idx < 0) {
      return result;
    }
  }
  std::reverse(anchors.begin(), anchors.end());
  result.anchors = std::move(anchors);
  return result;
}

Key MakeLinearShapeKey(const PropertySamples& reduced,
                       int sample_idx,
                       bool first,
                       bool last) {
  Key key;
  key.t_sec = reduced.samples[static_cast<std::size_t>(sample_idx)].t_sec;
  key.v = reduced.samples[static_cast<std::size_t>(sample_idx)].v;
  key.interp_in = first ? InterpType::Bezier : InterpType::Linear;
  key.interp_out = last ? InterpType::Bezier : InterpType::Linear;
  key.temporal_ease_in = NeutralEase();
  key.temporal_ease_out = NeutralEase();
  key.temporal_continuous = false;
  key.spatial_continuous = false;
  key.temporal_auto_bezier = false;
  key.spatial_auto_bezier = false;
  key.roving = false;
  return key;
}

Key MakeShapeKeyFromValue(const PropertySamples& reduced,
                          int sample_idx,
                          std::vector<double> value,
                          bool first,
                          bool last) {
  Key key = MakeLinearShapeKey(reduced, sample_idx, first, last);
  key.v = std::move(value);
  return key;
}

PropertyKeys BuildCandidate(const PropertySamples& reduced,
                            const std::vector<int>& anchors) {
  PropertyKeys keys;
  keys.property_id = reduced.property.id;
  keys.converged = false;
  if (anchors.empty()) {
    keys.notes = "shape_multimode_no_anchors";
    return keys;
  }
  keys.keys.reserve(anchors.size());
  for (std::size_t idx = 0; idx < anchors.size(); ++idx) {
    keys.keys.push_back(MakeLinearShapeKey(reduced,
                                           anchors[idx],
                                           idx == 0,
                                           idx + 1 == anchors.size()));
  }
  keys.segments.reserve(anchors.size() > 1 ? anchors.size() - 1 : 0);
  for (std::size_t idx = 0; idx + 1 < anchors.size(); ++idx) {
    SegmentReport report;
    report.start_idx = anchors[idx];
    report.end_idx = anchors[idx + 1];
    report.reason = "replacement_shape_multimode_linear_union";
    keys.segments.push_back(std::move(report));
  }
  return keys;
}

bool CandidateKeyBudgetExceeded(int key_count,
                                int sample_count,
                                double max_candidate_key_ratio) {
  if (!(max_candidate_key_ratio > 0.0)) {
    return false;
  }
  return static_cast<double>(key_count) >
         static_cast<double>(sample_count) * max_candidate_key_ratio + 1e-9;
}

int ValidationWorkUnits(const PropertySamples& original,
                        int candidate_vertex_count,
                        const PathFrameFitOptions& options) {
  const int source_vertex_count = MaxShapeFlatVertexCount(original);
  const int subdivisions =
      std::max(1, options.max_subdivisions_per_segment);
  return static_cast<int>(original.samples.size()) *
         std::max(1, source_vertex_count + candidate_vertex_count) *
         subdivisions;
}

std::vector<double> LinearInterpolateShapeFlat(const std::vector<double>& a,
                                               const std::vector<double>& b,
                                               double u) {
  if (!SameShapeFlatTopology(a, b)) {
    return {};
  }
  std::vector<double> out = a;
  const double clamped_u = std::clamp(u, 0.0, 1.0);
  for (std::size_t idx = 2; idx < out.size(); ++idx) {
    out[idx] = a[idx] + (b[idx] - a[idx]) * clamped_u;
  }
  return out;
}

}  // namespace path_multimode
}  // namespace bbsolver
