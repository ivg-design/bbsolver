#include "bbsolver/temporal/refit/temporal_refit_resample.hpp"

#include "bbsolver/domain.hpp"
#include "bbsolver/temporal/refit/temporal_refit_dimensions.hpp"
#include "bbsolver/temporal/refit/temporal_refit_shape.hpp"
#include "bbsolver/verify/verifier.hpp"

#include <utility>

namespace bbsolver {

PropertySamples ResampleAcceptedAtSourceTimes(
    const PropertyKeys& accepted_keys,
    const PropertySamples& source_template) {
  if (TemporalRefitIsShapeFlatProperty(source_template)) {
    return ResampleShapeFlatAcceptedAtSourceTimes(accepted_keys,
                                                 source_template);
  }

  PropertySamples out = source_template;
  out.samples.clear();
  if (TemporalRefitIsCustomProperty(source_template)) {
    return out;
  }
  out.samples.reserve(source_template.samples.size());

  for (const Sample& source_sample : source_template.samples) {
    Sample sample;
    sample.t_sec = source_sample.t_sec;
    sample.v = EvalKeysAt(accepted_keys.keys, source_sample.t_sec);
    if (!TemporalRefitValuesMatchDimensions(sample.v, source_template)) {
      out.samples.clear();
      return out;
    }
    out.samples.push_back(std::move(sample));
  }
  return out;
}

}  // namespace bbsolver
