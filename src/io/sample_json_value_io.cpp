#include "bbsolver/io/sample_json_value_io.hpp"

#include "bbsolver/domain.hpp"
#include <stdexcept>
#include <string>

namespace bbsolver {

ValueKind ParseSampleValueKindJson(const std::string& value) {
  if (value == "Scalar") return ValueKind::Scalar;
  if (value == "TwoD") return ValueKind::TwoD;
  if (value == "ThreeD") return ValueKind::ThreeD;
  if (value == "TwoD_Spatial") return ValueKind::TwoD_Spatial;
  if (value == "ThreeD_Spatial") return ValueKind::ThreeD_Spatial;
  if (value == "Color") return ValueKind::Color;
  if (value == "Custom") return ValueKind::Custom;
  throw std::runtime_error("Unknown ValueKind: " + value);
}

}  // namespace bbsolver
