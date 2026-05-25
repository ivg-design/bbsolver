#include "bbsolver/path/temporal/path_temporal_influence.hpp"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <vector>

#include "bbsolver/path/temporal/path_temporal_progress.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"

namespace bbsolver {
namespace {

void AddInfluenceCandidate(std::vector<ShapeTemporalInfluencePair>& candidates,
                           double out_influence,
                           double in_influence,
                           const ShapeMorphProgressBandOptions& options) {
  const double out =
      ClampShapeTemporalInfluencePercent(out_influence,
                                         options.min_bezier_influence,
                                         options.max_bezier_influence);
  const double in =
      ClampShapeTemporalInfluencePercent(in_influence,
                                         options.min_bezier_influence,
                                         options.max_bezier_influence);
  for (const ShapeTemporalInfluencePair& candidate : candidates) {
    if (ShapeTemporalInfluencesAlmostSame(candidate.out_influence, out) &&
        ShapeTemporalInfluencesAlmostSame(candidate.in_influence, in)) {
      return;
    }
  }
  candidates.push_back({out, in});
}

std::vector<double> UniformInfluenceValues(
    const ShapeMorphProgressBandOptions& options) {
  const double lo =
      ClampShapeTemporalInfluencePercent(options.min_bezier_influence,
                                         options.min_bezier_influence,
                                         options.max_bezier_influence);
  const double hi =
      ClampShapeTemporalInfluencePercent(options.max_bezier_influence,
                                         options.min_bezier_influence,
                                         options.max_bezier_influence);
  const int steps = std::max(2, options.bezier_influence_grid_steps);
  std::vector<double> values;
  values.reserve(static_cast<std::size_t>(steps + 1));
  for (int idx = 0; idx < steps; ++idx) {
    const double alpha =
        steps == 1 ? 0.0 : static_cast<double>(idx) /
                                  static_cast<double>(steps - 1);
    values.push_back(lo + (hi - lo) * alpha);
  }
  values.push_back(33.3);
  std::sort(values.begin(), values.end());
  values.erase(std::unique(values.begin(),
                           values.end(),
                           [](double a, double b) {
                             return ShapeTemporalInfluencesAlmostSame(a, b);
                           }),
               values.end());
  return values;
}

}  // namespace

bool ShapeTemporalInfluencesAlmostSame(double a, double b) {
  return std::abs(a - b) <= 1e-6;
}

std::vector<ShapeTemporalInfluencePair>
BuildInitialShapeTemporalInfluenceCandidates(
    const ShapeMorphProgressBandOptions& options) {
  std::vector<ShapeTemporalInfluencePair> candidates;
  const double lo =
      ClampShapeTemporalInfluencePercent(options.min_bezier_influence,
                                         options.min_bezier_influence,
                                         options.max_bezier_influence);
  const double hi =
      ClampShapeTemporalInfluencePercent(options.max_bezier_influence,
                                         options.min_bezier_influence,
                                         options.max_bezier_influence);
  const double q1 = lo + (hi - lo) * 0.25;
  const double mid = lo + (hi - lo) * 0.5;
  const double q3 = lo + (hi - lo) * 0.75;

  AddInfluenceCandidate(candidates, q3, q1, options);
  AddInfluenceCandidate(candidates, q1, q3, options);
  AddInfluenceCandidate(candidates, 90.0, 10.0, options);
  AddInfluenceCandidate(candidates, 10.0, 90.0, options);
  AddInfluenceCandidate(candidates, 80.0, 20.0, options);
  AddInfluenceCandidate(candidates, 20.0, 80.0, options);
  AddInfluenceCandidate(candidates, mid, mid, options);
  AddInfluenceCandidate(candidates, hi, lo, options);
  AddInfluenceCandidate(candidates, lo, hi, options);
  AddInfluenceCandidate(candidates, q3, q3, options);
  AddInfluenceCandidate(candidates, q1, q1, options);

  const std::vector<double> values = UniformInfluenceValues(options);
  for (double out : values) {
    for (double in : values) {
      AddInfluenceCandidate(candidates, out, in, options);
    }
  }
  return candidates;
}

}  // namespace bbsolver
