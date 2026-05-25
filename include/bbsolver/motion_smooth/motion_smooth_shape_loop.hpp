#pragma once

#include <cstddef>
#include <vector>

// MS1-MS5 façade re-exports. Pre-MS consumers `#include
// "motion_smooth_shape_loop.hpp"` and expect to find every symbol the
// MS1-MS5 split moved into sibling sub-headers. The four includes
// below are intentionally not used directly by this header but must
// remain so transitive symbol resolution continues to work. The
// `// IWYU pragma: keep` comments tell clangd's include-cleaner that
// the unused-include verdict is by design.
#include "bbsolver/motion_smooth/motion_smooth_shape_loop_adaptive.hpp"  // IWYU pragma: keep
#include "bbsolver/motion_smooth/motion_smooth_shape_loop_schedule.hpp"  // IWYU pragma: keep
#include "bbsolver/motion_smooth/motion_smooth_shape_quality.hpp"  // IWYU pragma: keep
#include "bbsolver/motion_smooth/motion_smooth_shape_tangent_lock.hpp"  // IWYU pragma: keep

namespace bbsolver {

std::vector<double> EvenTimesForValueCount(double start_t,
                                           double end_t,
                                           std::size_t count);

}  // namespace bbsolver
