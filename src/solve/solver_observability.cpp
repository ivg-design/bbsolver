#include "bbsolver/solve/solver_observability.hpp"
#include "bbsolver/domain.hpp"

#include <chrono>
#include <ratio>

namespace bbsolver {

const char* PropertyName(const PropertySamples& property_samples) {
  return property_samples.property.id.empty()
      ? "<unnamed>"
      : property_samples.property.id.c_str();
}

double MillisecondsSince(std::chrono::steady_clock::time_point start) {
  return std::chrono::duration<double, std::milli>(
             std::chrono::steady_clock::now() - start)
      .count();
}

}  // namespace bbsolver
