#include "bbsolver/motion_smooth/motion_smooth_dispatch.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/app/cli_options.hpp"

#include "bbsolver/motion_smooth/motion_smooth_endpoint_keys.hpp"
#include "bbsolver/motion_smooth/motion_path_smooth_spatial_trajectory.hpp"
#include "bbsolver/motion_smooth/motion_smooth_sample_points.hpp"
#include "bbsolver/motion_smooth/motion_smooth_shape_flat.hpp"
#include "bbsolver/motion_smooth/motion_smooth_spatial_trajectory.hpp"
#include "bbsolver/routing/property_classification.hpp"
#include "bbsolver/routing/solve_mode_policy.hpp"

namespace bbsolver {

PropertyKeys MotionSmoothKeys(const PropertySamples& property_samples,
                              const SolverConfig& config,
                              const CompInfo& comp,
                              const SolveOptions& options) {
  if (IsMotionSmoothSpatialProperty(property_samples)) {
    (void)comp;
    (void)options;
    if (SolveModeIsMotionPathSmooth(config)) {
      return MotionPathSmoothSpatialTrajectoryKeys(property_samples, config);
    }
    return MotionSmoothSpatialTrajectoryKeys(property_samples, config);
  }
  if (IsShapeFlatPath(property_samples)) {
    (void)comp;
    (void)options;
    return MotionSmoothShapeFlatTrajectoryKeys(property_samples, config);
  }

  return MotionSmoothEndpointKeys(property_samples, config);
}

}  // namespace bbsolver
