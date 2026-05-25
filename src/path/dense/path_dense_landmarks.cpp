// Implements the arc-length sampling + landmark insertion helpers declared
// in path_dense_landmarks.hpp. Byte-faithful with the previous anonymous-
// namespace definitions in path_frame_fit.cpp.
//
// Diagnostics decision: **none / pure layout**. Pure geometric arithmetic.
// No DiagnosticsWriter, no progress events, no cancellation, no operator
// state. Caller-owned diagnostics; failure is a `false` return or a sentinel
// SampledArcPoint.

#include "bbsolver/path/dense/path_dense_landmarks.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "bbsolver/path/dense/path_dense_polyline.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit_geometry.hpp"

namespace bbsolver {
namespace pff_landmarks {

SampledArcPoint SampleDenseAtArc(const std::vector<DensePoint>& dense,
                                 bool closed,
                                 double arc,
                                 double total_length) {
  SampledArcPoint sampled;
  if (dense.empty()) {
    return sampled;
  }
  const int n = static_cast<int>(dense.size());
  if (!(total_length > 1e-9)) {
    sampled.arc = 0.0;
    sampled.dense = dense.front();
    return sampled;
  }
  if (closed) {
    arc = arc - std::floor(arc / total_length) * total_length;
    if (arc >= total_length - 1e-8 || arc < 1e-8) {
      arc = 0.0;
    }
  } else {
    arc = std::clamp(arc, 0.0, total_length);
  }

  double cursor = 0.0;
  const int segments = closed ? n : std::max(0, n - 1);
  for (int i = 0; i < segments; ++i) {
    const int next = (i + 1) % n;
    const pff_geom::Point a = dense[static_cast<std::size_t>(i)].p;
    const pff_geom::Point b = dense[static_cast<std::size_t>(next)].p;
    const double length = pff_geom::Distance(a, b);
    if (length <= 1e-12) {
      continue;
    }
    if (arc <= cursor + length + 1e-8) {
      const double local = std::clamp((arc - cursor) / length, 0.0, 1.0);
      sampled.arc = cursor + local * length;
      if (local <= 1e-8) {
        sampled.dense = dense[static_cast<std::size_t>(i)];
      } else if (local >= 1.0 - 1e-8) {
        sampled.dense = dense[static_cast<std::size_t>(next)];
      } else {
        sampled.dense = {pff_geom::Lerp(a, b, local), -1};
      }
      return sampled;
    }
    cursor += length;
  }

  sampled.arc = closed ? 0.0 : total_length;
  sampled.dense = closed ? dense.front() : dense.back();
  return sampled;
}

bool BuildDenseWithFractionLandmarks(const std::vector<DensePoint>& dense,
                                     const std::vector<double>& fractions,
                                     bool closed,
                                     std::vector<DensePoint>* combined,
                                     std::vector<int>* kept) {
  combined->clear();
  kept->clear();
  if (dense.empty() || fractions.empty()) {
    return false;
  }

  const double total_length = pff_dense::DensePerimeter(dense, closed);
  if (!(total_length > 1e-9)) {
    return false;
  }
  const std::vector<double> arcs = pff_dense::DenseArcPositions(dense, closed);

  std::vector<ArcLengthPoint> points;
  points.reserve(dense.size() + fractions.size());
  for (std::size_t i = 0; i < dense.size(); ++i) {
    points.push_back({arcs[i], dense[i], -1});
  }

  for (std::size_t i = 0; i < fractions.size(); ++i) {
    const double arc = fractions[i] * total_length;
    SampledArcPoint sampled = SampleDenseAtArc(dense, closed, arc, total_length);
    sampled.arc = closed && sampled.arc >= total_length - 1e-8 ? 0.0 : sampled.arc;
    points.push_back({sampled.arc, sampled.dense, static_cast<int>(i)});
  }

  std::stable_sort(points.begin(), points.end(), [](const ArcLengthPoint& a, const ArcLengthPoint& b) {
    if (std::abs(a.arc - b.arc) > 1e-8) {
      return a.arc < b.arc;
    }
    return a.request_index < b.request_index;
  });

  std::vector<int> request_to_index(fractions.size(), -1);
  std::vector<double> combined_arcs;
  combined_arcs.reserve(points.size());
  for (const ArcLengthPoint& point : points) {
    bool merged = false;
    if (!combined->empty() &&
        pff_geom::Distance(combined->back().p, point.dense.p) < 1e-8 &&
        std::abs(combined_arcs.back() - point.arc) < 1e-8) {
      merged = true;
      if (combined->back().source_vertex_index < 0 && point.dense.source_vertex_index >= 0) {
        combined->back().source_vertex_index = point.dense.source_vertex_index;
      }
    }
    if (!merged) {
      combined->push_back(point.dense);
      combined_arcs.push_back(point.arc);
    }
    const int combined_index = static_cast<int>(combined->size()) - 1;
    if (point.request_index >= 0) {
      request_to_index[static_cast<std::size_t>(point.request_index)] = combined_index;
    }
  }

  kept->reserve(fractions.size());
  for (int index : request_to_index) {
    if (index < 0 || (!kept->empty() && index <= kept->back())) {
      return false;
    }
    kept->push_back(index);
  }
  return true;
}

}  // namespace pff_landmarks
}  // namespace bbsolver
