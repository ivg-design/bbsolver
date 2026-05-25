#pragma once

#include "bbsolver/domain.hpp"

#include <functional>
#include <limits>
#include <vector>

namespace bbsolver {

struct ErrorReport {
  double max_err = 0.0;
  double max_err_screen_px = 0.0;
  double rms_err = 0.0;
  int worst_sample_idx = -1;
  int units_evaluated = 0;
  bool fail_fast_exit = false;
  double shape_outline_wall_ms = 0.0;
};

ErrorReport ComputeError(
    const PropertySamples& ps,
    int i,
    int j,
    const std::function<std::vector<double>(double t)>& reconstruct,
    const SolverConfig& cfg,
    const CompInfo& comp,
    const LayerXform* layer_xform_opt = nullptr,
    double fail_fast_property_ceiling = std::numeric_limits<double>::infinity(),
    double fail_fast_screen_ceiling = std::numeric_limits<double>::infinity(),
    bool collect_attribution = false);

}  // namespace bbsolver
