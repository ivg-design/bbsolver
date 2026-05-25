#include "bbsolver/path/frame_fit/path_frame_fit_decimate.hpp"

#include <cassert>
#include <cstddef>
#include <vector>
#include <utility>

#include "bbsolver/path/dense/path_dense_polyline.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit_geometry.hpp"

namespace {

using bbsolver::DensePoint;
using bbsolver::pff_decimate::EnsureMinimumKept;
using bbsolver::pff_decimate::SimplifyDensePolyline;

// Equally-spaced points along a unit square's perimeter, with all source
// vertices flagged at corners. 16 dense points; corners at dense indices
// 0/4/8/12 (source 0/1/2/3).
std::vector<DensePoint> SquareDense16(std::vector<int>& source_to_dense) {
  source_to_dense.assign(4, -1);
  std::vector<DensePoint> dense;
  dense.reserve(16);
  for (int edge = 0; edge < 4; ++edge) {
    for (int step = 0; step < 4; ++step) {
      const double t = static_cast<double>(step) / 4.0;
      bbsolver::pff_geom::Point p{0.0, 0.0};
      switch (edge) {
        case 0:  p = {t, 0.0};         break;
        case 1:  p = {1.0, t};         break;
        case 2:  p = {1.0 - t, 1.0};   break;
        case 3:  p = {0.0, 1.0 - t};   break;
      }
      const int source = (step == 0) ? edge : -1;
      if (source >= 0) {
        source_to_dense[static_cast<std::size_t>(source)] = static_cast<int>(dense.size());
      }
      dense.push_back({p, source});
    }
  }
  return dense;
}

void TestSimplifyEmptyReturnsEmpty() {
  std::vector<int> source_to_dense;
  const std::vector<bool> required = {true, true, true, true};
  const std::vector<bool> locked = {true, true, true, true};
  const std::vector<int> kept = SimplifyDensePolyline({}, source_to_dense, required, locked, true, 0.5);
  assert(kept.empty());
}

void TestSimplifyClosedSquareKeepsCorners() {
  std::vector<int> source_to_dense;
  const std::vector<DensePoint> dense = SquareDense16(source_to_dense);
  // All 4 source corners marked required + tangent-locked.
  const std::vector<bool> required(4, true);
  const std::vector<bool> locked(4, true);
  const std::vector<int> kept = SimplifyDensePolyline(
      dense, source_to_dense, required, locked, /*closed=*/true, /*tolerance=*/0.01);
  // At minimum the 4 corner dense indices (0, 4, 8, 12) survive.
  assert(kept.size() >= 4);
  // Corners must all be in `kept`.
  for (int corner : {0, 4, 8, 12}) {
    bool found = false;
    for (int k : kept) {
      if (k == corner) {
        found = true;
        break;
      }
    }
    assert(found);
  }
}

void TestEnsureMinimumKeptHonorsClosed() {
  // Closed path with at least 3 dense points and an initially-empty kept
  // list should grow back to 3 entries.
  std::vector<int> source_to_dense;
  const std::vector<DensePoint> dense = SquareDense16(source_to_dense);
  std::vector<int> kept;
  kept = EnsureMinimumKept(dense, std::move(kept), /*closed=*/true);
  assert(kept.size() == 3);
  // Strictly increasing dense indices.
  for (std::size_t i = 1; i < kept.size(); ++i) {
    assert(kept[i] > kept[i - 1]);
  }
}

void TestEnsureMinimumKeptHonorsOpen() {
  std::vector<int> source_to_dense;
  const std::vector<DensePoint> dense = SquareDense16(source_to_dense);
  std::vector<int> kept;
  kept = EnsureMinimumKept(dense, std::move(kept), /*closed=*/false);
  // Open path minimum is 2.
  assert(kept.size() == 2);
  assert(kept[1] > kept[0]);
}

void TestEnsureMinimumKeptIdempotentWhenAlreadyLarge() {
  std::vector<int> source_to_dense;
  const std::vector<DensePoint> dense = SquareDense16(source_to_dense);
  std::vector<int> kept = {0, 4, 8, 12};
  const std::vector<int> after = EnsureMinimumKept(dense, kept, /*closed=*/true);
  assert(after.size() == 4);
  for (std::size_t i = 0; i < 4; ++i) {
    assert(after[i] == kept[i]);
  }
}

}  // namespace

int main() {
  TestSimplifyEmptyReturnsEmpty();
  TestSimplifyClosedSquareKeepsCorners();
  TestEnsureMinimumKeptHonorsClosed();
  TestEnsureMinimumKeptHonorsOpen();
  TestEnsureMinimumKeptIdempotentWhenAlreadyLarge();
  return 0;
}
