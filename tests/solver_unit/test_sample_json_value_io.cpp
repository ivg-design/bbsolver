#include "bbsolver/io/sample_json_value_io.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

namespace {

void TestValueKindStrings() {
  assert(bbsolver::ParseSampleValueKindJson("Scalar") ==
         bbsolver::ValueKind::Scalar);
  assert(bbsolver::ParseSampleValueKindJson("TwoD") ==
         bbsolver::ValueKind::TwoD);
  assert(bbsolver::ParseSampleValueKindJson("ThreeD") ==
         bbsolver::ValueKind::ThreeD);
  assert(bbsolver::ParseSampleValueKindJson("TwoD_Spatial") ==
         bbsolver::ValueKind::TwoD_Spatial);
  assert(bbsolver::ParseSampleValueKindJson("ThreeD_Spatial") ==
         bbsolver::ValueKind::ThreeD_Spatial);
  assert(bbsolver::ParseSampleValueKindJson("Color") ==
         bbsolver::ValueKind::Color);
  assert(bbsolver::ParseSampleValueKindJson("Custom") ==
         bbsolver::ValueKind::Custom);
}

void TestUnknownValueKindThrows() {
  bool threw = false;
  try {
    (void)bbsolver::ParseSampleValueKindJson("Nope");
  } catch (const std::runtime_error& err) {
    threw = std::string(err.what()) == "Unknown ValueKind: Nope";
  }
  assert(threw);
}

}  // namespace

int main() {
  TestValueKindStrings();
  TestUnknownValueKindThrows();
  return 0;
}
