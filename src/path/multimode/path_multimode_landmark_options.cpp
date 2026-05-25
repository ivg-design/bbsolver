#include "bbsolver/path/multimode/path_multimode_landmark_options.hpp"
#include "bbsolver/path/multimode/path_multimode_solver.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>

namespace bbsolver {
namespace path_multimode {

ShapeFlatLandmarkSubpathOptions NormalizeLandmarkSubpathOptions(
    ShapeFlatLandmarkSubpathOptions options) {
  const bool explicit_deep =
      options.diagnose_dense_runs ||
      options.diagnose_segment_gaps ||
      options.diagnose_outlier_slots ||
      options.diagnose_mask_channels;
  const char* env = std::getenv("BBSOLVER_LANDMARK_DIAGNOSTICS");
  if (env != nullptr) {
    const std::string mode(env);
    if (mode == "deep" || mode == "full" || mode == "1" || mode == "true") {
      options.diagnose_dense_runs = true;
      options.diagnose_segment_gaps = true;
      options.diagnose_outlier_slots = true;
      options.diagnose_mask_channels = true;
      options.fast_summary_only = false;
    } else if (!explicit_deep &&
               (mode == "fast" || mode == "summary" || mode == "0" ||
                mode == "false" || mode == "off")) {
      options.diagnose_dense_runs = false;
      options.diagnose_segment_gaps = false;
      options.diagnose_outlier_slots = false;
      options.diagnose_mask_channels = false;
      options.fast_summary_only = true;
    }
  }
  if (options.diagnose_mask_channels) {
    options.diagnose_outlier_slots = true;
  }
  const char* protocol_env = std::getenv("BBSOLVER_LANDMARK_PROTOCOL");
  if (protocol_env != nullptr) {
    const std::string protocol(protocol_env);
    if (protocol == "shape_channel" || protocol == "visible_shape_channel") {
      options.emit_visible_shape_channels = true;
    } else if (protocol == "landmark" || protocol == "diagnostic") {
      options.emit_visible_shape_channels = false;
    }
  }
  const char* visible_probe_env = std::getenv("BBSOLVER_VISIBLE_CHANNEL_PROBE");
  if (visible_probe_env != nullptr) {
    const std::string probe(visible_probe_env);
    options.probe_visible_channels =
        probe == "1" || probe == "true" || probe == "yes" || probe == "on";
  }
  const char* visible_baseline_env =
      std::getenv("BBSOLVER_VISIBLE_CHANNEL_BASELINE");
  if (visible_baseline_env != nullptr) {
    try {
      options.visible_baseline_keys =
          std::max(0, std::stoi(std::string(visible_baseline_env)));
    } catch (...) {
      options.visible_baseline_keys = 0;
    }
  }
  return options;
}

}  // namespace path_multimode
}  // namespace bbsolver
