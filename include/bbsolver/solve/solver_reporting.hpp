#pragma once

#include "bbsolver/domain.hpp"

#include <iosfwd>
#include <string>

namespace bbsolver {

// Append a solver note to `keys.notes`, joining with "; " and skipping
// duplicates that already appear anywhere in the existing notes string.
void AppendSolverNote(PropertyKeys& keys, const std::string& note);

// Append a note fragment to a plain string, joining with "; " and preserving
// duplicates. Empty additions are ignored.
void AppendJoinedNote(std::string& notes, const std::string& note);

// Annotate a solver note string with the preservation status of source
// key timing for the given output key count.
void AppendSampleTimingNote(std::string& note,
                            int keys_count,
                            int preserved_timing_count);

// Build the accuracy-gate note advising tolerance changes when the solver
// produced few or no reductions relative to the source sample count.
// Returns an empty string when no advisory is warranted.
std::string AccuracyGateOptimizationNote(
    const PropertySamples& source_samples,
    const SolverConfig& config,
    const PropertyKeys& keys);

void WarnUnifiedSpatialPropertyIfNeeded(const PropertySamples& property_samples,
                                        std::ostream& out);

void ApplyFinalStaticTrimNote(PropertyKeys& keys, const std::string& note);

}  // namespace bbsolver
