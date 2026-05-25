#include "bbsolver/replacement_temporal/replacement_temporal_solve_options.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/app/cli_options.hpp"

#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/path/config/path_solver_config.hpp"
#include "bbsolver/replacement_temporal/replacement_temporal_solver.hpp"
#include "bbsolver/progress/solve_cancellation.hpp"

namespace bbsolver {

ReplacementTemporalSolverOptions ReplacementTemporalOptions(
    const SolverConfig& config,
    int max_gap_samples,
    const SolveOptions& options,
    PlacementProgressFn placement_progress_fn) {
  ReplacementTemporalSolverOptions replacement_options;
  replacement_options.band_options.frame_fit_options.outline_tolerance =
      EffectivePathTolerance(config);
  // This is a segment-screening oracle, not the final acceptance gate. Keep it
  // coarse enough for interactive path bakes; stage-4 source-outline validation
  // remains the authoritative full-quality check after keys are chosen.
  replacement_options.band_options.frame_fit_options.max_subdivisions_per_segment = 8;
  replacement_options.band_options.progress_steps = 16;
  replacement_options.band_options.max_window_samples =
      max_gap_samples > 0 ? max_gap_samples + 1 : 16;
  replacement_options.band_options.max_evaluations =
      replacement_options.band_options.max_window_samples *
      (replacement_options.band_options.progress_steps + 3);
  replacement_options.max_gap_samples = max_gap_samples;
  replacement_options.cancel_fn =
      [&options]() { return CancelFileExists(options.cancel_file); };
  replacement_options.placement_progress_fn = placement_progress_fn;
  return replacement_options;
}

}  // namespace bbsolver
