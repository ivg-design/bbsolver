#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/dp/dp_placer.hpp"

#include <vector>

namespace bbsolver {

std::vector<double> SampleValueAt(const PropertySamples& ps,
                                  int sample_idx,
                                  int dims);

std::vector<double> SegmentStartValue(const PropertySamples& ps,
                                      const SegmentFitResult& segment,
                                      int sample_idx,
                                      int dims);

std::vector<double> SegmentEndValue(const PropertySamples& ps,
                                    const SegmentFitResult& segment,
                                    int sample_idx,
                                    int dims);

bool ValuesCompatible(const std::vector<double>& a,
                      const std::vector<double>& b);

PropertyKeys AssembleKeys(const PropertySamples& ps, const DPPlacement& pl);

}  // namespace bbsolver
