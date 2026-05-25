#include "bbsolver/temporal/refit/temporal_refit_structural.hpp"

#include "bbsolver/domain.hpp"
#include "bbsolver/temporal/refit/temporal_refit_dimensions.hpp"
#include "bbsolver/temporal/refit/temporal_refit_shape.hpp"

#include <cmath>
#include <string>

namespace bbsolver {

std::string RefitStructuralRejection(const PropertySamples& source,
                                     const PropertyKeys& accepted_keys) {
  if (source.samples_per_frame != 1) {
    return "ineligible_subframe_samples";
  }
  if (source.samples.size() < 2 || accepted_keys.keys.empty()) {
    return "degenerate";
  }
  if (accepted_keys.keys.size() < 3) {
    // Fewer than 3 keys is already optimal under L-infinity for any
    // monotonic interval, so there is nothing to reduce.
    return "degenerate";
  }

  const double src_t0 = source.samples.front().t_sec;
  const double src_t1 = source.samples.back().t_sec;
  const double key_t0 = accepted_keys.keys.front().t_sec;
  const double key_t1 = accepted_keys.keys.back().t_sec;
  const double eps = 1e-9;
  if (std::abs(key_t0 - src_t0) > eps || std::abs(key_t1 - src_t1) > eps) {
    return "ineligible_endpoint_mismatch";
  }

  if (TemporalRefitIsShapeFlatProperty(source)) {
    if (!AllTemporalRefitSourceSamplesAreValidShapeFlat(source)) {
      return "ineligible_shape_flat_source_malformed";
    }
    if (!AllTemporalRefitShapeFlatKeysHaveStableTopology(accepted_keys)) {
      return "ineligible_shape_flat_key_topology";
    }
    return "";
  }
  if (TemporalRefitIsCustomProperty(source)) {
    return "ineligible_custom_property";
  }
  if (TemporalRefitExpectedDimensions(source) == 0) {
    return "ineligible_dimensions";
  }
  if (!AllTemporalRefitCandidateKeysMatchDimensions(accepted_keys, source)) {
    return "ineligible_dimensions";
  }
  return "";
}

}  // namespace bbsolver
