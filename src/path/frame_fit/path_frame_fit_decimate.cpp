// Implements the decimation pipeline declared in
// path_frame_fit_decimate.hpp. Behavior is byte-faithful with the previous
// anonymous-namespace definitions in path_frame_fit.cpp.
//
// Diagnostics decision: **none / pure layout**. Pure geometric decimation.
// No DiagnosticsWriter, no progress, no cancellation, no operator state.
// Diagnostics ownership: caller-owned.

#include "bbsolver/path/frame_fit/path_frame_fit_decimate.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

#include "bbsolver/path/dense/path_dense_polyline.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit_cubic_span.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit_geometry.hpp"

namespace bbsolver {
namespace pff_decimate {
namespace {

using pff_cubic_span::CubicSpanFit;
using pff_cubic_span::FitDenseSpanCubic;
using pff_dense::DirectedPolylineDistance;

// Private local copy of UniqueSortedIndices (was anonymous-namespace in
// path_frame_fit.cpp and is used 9 times there; keeping a local copy here
// avoids promoting a generic int-vector dedup helper into a shared header
// just for this TU).
std::vector<int> UniqueSortedIndices(std::vector<int> indices) {
  std::sort(indices.begin(), indices.end());
  indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
  return indices;
}

int ForwardDenseSpan(int begin, int end, int dense_count, bool closed) {
  if (begin < 0 || end < 0 || dense_count <= 0) {
    return 0;
  }
  if (end >= begin) {
    return end - begin;
  }
  return closed ? dense_count - begin + end: 0;
}

void CubicMarkKept(const std::vector<DensePoint>& dense,
                   int begin,
                   int end,
                   double tolerance,
                   const std::vector<bool>& sharp_source_vertices,
                   bool closed,
                   std::vector<bool>& keep) {
  std::vector<std::pair<int, int>> stack;
  stack.push_back({begin, end});
  while (!stack.empty()) {
    const auto [a, b] = stack.back();
    stack.pop_back();
    const int span = ForwardDenseSpan(a, b, static_cast<int>(dense.size()), closed);
    if (span <= 1) {
      continue;
    }
    const CubicSpanFit fit = FitDenseSpanCubic(dense, a, b, sharp_source_vertices, closed);
    int worst = fit.worst_dense_index;
    if (fit.max_error <= tolerance || worst < 0 || worst == a || worst == b) {
      continue;
    }
    keep[static_cast<std::size_t>(worst)] = true;
    stack.push_back({a, worst});
    stack.push_back({worst, b});
  }
}

std::vector<int> BuildForwardSourceVertexCandidates(const std::vector<int>& source_to_dense,
                                                    int dense_count,
                                                    int begin,
                                                    int end,
                                                    bool closed) {
  std::vector<std::pair<int, int>> ordered;
  const int span = ForwardDenseSpan(begin, end, dense_count, closed);
  if (span <= 0) {
    return {};
  }
  ordered.push_back({0, begin});
  for (int dense_index: source_to_dense) {
    if (dense_index < 0 || dense_index >= dense_count ||
        dense_index == begin || dense_index == end) {
      continue;
    }
    int offset = dense_index - begin;
    if (closed && offset < 0) {
      offset += dense_count;
    }
    if (offset > 0 && offset < span) {
      ordered.push_back({offset, dense_index});
    }
  }
  ordered.push_back({span, end});
  std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) {
    if (a.first != b.first) {
      return a.first < b.first;
    }
    return a.second < b.second;
  });
  ordered.erase(std::unique(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) {
                  return a.second == b.second;
                }),
                ordered.end());

  std::vector<int> candidates;
  candidates.reserve(ordered.size());
  for (const auto& [offset, dense_index]: ordered) {
    (void)offset;
    candidates.push_back(dense_index);
  }
  return candidates;
}

std::vector<int> SimplifyCandidateIntervalDp(
    const std::vector<DensePoint>& dense,
    const std::vector<int>& candidates,
    const std::vector<bool>& tangent_locked_source_vertices,
    bool closed,
    double tolerance) {
  const int n = static_cast<int>(candidates.size());
  if (n < 2) {
    return {};
  }
  constexpr int kInf = 1000000000;
  std::vector<int> cost(static_cast<std::size_t>(n), kInf);
  std::vector<int> previous(static_cast<std::size_t>(n), -1);
  cost[0] = 1;
  for (int i = 0; i < n; ++i) {
    if (cost[static_cast<std::size_t>(i)] >= kInf) {
      continue;
    }
    for (int j = i + 1; j < n; ++j) {
      const CubicSpanFit fit = FitDenseSpanCubic(
          dense,
          candidates[static_cast<std::size_t>(i)],
          candidates[static_cast<std::size_t>(j)],
          tangent_locked_source_vertices,
          closed);
      if (fit.max_error > tolerance + 1e-9) {
        continue;
      }
      const int next_cost = cost[static_cast<std::size_t>(i)] + 1;
      if (next_cost < cost[static_cast<std::size_t>(j)]) {
        cost[static_cast<std::size_t>(j)] = next_cost;
        previous[static_cast<std::size_t>(j)] = i;
      }
    }
  }
  if (cost[static_cast<std::size_t>(n - 1)] >= kInf) {
    return {};
  }

  std::vector<int> out;
  for (int at = n - 1; at >= 0; at = previous[static_cast<std::size_t>(at)]) {
    out.push_back(candidates[static_cast<std::size_t>(at)]);
    if (at == 0) {
      break;
    }
  }
  std::reverse(out.begin(), out.end());
  return out;
}

std::vector<int> SimplifySourceVertexIntervals(
    const std::vector<DensePoint>& dense,
    const std::vector<int>& source_to_dense,
    const std::vector<int>& required,
    const std::vector<bool>& tangent_locked_source_vertices,
    bool closed,
    double tolerance) {
  if (dense.empty() || required.empty()) {
    return {};
  }
  std::vector<int> required_sorted = UniqueSortedIndices(required);
  if (!closed && required_sorted.size() < 2) {
    return {};
  }

  std::vector<int> kept;
  const int interval_count = closed ? static_cast<int>(required_sorted.size())
: std::max(0, static_cast<int>(required_sorted.size()) - 1);
  for (int interval = 0; interval < interval_count; ++interval) {
    const int begin = required_sorted[static_cast<std::size_t>(interval)];
    const int end = closed
        ? required_sorted[static_cast<std::size_t>((interval + 1) % required_sorted.size())]
: required_sorted[static_cast<std::size_t>(interval + 1)];
    std::vector<int> candidates = BuildForwardSourceVertexCandidates(
        source_to_dense, static_cast<int>(dense.size()), begin, end, closed);
    std::vector<int> interval_kept = SimplifyCandidateIntervalDp(
        dense, candidates, tangent_locked_source_vertices, closed, tolerance);
    if (interval_kept.empty()) {
      return {};
    }
    if (!kept.empty() && !interval_kept.empty() && kept.back() == interval_kept.front()) {
      interval_kept.erase(interval_kept.begin());
    }
    kept.insert(kept.end(), interval_kept.begin(), interval_kept.end());
  }
  return UniqueSortedIndices(std::move(kept));
}

}  // namespace

std::vector<int> SimplifyDensePolyline(const std::vector<DensePoint>& dense,
                                       const std::vector<int>& source_to_dense,
                                       const std::vector<bool>& required_source_vertices,
                                       const std::vector<bool>& tangent_locked_source_vertices,
                                       bool closed,
                                       double tolerance) {
  std::vector<int> required;
  if (dense.empty()) {
    return required;
  }
  required.push_back(0);
  if (!closed) {
    required.push_back(static_cast<int>(dense.size()) - 1);
  }
  for (std::size_t source_index = 0; source_index < source_to_dense.size(); ++source_index) {
    if (source_index < required_source_vertices.size() && required_source_vertices[source_index] &&
        source_to_dense[source_index] >= 0) {
      required.push_back(source_to_dense[source_index]);
    }
  }
  required = UniqueSortedIndices(std::move(required));

  if (closed && required.size() == 1 && dense.size() > 1) {
    int farthest = -1;
    double max_distance = -1.0;
    for (std::size_t i = 1; i < dense.size(); ++i) {
      const double distance = pff_geom::Distance(dense.front().p, dense[i].p);
      if (distance > max_distance) {
        max_distance = distance;
        farthest = static_cast<int>(i);
      }
    }
    if (farthest > 0) {
      required.push_back(farthest);
      required = UniqueSortedIndices(std::move(required));
    }
  }

  std::vector<bool> keep(dense.size(), false);
  for (int index: required) {
    if (index >= 0 && index < static_cast<int>(keep.size())) {
      keep[static_cast<std::size_t>(index)] = true;
    }
  }

  if (closed) {
    for (std::size_t i = 0; i < required.size(); ++i) {
      CubicMarkKept(dense,
                    required[i],
                    required[(i + 1) % required.size()],
                    tolerance,
                    tangent_locked_source_vertices,
                    closed,
                    keep);
    }
  } else {
    for (std::size_t i = 0; i + 1 < required.size(); ++i) {
      CubicMarkKept(dense,
                    required[i],
                    required[i + 1],
                    tolerance,
                    tangent_locked_source_vertices,
                    closed,
                    keep);
    }
  }

  std::vector<int> kept;
  for (std::size_t i = 0; i < keep.size(); ++i) {
    if (!keep[i]) {
      continue;
    }
    kept.push_back(static_cast<int>(i));
  }
  kept = UniqueSortedIndices(std::move(kept));

  const std::vector<int> dp_kept = SimplifySourceVertexIntervals(
      dense, source_to_dense, required, tangent_locked_source_vertices, closed, tolerance);
  if (!dp_kept.empty() && dp_kept.size() < kept.size()) {
    return dp_kept;
  }
  return kept;
}

std::vector<int> EnsureMinimumKept(const std::vector<DensePoint>& dense,
                                   std::vector<int> kept,
                                   bool closed) {
  const int min_count = closed ? std::min(3, static_cast<int>(dense.size()))
: std::min(2, static_cast<int>(dense.size()));
  kept = UniqueSortedIndices(std::move(kept));
  while (static_cast<int>(kept.size()) < min_count) {
    double best_err = -1.0;
    int best_index = -1;
    std::vector<pff_geom::Point> kept_points;
    kept_points.reserve(kept.size());
    for (int index: kept) {
      kept_points.push_back(dense[static_cast<std::size_t>(index)].p);
    }
    for (std::size_t i = 0; i < dense.size(); ++i) {
      if (std::find(kept.begin(), kept.end(), static_cast<int>(i)) != kept.end()) {
        continue;
      }
      double err = std::numeric_limits<double>::infinity();
      if (!kept_points.empty()) {
        err = DirectedPolylineDistance({dense[i].p}, kept_points, closed, nullptr);
      }
      if (err > best_err) {
        best_err = err;
        best_index = static_cast<int>(i);
      }
    }
    if (best_index < 0) {
      break;
    }
    kept.push_back(best_index);
    kept = UniqueSortedIndices(std::move(kept));
  }
  return kept;
}

}  // namespace pff_decimate
}  // namespace bbsolver
