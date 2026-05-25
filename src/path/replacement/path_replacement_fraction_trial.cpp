#include "bbsolver/path/replacement/path_replacement_fraction_trial.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/shape/shape_flat_topology.hpp"

namespace bbsolver {

ReplacementFractionTrialResult TryReplacementFractionLayout(
    const PropertySamples& property_samples,
    const std::vector<double>& fractions,
    const PathFrameFitOptions& coherence_options,
    const ReplacementFractionTrialProgressFn& progress_fn) {
  ReplacementFractionTrialResult result;
  result.samples = property_samples;
  result.samples.samples.clear();
  result.samples.samples.reserve(property_samples.samples.size());
  result.fraction_count = static_cast<int>(fractions.size());
  result.fractions = fractions;

  const int trial_frame_total =
      static_cast<int>(property_samples.samples.size());
  const int trial_stride = std::max(1, trial_frame_total / 24);
  for (int sample_idx = 0; sample_idx < trial_frame_total; ++sample_idx) {
    const Sample& sample =
        property_samples.samples[static_cast<std::size_t>(sample_idx)];
    PathFrameFitResult frac_fit =
        FitShapeFlatFrameAtFractions(sample.v, fractions, coherence_options);
    const double frame_err = frac_fit.max_outline_error;
    result.max_outline_error = std::max(result.max_outline_error, frame_err);
    if (!frac_fit.ok || !frac_fit.applied ||
        frac_fit.fitted_vertex_count != result.fraction_count ||
        ShapeFlatVertexCount(frac_fit.fitted) != result.fraction_count ||
        frame_err > coherence_options.outline_tolerance + 1e-9) {
      result.ok = false;
      return result;
    }
    Sample accepted;
    accepted.t_sec = sample.t_sec;
    accepted.v = std::move(frac_fit.fitted);
    result.samples.samples.push_back(std::move(accepted));
    if (progress_fn &&
        (sample_idx == 0 ||
         sample_idx + 1 == trial_frame_total ||
         ((sample_idx + 1) % trial_stride) == 0)) {
      progress_fn("path_replacement_target_layout_progress",
                  "Testing replacement fraction layout",
                  0.30 + 0.45 *
                             (static_cast<double>(sample_idx + 1) /
                              static_cast<double>(
                                  std::max(1, trial_frame_total))),
                  sample_idx + 1,
                  trial_frame_total);
    }
  }
  result.ok = true;
  return result;
}

}  // namespace bbsolver
