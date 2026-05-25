#include "bbsolver/path/config/path_gap_policy.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/runtime/runtime_env.hpp"

#include <algorithm>
#include <cmath>

namespace bbsolver {

int InteractivePathMaxGap(const CompInfo& comp) {
  return std::max(6, static_cast<int>(std::round(std::max(1.0, comp.fps) / 3.0)));
}

int PathSpecificMaxGap(const CompInfo& comp,
                       const SolverConfig& config) {
  if (config.path_specific_max_gap > 0) {
    return config.path_specific_max_gap;
  }
  const int env_override = EnvPositiveInt("BBSOLVER_PATH_SPECIFIC_MAX_GAP");
  if (env_override > 0) {
    return env_override;
  }
  return std::max(6, std::min(8, InteractivePathMaxGap(comp)));
}

}  // namespace bbsolver
