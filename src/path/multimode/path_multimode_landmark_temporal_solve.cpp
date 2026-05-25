#include "bbsolver/path/multimode/path_multimode_landmark_temporal_solve.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/path/multimode/path_multimode_landmark_segment_fit.hpp"
#include "bbsolver/path/multimode/path_multimode_reconstruction.hpp"
#include "bbsolver/path/multimode/path_multimode_temporal.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"

#include <algorithm>
#include <atomic>

namespace bbsolver {
namespace path_multimode {
namespace {

LandmarkSubpathTemporalResult SolveLandmarkRegionTemporalPass(
    const PropertySamples& region_samples,
    double tolerance,
    int max_gap,
    int max_segment_checks,
    const CancelFn& cancel_fn,
    bool allow_relaxed_endpoints) {
  LandmarkSubpathTemporalResult result;
  if (region_samples.samples.empty()) {
    result.notes = "landmark_subpath_temporal_no_samples";
    return result;
  }

  SolverConfig config;
  config.tolerance = std::max(tolerance, 0.0);
  config.allow_hold = false;
  config.allow_linear = true;
  config.allow_bezier = true;
  config.allow_shape_temporal_bezier = true;

  const ShapeMorphProgressBandOptions band_options =
      LandmarkBandOptions(config.tolerance, max_gap);

  CompInfo comp;
  comp.fps = 24.0;
  comp.duration_sec = region_samples.t_end_sec - region_samples.t_start_sec;

  std::atomic<int> segment_checks{0};
  std::atomic<bool> budget_exceeded{false};
  SegmentFitFn fit_fn =
      [&segment_checks,
       &budget_exceeded,
       max_segment_checks,
       &band_options,
       allow_relaxed_endpoints](
          int i,
          int j,
          const PropertySamples& ps,
          const SolverConfig& cfg,
          const CompInfo&) {
        const int checks = segment_checks.fetch_add(1) + 1;
        if (max_segment_checks > 0 &&
            checks > max_segment_checks) {
          budget_exceeded.store(true);
          SegmentFitResult out;
          out.reason = "landmark_subpath_temporal_budget_exceeded";
          return out;
        }
        return FitLandmarkRegionShapeSegment(i, j, ps, cfg, band_options,
                                             allow_relaxed_endpoints);
      };

  result.keys = SolveProperty(region_samples, config, comp, fit_fn,
                              cancel_fn, max_gap);
  result.segment_checks = segment_checks.load();
  result.budget_exceeded = budget_exceeded.load();
  if (result.keys.notes == "cancelled" ||
      (cancel_fn && cancel_fn())) {
    result.notes = "cancelled";
    return result;
  }
  if (result.budget_exceeded) {
    result.notes = "landmark_subpath_temporal_budget_exceeded";
    return result;
  }
  if (!result.keys.converged || result.keys.keys.empty()) {
    result.notes = "landmark_subpath_temporal_not_converged";
    return result;
  }

  result.reconstruction =
      EvaluateLandmarkSubpathCandidate(region_samples, result.keys, tolerance);
  if (!result.reconstruction.ok) {
    result.notes = "landmark_subpath_temporal_validation_failed";
    return result;
  }

  result.ok = true;
  result.notes = "landmark_subpath_temporal_accepted";
  return result;
}

}  // namespace

LandmarkSubpathTemporalResult SolveLandmarkRegionTemporal(
    const PropertySamples& region_samples,
    double tolerance,
    int max_gap,
    int max_segment_checks,
    const CancelFn& cancel_fn) {
  LandmarkSubpathTemporalResult exact =
      SolveLandmarkRegionTemporalPass(region_samples,
                                      tolerance,
                                      max_gap,
                                      max_segment_checks,
                                      cancel_fn,
                                      false);
  if (exact.notes == "cancelled") {
    return exact;
  }

  LandmarkSubpathTemporalResult relaxed =
      SolveLandmarkRegionTemporalPass(region_samples,
                                      tolerance,
                                      max_gap,
                                      max_segment_checks,
                                      cancel_fn,
                                      true);
  if (relaxed.notes == "cancelled") {
    return relaxed;
  }

  const bool relaxed_better =
      relaxed.ok &&
      (!exact.ok || relaxed.keys.keys.size() < exact.keys.keys.size());
  if (relaxed_better) {
    relaxed.notes = "landmark_subpath_temporal_relaxed_accepted";
    return relaxed;
  }
  return exact;
}

}  // namespace path_multimode
}  // namespace bbsolver
