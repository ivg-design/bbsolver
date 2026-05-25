#include "bbsolver/routing/property_classification.hpp"
#include "bbsolver/domain.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    std::abort();
  }
}

bbsolver::PropertySamples Property() {
  bbsolver::PropertySamples property_samples;
  property_samples.property.kind = bbsolver::ValueKind::Scalar;
  property_samples.property.units_label = "";
  property_samples.property.is_spatial = false;
  property_samples.property.is_separated = false;
  return property_samples;
}

void TestShapeFlatPathTrueForCustomShapeFlatUnits() {
  bbsolver::PropertySamples property_samples = Property();
  property_samples.property.kind = bbsolver::ValueKind::Custom;
  property_samples.property.units_label = "shape_flat";
  Require(bbsolver::IsShapeFlatPath(property_samples),
          "custom shape_flat property must be classified as shape-flat path");
}

void TestShapeFlatPathFalseForCustomWrongUnits() {
  bbsolver::PropertySamples property_samples = Property();
  property_samples.property.kind = bbsolver::ValueKind::Custom;
  property_samples.property.units_label = "not_shape_flat";
  Require(!bbsolver::IsShapeFlatPath(property_samples),
          "custom property with other units must not be shape-flat path");
}

void TestShapeFlatPathFalseForNonCustomShapeFlatUnits() {
  bbsolver::PropertySamples property_samples = Property();
  property_samples.property.kind = bbsolver::ValueKind::TwoD;
  property_samples.property.units_label = "shape_flat";
  Require(!bbsolver::IsShapeFlatPath(property_samples),
          "non-custom property with shape_flat units must not be shape-flat path");
}

void TestUnseparatedSpatialTrue() {
  bbsolver::PropertySamples property_samples = Property();
  property_samples.property.is_spatial = true;
  property_samples.property.is_separated = false;
  Require(bbsolver::IsUnseparatedSpatial(property_samples),
          "spatial unseparated property must be classified");
}

void TestUnseparatedSpatialFalseWhenSeparated() {
  bbsolver::PropertySamples property_samples = Property();
  property_samples.property.is_spatial = true;
  property_samples.property.is_separated = true;
  Require(!bbsolver::IsUnseparatedSpatial(property_samples),
          "spatial separated property must not be unseparated spatial");
}

void TestUnseparatedSpatialFalseWhenNonSpatial() {
  bbsolver::PropertySamples property_samples = Property();
  property_samples.property.is_spatial = false;
  property_samples.property.is_separated = false;
  Require(!bbsolver::IsUnseparatedSpatial(property_samples),
          "non-spatial property must not be unseparated spatial");
}

}  // namespace

int main() {
  TestShapeFlatPathTrueForCustomShapeFlatUnits();
  TestShapeFlatPathFalseForCustomWrongUnits();
  TestShapeFlatPathFalseForNonCustomShapeFlatUnits();
  TestUnseparatedSpatialTrue();
  TestUnseparatedSpatialFalseWhenSeparated();
  TestUnseparatedSpatialFalseWhenNonSpatial();
  std::cout << "[PASS] test_property_classification\n";
  return 0;
}
