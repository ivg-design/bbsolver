#pragma once

#include "bbsolver/domain.hpp"

namespace bbsolver {

bool IsShapeFlatPath(const PropertySamples& property_samples);

bool IsUnseparatedSpatial(const PropertySamples& property_samples);

}  // namespace bbsolver
