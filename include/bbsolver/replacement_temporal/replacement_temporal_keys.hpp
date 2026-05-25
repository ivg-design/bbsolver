#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/dp/dp_placer.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace bbsolver {
namespace replacement_temporal {

std::vector<TemporalEase> NeutralShapeEase();

void AppendNote(PropertyKeys& keys, const std::string& note);

bool IsShapeFlatPath(const PropertySamples& ps);

bool SameSampleTimes(const PropertySamples& a, const PropertySamples& b);

bool IsValidShapeFlat(const std::vector<double>& v);

int ShapeFlatVertexCount(const std::vector<double>& v);

std::vector<double> LerpShapeFlatChord(const std::vector<double>& a,
                                       const std::vector<double>& b,
                                       double u);

std::vector<double> ValueAt(const PropertySamples& ps, int sample_idx);

PropertyKeys AssembleReplacementKeys(const PropertySamples& reduced,
                                     const DPPlacement& placement);

bool IsAllSamplesAnchorFallback(const PropertyKeys& keys,
                                const PropertySamples& reduced);

void MarkAnchorFallbackAsHoldForExport(PropertyKeys& keys);

PropertyKeys AssembleLinearPruneKeys(
    const PropertySamples& reduced,
    const std::vector<int>& anchors,
    const std::vector<SegmentFitResult>& segments);

}  // namespace replacement_temporal
}  // namespace bbsolver
