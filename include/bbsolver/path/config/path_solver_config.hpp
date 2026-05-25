#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"

namespace bbsolver {

int PathChildMaxGap(const CompInfo& comp);

SolverConfig PathChildConfig(SolverConfig config);

double EffectivePathTolerance(const SolverConfig& config);

PathFrameFitOptions ReplacementFrameFitOptions(const SolverConfig& config);

PathFrameFitOptions VisibleOutlineFrameFitOptions(const SolverConfig& config);

PathTemporalValidationOptions ReplacementPathTemporalValidationOptions(
    const SolverConfig& config,
    bool visible_outline_reference);

SolverConfig ReplacementTemporalConfig(SolverConfig config,
                                       double frame_outline_error);

}  // namespace bbsolver
