#pragma once

#include <vector>

#include "bbsolver/domain.hpp"

namespace bbsolver {

// the entry-stage validation of the shape-flat
// Motion Smooth route. Returns one of four shapes:
//
//   ok=true   → vertex_count/dims/key_times populated; caller proceeds
//   ok=false  → fallback_keys carries the early-return PropertyKeys
//               (either a `no_shape_motion_span` direct-notes bundle
//               or a ShapeFlatFrameKeyFallback result for invalid /
//               variable / no-source-key cases). Caller returns it
//               directly without further work.
struct MotionSmoothShapeFlatTopologyGateResult {
  bool ok = false;
  PropertyKeys fallback_keys;
  int vertex_count = 0;
  int dims = 0;
  std::vector<double> key_times;
};

MotionSmoothShapeFlatTopologyGateResult
ValidateMotionSmoothShapeFlatTopology(const PropertySamples& property_samples);

}  // namespace bbsolver
