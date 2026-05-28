#include "bbsolver/path/multimode/path_multimode_input_validation.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/path/multimode/path_multimode_geometry.hpp"

namespace bbsolver::path_multimode {

ShapeFlatInputValidation ValidateShapeFlatMultiModeInputs(
    const PropertySamples& original,
    const PropertySamples& reduced) {
  ShapeFlatInputValidation out;
  if (!IsShapeFlatPath(original) || !IsShapeFlatPath(reduced)) {
    out.note = "shape_multimode_requires_shape_flat";
    return out;
  }
  if (original.samples.empty() || reduced.samples.empty()) {
    out.note = "shape_multimode_no_samples";
    return out;
  }
  if (!SameSampleTimes(original, reduced)) {
    out.note = "shape_multimode_sample_time_mismatch";
    return out;
  }

  const int vertex_count = ShapeFlatVertexCount(reduced.samples.front().v);
  if (vertex_count <= 0) {
    out.note = "shape_multimode_malformed_topology";
    return out;
  }
  for (const Sample& sample: reduced.samples) {
    if (!SameShapeFlatTopology(reduced.samples.front().v, sample.v)) {
      out.note = "shape_multimode_unstable_topology";
      return out;
    }
  }

  out.ok = true;
  out.vertex_count = vertex_count;
  return out;
}

ShapeFlatInputValidation ValidateShapeFlatLandmarkInput(
    const PropertySamples& reduced) {
  ShapeFlatInputValidation out;
  if (!IsShapeFlatPath(reduced) || reduced.samples.empty()) {
    return out;
  }

  const int vertex_count = ShapeFlatVertexCount(reduced.samples.front().v);
  if (vertex_count <= 0) {
    return out;
  }
  for (const Sample& sample: reduced.samples) {
    if (!SameShapeFlatTopology(reduced.samples.front().v, sample.v)) {
      return out;
    }
  }

  out.ok = true;
  out.vertex_count = vertex_count;
  return out;
}

}  // namespace bbsolver::path_multimode
