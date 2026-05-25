#include "bbsolver/path/replacement/path_replacement_baseline_solve.hpp"

#include "bbsolver/domain.hpp"
#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/path/replacement/path_replacement_progress.hpp"
#include "bbsolver/solve/plain_property_solver.hpp"
#include "bbsolver/progress/progress.hpp"

namespace bbsolver {

ReplacementBaselineSolveResult SolveReplacementBaseline(
    const ReplacementBaselineSolveRequest& request) {
  ReplacementBaselineSolveResult result;
  request.progress->Emit(
      ReplacementBaselineStartProgressEvent(
          *request.original_property_samples,
          request.property_idx,
          request.property_count));

  SolverConfig original_temporal_config = *request.config;
  original_temporal_config.allow_path_replacement_fit = false;
  auto baseline_progress =
      [&](const PlacementProgress& placement) {
        request.progress->Emit(
            ReplacementBaselinePlacementProgressEvent(
                *request.original_property_samples,
                request.property_idx,
                request.property_count,
                placement));
      };
  result.keys = SolvePlainProperty(
      *request.original_property_samples,
      original_temporal_config,
      *request.comp,
      *request.options,
      0,
      baseline_progress);
  if (result.keys.notes == "cancelled" ||
      (request.cancel_fn && request.cancel_fn())) {
    result.cancelled = true;
    result.cancel_phase = "path_replacement_baseline";
    return result;
  }

  request.progress->Emit(
      ReplacementBaselineDoneProgressEvent(
          *request.original_property_samples,
          request.property_idx,
          request.property_count,
          result.keys.keys.size(),
          result.keys.max_err,
          result.keys.converged));
  return result;
}

}  // namespace bbsolver
