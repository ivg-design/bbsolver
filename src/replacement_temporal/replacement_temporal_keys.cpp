#include "bbsolver/replacement_temporal/replacement_temporal_keys.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/dp/dp_placer.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <cstddef>
#include <string>
#include <vector>

namespace bbsolver {
namespace replacement_temporal {

std::vector<TemporalEase> NeutralShapeEase() {
  return {TemporalEase{0.0, 33.3}};
}

void AppendNote(PropertyKeys& keys, const std::string& note) {
  keys.notes = keys.notes.empty() ? note: keys.notes + "; " + note;
}

bool IsShapeFlatPath(const PropertySamples& ps) {
  return ps.property.kind == ValueKind::Custom &&
         ps.property.units_label == "shape_flat";
}

bool SameSampleTimes(const PropertySamples& a, const PropertySamples& b) {
  if (a.samples.size() != b.samples.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.samples.size(); ++i) {
    if (std::abs(a.samples[i].t_sec - b.samples[i].t_sec) > 1e-9) {
      return false;
    }
  }
  return true;
}

bool IsValidShapeFlat(const std::vector<double>& v) {
  if (static_cast<int>(v.size()) < 2) {
    return false;
  }
  const int n = static_cast<int>(std::llround(v[1]));
  return n >= 1 && static_cast<int>(v.size()) == 2 + 6 * n;
}

int ShapeFlatVertexCount(const std::vector<double>& v) {
  if (!IsValidShapeFlat(v)) {
    return 0;
  }
  return static_cast<int>(std::llround(v[1]));
}

std::vector<double> LerpShapeFlatChord(const std::vector<double>& a,
                                       const std::vector<double>& b,
                                       double u) {
  std::vector<double> out(a.size(), 0.0);
  if (a.empty() || a.size() != b.size()) {
    return out;
  }
  out[0] = a[0];
  if (a.size() > 1) {
    out[1] = a[1];
  }
  for (std::size_t idx = 2; idx < a.size(); ++idx) {
    out[idx] = a[idx] + (b[idx] - a[idx]) * u;
  }
  return out;
}

std::vector<double> ValueAt(const PropertySamples& ps, int sample_idx) {
  if (sample_idx < 0 || sample_idx >= static_cast<int>(ps.samples.size())) {
    return {};
  }
  return ps.samples[static_cast<std::size_t>(sample_idx)].v;
}

Key MakeReplacementKey(const PropertySamples& reduced,
                       const DPPlacement& placement,
                       std::size_t key_idx) {
  const int sample_idx = placement.sample_indices[key_idx];
  Key key;
  key.t_sec = reduced.samples[static_cast<std::size_t>(sample_idx)].t_sec;
  if (key_idx > 0 &&
      !placement.segments[key_idx - 1].key_value_at_j.empty()) {
    key.v = placement.segments[key_idx - 1].key_value_at_j;
  } else if (key_idx + 1 < placement.sample_indices.size() &&
             !placement.segments[key_idx].key_value_at_i.empty()) {
    key.v = placement.segments[key_idx].key_value_at_i;
  } else {
    key.v = ValueAt(reduced, sample_idx);
  }
  key.temporal_ease_in = NeutralShapeEase();
  key.temporal_ease_out = NeutralShapeEase();
  key.temporal_continuous = false;
  key.spatial_continuous = false;
  key.temporal_auto_bezier = false;
  key.spatial_auto_bezier = false;
  key.roving = false;

  if (key_idx > 0) {
    const SegmentFitResult& prev = placement.segments[key_idx - 1];
    key.interp_in = prev.interp;
    key.temporal_ease_in =
        prev.ease_in_at_j.empty() ? NeutralShapeEase(): prev.ease_in_at_j;
  } else {
    key.interp_in = InterpType::Bezier;
  }

  if (key_idx + 1 < placement.sample_indices.size()) {
    const SegmentFitResult& next = placement.segments[key_idx];
    key.interp_out = next.interp;
    key.temporal_ease_out =
        next.ease_out_at_i.empty() ? NeutralShapeEase(): next.ease_out_at_i;
  } else {
    key.interp_out = InterpType::Bezier;
  }

  return key;
}

PropertyKeys AssembleReplacementKeys(const PropertySamples& reduced,
                                     const DPPlacement& placement) {
  PropertyKeys out;
  out.property_id = reduced.property.id;
  out.converged = placement.converged;
  out.max_err = placement.max_err;
  out.max_err_screen_px = placement.max_err_screen_px;
  out.notes = placement.notes;

  out.keys.reserve(placement.sample_indices.size());
  for (std::size_t key_idx = 0; key_idx < placement.sample_indices.size();
       ++key_idx) {
    out.keys.push_back(MakeReplacementKey(reduced, placement, key_idx));
  }

  out.segments.reserve(placement.segments.size());
  for (std::size_t seg_idx = 0; seg_idx < placement.segments.size();
       ++seg_idx) {
    SegmentReport report;
    report.start_idx = placement.sample_indices[seg_idx];
    report.end_idx = placement.sample_indices[seg_idx + 1];
    report.max_err = placement.segments[seg_idx].max_err;
    report.max_err_screen_px = placement.segments[seg_idx].max_err_screen_px;
    report.rms_err = placement.segments[seg_idx].rms_err;
    report.iters = placement.segments[seg_idx].iters;
    report.reason = placement.segments[seg_idx].reason;
    out.segments.push_back(std::move(report));
  }

  return out;
}

bool IsAllSamplesAnchorFallback(const PropertyKeys& keys,
                                const PropertySamples& reduced) {
  if (keys.notes.find("falling back to all-samples-as-anchors") ==
          std::string::npos ||
      keys.keys.size() != reduced.samples.size()) {
    return false;
  }
  if (!reduced.samples.empty() &&
      keys.segments.size() + 1 != reduced.samples.size()) {
    return false;
  }
  for (std::size_t idx = 0; idx < keys.keys.size(); ++idx) {
    if (std::abs(keys.keys[idx].t_sec - reduced.samples[idx].t_sec) > 1e-9) {
      return false;
    }
  }
  for (const SegmentReport& segment: keys.segments) {
    if (segment.reason != "fallback_linear_anchor") {
      return false;
    }
  }
  return true;
}

void MarkAnchorFallbackAsHoldForExport(PropertyKeys& keys) {
  if (keys.keys.size() < 2) {
    return;
  }
  for (std::size_t idx = 0; idx + 1 < keys.keys.size(); ++idx) {
    keys.keys[idx].interp_out = InterpType::Hold;
    keys.keys[idx + 1].interp_in = InterpType::Hold;
  }
  for (SegmentReport& segment: keys.segments) {
    if (segment.reason == "fallback_linear_anchor") {
      segment.reason = "fallback_hold_anchor";
    }
  }
}

Key MakePruneKey(const PropertySamples& reduced,
                 const std::vector<SegmentFitResult>& segments,
                 int sample_idx,
                 std::size_t key_idx,
                 std::size_t key_count) {
  Key key;
  key.t_sec = reduced.samples[static_cast<std::size_t>(sample_idx)].t_sec;
  if (key_idx > 0 &&
      !segments[static_cast<std::size_t>(key_idx - 1)].key_value_at_j.empty()) {
    key.v = segments[static_cast<std::size_t>(key_idx - 1)].key_value_at_j;
  } else if (key_idx + 1 < key_count &&
             !segments[static_cast<std::size_t>(key_idx)].key_value_at_i.empty()) {
    key.v = segments[static_cast<std::size_t>(key_idx)].key_value_at_i;
  } else {
    key.v = ValueAt(reduced, sample_idx);
  }
  key.temporal_ease_in = NeutralShapeEase();
  key.temporal_ease_out = NeutralShapeEase();
  key.temporal_continuous = false;
  key.spatial_continuous = false;
  key.temporal_auto_bezier = false;
  key.spatial_auto_bezier = false;
  key.roving = false;
  if (key_idx > 0) {
    const SegmentFitResult& prev =
        segments[static_cast<std::size_t>(key_idx - 1)];
    key.interp_in = prev.interp;
    key.temporal_ease_in =
        prev.ease_in_at_j.empty() ? NeutralShapeEase(): prev.ease_in_at_j;
  } else {
    key.interp_in = InterpType::Bezier;
  }
  if (key_idx + 1 < key_count) {
    const SegmentFitResult& next = segments[static_cast<std::size_t>(key_idx)];
    key.interp_out = next.interp;
    key.temporal_ease_out =
        next.ease_out_at_i.empty() ? NeutralShapeEase(): next.ease_out_at_i;
  } else {
    key.interp_out = InterpType::Bezier;
  }
  return key;
}

PropertyKeys AssembleLinearPruneKeys(
    const PropertySamples& reduced,
    const std::vector<int>& anchors,
    const std::vector<SegmentFitResult>& segments) {
  PropertyKeys out;
  out.property_id = reduced.property.id;
  out.converged = true;
  out.keys.reserve(anchors.size());
  for (std::size_t key_idx = 0; key_idx < anchors.size(); ++key_idx) {
    out.keys.push_back(MakePruneKey(reduced,
                                    segments,
                                    anchors[key_idx],
                                    key_idx,
                                    anchors.size()));
  }
  out.segments.reserve(segments.size());
  for (std::size_t idx = 0; idx < segments.size(); ++idx) {
    const SegmentFitResult& segment = segments[idx];
    out.max_err = std::max(out.max_err, segment.max_err);
    out.max_err_screen_px =
        std::max(out.max_err_screen_px, segment.max_err_screen_px);
    SegmentReport report;
    report.start_idx = anchors[idx];
    report.end_idx = anchors[idx + 1];
    report.max_err = segment.max_err;
    report.max_err_screen_px = segment.max_err_screen_px;
    report.rms_err = segment.rms_err;
    report.iters = segment.iters;
    report.reason =
        segment.reason.empty() ? "exact_anchor_fallback_linear_prune"
: segment.reason;
    out.segments.push_back(std::move(report));
  }
  return out;
}

}  // namespace replacement_temporal
}  // namespace bbsolver
