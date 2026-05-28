// Implements the across-frame feature-anchor cluster helpers declared in
// path_feature_cluster.hpp. Behavior is byte-faithful with the previous
// anonymous-namespace definitions in path_frame_fit.cpp.
//
// Diagnostics decision: **none / pure layout**. Pure geometric clustering
// + arithmetic helpers. No DiagnosticsWriter, no progress, no cancellation,
// no operator state. Caller-owned diagnostics via the canonical-layout
// builder's notes string.

#include "bbsolver/path/geometry/path_feature_cluster.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <vector>

#include "bbsolver/path/geometry/path_fraction_helpers.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit_geometry.hpp"

namespace bbsolver {
namespace pff_cluster {

double ClusterFractionCenter(const std::vector<double>& fractions, bool closed) {
  if (fractions.empty()) {
    return 0.0;
  }
  if (!closed) {
    const double sum = std::accumulate(fractions.begin(), fractions.end(), 0.0);
    return std::clamp(sum / static_cast<double>(fractions.size()), 0.0, 1.0);
  }

  double x = 0.0;
  double y = 0.0;
  for (double fraction: fractions) {
    const double angle = 2.0 * pff_geom::kPi * fraction;
    x += std::cos(angle);
    y += std::sin(angle);
  }
  if (std::abs(x) <= 1e-12 && std::abs(y) <= 1e-12) {
    return 0.0;
  }
  double center = std::atan2(y, x) / (2.0 * pff_geom::kPi);
  if (center < 0.0) {
    center += 1.0;
  }
  if (center >= 1.0 - pff_fractions::kFractionEpsilon ||
      center < pff_fractions::kFractionEpsilon) {
    center = 0.0;
  }
  return center;
}

std::vector<FeatureClusterRecord> ClusterFeatureObservations(
    std::vector<FeatureObservation> observations,
    bool closed,
    double radius) {
  observations.erase(
      std::remove_if(observations.begin(), observations.end(), [](const FeatureObservation& obs) {
        return !std::isfinite(obs.fraction);
      }),
      observations.end());
  if (observations.empty()) {
    return {};
  }
  for (FeatureObservation& obs: observations) {
    if (closed) {
      obs.fraction = obs.fraction - std::floor(obs.fraction);
      if (obs.fraction >= 1.0 - pff_fractions::kFractionEpsilon ||
          obs.fraction < pff_fractions::kFractionEpsilon) {
        obs.fraction = 0.0;
      }
    } else {
      obs.fraction = std::clamp(obs.fraction, 0.0, 1.0);
    }
  }
  std::sort(observations.begin(), observations.end(), [](const FeatureObservation& a,
                                                         const FeatureObservation& b) {
    return a.fraction < b.fraction;
  });

  std::vector<std::vector<FeatureObservation>> clusters;
  for (const FeatureObservation& obs: observations) {
    if (clusters.empty() || obs.fraction - clusters.back().back().fraction > radius) {
      clusters.push_back({obs});
    } else {
      clusters.back().push_back(obs);
    }
  }
  if (closed && clusters.size() > 1) {
    const double wrap_gap =
        clusters.front().front().fraction + 1.0 - clusters.back().back().fraction;
    if (wrap_gap <= radius) {
      clusters.back().insert(
          clusters.back().end(), clusters.front().begin(), clusters.front().end());
      clusters.erase(clusters.begin());
    }
  }

  std::vector<FeatureClusterRecord> records;
  records.reserve(clusters.size());
  for (const std::vector<FeatureObservation>& cluster: clusters) {
    std::vector<double> fractions;
    std::vector<int> frame_indices;
    FeatureClusterRecord record;
    record.observation_count = static_cast<int>(cluster.size());
    fractions.reserve(cluster.size());
    frame_indices.reserve(cluster.size());
    for (const FeatureObservation& obs: cluster) {
      fractions.push_back(obs.fraction);
      if (obs.frame_index >= 0) {
        frame_indices.push_back(obs.frame_index);
      }
      if (obs.zero_tangent_cue) {
        ++record.zero_tangent_count;
      }
      record.max_turn_radians = std::max(record.max_turn_radians, obs.turn_radians);
    }
    std::sort(frame_indices.begin(), frame_indices.end());
    frame_indices.erase(std::unique(frame_indices.begin(), frame_indices.end()),
                        frame_indices.end());
    record.observed_frame_count = static_cast<int>(frame_indices.size());
    record.fraction = ClusterFractionCenter(fractions, closed);
    records.push_back(record);
  }

  std::sort(records.begin(), records.end(), [](const FeatureClusterRecord& a,
                                               const FeatureClusterRecord& b) {
    return a.fraction < b.fraction;
  });
  return records;
}

int PersistentFeatureFrameThreshold(int frame_count) {
  if (frame_count <= 2) {
    return 1;
  }
  return std::max(2, static_cast<int>(std::ceil(static_cast<double>(frame_count) * 0.35)));
}

double FeatureClusterScore(const FeatureClusterRecord& cluster) {
  return static_cast<double>(cluster.observed_frame_count) * 1000.0 +
         static_cast<double>(cluster.zero_tangent_count) * 100.0 +
         std::min(cluster.max_turn_radians, pff_geom::kPi);
}

}  // namespace pff_cluster
}  // namespace bbsolver
