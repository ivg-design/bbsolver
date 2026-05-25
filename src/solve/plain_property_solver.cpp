#include "bbsolver/solve/plain_property_solver.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/app/cli_options.hpp"

#include <algorithm>
#include <cmath>

#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/path/config/path_gap_policy.hpp"
#include "bbsolver/routing/property_classification.hpp"
#include "bbsolver/fit/segment_fitter.hpp"
#include "bbsolver/progress/solve_cancellation.hpp"

namespace bbsolver {

PropertyKeys SolvePlainProperty(const PropertySamples& property_samples,
                                const SolverConfig& config,
                                const CompInfo& comp,
                                const SolveOptions& options,
                                int max_gap_samples,
                                PlacementProgressFn progress_fn) {
  int effective_max_gap_samples = max_gap_samples;
  if (effective_max_gap_samples <= 0 &&
      IsShapeFlatPath(property_samples) &&
      property_samples.property.dimensions > 64) {
    const bool path_specific_fit_enabled =
        config.allow_shape_temporal_bezier ||
        config.allow_path_spatial_fit ||
        config.allow_path_replacement_fit;
    const int fps_one_second =
        static_cast<int>(std::round(std::max(1.0, comp.fps)));
    effective_max_gap_samples = path_specific_fit_enabled
        ? PathSpecificMaxGap(comp, config)
        : std::max(12, std::min(60, fps_one_second));
  }
  return SolveProperty(
      property_samples,
      config,
      comp,
      FitSegment,
      [&options]() { return CancelFileExists(options.cancel_file); },
      effective_max_gap_samples,
      progress_fn);
}

}  // namespace bbsolver
