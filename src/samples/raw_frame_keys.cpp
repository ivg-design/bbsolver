#include "bbsolver/samples/raw_frame_keys.hpp"
#include "bbsolver/domain.hpp"

#include <cmath>
#include <string>
#include <utility>

#include "bbsolver/samples/sample_key_timing.hpp"
#include "bbsolver/samples/sample_value_helpers.hpp"
#include "bbsolver/solve/solver_reporting.hpp"

namespace bbsolver {

PropertyKeys ShapeFlatFrameKeyFallback(const PropertySamples& property_samples,
                                       const std::string& note) {
  PropertyKeys keys;
  keys.property_id = property_samples.property.id;
  keys.converged = true;
  keys.max_err = 0.0;
  keys.max_err_screen_px = 0.0;

  int first_vertex_count = -1;
  bool has_variable_topology = false;
  int skipped_malformed = 0;
  int preserved_timing_count = 0;

  keys.keys.reserve(property_samples.samples.size());
  for (const Sample& sample: property_samples.samples) {
    if (sample.v.size() < 2) {
      ++skipped_malformed;
      continue;
    }
    const int n = static_cast<int>(std::llround(sample.v[1]));
    const int expected_size = 2 + n * 6;
    if (n <= 0 || static_cast<int>(sample.v.size()) != expected_size) {
      ++skipped_malformed;
      continue;
    }
    if (first_vertex_count < 0) {
      first_vertex_count = n;
    } else if (n != first_vertex_count) {
      has_variable_topology = true;
    }

    Key key;
    key.t_sec = sample.t_sec;
    key.v = sample.v;
    if (ApplySampleKeyTiming(property_samples, sample, key)) {
      ++preserved_timing_count;
    } else {
      key.interp_in = InterpType::Linear;
      key.interp_out = InterpType::Linear;
      key.temporal_ease_in = {{0.0, 33.3}};
      key.temporal_ease_out = {{0.0, 33.3}};
    }
    keys.keys.push_back(std::move(key));
  }

  std::string status_note =
      note + "; raw_frame_keys=" + std::to_string(keys.keys.size());
  AppendSampleTimingNote(
      status_note,
      static_cast<int>(keys.keys.size()),
      preserved_timing_count);
  if (skipped_malformed > 0) {
    keys.converged = false;
    status_note += "; skipped_malformed=" +
                   std::to_string(skipped_malformed);
  }
  if (has_variable_topology) {
    status_note += "; variable_topology=true";
  }
  keys.notes = status_note;

  if (!property_samples.samples.empty()) {
    SegmentReport segment;
    segment.start_idx = 0;
    segment.end_idx = static_cast<int>(property_samples.samples.size()) - 1;
    segment.reason = "shape_flat_raw_frame_keys";
    keys.segments.push_back(segment);
  }
  return keys;
}

PropertyKeys RawFrameKeyFallback(const PropertySamples& property_samples,
                                 const std::string& note) {
  PropertyKeys keys;
  keys.property_id = property_samples.property.id;
  keys.converged = true;
  keys.max_err = 0.0;
  keys.max_err_screen_px = 0.0;
  keys.keys.reserve(property_samples.samples.size());
  int preserved_timing_count = 0;
  for (const Sample& sample: property_samples.samples) {
    Key key;
    key.t_sec = sample.t_sec;
    key.v = SampleVectorOrZeros(property_samples, sample);
    if (ApplySampleKeyTiming(property_samples, sample, key)) {
      ++preserved_timing_count;
    } else {
      key.interp_in = InterpType::Linear;
      key.interp_out = InterpType::Linear;
      key.temporal_ease_in = DefaultEasesForProperty(property_samples);
      key.temporal_ease_out = DefaultEasesForProperty(property_samples);
    }
    keys.keys.push_back(std::move(key));
  }
  keys.notes =
      note + "; raw_frame_keys=" + std::to_string(keys.keys.size());
  AppendSampleTimingNote(
      keys.notes,
      static_cast<int>(keys.keys.size()),
      preserved_timing_count);
  if (!property_samples.samples.empty()) {
    SegmentReport segment;
    segment.start_idx = 0;
    segment.end_idx = static_cast<int>(property_samples.samples.size()) - 1;
    segment.reason = "raw_frame_keys";
    keys.segments.push_back(segment);
  }
  return keys;
}

}  // namespace bbsolver
