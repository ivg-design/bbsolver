#pragma once

#include "bbsolver/domain.hpp"

#include <functional>
#include <string>
#include <vector>

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/path/replacement/path_replacement_fraction_layout.hpp"

namespace bbsolver {

using ReplacementTargetProgressFn =
    std::function<void(const char* event,
                       const std::string& phase,
                       double local_fraction,
                       int frame_index,
                       int frame_total)>;

struct ReplacementPhase2FitResult {
  bool ok = true;
  std::string warning;
  std::vector<ReplacementFrameFitRecord> records;
};

ReplacementPhase2FitResult FitReplacementPhase2Records(
    const PropertySamples& property_samples,
    const PathFrameFitOptions& targeted_options,
    int target_vertices,
    const ReplacementTargetProgressFn& progress_fn = {});

}  // namespace bbsolver
