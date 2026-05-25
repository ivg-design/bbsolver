#pragma once

#include "bbsolver/domain.hpp"

#include <string>

namespace bbsolver {

std::string RefitStructuralRejection(const PropertySamples& source,
                                     const PropertyKeys& accepted_keys);

}  // namespace bbsolver
