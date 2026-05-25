#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/temporal/refit/temporal_refit.hpp"

#include <string>

namespace bbsolver {

void EmitTemporalRefitProgress(const TemporalRefitOptions& options,
                               const std::string& stage,
                               int step_index,
                               int step_total);

bool TemporalRefitCancelled(const TemporalRefitOptions& options);

std::string BuildTemporalRefitNotes(const TemporalRefitResult& result);

std::string TemporalRefitValidationNote(const PropertySamples& source);

}  // namespace bbsolver
