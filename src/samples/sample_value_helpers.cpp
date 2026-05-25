#include "bbsolver/samples/sample_value_helpers.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace bbsolver {

std::vector<double> SampleVectorOrZeros(
    const PropertySamples& property_samples,
    const Sample& sample) {
  if (!sample.v.empty()) {
    return sample.v;
  }
  std::vector<double> values(
      static_cast<std::size_t>(std::max(property_samples.property.dimensions, 1)),
      0.0);
  return values;
}

}  // namespace bbsolver
