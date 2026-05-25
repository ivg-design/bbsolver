#pragma once

#include "bbsolver/domain.hpp"

#include <string>

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

namespace bbsolver {

struct ReplacementInitialFrameScan {
  bool ok = true;
  std::string warning;
  int source_min_vertices = 0;
  int source_max_vertices = 0;
  int auto_min_vertices = 0;
  int auto_max_vertices = 0;
  int auto_changed_frames = 0;
};

ReplacementInitialFrameScan ScanReplacementInitialFrames(
    const PropertySamples& property_samples,
    const PathFrameFitOptions& frame_options);

}  // namespace bbsolver
