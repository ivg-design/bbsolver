#pragma once

// façade: the original solver/src/motion_smooth_shape_schedule.cpp
// (382 LOC, three independent responsibilities) was split into:
//   - motion_smooth_shape_source_key_schedule
//   - motion_smooth_shape_trajectory_smooth
//   - motion_smooth_shape_rove_schedule
// This header re-exports the three sibling headers so existing consumers
// of motion_smooth_shape_schedule.hpp keep compiling unchanged. The
// `// IWYU pragma: keep` comments tell clangd's include-cleaner that
// each re-export is intentional even though no symbol from the
// sub-header is referenced in this façade body.

#include "bbsolver/motion_smooth/motion_smooth_shape_rove_schedule.hpp"  // IWYU pragma: keep
#include "bbsolver/motion_smooth/motion_smooth_shape_source_key_schedule.hpp"  // IWYU pragma: keep
#include "bbsolver/motion_smooth/motion_smooth_shape_trajectory_smooth.hpp"  // IWYU pragma: keep
