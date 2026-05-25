#include "bbsolver/path/multimode/path_multimode_solver_notes.hpp"
#include "bbsolver/path/multimode/path_multimode_geometry.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    std::abort();
  }
}

std::vector<bbsolver::path_multimode::VertexRegion> Regions() {
  return {{0, 2}, {2, 4}};
}

void TestBudgetNotes() {
  const std::vector<bbsolver::path_multimode::VertexRegion> regions =
      Regions();
  Require(bbsolver::path_multimode::MultiModeRegionBudgetExceededNote(
              17, 16, regions) ==
              "shape_multimode_region_budget_exceeded; "
              "region_segment_checks=17; max_region_segment_checks=16; "
              "regions=2; region_ranges=0-2,2-4",
          "region-budget note must preserve fields and order");

  Require(bbsolver::path_multimode::MultiModeValidationBudgetExceededNote(
              5, 9, 8, 144, 128, regions) ==
              "shape_multimode_validation_budget_exceeded; union_keys=5; "
              "source_samples=9; max_validation_samples=8; "
              "validation_work_units=144; max_validation_work_units=128; "
              "regions=2; region_ranges=0-2,2-4",
          "validation-budget note must preserve fields and order");

  Require(bbsolver::path_multimode::MultiModeCandidateKeyBudgetExceededNote(
              6,
              9,
              0.5,
              regions,
              "shape_multimode_recombined_region_temporal=not_selected") ==
              "shape_multimode_candidate_key_budget_exceeded; union_keys=6; "
              "source_samples=9; max_candidate_key_ratio=0.500000; "
              "regions=2; region_ranges=0-2,2-4; "
              "shape_multimode_recombined_region_temporal=not_selected",
          "candidate-budget note must append recombined note when present");

  Require(bbsolver::path_multimode::MultiModeCandidateKeyBudgetExceededNote(
              6, 9, 0.5, regions, "") ==
              "shape_multimode_candidate_key_budget_exceeded; union_keys=6; "
              "source_samples=9; max_candidate_key_ratio=0.500000; "
              "regions=2; region_ranges=0-2,2-4",
          "candidate-budget note must omit empty recombined note");
}

void TestCandidateNote() {
  const std::string note =
      bbsolver::path_multimode::MultiModeCandidateNote(
          Regions(),
          3,
          {2, 4},
          17,
          144,
          5,
          {0, 2, 4},
          "ok",
          "shape_multimode_recombined_region_temporal=not_selected");
  Require(note ==
              "shape_multimode_candidate; regions=2; "
              "region_ranges=0-2,2-4; max_gap_samples=3; "
              "region_key_counts=2,4; region_segment_checks=17; "
              "validation_work_units=144; union_keys=5; "
              "union_anchors=0,2,4; source_outline_validation: ok; "
              "shape_multimode_recombined_region_temporal=not_selected",
          "candidate note must preserve field order and recombined suffix");
}

}  // namespace

int main() {
  TestBudgetNotes();
  TestCandidateNote();
  std::cout << "[PASS] test_path_multimode_solver_notes\n";
  return 0;
}
