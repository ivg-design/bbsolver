#include "bbsolver/dp/dp_key_assembly.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/dp/dp_placer.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>
#include <cstddef>

namespace bbsolver {
namespace {

std::vector<TemporalEase> NeutralEase(std::size_t dims) {
  std::vector<TemporalEase> e;
  e.assign(std::max<std::size_t>(1, dims), TemporalEase{0.0, 33.3});
  return e;
}

int TemporalChannelsForProperty(const PropertySamples& ps) {
  return ps.property.is_separated ? std::max(1, ps.property.dimensions) : 1;
}

std::vector<double> NeutralSpatial(std::size_t dims, bool is_spatial) {
  if (!is_spatial) {
    return {};
  }
  return std::vector<double>(dims, 0.0);
}

std::vector<double> SampleValueForKey(const PropertySamples& ps,
                                      const DPPlacement& pl,
                                      int key_idx,
                                      int dims) {
  const int sample_idx = pl.sample_indices[static_cast<std::size_t>(key_idx)];
  if (key_idx > 0) {
    const std::vector<double>& prev_value =
        pl.segments[static_cast<std::size_t>(key_idx - 1)].key_value_at_j;
    if (!prev_value.empty()) {
      return prev_value;
    }
  }
  if (key_idx + 1 < static_cast<int>(pl.sample_indices.size())) {
    const std::vector<double>& next_value =
        pl.segments[static_cast<std::size_t>(key_idx)].key_value_at_i;
    if (!next_value.empty()) {
      return next_value;
    }
  }

  return SampleValueAt(ps, sample_idx, dims);
}

}  // namespace

std::vector<double> SampleValueAt(const PropertySamples& ps,
                                  int sample_idx,
                                  int dims) {
  const std::vector<double>& sample_v =
      ps.samples[static_cast<std::size_t>(sample_idx)].v;
  if (static_cast<int>(sample_v.size()) >= dims) {
    return std::vector<double>(sample_v.begin(), sample_v.begin() + dims);
  }
  return sample_v;
}

std::vector<double> SegmentStartValue(const PropertySamples& ps,
                                      const SegmentFitResult& segment,
                                      int sample_idx,
                                      int dims) {
  return segment.key_value_at_i.empty()
             ? SampleValueAt(ps, sample_idx, dims)
             : segment.key_value_at_i;
}

std::vector<double> SegmentEndValue(const PropertySamples& ps,
                                    const SegmentFitResult& segment,
                                    int sample_idx,
                                    int dims) {
  return segment.key_value_at_j.empty()
             ? SampleValueAt(ps, sample_idx, dims)
             : segment.key_value_at_j;
}

bool ValuesCompatible(const std::vector<double>& a,
                      const std::vector<double>& b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (std::size_t idx = 0; idx < a.size(); ++idx) {
    if (std::abs(a[idx] - b[idx]) > 1e-7) {
      return false;
    }
  }
  return true;
}

PropertyKeys AssembleKeys(const PropertySamples& ps, const DPPlacement& pl) {
  PropertyKeys out;
  out.property_id = ps.property.id;
  out.converged = pl.converged;
  out.max_err = pl.max_err;
  out.max_err_screen_px = pl.max_err_screen_px;
  if (!pl.notes.empty()) {
    out.notes = pl.notes;
  }

  if (pl.sample_indices.empty()) {
    return out;
  }

  const int key_count = static_cast<int>(pl.sample_indices.size());
  out.keys.reserve(key_count);
  const int dims = std::max(1, ps.property.dimensions);
  const int temporal_dims = TemporalChannelsForProperty(ps);
  const bool spatial = ps.property.is_spatial;

  for (int key_idx = 0; key_idx < key_count; ++key_idx) {
    const int sample_idx = pl.sample_indices[key_idx];
    Key key;
    key.t_sec = ps.samples[sample_idx].t_sec;
    key.v = SampleValueForKey(ps, pl, key_idx, dims);

    key.interp_in = InterpType::Bezier;
    key.interp_out = InterpType::Bezier;
    key.temporal_continuous = false;
    key.spatial_continuous = false;
    key.temporal_auto_bezier = false;
    key.spatial_auto_bezier = false;
    key.roving = false;

    if (key_idx > 0) {
      const auto& segment_in = pl.segments[static_cast<std::size_t>(
          key_idx - 1)];
      key.interp_in = segment_in.interp;
      key.temporal_ease_in = segment_in.ease_in_at_j.empty()
                                 ? NeutralEase(dims)
                                 : segment_in.ease_in_at_j;
      key.spatial_in = (spatial && !segment_in.spatial_in_at_j.empty())
                            ? segment_in.spatial_in_at_j
                            : NeutralSpatial(dims, spatial);
    } else {
      key.temporal_ease_in = NeutralEase(temporal_dims);
      key.spatial_in = NeutralSpatial(dims, spatial);
    }

    if (key_idx + 1 < key_count) {
      const auto& segment_out =
          pl.segments[static_cast<std::size_t>(key_idx)];
      key.interp_out = segment_out.interp;
      key.temporal_ease_out = segment_out.ease_out_at_i.empty()
                                  ? NeutralEase(dims)
                                  : segment_out.ease_out_at_i;
      key.spatial_out = (spatial && !segment_out.spatial_out_at_i.empty())
                             ? segment_out.spatial_out_at_i
                             : NeutralSpatial(dims, spatial);
    } else {
      key.temporal_ease_out = NeutralEase(temporal_dims);
      key.spatial_out = NeutralSpatial(dims, spatial);
    }
    out.keys.push_back(std::move(key));
  }

  out.segments.reserve(pl.segments.size());
  for (std::size_t segment_idx = 0; segment_idx < pl.segments.size();
       ++segment_idx) {
    SegmentReport rep;
    rep.start_idx = pl.sample_indices[segment_idx];
    rep.end_idx = pl.sample_indices[segment_idx + 1];
    rep.max_err = pl.segments[segment_idx].max_err;
    rep.max_err_screen_px = pl.segments[segment_idx].max_err_screen_px;
    rep.rms_err = pl.segments[segment_idx].rms_err;
    rep.iters = pl.segments[segment_idx].iters;
    rep.reason = pl.segments[segment_idx].reason;
    out.segments.push_back(std::move(rep));
  }

  return out;
}

}  // namespace bbsolver
