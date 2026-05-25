#pragma once

#include "bbsolver/domain.hpp"

#include <string>

namespace bbsolver::path_multimode {

struct ShapeFlatInputValidation {
  bool ok = false;
  int vertex_count = 0;
  std::string note;
};

ShapeFlatInputValidation ValidateShapeFlatMultiModeInputs(
    const PropertySamples& original,
    const PropertySamples& reduced);

ShapeFlatInputValidation ValidateShapeFlatLandmarkInput(
    const PropertySamples& reduced);

}  // namespace bbsolver::path_multimode
