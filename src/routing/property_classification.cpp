#include "bbsolver/routing/property_classification.hpp"
#include "bbsolver/domain.hpp"

namespace bbsolver {

bool IsShapeFlatPath(const PropertySamples& property_samples) {
  return property_samples.property.kind == ValueKind::Custom &&
         property_samples.property.units_label == "shape_flat";
}

bool IsUnseparatedSpatial(const PropertySamples& property_samples) {
  return property_samples.property.is_spatial &&
         !property_samples.property.is_separated;
}

}  // namespace bbsolver
