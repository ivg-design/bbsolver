#include "bbsolver/path/replacement/path_replacement_initial_scan.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <limits>
#include <string>

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/shape/shape_flat_topology.hpp"

namespace bbsolver {

ReplacementInitialFrameScan ScanReplacementInitialFrames(
    const PropertySamples& property_samples,
    const PathFrameFitOptions& frame_options) {
  ReplacementInitialFrameScan scan;
  scan.source_min_vertices = std::numeric_limits<int>::max();
  scan.auto_min_vertices = std::numeric_limits<int>::max();

  for (const Sample& sample: property_samples.samples) {
    const int source_vertices = ShapeFlatVertexCount(sample.v);
    if (source_vertices <= 0) {
      scan.ok = false;
      scan.warning =
          "path_replacement_fit skipped: malformed shape_flat sample at t=" +
          std::to_string(sample.t_sec);
      return scan;
    }

    PathFrameFitResult frame_fit =
        FitShapeFlatFrame(sample.v, frame_options);
    if (!frame_fit.ok) {
      scan.ok = false;
      scan.warning = "path_replacement_fit skipped: " + frame_fit.warning +
                     " at t=" + std::to_string(sample.t_sec);
      return scan;
    }

    const int auto_vertices =
        frame_fit.applied ? frame_fit.fitted_vertex_count: source_vertices;
    scan.source_min_vertices =
        std::min(scan.source_min_vertices, source_vertices);
    scan.source_max_vertices =
        std::max(scan.source_max_vertices, source_vertices);
    scan.auto_min_vertices = std::min(scan.auto_min_vertices, auto_vertices);
    scan.auto_max_vertices = std::max(scan.auto_max_vertices, auto_vertices);
    if (auto_vertices < source_vertices) {
      ++scan.auto_changed_frames;
    }
  }

  if (property_samples.samples.empty()) {
    scan.source_min_vertices = 0;
    scan.auto_min_vertices = 0;
  }
  return scan;
}

}  // namespace bbsolver
