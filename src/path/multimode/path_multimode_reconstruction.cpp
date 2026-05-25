#include "bbsolver/path/multimode/path_multimode_reconstruction.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/path/multimode/path_multimode_geometry.hpp"
#include "bbsolver/path/multimode/path_multimode_region_candidate.hpp"
#include "bbsolver/path/multimode/path_multimode_temporal.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"

#include <algorithm>
#include <utility>
#include <cstddef>
#include <vector>

namespace bbsolver {
namespace path_multimode {

LandmarkSubpathReconstructionResult EvaluateLandmarkSubpathReconstruction(
    const PropertySamples& reduced,
    VertexRegion region,
    const std::vector<int>& anchors,
    double tolerance) {
  LandmarkSubpathReconstructionResult result;
  if (reduced.samples.empty() || anchors.empty()) {
    return result;
  }

  PathFrameFitOptions fit_options;
  fit_options.outline_tolerance = std::max(tolerance, 0.0);
  fit_options.max_subdivisions_per_segment = 8;

  std::vector<std::vector<double>> anchor_shapes;
  anchor_shapes.reserve(anchors.size());
  for (int anchor : anchors) {
    if (anchor < 0 || anchor >= static_cast<int>(reduced.samples.size())) {
      return result;
    }
    std::vector<double> shape = ShapeFlatRegion(
        reduced.samples[static_cast<std::size_t>(anchor)].v, region);
    if (shape.empty()) {
      return result;
    }
    anchor_shapes.push_back(std::move(shape));
  }

  std::size_t segment_idx = 0;
  for (std::size_t sample_idx = 0; sample_idx < reduced.samples.size();
       ++sample_idx) {
    const Sample& sample = reduced.samples[sample_idx];
    while (segment_idx + 1 < anchors.size() &&
           anchors[segment_idx + 1] < static_cast<int>(sample_idx)) {
      ++segment_idx;
    }

    std::vector<double> reconstructed;
    if (segment_idx + 1 >= anchors.size() ||
        anchors[segment_idx] == static_cast<int>(sample_idx)) {
      reconstructed = anchor_shapes[segment_idx];
    } else if (anchors[segment_idx + 1] == static_cast<int>(sample_idx)) {
      reconstructed = anchor_shapes[segment_idx + 1];
    } else {
      const Sample& start =
          reduced.samples[static_cast<std::size_t>(anchors[segment_idx])];
      const Sample& end =
          reduced.samples[static_cast<std::size_t>(anchors[segment_idx + 1])];
      const double dt = end.t_sec - start.t_sec;
      if (!(dt > 0.0)) {
        return result;
      }
      const double u = (sample.t_sec - start.t_sec) / dt;
      reconstructed = LinearInterpolateShapeFlat(
          anchor_shapes[segment_idx], anchor_shapes[segment_idx + 1], u);
      if (reconstructed.empty()) {
        return result;
      }
    }

    const std::vector<double> expected = ShapeFlatRegion(sample.v, region);
    if (expected.empty()) {
      return result;
    }
    const double error =
        ShapeFlatFrameOutlineError(expected, reconstructed, fit_options);
    if (error > result.max_outline_error) {
      result.max_outline_error = error;
      result.worst_sample_idx = static_cast<int>(sample_idx);
      result.worst_t_sec = sample.t_sec;
    }
    ++result.samples_checked;
  }

  result.ok = result.samples_checked == static_cast<int>(reduced.samples.size()) &&
              result.max_outline_error <= fit_options.outline_tolerance + 1e-9;
  return result;
}

LandmarkSubpathReconstructionResult EvaluateLandmarkSubpathCandidate(
    const PropertySamples& region_samples,
    const PropertyKeys& candidate,
    double tolerance) {
  LandmarkSubpathReconstructionResult result;

  PathTemporalValidationOptions validation_options;
  validation_options.frame_fit_options.outline_tolerance =
      std::max(tolerance, 0.0);
  validation_options.frame_fit_options.max_subdivisions_per_segment = 8;
  const PathTemporalValidationResult validation =
      ValidatePathTemporalCandidate(region_samples, candidate,
                                    validation_options);
  result.ok = validation.ok;
  result.max_outline_error = validation.max_outline_error;
  result.worst_sample_idx = validation.worst_sample_idx;
  result.worst_t_sec = validation.worst_t_sec;
  result.samples_checked = validation.samples_checked;
  return result;
}

LandmarkSubpathRefinementResult RefineLandmarkSubpathAnchors(
    const PropertySamples& reduced,
    VertexRegion region,
    const std::vector<int>& initial_anchors,
    double tolerance,
    const CancelFn& cancel_fn) {
  LandmarkSubpathRefinementResult result;
  const int n = static_cast<int>(reduced.samples.size());
  if (n <= 0 || initial_anchors.empty()) {
    return result;
  }

  result.anchors = initial_anchors;
  std::sort(result.anchors.begin(), result.anchors.end());
  result.anchors.erase(std::unique(result.anchors.begin(), result.anchors.end()),
                       result.anchors.end());
  if (result.anchors.empty() || result.anchors.front() != 0 ||
      result.anchors.back() != n - 1) {
    return {};
  }

  while (static_cast<int>(result.anchors.size()) <= n) {
    if (cancel_fn && cancel_fn()) {
      return {};
    }
    result.reconstruction = EvaluateLandmarkSubpathReconstruction(
        reduced, region, result.anchors, tolerance);
    if (result.reconstruction.ok) {
      result.ok = true;
      return result;
    }
    if (result.reconstruction.samples_checked != n ||
        result.reconstruction.worst_sample_idx < 0 ||
        result.reconstruction.worst_sample_idx >= n) {
      return {};
    }

    const int worst = result.reconstruction.worst_sample_idx;
    auto insert_at =
        std::lower_bound(result.anchors.begin(), result.anchors.end(), worst);
    if (insert_at != result.anchors.end() && *insert_at == worst) {
      return {};
    }
    result.anchors.insert(insert_at, worst);
    ++result.inserted_samples;
  }
  return {};
}

std::vector<double> EvaluateTemporalShapeAtSample(
    const PropertySamples& samples,
    const PropertyKeys& keys,
    int sample_idx,
    const ShapeMorphProgressBandOptions& band_options) {
  if (sample_idx < 0 ||
      sample_idx >= static_cast<int>(samples.samples.size()) ||
      keys.keys.empty()) {
    return {};
  }
  if (keys.segments.empty()) {
    return keys.keys.front().v;
  }
  if (keys.keys.size() != keys.segments.size() + 1) {
    return {};
  }

  for (std::size_t segment_idx = 0; segment_idx < keys.segments.size();
       ++segment_idx) {
    const SegmentReport& segment = keys.segments[segment_idx];
    if (sample_idx < segment.start_idx || sample_idx > segment.end_idx) {
      continue;
    }
    const Key& start_key = keys.keys[segment_idx];
    const Key& end_key = keys.keys[segment_idx + 1];
    if (sample_idx == segment.start_idx) {
      return start_key.v;
    }
    if (sample_idx == segment.end_idx) {
      return end_key.v;
    }
    const double t0 =
        samples.samples[static_cast<std::size_t>(segment.start_idx)].t_sec;
    const double t1 =
        samples.samples[static_cast<std::size_t>(segment.end_idx)].t_sec;
    if (!(t1 > t0)) {
      return {};
    }
    const double t = samples.samples[static_cast<std::size_t>(sample_idx)].t_sec;
    const double alpha = std::clamp((t - t0) / (t1 - t0), 0.0, 1.0);
    const InterpType out_interp = start_key.interp_out;
    const InterpType in_interp = end_key.interp_in;
    double progress = alpha;
    if (out_interp == InterpType::Hold || in_interp == InterpType::Hold) {
      progress = 0.0;
    } else if (out_interp == InterpType::Bezier &&
               in_interp == InterpType::Bezier) {
      const TemporalEase ease_out =
          start_key.temporal_ease_out.empty()
              ? TemporalEase{0.0, 33.3}
              : start_key.temporal_ease_out.front();
      const TemporalEase ease_in =
          end_key.temporal_ease_in.empty()
              ? TemporalEase{0.0, 33.3}
              : end_key.temporal_ease_in.front();
      progress =
          ShapeTemporalBezierProgress(alpha, ease_out, ease_in, band_options);
    }
    return LinearInterpolateShapeFlat(start_key.v, end_key.v, progress);
  }
  return {};
}

}  // namespace path_multimode
}  // namespace bbsolver
