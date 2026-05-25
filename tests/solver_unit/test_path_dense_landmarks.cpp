#include "bbsolver/path/dense/path_dense_landmarks.hpp"

#include <cassert>
#include <cstddef>
#include <vector>

#include "bbsolver/path/dense/path_dense_polyline.hpp"

namespace {

using bbsolver::DensePoint;
using bbsolver::pff_landmarks::SampleDenseAtArc;
using bbsolver::pff_landmarks::SampledArcPoint;

std::vector<DensePoint> SquareDense() {
  return {
      {{0.0, 0.0}, 0},
      {{1.0, 0.0}, 1},
      {{1.0, 1.0}, 2},
      {{0.0, 1.0}, 3},
  };
}

void TestSampleEmptyDense() {
  const SampledArcPoint sampled = SampleDenseAtArc({}, true, 0.5, 4.0);
  assert(sampled.arc == 0.0);
  assert(sampled.dense.source_vertex_index == -1);
}

void TestSampleZeroPerimeterReturnsFirst() {
  const std::vector<DensePoint> single = {{{0.0, 0.0}, 0}};
  const SampledArcPoint sampled = SampleDenseAtArc(single, false, 0.5, 0.0);
  assert(sampled.arc == 0.0);
  assert(sampled.dense.source_vertex_index == 0);
}

void TestSampleClosedSeamSnap() {
  const std::vector<DensePoint> dense = SquareDense();
  // Closed perimeter is 4.0; arc=4.0 wraps to arc=0.0 (seam snap).
  const SampledArcPoint at_seam = SampleDenseAtArc(dense, true, 4.0, 4.0);
  assert(at_seam.arc == 0.0);
  // arc=1.0 lands exactly at vertex 1.
  const SampledArcPoint at_v1 = SampleDenseAtArc(dense, true, 1.0, 4.0);
  assert(std::abs(at_v1.arc - 1.0) < 1e-8);
  assert(at_v1.dense.source_vertex_index == 1);
  // arc=0.5 is midway on the first edge: between source vertex 0 and 1,
  // so source_vertex_index is -1 (synthesized point).
  const SampledArcPoint mid = SampleDenseAtArc(dense, true, 0.5, 4.0);
  assert(mid.dense.source_vertex_index == -1);
  assert(std::abs(mid.dense.p.x - 0.5) < 1e-9);
}

void TestSampleOpenClamps() {
  const std::vector<DensePoint> dense = SquareDense();
  // Open: arc beyond perimeter clamps to end vertex.
  const SampledArcPoint clamped = SampleDenseAtArc(dense, false, 100.0, 4.0);
  assert(clamped.arc == 4.0);
  assert(clamped.dense.source_vertex_index == 3);  // last
  // Open: negative arc clamps to 0 -> first vertex.
  const SampledArcPoint zero = SampleDenseAtArc(dense, false, -10.0, 4.0);
  assert(zero.arc == 0.0);
  assert(zero.dense.source_vertex_index == 0);
}

void TestBuildLandmarksEmpty() {
  std::vector<DensePoint> combined;
  std::vector<int> kept;
  assert(!BuildDenseWithFractionLandmarks({}, {0.5}, true, &combined, &kept));
  assert(combined.empty());
  assert(kept.empty());
  assert(!BuildDenseWithFractionLandmarks(SquareDense(), {}, true, &combined, &kept));
  assert(combined.empty());
  assert(kept.empty());
}

void TestBuildLandmarksHappyPath() {
  const std::vector<DensePoint> dense = SquareDense();
  std::vector<DensePoint> combined;
  std::vector<int> kept;
  // Insert 4 fractions at the corners: 0.0, 0.25, 0.5, 0.75 of unit perimeter.
  const std::vector<double> fractions = {0.0, 0.25, 0.5, 0.75};
  assert(BuildDenseWithFractionLandmarks(dense, fractions, true, &combined, &kept));
  // kept must be strictly increasing.
  assert(kept.size() == 4);
  for (std::size_t i = 1; i < kept.size(); ++i) {
    assert(kept[i] > kept[i - 1]);
  }
  // Combined size includes both source vertices and landmarks (deduped at
  // overlapping arcs). For corner-aligned fractions on a unit square,
  // each landmark coincides with a source vertex, so combined.size() == 4.
  assert(combined.size() == 4);
}

void TestBuildLandmarksMidEdgeInsertion() {
  const std::vector<DensePoint> dense = SquareDense();
  std::vector<DensePoint> combined;
  std::vector<int> kept;
  // 0.125 is mid-first-edge -> distinct landmark inserted between source 0 and 1.
  const std::vector<double> fractions = {0.125, 0.5};
  assert(BuildDenseWithFractionLandmarks(dense, fractions, true, &combined, &kept));
  assert(kept.size() == 2);
  assert(combined.size() == 5);  // 4 source + 1 distinct mid-edge insertion
}

void TestBuildLandmarksRejectsNonMonotone() {
  const std::vector<DensePoint> dense = SquareDense();
  std::vector<DensePoint> combined;
  std::vector<int> kept;
  // Two fractions that round to the same arc-bucket and would yield non-
  // monotone kept indices.
  const std::vector<double> fractions = {0.5, 0.5};
  assert(!BuildDenseWithFractionLandmarks(dense, fractions, true, &combined, &kept));
}

}  // namespace

int main() {
  TestSampleEmptyDense();
  TestSampleZeroPerimeterReturnsFirst();
  TestSampleClosedSeamSnap();
  TestSampleOpenClamps();
  TestBuildLandmarksEmpty();
  TestBuildLandmarksHappyPath();
  TestBuildLandmarksMidEdgeInsertion();
  TestBuildLandmarksRejectsNonMonotone();
  return 0;
}
