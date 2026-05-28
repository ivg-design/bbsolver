#include "bbsolver/path/multimode/path_multimode_temporal.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <cmath>
#include <vector>
#include <cstddef>

#include "bbsolver/path/multimode/path_multimode_geometry.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"

namespace bbsolver::path_multimode {

namespace {

constexpr int kMaxRelaxedLandmarkInfluencePairs = 12;

double CubicScalar(double t, double p0, double p1, double p2, double p3) {
  const double omt = 1.0 - t;
  return omt * omt * omt * p0 +
         3.0 * omt * omt * t * p1 +
         3.0 * omt * t * t * p2 +
         t * t * t * p3;
}

void AddLandmarkInfluencePair(std::vector<LandmarkInfluencePair>& out,
                              double out_influence,
                              double in_influence,
                              const ShapeMorphProgressBandOptions& options) {
  LandmarkInfluencePair pair;
  pair.out_influence = ClampInfluence(out_influence, options);
  pair.in_influence = ClampInfluence(in_influence, options);
  for (const LandmarkInfluencePair& existing: out) {
    if (SameInfluencePair(existing, pair)) {
      return;
    }
  }
  out.push_back(pair);
}

}  // namespace

std::vector<TemporalEase> NeutralEase() {
  return {TemporalEase{0.0, 33.3}};
}

std::vector<TemporalEase> ShapeEase(double influence) {
  if (!std::isfinite(influence)) {
    influence = 33.3;
  }
  return {TemporalEase{0.0, std::clamp(influence, 0.1, 100.0)}};
}

double ClampInfluence(double influence,
                      const ShapeMorphProgressBandOptions& options) {
  if (!std::isfinite(influence)) {
    return 33.3;
  }
  const double lo = std::max(0.1, std::min(options.min_bezier_influence,
                                           options.max_bezier_influence));
  const double hi = std::min(100.0, std::max(options.min_bezier_influence,
                                             options.max_bezier_influence));
  return std::clamp(influence, lo, std::max(lo, hi));
}

double ShapeTemporalBezierProgress(
    double alpha,
    TemporalEase ease_out,
    TemporalEase ease_in,
    const ShapeMorphProgressBandOptions& options) {
  const double out_influence = ClampInfluence(ease_out.influence, options) / 100.0;
  const double in_influence = ClampInfluence(ease_in.influence, options) / 100.0;
  const double x1 = out_influence;
  const double x2 = 1.0 - in_influence;
  double lo = 0.0;
  double hi = 1.0;
  for (int iter = 0; iter < 40; ++iter) {
    const double mid = (lo + hi) * 0.5;
    const double x = CubicScalar(mid, 0.0, x1, x2, 1.0);
    if (x < alpha) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  return CubicScalar((lo + hi) * 0.5, 0.0, 0.0, 1.0, 1.0);
}

std::vector<double> SegmentProgressValues(
    const PropertySamples& ps,
    int i,
    int j,
    bool bezier,
    TemporalEase ease_out,
    TemporalEase ease_in,
    const ShapeMorphProgressBandOptions& options) {
  std::vector<double> progress;
  if (i < 0 || j <= i || j >= static_cast<int>(ps.samples.size())) {
    return progress;
  }
  progress.reserve(static_cast<std::size_t>(j - i + 1));
  const double t0 = ps.samples[static_cast<std::size_t>(i)].t_sec;
  const double t1 = ps.samples[static_cast<std::size_t>(j)].t_sec;
  for (int sample_idx = i; sample_idx <= j; ++sample_idx) {
    const double t = ps.samples[static_cast<std::size_t>(sample_idx)].t_sec;
    const double alpha = (t1 > t0)
                             ? std::clamp((t - t0) / (t1 - t0), 0.0, 1.0)
: 0.0;
    progress.push_back(bezier
                           ? ShapeTemporalBezierProgress(alpha, ease_out,
                                                          ease_in, options)
: alpha);
  }
  return progress;
}

bool SameInfluencePair(const LandmarkInfluencePair& a,
                       const LandmarkInfluencePair& b) {
  return std::abs(a.out_influence - b.out_influence) <= 1e-6 &&
         std::abs(a.in_influence - b.in_influence) <= 1e-6;
}

std::vector<LandmarkInfluencePair> BuildLandmarkInfluencePairs(
    const ShapeMorphProgressBandOptions& options,
    const ShapeMorphProgressBandResult& strict_oracle) {
  std::vector<LandmarkInfluencePair> pairs;
  AddLandmarkInfluencePair(pairs, 33.3, 33.3, options);
  if (strict_oracle.fitted_bezier_pairs_tried > 0 &&
      std::isfinite(strict_oracle.max_fitted_bezier_error)) {
    AddLandmarkInfluencePair(pairs,
                             strict_oracle.fitted_bezier_out_influence,
                             strict_oracle.fitted_bezier_in_influence,
                             options);
  }

  const double lo = ClampInfluence(options.min_bezier_influence, options);
  const double hi = ClampInfluence(options.max_bezier_influence, options);
  const double q1 = lo + (hi - lo) * 0.25;
  const double mid = lo + (hi - lo) * 0.5;
  const double q3 = lo + (hi - lo) * 0.75;
  AddLandmarkInfluencePair(pairs, q3, q1, options);
  AddLandmarkInfluencePair(pairs, q1, q3, options);
  AddLandmarkInfluencePair(pairs, 90.0, 10.0, options);
  AddLandmarkInfluencePair(pairs, 10.0, 90.0, options);
  AddLandmarkInfluencePair(pairs, 80.0, 20.0, options);
  AddLandmarkInfluencePair(pairs, 20.0, 80.0, options);
  AddLandmarkInfluencePair(pairs, mid, mid, options);
  AddLandmarkInfluencePair(pairs, hi, lo, options);
  AddLandmarkInfluencePair(pairs, lo, hi, options);
  AddLandmarkInfluencePair(pairs, q3, q3, options);
  AddLandmarkInfluencePair(pairs, q1, q1, options);

  const int max_pairs = kMaxRelaxedLandmarkInfluencePairs;
  if (static_cast<int>(pairs.size()) > max_pairs) {
    pairs.resize(static_cast<std::size_t>(max_pairs));
  }
  return pairs;
}

bool CanRunExtendedRelaxedBezierSearch(const PropertySamples& region_samples,
                                       int i,
                                       int j) {
  const int sample_count = j - i + 1;
  if (sample_count > 8 ||
      static_cast<int>(region_samples.samples.size()) > 16 ||
      region_samples.samples.empty()) {
    return false;
  }
  const int vertex_count = ShapeFlatVertexCount(region_samples.samples.front().v);
  return vertex_count > 0 && vertex_count <= 2;
}

ShapeMorphProgressBandOptions LandmarkBandOptions(double tolerance,
                                                  int max_gap) {
  ShapeMorphProgressBandOptions band_options;
  band_options.frame_fit_options.outline_tolerance = std::max(tolerance, 0.0);
  band_options.frame_fit_options.max_subdivisions_per_segment = 8;
  band_options.progress_steps = 16;
  band_options.max_window_samples = std::max(2, max_gap + 1);
  band_options.max_evaluations =
      std::max(128, band_options.max_window_samples * 64);
  band_options.fit_bezier_influence_pairs = true;
  band_options.bezier_influence_grid_steps = 5;
  band_options.bezier_influence_refinement_steps = 1;
  band_options.max_bezier_influence_pairs = 8;
  return band_options;
}

}  // namespace bbsolver::path_multimode
