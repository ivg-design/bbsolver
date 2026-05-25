#pragma once

#include <optional>
#include <string>

#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>

#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/metrics/error_metrics.hpp"
#include "bbsolver/fit/segment_fit_ceres.hpp"

namespace bbsolver::segment_fit {

constexpr int kSegmentFitDiagnosticEventSchemaVersion = 1;

// Pure diagnostics payload builders for segment-fit decisions. These functions
// do not write logs or emit diagnostics; callers own any runtime emission.
nlohmann::json BuildSegmentFitPolicyDiagnostic(
    const std::string& request_id,
    const std::string& surface,
    int sample_i,
    int sample_j,
    bool passed,
    const SegmentFitResult& result,
    const std::optional<ErrorReport>& report = std::nullopt);

nlohmann::json BuildSegmentFitCeresAdapterDiagnostic(
    const std::string& request_id,
    const std::string& adapter,
    int sample_i,
    int sample_j,
    int dim,
    const DimCeresResult& result);

nlohmann::json BuildSegmentFitShapeTemporalDiagnostic(
    const std::string& request_id,
    int sample_i,
    int sample_j,
    const SegmentFitResult& result);

nlohmann::json BuildSegmentFitUnifiedSpatialDiagnostic(
    const std::string& request_id,
    int sample_i,
    int sample_j,
    double path_length,
    int target_count,
    const SegmentFitResult& result);

}  // namespace bbsolver::segment_fit
