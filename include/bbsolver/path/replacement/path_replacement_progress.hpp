#pragma once

#include "bbsolver/domain.hpp"

#include <cstddef>

#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>

#include "bbsolver/dp/dp_placer.hpp"

namespace bbsolver {

nlohmann::json ReplacementBaselineStartProgressEvent(
    const PropertySamples& original,
    std::size_t property_idx,
    std::size_t property_count);

nlohmann::json ReplacementBaselinePlacementProgressEvent(
    const PropertySamples& original,
    std::size_t property_idx,
    std::size_t property_count,
    const PlacementProgress& placement);

nlohmann::json ReplacementBaselineDoneProgressEvent(
    const PropertySamples& original,
    std::size_t property_idx,
    std::size_t property_count,
    std::size_t key_count,
    double max_error,
    bool converged);

nlohmann::json ReplacementValidationStartProgressEvent(
    const PropertySamples& candidate,
    std::size_t property_idx,
    std::size_t property_count);

nlohmann::json ReplacementRetryStartProgressEvent(
    const PropertySamples& original,
    std::size_t property_idx,
    std::size_t property_count,
    int retry,
    int target_vertices);

nlohmann::json ReplacementRetryDoneProgressEvent(
    const PropertySamples& original,
    std::size_t property_idx,
    std::size_t property_count,
    int retry,
    int target_vertices,
    int fitted_vertices,
    double max_outline_error,
    bool accepted,
    bool sharp_corners_ok);

nlohmann::json ReplacementFastVertexValidationDoneProgressEvent(
    const PropertySamples& candidate,
    std::size_t property_idx,
    std::size_t property_count,
    double max_outline_error,
    int samples_checked,
    std::size_t candidate_key_count,
    bool sharp_corners_ok);

nlohmann::json ReplacementValidationDoneProgressEvent(
    const PropertySamples& candidate,
    std::size_t property_idx,
    std::size_t property_count,
    bool ok,
    double max_outline_error,
    int samples_checked,
    std::size_t candidate_key_count,
    std::size_t source_key_count,
    bool sharp_corners_ok);

}  // namespace bbsolver
