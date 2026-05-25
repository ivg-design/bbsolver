#pragma once

#include "bbsolver/domain.hpp"

namespace bbsolver {

PropertySamples ResampleAcceptedAtSourceTimes(
    const PropertyKeys& accepted_keys,
    const PropertySamples& source_template);

}  // namespace bbsolver
