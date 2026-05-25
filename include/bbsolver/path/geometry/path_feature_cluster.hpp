#pragma once

// Across-frame feature-anchor clustering used by the canonical-layout builder
// (BuildShapeFlatFeatureFractionLayout, declared in path_frame_fit.hpp).
//
// Pure layout / pure geometry leaf: no DiagnosticsWriter, no progress,
// no acceptance state. Diagnostics ownership: **caller-owned** — the
// canonical-layout builder surfaces cluster-derived counts through the
// PathFeatureFractionLayoutResult.notes string.

#include <vector>


namespace bbsolver {
namespace pff_cluster {

// One per-frame observation of a feature anchor. `frame_index` is the source
// frame's position in the input sequence; `fraction` is the normalized
// outline fraction in that frame.
struct FeatureObservation {
  double fraction = 0.0;
  int frame_index = -1;
  double turn_radians = 0.0;
  bool zero_tangent_cue = false;
};

// One cluster of observations. `fraction` is the cluster centroid (closed
// paths use circular mean over the unit interval); the count fields are
// aggregated across the observation set.
struct FeatureClusterRecord {
  double fraction = 0.0;
  int observed_frame_count = 0;
  int observation_count = 0;
  int zero_tangent_count = 0;
  double max_turn_radians = 0.0;
};

// Circular-mean centroid for a set of fractions. Open paths fall back to
// arithmetic mean; closed paths use atan2 of the unit-circle embedding and
// snap to the seam when the centroid lands within kFractionEpsilon.
double ClusterFractionCenter(const std::vector<double>& fractions, bool closed);

// Greedily group observations into clusters using the supplied `radius` in
// normalized outline-fraction units. Closed paths additionally merge wrap-
// around clusters whose seam-spanning gap is within `radius`. Output is
// sorted by fraction.
std::vector<FeatureClusterRecord> ClusterFeatureObservations(
    std::vector<FeatureObservation> observations,
    bool closed,
    double radius);

// Minimum observed-frame count for a cluster to be treated as "persistent"
// across the input sequence. Constant 1 for very short sequences (≤ 2
// frames); otherwise the smaller of 2 or ⌈0.35 * frame_count⌉.
int PersistentFeatureFrameThreshold(int frame_count);

// Cluster sort key used by the canonical-layout builder. Lexicographic over
// (observed_frame_count desc, zero_tangent_count desc, max_turn_radians
// asc, clamped at kPi).
double FeatureClusterScore(const FeatureClusterRecord& cluster);

}  // namespace pff_cluster
}  // namespace bbsolver
