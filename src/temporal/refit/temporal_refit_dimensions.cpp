#include "bbsolver/temporal/refit/temporal_refit_dimensions.hpp"

#include "bbsolver/domain.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace bbsolver {

bool TemporalRefitIsCustomProperty(const PropertySamples& source) {
  return source.property.kind == ValueKind::Custom;
}

std::size_t TemporalRefitExpectedDimensions(const PropertySamples& source) {
  return static_cast<std::size_t>(std::max(source.property.dimensions, 1));
}

bool TemporalRefitValuesMatchDimensions(const std::vector<double>& values,
                                        const PropertySamples& source) {
  return values.size() == TemporalRefitExpectedDimensions(source);
}

bool AllTemporalRefitCandidateKeysMatchDimensions(
    const PropertyKeys& keys,
    const PropertySamples& source) {
  if (keys.keys.empty()) {
    return false;
  }
  for (const Key& key: keys.keys) {
    if (!TemporalRefitValuesMatchDimensions(key.v, source)) {
      return false;
    }
  }
  return true;
}

}  // namespace bbsolver
