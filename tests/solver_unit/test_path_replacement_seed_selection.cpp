#include "bbsolver/path/replacement/path_replacement_seed_selection.hpp"
#include "bbsolver/path/replacement/path_replacement_fraction_layout.hpp"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

bbsolver::ReplacementFrameFitRecord Record(int fraction_count,
                                           double max_outline_error) {
  bbsolver::ReplacementFrameFitRecord record;
  record.max_outline_error = max_outline_error;
  record.outline_fractions.assign(static_cast<std::size_t>(fraction_count), 0.0);
  return record;
}

void TestSelectsFirstMiddleLastAndWorstWithoutDuplicates() {
  const std::vector<int> seeds =
      bbsolver::SelectReplacementPhase2SeedIndices(
          {
              Record(4, 1.0),
              Record(4, 2.0),
              Record(4, 3.0),
              Record(4, 8.0),
              Record(4, 5.0),
          },
          4);

  Require((seeds == std::vector<int>{0, 2, 4, 3}),
          "seed order should be first, middle, last, worst-error");
}

void TestSingleRecordDoesNotDuplicateSeeds() {
  const std::vector<int> seeds =
      bbsolver::SelectReplacementPhase2SeedIndices({Record(3, 4.0)}, 3);

  Require((seeds == std::vector<int>{0}),
          "single record should appear once");
}

void TestSkipsRecordsWithMismatchedFractionCount() {
  const std::vector<int> seeds =
      bbsolver::SelectReplacementPhase2SeedIndices(
          {Record(2, 10.0), Record(4, 1.0), Record(2, 20.0)}, 4);

  Require((seeds == std::vector<int>{1}),
          "mismatched records should not be selected");
}

void TestEmptyRecordsReturnEmptySeeds() {
  Require(bbsolver::SelectReplacementPhase2SeedIndices({}, 4).empty(),
          "empty records should produce no seeds");
}

}  // namespace

int main() {
  TestSelectsFirstMiddleLastAndWorstWithoutDuplicates();
  TestSingleRecordDoesNotDuplicateSeeds();
  TestSkipsRecordsWithMismatchedFractionCount();
  TestEmptyRecordsReturnEmptySeeds();
  std::cout << "[PASS] test_path_replacement_seed_selection\n";
  return 0;
}
