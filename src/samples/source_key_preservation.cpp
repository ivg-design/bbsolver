#include "bbsolver/samples/source_key_preservation.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/samples/sample_key_timing.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

namespace bbsolver {

double SourceKeySampleTimeEpsilon(const PropertySamples& property_samples) {
  double min_step = std::numeric_limits<double>::infinity();
  for (std::size_t i = 1; i < property_samples.samples.size(); ++i) {
    const double step =
        property_samples.samples[i].t_sec -
        property_samples.samples[i - 1].t_sec;
    if (step > 1e-9 && step < min_step) {
      min_step = step;
    }
  }
  if (!std::isfinite(min_step)) {
    return 1e-6;
  }
  return std::max(1e-6, min_step * 0.25);
}

const Sample* FindSampleAtSourceKeyTime(const PropertySamples& property_samples,
                                        double t_sec) {
  const double eps = SourceKeySampleTimeEpsilon(property_samples);
  const Sample* best = nullptr;
  double best_distance = std::numeric_limits<double>::infinity();
  for (const Sample& sample : property_samples.samples) {
    const double distance = std::abs(sample.t_sec - t_sec);
    if (distance < best_distance) {
      best = &sample;
      best_distance = distance;
    }
  }
  return best != nullptr && best_distance <= eps ? best : nullptr;
}

PropertyKeys BuildShapeFlatSourceKeyPreservationKeys(
    const PropertySamples& property_samples,
    const std::vector<double>& source_key_times,
    int* preserved_timing_count_out) {
  PropertyKeys keys;
  keys.property_id = property_samples.property.id;
  keys.converged = true;
  keys.max_err = 0.0;
  keys.max_err_screen_px = 0.0;
  int preserved_timing_count = 0;
  keys.keys.reserve(source_key_times.size());

  for (std::size_t i = 0; i < source_key_times.size(); ++i) {
    const Sample* sample =
        FindSampleAtSourceKeyTime(property_samples, source_key_times[i]);
    if (sample == nullptr) {
      keys.keys.clear();
      keys.converged = false;
      return keys;
    }
    Key key;
    key.t_sec = source_key_times[i];
    key.v = sample->v;
    if (ApplySampleKeyTiming(property_samples, *sample, key)) {
      ++preserved_timing_count;
    } else {
      key.interp_in = i == 0
          ? InterpType::Linear
          : InterpType::Linear;
      key.interp_out = i + 1 == source_key_times.size()
          ? InterpType::Linear
          : InterpType::Linear;
      key.temporal_ease_in = DefaultEasesForProperty(property_samples);
      key.temporal_ease_out = DefaultEasesForProperty(property_samples);
    }
    keys.keys.push_back(std::move(key));
  }

  if (preserved_timing_count_out != nullptr) {
    *preserved_timing_count_out = preserved_timing_count;
  }
  if (!keys.keys.empty()) {
    SegmentReport segment;
    segment.start_idx = 0;
    segment.end_idx = static_cast<int>(property_samples.samples.size()) - 1;
    segment.reason = "shape_flat_near_optimal_source_keys";
    keys.segments.push_back(std::move(segment));
  }
  return keys;
}

}  // namespace bbsolver
