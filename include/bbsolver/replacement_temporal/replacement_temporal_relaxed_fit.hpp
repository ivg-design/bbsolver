#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"

#include <vector>

namespace bbsolver {
namespace replacement_temporal {

struct RelaxedEndpointFit {
  bool ok = false;
  std::vector<double> endpoint_a;
  std::vector<double> endpoint_b;
};

struct RelaxedValidation {
  bool ok = false;
  double max_err = 0.0;
  double rms_err = 0.0;
  int outline_checks = 0;
  double outline_wall_ms = 0.0;
};

std::vector<TemporalEase> ShapeEaseForInfluence(double influence);

void AddReplacementFitAttribution(SegmentFitResult& dst,
                                  const SegmentFitResult& src);

std::vector<double> SegmentProgressValues(
    const PropertySamples& ps,
    int i,
    int j,
    bool bezier,
    TemporalEase ease_out,
    TemporalEase ease_in,
    const ShapeMorphProgressBandOptions& options);

RelaxedValidation ValidateRelaxedChord(
    const PropertySamples& original,
    int i,
    int j,
    const std::vector<double>& endpoint_a,
    const std::vector<double>& endpoint_b,
    const std::vector<double>& progress,
    const ShapeMorphProgressBandOptions& options);

SegmentFitResult TryRelaxedReplacementSegment(
    int i,
    int j,
    const PropertySamples& reduced,
    const PropertySamples& original,
    const SolverConfig& config,
    const ShapeMorphProgressBandOptions& band_options,
    const ShapeMorphProgressBandResult& strict_oracle);

}  // namespace replacement_temporal
}  // namespace bbsolver
