#pragma once

#include "bbsolver/domain.hpp"

#include <string>

namespace bbsolver {

struct ShapeFlatNearOptimalResult {
  bool applied = false;
  PropertyKeys keys;
  std::string note;
  int source_key_count = 0;
  int source_vertices = 0;
  int source_samples = 0;
};

struct ShapeMotionReductionGateResult {
  bool attempted = false;
  bool rejected = false;
  PropertyKeys preserved_keys;
  std::string note;
};

ShapeMotionReductionGateResult GateShapeMotionQualityRegression(
    const PropertySamples& property_samples,
    const PropertyKeys& candidate_keys,
    const SolverConfig& config);

ShapeFlatNearOptimalResult TryShapeFlatAlreadyNearOptimalFastPath(
    const PropertySamples& property_samples,
    const SolverConfig& config,
    const CompInfo& comp);

}  // namespace bbsolver
