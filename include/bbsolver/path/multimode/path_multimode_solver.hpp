#pragma once

#include <vector>

#include "bbsolver/domain.hpp"

#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

namespace bbsolver {

struct ShapeFlatMultiModeOptions {
  PathFrameFitOptions frame_fit_options;
  int max_regions = 4;
  int max_gap_samples = 24;
  double region_tolerance = 0.5;
  // Hard budgets keep this prototype cheap enough to run as a precheck before
  // the production replacement temporal DP. Non-positive values disable the
  // corresponding cap.
  int max_region_segment_checks = 20000;
  int max_validation_samples = 180;
  int max_validation_work_units = 80000;
  double max_candidate_key_ratio = 0.60;
  CancelFn cancel_fn = {};
};

struct ShapeFlatLandmarkSubpathOptions {
  bool enabled = false;
  int max_regions = 4;
  int max_gap_samples = 24;
  double region_tolerance = 0.5;
  int max_region_segment_checks = 20000;
  // Deep diagnostics are intentionally opt-in: they can multiply work on hard
  // noodle paths and are not needed for the fast landmark-subpath summary.
  bool diagnose_dense_runs = false;
  bool diagnose_segment_gaps = false;
  bool diagnose_outlier_slots = false;
  bool diagnose_mask_channels = false;
  bool fast_summary_only = false;
  bool emit_visible_shape_channels = false;
  bool probe_visible_channels = false;
  int visible_baseline_keys = 0;
  CancelFn cancel_fn = {};
};

// Prototype replacement-path compressor that treats contiguous outline regions
// as independent temporal modes, unions their anchor times, then emits ordinary
// full Shape Path keys at those anchors. The returned key stream is accepted
// only after full source-outline validation against original.
PropertyKeys SolveShapeFlatMultiModeTemporal(
    const PropertySamples& original,
    const PropertySamples& reduced,
    const ShapeFlatMultiModeOptions& options = {});

// Default-off protocol emitter for experimental landmark sub-path consumers.
// Each returned PropertyKeys shares reduced.property.id and contains ordinary
// shape_flat keys for one contiguous vertex region. The notes field includes
// "landmark_subpath; subpath_index=N" so downstream code can group the entries
// without changing the KeyBundle schema.
std::vector<PropertyKeys> EmitShapeFlatLandmarkSubpathKeys(
    const PropertySamples& reduced,
    const ShapeFlatLandmarkSubpathOptions& options = {});

}  // namespace bbsolver
