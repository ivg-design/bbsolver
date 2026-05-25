#pragma once

#include <vector>

#include "bbsolver/domain.hpp"
#include "bbsolver/dp/dp_placer.hpp"

namespace bbsolver {

// True when the property is a spatial (animated) two-component value
// that the Motion Smooth route should treat as a single 2D curve —
// excludes shape-flat paths and properties that are spatially
// separated (X/Y on independent timelines).
bool IsMotionSmoothSpatialProperty(const PropertySamples& property_samples);

// Returns the fitted endpoint value when present, otherwise falls
// back to the corresponding sample's value vector. Used by the
// motion-smooth route to honour a segment fitter's pinned endpoint
// when one is available and the sample otherwise.
std::vector<double> SegmentEndpointValueOrSample(
    const PropertySamples& property_samples,
    const SegmentFitResult& fit,
    bool start_endpoint);

// Distinct, sorted source-key times that fall inside the property's
// active sample window. Uses a `1e-6` tolerance for both inclusion
// and dedup so neighbouring source keys that differ only by float
// noise collapse to a single entry.
std::vector<double> MotionSmoothSourceKeyTimes(
    const PropertySamples& property_samples);

// One value vector per input sample, padded to `dims` with zeros.
// The output preserves sample order and is suitable for sampling via
// MotionSmoothInterpolatedVector.
std::vector<std::vector<double>> MotionSmoothRawPoints(
    const PropertySamples& property_samples,
    int dims);

// Wrapper around motion_smooth_geometry's MotionSmoothInterpolatedPoint
// that pads the returned value to at least `dims` components. Returns
// the value at `t_sec` linearly interpolated between adjacent points.
std::vector<double> MotionSmoothInterpolatedVector(
    const PropertySamples& property_samples,
    const std::vector<std::vector<double>>& points,
    double t_sec,
    int dims);

}  // namespace bbsolver
