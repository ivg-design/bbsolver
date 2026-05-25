#pragma once

#include "bbsolver/domain.hpp"

#include <vector>

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/path/temporal/path_temporal_influence.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"

namespace bbsolver {

std::vector<ShapeMorphProgressInterval> BuildShapeMorphProgressIntervals(
    const std::vector<bool>& accepted,
    int progress_steps);

bool ShapeMorphHasMonotoneProgressPath(
    const std::vector<std::vector<bool>>& accepted_rows);

double EvaluateShapeTemporalInfluencePairMaxError(
    const PropertySamples& original,
    int start_sample_idx,
    int end_sample_idx,
    const std::vector<ShapeFlatOutlinePolyline>& source_outlines,
    const std::vector<double>& endpoint_a,
    const std::vector<double>& endpoint_b,
    const ShapeMorphProgressBandOptions& options,
    ShapeTemporalInfluencePair pair,
    double cutoff_error,
    int* evaluations,
    double* outline_error_wall_ms);

}  // namespace bbsolver
