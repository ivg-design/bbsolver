#include "bbsolver/path/geometry/path_fraction_helpers.hpp"

#include <cassert>
#include <vector>

namespace {

using bbsolver::pff_fractions::FractionSegmentsByDescendingGap;
using bbsolver::pff_fractions::InsertSplitFraction;

void TestStrictSeamOrderRejectsEmptyNonFiniteOutOfRangeDuplicates() {
  assert(!FractionsInStrictSeamOrder({}, true));
  assert(!FractionsInStrictSeamOrder({std::numeric_limits<double>::quiet_NaN()}, false));
  // Closed paths must stay in [0, 1).
  assert(!FractionsInStrictSeamOrder({0.0, 1.0}, true));
  // Open paths may include both endpoints.
  assert(FractionsInStrictSeamOrder({0.0, 0.5, 1.0}, false));
  // Reject duplicates (the strict-order tolerance is kFractionEpsilon * 0.001).
  assert(!FractionsInStrictSeamOrder({0.0, 0.25, 0.25}, true));
  // Strictly increasing closed layout.
  assert(FractionsInStrictSeamOrder({0.0, 0.25, 0.5, 0.75}, true));
}

void TestFractionSegmentCountClosedVsOpen() {
  assert(FractionSegmentCount({}, true) == 0);
  assert(FractionSegmentCount({}, false) == 0);
  assert(FractionSegmentCount({0.0}, false) == 0);  // open path with 1 vertex has 0 segments
  assert(FractionSegmentCount({0.0, 0.5}, false) == 1);
  // Closed wraps: N fractions == N segments
  assert(FractionSegmentCount({0.0, 0.25, 0.5, 0.75}, true) == 4);
  assert(FractionSegmentCount({0.0, 0.25, 0.5, 0.75}, false) == 3);
}

void TestFractionGapHandlesClosedSeamWraparound() {
  const std::vector<double> closed_quad = {0.0, 0.25, 0.5, 0.75};
  // Every closed segment is 0.25 wide, including the seam-spanning last one.
  assert(std::abs(FractionGap(closed_quad, 0, true) - 0.25) < 1e-12);
  assert(std::abs(FractionGap(closed_quad, 1, true) - 0.25) < 1e-12);
  assert(std::abs(FractionGap(closed_quad, 2, true) - 0.25) < 1e-12);
  assert(std::abs(FractionGap(closed_quad, 3, true) - 0.25) < 1e-12);  // 0.75 -> 1.0 (wraps to 0.0)

  // Out-of-range segment index returns 0 instead of throwing.
  assert(FractionGap(closed_quad, -1, true) == 0.0);
  assert(FractionGap(closed_quad, 4, true) == 0.0);

  // Open path: only N-1 gaps; the seam-spanning gap does not exist.
  assert(std::abs(FractionGap(closed_quad, 0, false) - 0.25) < 1e-12);
  assert(std::abs(FractionGap(closed_quad, 2, false) - 0.25) < 1e-12);
  assert(FractionGap(closed_quad, 3, false) == 0.0);
}

void TestLargestFractionGapSegmentPicksBiggest() {
  const std::vector<double> uneven_closed = {0.0, 0.1, 0.2, 0.9};
  // Closed gaps: 0.1, 0.1, 0.7, 0.1 -> segment 2 is biggest.
  assert(LargestFractionGapSegment(uneven_closed, true) == 2);
  // Open: 0.1, 0.1, 0.7 -> segment 2 still wins.
  assert(LargestFractionGapSegment(uneven_closed, false) == 2);
}

void TestFractionSegmentsByDescendingGapStableSort() {
  // Equal-gap closed quad: stable sort preserves segment order 0,1,2,3.
  const std::vector<double> closed_quad = {0.0, 0.25, 0.5, 0.75};
  const std::vector<int> all = FractionSegmentsByDescendingGap(closed_quad, true, 0);
  assert(all == std::vector<int>({0, 1, 2, 3}));

  // max_segments truncates.
  const std::vector<int> top2 = FractionSegmentsByDescendingGap(closed_quad, true, 2);
  assert(top2 == std::vector<int>({0, 1}));

  // Uneven gaps: descending order.
  const std::vector<double> uneven = {0.0, 0.1, 0.2, 0.9};
  const std::vector<int> sorted = FractionSegmentsByDescendingGap(uneven, true, 0);
  assert(sorted.front() == 2);  // biggest gap
}

void TestInsertFractionValueClampsAndDedups() {
  std::vector<double> closed = {0.0, 0.5};
  // Insertion at 0.25 succeeds and keeps strict order.
  assert(InsertFractionValue(&closed, 0.25, true));
  assert(closed == std::vector<double>({0.0, 0.25, 0.5}));
  // Duplicate within kFractionEpsilon rejected.
  assert(!InsertFractionValue(&closed, 0.25 + kFractionEpsilon * 0.1, true));
  assert(closed.size() == 3);

  // Closed wraparound: 1.25 -> 0.25 (already present, rejected).
  assert(!InsertFractionValue(&closed, 1.25, true));

  // Non-finite rejected.
  assert(!InsertFractionValue(&closed, std::numeric_limits<double>::quiet_NaN(), true));
  // Null pointer rejected.
  assert(!InsertFractionValue(nullptr, 0.5, true));

  // Open path out-of-range rejected.
  std::vector<double> open = {0.0, 1.0};
  assert(!InsertFractionValue(&open, 1.5, false));
  // Open path in-range accepted.
  assert(InsertFractionValue(&open, 0.5, false));
  assert(open == std::vector<double>({0.0, 0.5, 1.0}));
}

void TestInsertSplitFractionMidpointHandlesSeamWraparound() {
  std::vector<double> closed = {0.0, 0.5};
  // Split segment 1 (closed): midpoint between 0.5 and 1.0 (wraps to 0.0) is 0.75.
  InsertSplitFraction(&closed, 1, true);
  assert(closed.size() == 3);
  assert(std::abs(closed[2] - 0.75) < 1e-12);

  // Split segment 0: midpoint between 0.0 and 0.5 is 0.25.
  std::vector<double> closed2 = {0.0, 0.5};
  InsertSplitFraction(&closed2, 0, true);
  assert(closed2.size() == 3);
  assert(std::abs(closed2[1] - 0.25) < 1e-12);

  // Out-of-range segment index is a no-op.
  std::vector<double> noop = {0.0, 0.5};
  InsertSplitFraction(&noop, 5, true);
  assert(noop.size() == 2);
  InsertSplitFraction(nullptr, 0, true);  // null is no-op
}

void TestFractionDistanceClosedTakesShorterWayAroundSeam() {
  // Closed: distance between 0.1 and 0.9 is min(0.8, 0.2) = 0.2 (across the seam).
  assert(std::abs(FractionDistance(0.1, 0.9, true) - 0.2) < 1e-12);
  // Open: just direct distance = 0.8.
  assert(std::abs(FractionDistance(0.1, 0.9, false) - 0.8) < 1e-12);
  // Same fraction.
  assert(FractionDistance(0.5, 0.5, true) == 0.0);
  // Reversed args.
  assert(std::abs(FractionDistance(0.9, 0.1, true) - 0.2) < 1e-12);
}

void TestNormalizeOutlineFractionsClampsAndRejects() {
  std::vector<double> out;

  // Closed: 1.0 wraps to 0.0, then snap-to-seam.
  assert(NormalizeOutlineFractions({0.0, 0.25, 0.5, 0.75}, true, &out));
  assert(out == std::vector<double>({0.0, 0.25, 0.5, 0.75}));

  // Closed: 1.25 wraps to 0.25; result must reject because previous was 0.5 (out of order).
  assert(!NormalizeOutlineFractions({0.0, 0.5, 1.25}, true, &out));

  // Open: out-of-range rejected.
  assert(!NormalizeOutlineFractions({-0.1, 0.5}, false, &out));
  assert(!NormalizeOutlineFractions({0.5, 1.1}, false, &out));

  // Open: clamp tiny epsilon overshoot.
  assert(NormalizeOutlineFractions({0.0, 0.5, 1.0 + kFractionEpsilon * 0.5}, false, &out));
  assert(out.size() == 3);
  assert(std::abs(out.back() - 1.0) < 1e-12);

  // Non-finite rejected.
  assert(!NormalizeOutlineFractions({std::numeric_limits<double>::infinity()}, true, &out));

  // Duplicate rejected.
  assert(!NormalizeOutlineFractions({0.0, 0.5, 0.5}, true, &out));
}

}  // namespace

int main() {
  TestStrictSeamOrderRejectsEmptyNonFiniteOutOfRangeDuplicates();
  TestFractionSegmentCountClosedVsOpen();
  TestFractionGapHandlesClosedSeamWraparound();
  TestLargestFractionGapSegmentPicksBiggest();
  TestFractionSegmentsByDescendingGapStableSort();
  TestInsertFractionValueClampsAndDedups();
  TestInsertSplitFractionMidpointHandlesSeamWraparound();
  TestFractionDistanceClosedTakesShorterWayAroundSeam();
  TestNormalizeOutlineFractionsClampsAndRejects();
  return 0;
}
