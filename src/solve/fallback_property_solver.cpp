#include "bbsolver/solve/fallback_property_solver.hpp"

#ifndef BBSOLVER_HAVE_DP_PLACER

#include "bbsolver/domain.hpp"
#include "bbsolver/dp/dp_placer.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
#define BBSOLVER_WEAK_FALLBACK_SOLVE_PROPERTY
#else
#define BBSOLVER_WEAK_FALLBACK_SOLVE_PROPERTY __attribute__((weak))
#endif

namespace bbsolver {

BBSOLVER_WEAK_FALLBACK_SOLVE_PROPERTY PropertyKeys SolveProperty(
    const PropertySamples& ps,
    const SolverConfig& cfg,
    const CompInfo& comp,
    SegmentFitFn fit_fn,
    CancelFn cancel_fn,
    int,
    PlacementProgressFn) {
  PropertyKeys result;
  result.property_id = ps.property.id;
  result.converged = true;
  result.notes = "weak fallback recursive placement";

  if (cancel_fn && cancel_fn()) {
    result.converged = false;
    result.notes = "cancelled";
    return result;
  }

  if (ps.samples.empty()) {
    result.converged = false;
    result.notes = "no samples";
    return result;
  }

  std::vector<fallback_property_solver::FittedSegment> segments;
  fallback_property_solver::FallbackSolveRange(
      0, static_cast<int>(ps.samples.size()) - 1, ps, cfg, comp, fit_fn,
      segments);

  result.keys.reserve(segments.size() + 1);
  for (std::size_t idx = 0; idx < segments.size(); ++idx) {
    const fallback_property_solver::FittedSegment& segment = segments[idx];
    if (idx == 0) {
      Key first;
      first.t_sec = ps.samples[static_cast<std::size_t>(segment.i)].t_sec;
      first.v = fallback_property_solver::FallbackSampleVector(ps, segment.i);
      first.temporal_ease_in =
          fallback_property_solver::FallbackDefaultEases(ps);
      first.temporal_ease_out =
          fallback_property_solver::FallbackDefaultEases(ps);
      fallback_property_solver::ApplyOutgoingKeyData(first, segment);
      result.keys.push_back(std::move(first));
    } else {
      fallback_property_solver::ApplyOutgoingKeyData(result.keys.back(),
                                                     segment);
    }

    Key next;
    next.t_sec = ps.samples[static_cast<std::size_t>(segment.j)].t_sec;
    next.v = fallback_property_solver::FallbackSampleVector(ps, segment.j);
    next.temporal_ease_in =
        fallback_property_solver::FallbackDefaultEases(ps);
    next.temporal_ease_out =
        fallback_property_solver::FallbackDefaultEases(ps);
    fallback_property_solver::ApplyIncomingKeyData(next, segment);
    result.keys.push_back(std::move(next));

    result.max_err = std::max(result.max_err, segment.fit.max_err);
    result.max_err_screen_px =
        std::max(result.max_err_screen_px, segment.fit.max_err_screen_px);
    result.converged = result.converged && segment.fit.feasible;
    result.segments.push_back(
        fallback_property_solver::ReportForSegment(segment));
  }

  return result;
}

}  // namespace bbsolver

#undef BBSOLVER_WEAK_FALLBACK_SOLVE_PROPERTY

#endif  // BBSOLVER_HAVE_DP_PLACER
