#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/replacement_temporal/replacement_temporal_solver.hpp"

namespace bbsolver {
namespace replacement_temporal {

bool ForwardLongestSpanEligible(
    const PropertySamples& original,
    const PropertySamples& reduced,
    const ReplacementTemporalSolverOptions& options);

SegmentFitResult FitForwardLongestSpanLinearSegment(
    int i,
    int j,
    const PropertySamples& reduced,
    const PropertySamples& original,
    const PathFrameFitOptions& frame_fit_options);

DPPlacement SolveForwardLongestSpanCandidate(
    const PropertySamples& original,
    const PropertySamples& reduced,
    const ReplacementTemporalSolverOptions& options);

PropertyKeys MaybeUseForwardLongestSpanCandidate(
    const PropertySamples& original,
    const PropertySamples& reduced,
    const PropertyKeys& current,
    const ReplacementTemporalSolverOptions& options);

}  // namespace replacement_temporal
}  // namespace bbsolver
