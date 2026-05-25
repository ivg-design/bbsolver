#pragma once

#include "bbsolver/domain.hpp"

#include <functional>
#include <string>
#include <vector>

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

namespace bbsolver {

using ReplacementFractionTrialProgressFn =
    std::function<void(const char* event,
                       const std::string& phase,
                       double local_fraction,
                       int frame_index,
                       int frame_total)>;

struct ReplacementFractionTrialResult {
  bool ok = false;
  PropertySamples samples;
  int fraction_count = 0;
  double max_outline_error = 0.0;
  std::vector<double> fractions;
};

ReplacementFractionTrialResult TryReplacementFractionLayout(
    const PropertySamples& property_samples,
    const std::vector<double>& fractions,
    const PathFrameFitOptions& coherence_options,
    const ReplacementFractionTrialProgressFn& progress_fn = {});

}  // namespace bbsolver
