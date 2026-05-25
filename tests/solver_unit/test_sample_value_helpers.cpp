#include "bbsolver/samples/sample_value_helpers.hpp"
#include "bbsolver/domain.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    std::abort();
  }
}

bbsolver::PropertySamples Property(int dimensions) {
  bbsolver::PropertySamples property;
  property.property.dimensions = dimensions;
  return property;
}

bbsolver::Sample SampleWithValue(std::vector<double> value) {
  bbsolver::Sample sample;
  sample.v = std::move(value);
  return sample;
}

void TestNonEmptyVectorReturnsExactValues() {
  const bbsolver::PropertySamples property = Property(5);
  const bbsolver::Sample sample = SampleWithValue({1.5, -2.0, 3.25});
  const std::vector<double> values =
      bbsolver::SampleVectorOrZeros(property, sample);
  Require(values == sample.v, "non-empty sample vector must be returned exactly");

  const bbsolver::PropertySamples small_dimension_property = Property(1);
  const bbsolver::Sample long_sample = SampleWithValue({1.0, 2.0, 3.0, 4.0});
  Require(bbsolver::SampleVectorOrZeros(small_dimension_property, long_sample) ==
              long_sample.v,
          "non-empty sample vector must not be truncated to property dimensions");
}

void TestEmptySampleUsesDimensionZeros() {
  const bbsolver::PropertySamples property = Property(3);
  const bbsolver::Sample sample;
  const std::vector<double> values =
      bbsolver::SampleVectorOrZeros(property, sample);
  Require(values == std::vector<double>({0.0, 0.0, 0.0}),
          "empty sample must use dimension-sized zero vector");
}

void TestZeroAndNegativeDimensionsClampToOneZero() {
  const bbsolver::Sample sample;
  Require(bbsolver::SampleVectorOrZeros(Property(0), sample) ==
              std::vector<double>({0.0}),
          "zero dimensions must clamp to one zero");
  Require(bbsolver::SampleVectorOrZeros(Property(-4), sample) ==
              std::vector<double>({0.0}),
          "negative dimensions must clamp to one zero");
}

void TestReturnedVectorsAreIndependentCopies() {
  const bbsolver::PropertySamples property = Property(2);
  bbsolver::Sample sample = SampleWithValue({4.0, 5.0});
  std::vector<double> values = bbsolver::SampleVectorOrZeros(property, sample);
  values[0] = 99.0;
  Require(sample.v == std::vector<double>({4.0, 5.0}),
          "returned non-empty vector must be independent of sample storage");

  const bbsolver::Sample empty_sample;
  std::vector<double> first =
      bbsolver::SampleVectorOrZeros(property, empty_sample);
  std::vector<double> second =
      bbsolver::SampleVectorOrZeros(property, empty_sample);
  first[0] = 99.0;
  Require(second == std::vector<double>({0.0, 0.0}),
          "returned zero vectors must be independent copies");
}

}  // namespace

int main() {
  TestNonEmptyVectorReturnsExactValues();
  TestEmptySampleUsesDimensionZeros();
  TestZeroAndNegativeDimensionsClampToOneZero();
  TestReturnedVectorsAreIndependentCopies();
  std::cout << "[PASS] test_sample_value_helpers\n";
  return 0;
}
