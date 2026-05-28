#include "bbsolver/path/replacement/path_replacement_fraction_layout.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void RequireNear(double actual, double expected, const char* message) {
  Require(std::abs(actual - expected) < 1e-9, message);
}

bbsolver::ReplacementFrameFitRecord Record(std::vector<double> fractions) {
  bbsolver::ReplacementFrameFitRecord record;
  record.outline_fractions = std::move(fractions);
  return record;
}

void TestMedianLayoutPinsSeamAndMedianSlots() {
  const std::vector<double> layout =
      bbsolver::BuildMedianStableFractionLayout(
          {
              Record({0.0, 0.20, 0.50, 0.80}),
              Record({0.0, 0.30, 0.60, 0.90}),
              Record({0.0, 0.25, 0.55, 0.85}),
          },
          4);

  Require(layout.size() == 4, "median layout should keep target count");
  RequireNear(layout[0], 0.0, "median layout must pin seam");
  RequireNear(layout[1], 0.25, "median layout should use middle slot median");
  RequireNear(layout[2], 0.55, "median layout should use middle slot median");
  RequireNear(layout[3], 0.85, "median layout should use middle slot median");
}

void TestMedianLayoutRejectsMismatchedOrUnorderedSlots() {
  Require(bbsolver::BuildMedianStableFractionLayout(
              {Record({0.0, 0.3}), Record({0.0, 0.4, 0.8})},
              3)
.empty(),
          "mismatched target counts must be rejected");
  Require(bbsolver::BuildMedianStableFractionLayout(
              {Record({0.0, 0.3, 0.3})},
              3)
.empty(),
          "non-increasing fraction slots must be rejected");
}

void TestMedianLayoutClampsTerminalSlotBelowOne() {
  const std::vector<double> layout =
      bbsolver::BuildMedianStableFractionLayout(
          {Record({0.0, 0.5, 1.0}), Record({0.0, 0.5, 1.0})},
          3);
  Require(layout.size() == 3, "terminal one layout should remain valid");
  Require(layout[2] < 1.0, "terminal fraction must be clamped below one");
  Require(layout[2] > 0.999, "terminal fraction should stay near one");
}

}  // namespace

int main() {
  TestMedianLayoutPinsSeamAndMedianSlots();
  TestMedianLayoutRejectsMismatchedOrUnorderedSlots();
  TestMedianLayoutClampsTerminalSlotBelowOne();
  std::cout << "[PASS] test_path_replacement_fraction_layout\n";
  return 0;
}
