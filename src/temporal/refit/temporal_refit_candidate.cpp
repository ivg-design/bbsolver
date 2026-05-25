#include "bbsolver/temporal/refit/temporal_refit_candidate.hpp"

#include "bbsolver/domain.hpp"

#include <utility>

namespace bbsolver {

PropertyKeys TwoEndpointCandidate(const PropertySamples& samples) {
  PropertyKeys keys;
  keys.property_id = samples.property.id;
  if (samples.samples.size() < 2) {
    return keys;
  }

  Key first;
  first.t_sec = samples.samples.front().t_sec;
  first.v = samples.samples.front().v;
  first.interp_in = InterpType::Linear;
  first.interp_out = InterpType::Linear;

  Key last;
  last.t_sec = samples.samples.back().t_sec;
  last.v = samples.samples.back().v;
  last.interp_in = InterpType::Linear;
  last.interp_out = InterpType::Linear;

  keys.keys = {std::move(first), std::move(last)};
  return keys;
}

}  // namespace bbsolver
