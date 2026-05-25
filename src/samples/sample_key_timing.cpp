#include "bbsolver/samples/sample_key_timing.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace bbsolver {

std::vector<TemporalEase> DefaultEasesForProperty(
    const PropertySamples& property_samples) {
  const int count = property_samples.property.is_separated
      ? std::max(property_samples.property.dimensions, 1)
      : 1;
  return std::vector<TemporalEase>(
      static_cast<std::size_t>(std::max(count, 1)),
      TemporalEase{0.0, 33.3});
}

bool ApplySampleKeyTiming(const PropertySamples& property_samples,
                          const Sample& sample,
                          Key& key) {
  if (!sample.key_timing.has_value()) {
    return false;
  }
  const KeyTiming& timing = *sample.key_timing;
  key.interp_in = timing.interp_in;
  key.interp_out = timing.interp_out;
  key.temporal_ease_in = timing.temporal_ease_in.empty()
      ? DefaultEasesForProperty(property_samples)
      : timing.temporal_ease_in;
  key.temporal_ease_out = timing.temporal_ease_out.empty()
      ? DefaultEasesForProperty(property_samples)
      : timing.temporal_ease_out;
  key.spatial_in = timing.spatial_in;
  key.spatial_out = timing.spatial_out;
  key.temporal_continuous = timing.temporal_continuous;
  key.spatial_continuous = timing.spatial_continuous;
  key.temporal_auto_bezier = timing.temporal_auto_bezier;
  key.spatial_auto_bezier = timing.spatial_auto_bezier;
  key.roving = timing.roving;
  return true;
}

}  // namespace bbsolver
