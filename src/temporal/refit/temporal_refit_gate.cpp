#include "bbsolver/temporal/refit/temporal_refit_gate.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/routing/property_classification.hpp"
#include "bbsolver/routing/solve_mode_policy.hpp"

namespace bbsolver {

bool PipelineAllowsTemporalRefit(const PropertySamples& property_samples,
                                 const PropertyKeys& keys,
                                 const SolverConfig& config) {
  return SolveModeAllowsTemporal(config) &&
         !SolveModeIsMotionSmooth(config) &&
         (property_samples.property.kind != ValueKind::Custom ||
          IsShapeFlatPath(property_samples)) &&
         property_samples.samples_per_frame == 1 &&
         keys.converged &&
         keys.keys.size() >= 3;
}

}  // namespace bbsolver
