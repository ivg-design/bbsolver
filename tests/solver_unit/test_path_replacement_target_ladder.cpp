#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

#include <cassert>
#include <vector>
#include <cstddef>

namespace {

void TestEmptyOnInvalidInputs() {
  bbsolver::PathReplacementTargetLadderOptions opts;
  // initial <= 0
  assert(bbsolver::BuildShapeFlatReplacementTargetLadder(0, 10, opts).empty());
  assert(bbsolver::BuildShapeFlatReplacementTargetLadder(-1, 10, opts).empty());
  // source_min <= 1 leaves no room to reduce.
  assert(bbsolver::BuildShapeFlatReplacementTargetLadder(5, 1, opts).empty());
  assert(bbsolver::BuildShapeFlatReplacementTargetLadder(5, 0, opts).empty());
}

void TestLadderStaysBelowSourceMin() {
  bbsolver::PathReplacementTargetLadderOptions opts;
  // Defaults: step=2, max_candidate_targets=4, include_source_min_minus_one=true
  // initial=22, source_min=28 -> max_target=27, ladder: 22,24,26,27
  const std::vector<int> ladder =
      bbsolver::BuildShapeFlatReplacementTargetLadder(22, 28, opts);
  assert(!ladder.empty());
  for (int target : ladder) {
    assert(target < 28);  // strict reduction over source_min
    assert(target >= 22);  // never below initial
  }
  // Ladder is strictly increasing.
  for (std::size_t i = 1; i < ladder.size(); ++i) {
    assert(ladder[i] > ladder[i - 1]);
  }
}

void TestMaxCandidateTargetsCaps() {
  bbsolver::PathReplacementTargetLadderOptions opts;
  opts.max_candidate_targets = 2;
  opts.step_vertices = 1;
  opts.include_source_min_minus_one = false;
  // initial=10, source_min=20 -> max_target=19, max 2 candidates: {10, 11}
  const std::vector<int> ladder =
      bbsolver::BuildShapeFlatReplacementTargetLadder(10, 20, opts);
  assert(ladder.size() == 2);
  assert(ladder[0] == 10);
  assert(ladder[1] == 11);
}

void TestIncludeSourceMinMinusOneAppendsCap() {
  bbsolver::PathReplacementTargetLadderOptions opts;
  opts.step_vertices = 5;
  opts.max_candidate_targets = 4;
  opts.include_source_min_minus_one = true;
  // initial=10, source_min=20 -> max_target=19; step=5: 10,15 then cap-append 19.
  const std::vector<int> ladder =
      bbsolver::BuildShapeFlatReplacementTargetLadder(10, 20, opts);
  assert(!ladder.empty());
  assert(ladder.back() == 19);
}

void TestInitialAboveMaxTargetReturnsEmpty() {
  bbsolver::PathReplacementTargetLadderOptions opts;
  // initial 30 vs source_min 10 -> max_target=9, initial already exceeds it.
  const std::vector<int> ladder =
      bbsolver::BuildShapeFlatReplacementTargetLadder(30, 10, opts);
  assert(ladder.empty());
}

void TestMinTargetClampsInitial() {
  bbsolver::PathReplacementTargetLadderOptions opts;
  opts.min_target_vertices = 8;
  opts.step_vertices = 1;
  opts.max_candidate_targets = 3;
  opts.include_source_min_minus_one = false;
  // initial=3, min=8, source_min=20 -> ladder starts at 8: {8,9,10}.
  const std::vector<int> ladder =
      bbsolver::BuildShapeFlatReplacementTargetLadder(3, 20, opts);
  assert(ladder.size() == 3);
  assert(ladder.front() == 8);
}

void TestMaxTargetClampsCap() {
  bbsolver::PathReplacementTargetLadderOptions opts;
  opts.max_target_vertices = 12;
  opts.step_vertices = 1;
  opts.max_candidate_targets = 50;
  opts.include_source_min_minus_one = false;
  // initial=10, source_min=100 -> max_target=12 (clamped from 99): {10,11,12}.
  const std::vector<int> ladder =
      bbsolver::BuildShapeFlatReplacementTargetLadder(10, 100, opts);
  assert(ladder.size() == 3);
  assert(ladder.back() == 12);
}

void TestRetryTargetLadderUsesConfigBoundsAndWiderBudget() {
  const std::vector<int> ladder =
      bbsolver::BuildShapeFlatReplacementRetryTargetLadder(
          10, 30, 14, 24);
  assert(!ladder.empty());
  assert(ladder.front() == 14);
  assert(ladder.back() == 24);
  assert(ladder.size() <= 10);
  for (int target : ladder) {
    assert(target >= 14);
    assert(target <= 24);
    assert(target < 30);
  }
}

}  // namespace

int main() {
  TestEmptyOnInvalidInputs();
  TestLadderStaysBelowSourceMin();
  TestMaxCandidateTargetsCaps();
  TestIncludeSourceMinMinusOneAppendsCap();
  TestInitialAboveMaxTargetReturnsEmpty();
  TestMinTargetClampsInitial();
  TestMaxTargetClampsCap();
  TestRetryTargetLadderUsesConfigBoundsAndWiderBudget();
  return 0;
}
