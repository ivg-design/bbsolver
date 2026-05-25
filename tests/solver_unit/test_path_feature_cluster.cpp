#include "bbsolver/path/geometry/path_feature_cluster.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

#include <cassert>
#include <cstddef>
#include <limits>
#include <vector>

namespace {

using bbsolver::pff_cluster::ClusterFeatureObservations;
using bbsolver::pff_cluster::ClusterFractionCenter;
using bbsolver::pff_cluster::FeatureClusterRecord;
using bbsolver::pff_cluster::FeatureObservation;

void TestClusterFractionCenterEmpty() {
  assert(ClusterFractionCenter({}, true) == 0.0);
  assert(ClusterFractionCenter({}, false) == 0.0);
}

void TestClusterFractionCenterOpenArithmeticMean() {
  // Open path: arithmetic mean clamped to [0, 1].
  assert(std::abs(ClusterFractionCenter({0.2, 0.4, 0.6}, false) - 0.4) < 1e-12);
  assert(std::abs(ClusterFractionCenter({0.0, 0.5, 1.0}, false) - 0.5) < 1e-12);
}

void TestClusterFractionCenterClosedCircularMean() {
  // Closed path: circular mean of 3 fractions on a unit circle.
  // {0.25, 0.5, 0.75} -> mean angle is π (i.e., fraction 0.5).
  assert(std::abs(ClusterFractionCenter({0.25, 0.5, 0.75}, true) - 0.5) < 1e-9);
  // {0.9, 0.95, 0.0, 0.05} wraps around the seam; circular mean should snap to 0.
  const double seam = ClusterFractionCenter({0.9, 0.95, 0.0, 0.05}, true);
  assert(seam == 0.0);  // snap to seam when within kFractionEpsilon
}

void TestClusterObservationsBasicGrouping() {
  // Three observations at 0.1, 0.12, 0.5. radius=0.05 -> two clusters.
  std::vector<FeatureObservation> obs = {
      {0.10, 0, 0.6, false},
      {0.12, 1, 0.7, true},
      {0.50, 2, 0.5, false},
  };
  const std::vector<FeatureClusterRecord> clusters =
      ClusterFeatureObservations(obs, true, 0.05);
  assert(clusters.size() == 2);
  // First cluster has 2 observations from 2 frames, one with zero_tangent_cue.
  assert(clusters[0].observation_count == 2);
  assert(clusters[0].observed_frame_count == 2);
  assert(clusters[0].zero_tangent_count == 1);
  assert(std::abs(clusters[0].max_turn_radians - 0.7) < 1e-12);
  // Second cluster is the lone 0.5 fraction.
  assert(clusters[1].observation_count == 1);
  assert(std::abs(clusters[1].fraction - 0.5) < 1e-9);
}

void TestClusterObservationsClosedSeamMerge() {
  // Closed path: clusters near 0.05 and 0.97 should merge across the seam
  // when wrap_gap <= radius. radius=0.10, wrap_gap = 0.05 + 1.0 - 0.97 = 0.08.
  std::vector<FeatureObservation> obs = {
      {0.05, 0, 0.5, false},
      {0.97, 1, 0.5, false},
  };
  const std::vector<FeatureClusterRecord> clusters =
      ClusterFeatureObservations(obs, true, 0.10);
  // Merged into a single cluster.
  assert(clusters.size() == 1);
  assert(clusters[0].observation_count == 2);
  assert(clusters[0].observed_frame_count == 2);
}

void TestClusterObservationsClosedSeamNoMergeWhenGapTooLarge() {
  std::vector<FeatureObservation> obs = {
      {0.20, 0, 0.5, false},
      {0.80, 1, 0.5, false},
  };
  // wrap_gap = 0.2 + 1.0 - 0.8 = 0.4 > radius 0.05 -> stay separate.
  const std::vector<FeatureClusterRecord> clusters =
      ClusterFeatureObservations(obs, true, 0.05);
  assert(clusters.size() == 2);
}

void TestClusterObservationsDropsNonFinite() {
  std::vector<FeatureObservation> obs = {
      {0.1, 0, 0.5, false},
      {std::numeric_limits<double>::quiet_NaN(), 1, 0.5, false},
      {std::numeric_limits<double>::infinity(), 2, 0.5, false},
      {0.5, 3, 0.5, false},
  };
  const std::vector<FeatureClusterRecord> clusters =
      ClusterFeatureObservations(obs, true, 0.05);
  assert(clusters.size() == 2);  // 0.1 and 0.5
}

void TestPersistentThresholdSmallSequences() {
  assert(PersistentFeatureFrameThreshold(0) == 1);
  assert(PersistentFeatureFrameThreshold(1) == 1);
  assert(PersistentFeatureFrameThreshold(2) == 1);
}

void TestPersistentThresholdLargerSequences() {
  // 5 frames: ⌈5 * 0.35⌉ = 2. Floor is 2.
  assert(PersistentFeatureFrameThreshold(5) == 2);
  // 10 frames: ⌈10 * 0.35⌉ = 4.
  assert(PersistentFeatureFrameThreshold(10) == 4);
  // 100 frames: ⌈100 * 0.35⌉ = 35.
  assert(PersistentFeatureFrameThreshold(100) == 35);
}

void TestFeatureClusterScoreOrders() {
  // Higher observed_frame_count dominates.
  FeatureClusterRecord a{0.2, /*observed=*/8, 8, 0, 1.0};
  FeatureClusterRecord b{0.5, /*observed=*/3, 3, 5, 1.0};
  assert(FeatureClusterScore(a) > FeatureClusterScore(b));

  // Equal observed_frame_count: more zero_tangent_count wins.
  FeatureClusterRecord c{0.2, 5, 5, 3, 1.0};
  FeatureClusterRecord d{0.5, 5, 5, 1, 1.0};
  assert(FeatureClusterScore(c) > FeatureClusterScore(d));

  // Equal observed + zero_tangent: max_turn_radians is clamped at kPi
  // when comparing scores (we only added min(max_turn_radians, kPi)).
  FeatureClusterRecord e{0.2, 5, 5, 1, 5.0};  // turn > pi
  FeatureClusterRecord f{0.5, 5, 5, 1, 1.5};  // turn < pi
  // e's contribution capped at pi (≈3.14), f's is 1.5; e > f.
  assert(FeatureClusterScore(e) > FeatureClusterScore(f));
}

void TestBuildFeatureLayoutHappyPath() {
  // Sanity check end-to-end through the public BuildShapeFlatFeatureFractionLayout
  // surface that PFF9 implements in the new path_feature_layout.cpp module.
  // Two closed square frames at different positions -> 4 features cluster
  // perfectly across both frames.
  const std::vector<std::vector<double>> frames = {
      {1.0, 4.0,
       0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
       1.0, 0.0, 0.0, 0.0, 0.0, 0.0,
       1.0, 1.0, 0.0, 0.0, 0.0, 0.0,
       0.0, 1.0, 0.0, 0.0, 0.0, 0.0},
      {1.0, 4.0,
       0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
       1.0, 0.0, 0.0, 0.0, 0.0, 0.0,
       1.0, 1.0, 0.0, 0.0, 0.0, 0.0,
       0.0, 1.0, 0.0, 0.0, 0.0, 0.0},
  };
  bbsolver::PathFrameFitOptions opts;
  opts.source_vertices_are_semantic_anchors = true;
  // Target 4 vertices == 4 corners -> trips "outline fraction count reaches
  // source vertex count" inside ExpandShapeFlatOutlineFractions? No, this is
  // BuildShapeFlatFeatureFractionLayout, which only requires target_count >=
  // shape minimum (3 for closed). Both source frames have 4 vertices; layout
  // target = 4 is allowed.
  const bbsolver::PathFeatureFractionLayoutResult layout =
      bbsolver::BuildShapeFlatFeatureFractionLayout(frames, 4, opts);
  // Should succeed (ok=true) with 4 outline fractions.
  assert(layout.target_count == 4);
  assert(layout.frame_count == 2);
  assert(layout.closed);
  // The output ought to include the seam + 3 additional slots.
  assert(layout.outline_fractions.size() == 4);
  assert(layout.outline_fractions.front() == 0.0);
}

}  // namespace

int main() {
  TestClusterFractionCenterEmpty();
  TestClusterFractionCenterOpenArithmeticMean();
  TestClusterFractionCenterClosedCircularMean();
  TestClusterObservationsBasicGrouping();
  TestClusterObservationsClosedSeamMerge();
  TestClusterObservationsClosedSeamNoMergeWhenGapTooLarge();
  TestClusterObservationsDropsNonFinite();
  TestPersistentThresholdSmallSequences();
  TestPersistentThresholdLargerSequences();
  TestFeatureClusterScoreOrders();
  TestBuildFeatureLayoutHappyPath();
  return 0;
}
