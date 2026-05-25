#pragma once

#include "bbsolver/domain.hpp"

#include <chrono>

namespace bbsolver {

const char* PropertyName(const PropertySamples& property_samples);

double MillisecondsSince(std::chrono::steady_clock::time_point start);

}  // namespace bbsolver
