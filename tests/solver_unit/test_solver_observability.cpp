#include "bbsolver/solve/solver_observability.hpp"
#include "bbsolver/domain.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    std::abort();
  }
}

void TestPropertyNameUsesUnnamedSentinelForEmptyId() {
  bbsolver::PropertySamples property_samples;
  property_samples.property.id.clear();
  Require(std::string(bbsolver::PropertyName(property_samples)) == "<unnamed>",
          "empty property id must use unnamed sentinel");
}

void TestPropertyNameUsesNonEmptyIdString() {
  bbsolver::PropertySamples property_samples;
  property_samples.property.id = "Layer/Transform/Position";
  const char* name = bbsolver::PropertyName(property_samples);
  Require(std::string(name) == property_samples.property.id,
          "non-empty property id must be returned as property name");
}

void TestPropertyNameReturnsPropertyIdPointer() {
  bbsolver::PropertySamples property_samples;
  property_samples.property.id = "Layer/Path";
  const char* name = bbsolver::PropertyName(property_samples);
  Require(name == property_samples.property.id.c_str(),
          "non-empty property name must resolve to property id storage");
}

void TestMillisecondsSincePastStartIsNonNegative() {
  const auto start =
      std::chrono::steady_clock::now() - std::chrono::milliseconds(5);
  const double elapsed_ms = bbsolver::MillisecondsSince(start);
  Require(elapsed_ms >= 0.0,
          "elapsed milliseconds from a past start must be non-negative");
  Require(elapsed_ms < 1000.0,
          "elapsed milliseconds test should stay within a tolerant bound");
}

}  // namespace

int main() {
  TestPropertyNameUsesUnnamedSentinelForEmptyId();
  TestPropertyNameUsesNonEmptyIdString();
  TestPropertyNameReturnsPropertyIdPointer();
  TestMillisecondsSincePastStartIsNonNegative();
  std::cout << "[PASS] test_solver_observability\n";
  return 0;
}
