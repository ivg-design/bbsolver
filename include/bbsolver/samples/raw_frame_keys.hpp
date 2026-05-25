#pragma once

#include "bbsolver/domain.hpp"

#include <string>

namespace bbsolver {

PropertyKeys ShapeFlatFrameKeyFallback(const PropertySamples& property_samples,
                                       const std::string& note);

PropertyKeys RawFrameKeyFallback(const PropertySamples& property_samples,
                                 const std::string& note);

}  // namespace bbsolver
