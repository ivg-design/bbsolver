#include "bbsolver/shape/sharp_corner_policy.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/shape/shape_flat_topology.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    std::abort();
  }
}

bbsolver::SolverConfig Config() {
  bbsolver::SolverConfig config;
  config.tolerance = 0.5;
  config.path_sharp_corner_angle_deg = 90.0;
  config.path_sharp_corner_tolerance = 1.5;
  return config;
}

bool Near(double a, double b, double epsilon = 1e-9) {
  return std::fabs(a - b) <= epsilon;
}

bool Contains(const std::string& text, const std::string& needle) {
  return text.find(needle) != std::string::npos;
}

std::vector<double> Flat(const std::vector<bbsolver::ShapeFlatVertex>& vertices,
                         bool closed = false) {
  return bbsolver::ShapeFlatFromVertices(vertices, closed);
}

bbsolver::Sample SampleWithValue(const std::vector<double>& value,
                                 double t_sec = 0.0) {
  bbsolver::Sample sample;
  sample.t_sec = t_sec;
  sample.v = value;
  return sample;
}

bbsolver::PropertySamples SamplesWithValues(
    const std::vector<std::vector<double>>& values) {
  bbsolver::PropertySamples samples;
  for (const std::vector<double>& value: values) {
    samples.samples.push_back(
        SampleWithValue(value, static_cast<double>(samples.samples.size())));
  }
  return samples;
}

bbsolver::PropertySamples ShapeSamplesWithValues(
    const std::vector<std::vector<double>>& values) {
  bbsolver::PropertySamples samples = SamplesWithValues(values);
  samples.property.kind = bbsolver::ValueKind::Custom;
  samples.property.units_label = "shape_flat";
  return samples;
}

bbsolver::Key KeyWithValue(const std::vector<double>& value) {
  bbsolver::Key key;
  key.v = value;
  return key;
}

bbsolver::PropertyKeys KeysWithValue(const std::vector<double>& value) {
  bbsolver::PropertyKeys keys;
  keys.keys.push_back(KeyWithValue(value));
  return keys;
}

std::vector<double> RightAngleFlat() {
  return Flat({
      {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      {1.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      {1.0, 1.0, 0.0, 0.0, 0.0, 0.0},
  });
}

std::vector<double> StraightFlat() {
  return Flat({
      {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      {1.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      {2.0, 0.0, 0.0, 0.0, 0.0, 0.0},
  });
}

std::vector<double> FourVertexFlat() {
  return Flat({
      {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      {1.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      {1.0, 1.0, 0.0, 0.0, 0.0, 0.0},
      {0.0, 1.0, 0.0, 0.0, 0.0, 0.0},
  });
}

std::vector<double> FarRightAngleFlat() {
  return Flat({
      {100.0, 100.0, 0.0, 0.0, 0.0, 0.0},
      {101.0, 100.0, 0.0, 0.0, 0.0, 0.0},
      {101.0, 101.0, 0.0, 0.0, 0.0, 0.0},
  });
}

void TestDefaultAndNonFiniteAngle() {
  bbsolver::SolverConfig config = Config();
  Require(bbsolver::SharpCornerAngleThresholdDeg(config) == 90.0,
          "default angle threshold must pass through");
  config.path_sharp_corner_angle_deg =
      std::numeric_limits<double>::infinity();
  Require(bbsolver::SharpCornerAngleThresholdDeg(config) == 90.0,
          "non-finite angle threshold must default to ninety");
}

void TestAngleClampAndPassThrough() {
  bbsolver::SolverConfig config = Config();
  config.path_sharp_corner_angle_deg = 2.0;
  Require(bbsolver::SharpCornerAngleThresholdDeg(config) == 5.0,
          "low angle threshold must clamp to five");
  config.path_sharp_corner_angle_deg = 180.0;
  Require(bbsolver::SharpCornerAngleThresholdDeg(config) == 175.0,
          "high angle threshold must clamp to one hundred seventy five");
  config.path_sharp_corner_angle_deg = 42.0;
  Require(bbsolver::SharpCornerAngleThresholdDeg(config) == 42.0,
          "in-range angle threshold must pass through");
}

void TestNonFiniteLockToleranceFallback() {
  bbsolver::SolverConfig config = Config();
  config.tolerance = 0.5;
  config.path_sharp_corner_tolerance =
      std::numeric_limits<double>::infinity();
  Require(bbsolver::SharpCornerLockTolerance(config) == 1.5,
          "non-finite lock tolerance must fall back to at least 1.5");

  config.tolerance = 2.0;
  Require(bbsolver::SharpCornerLockTolerance(config) == 3.0,
          "non-finite lock tolerance must respect scaled solver tolerance");
}

void TestFiniteLockToleranceVersusScaledTolerance() {
  bbsolver::SolverConfig config = Config();
  config.tolerance = 2.0;
  config.path_sharp_corner_tolerance = 1.0;
  Require(bbsolver::SharpCornerLockTolerance(config) == 3.0,
          "finite lock below scaled tolerance must use scaled tolerance");

  config.path_sharp_corner_tolerance = 4.0;
  Require(bbsolver::SharpCornerLockTolerance(config) == 4.0,
          "finite lock above scaled tolerance must pass through");
}

void TestNonFiniteConfigToleranceAndFloor() {
  bbsolver::SolverConfig config = Config();
  config.tolerance = std::numeric_limits<double>::quiet_NaN();
  config.path_sharp_corner_tolerance = 0.5;
  Require(bbsolver::SharpCornerLockTolerance(config) == 0.5,
          "non-finite solver tolerance must not contribute scaled tolerance");

  config.path_sharp_corner_tolerance = -2.0;
  Require(bbsolver::SharpCornerLockTolerance(config) == 1e-6,
          "finite lock tolerance result must be floored at 1e-6");
}

void TestDeflectionInvalidAndDegenerateReturnsZero() {
  Require(bbsolver::ShapeFlatDeflectionAngleDeg({}, 0) == 0.0,
          "malformed flat vectors must return zero deflection");

  const std::vector<double> right_angle = RightAngleFlat();
  Require(bbsolver::ShapeFlatDeflectionAngleDeg(right_angle, -1) == 0.0,
          "negative vertex indexes must return zero deflection");
  Require(bbsolver::ShapeFlatDeflectionAngleDeg(right_angle, 3) == 0.0,
          "out-of-range vertex indexes must return zero deflection");

  const std::vector<double> degenerate = Flat({
      {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      {1.0, 0.0, 0.0, 0.0, 0.0, 0.0},
  });
  Require(bbsolver::ShapeFlatDeflectionAngleDeg(degenerate, 1) == 0.0,
          "degenerate adjacent vectors must return zero deflection");
}

void TestDeflectionOpenEndpointAndRightAngle() {
  const std::vector<double> right_angle = RightAngleFlat();
  Require(bbsolver::ShapeFlatDeflectionAngleDeg(right_angle, 0) == 180.0,
          "open-path first endpoint must return 180 degrees");
  Require(bbsolver::ShapeFlatDeflectionAngleDeg(right_angle, 2) == 180.0,
          "open-path last endpoint must return 180 degrees");
  Require(Near(bbsolver::ShapeFlatDeflectionAngleDeg(right_angle, 1), 90.0),
          "right-angle interior vertex must return 90 degrees");
}

void TestSharpCornerCollectionsAndPoints() {
  bbsolver::SolverConfig config = Config();
  const std::vector<double> right_angle = RightAngleFlat();
  Require(bbsolver::ShapeFlatSharpCornerIndices(right_angle, config) ==
              std::vector<int>({0, 1, 2}),
          "thresholded corner indices must include angles at the threshold");

  config.path_sharp_corner_angle_deg = 100.0;
  Require(bbsolver::ShapeFlatSharpCornerIndices(right_angle, config) ==
              std::vector<int>({0, 2}),
          "thresholded corner indices must exclude below-threshold angles");
  const std::vector<bbsolver::ShapeFlatPoint> points =
      bbsolver::ShapeFlatSharpCornerPoints(right_angle, config);
  Require(points.size() == 2,
          "corner point mapping must preserve the selected corner count");
  Require(points[0].x == 0.0 && points[0].y == 0.0,
          "corner point mapping must preserve first endpoint coordinates");
  Require(points[1].x == 1.0 && points[1].y == 1.0,
          "corner point mapping must preserve last endpoint coordinates");
}

void TestShapeFlatIndexIsSharpCorner() {
  bbsolver::SolverConfig config = Config();
  const std::vector<double> right_angle = RightAngleFlat();
  Require(bbsolver::ShapeFlatIndexIsSharpCorner(right_angle, 1, config),
          "single-index check must include an angle at the threshold");

  config.path_sharp_corner_angle_deg = 100.0;
  Require(!bbsolver::ShapeFlatIndexIsSharpCorner(right_angle, 1, config),
          "single-index check must reject below-threshold angles");
  Require(bbsolver::ShapeFlatIndexIsSharpCorner(right_angle, 0, config),
          "single-index check must preserve open endpoint behavior");
  Require(!bbsolver::ShapeFlatIndexIsSharpCorner({}, 0, config),
          "single-index check must reject malformed vectors");
}

void TestPersistentSharpCornerNoValidSamples() {
  const bbsolver::PropertySamples samples =
      SamplesWithValues({{}, {0.0, 3.0}, {1.0, 0.0}});
  const std::vector<std::vector<int>> protected_by_count =
      bbsolver::PersistentShapeFlatSharpCornerIndicesByVertexCount(
          samples, Config());
  Require(protected_by_count.size() == 1,
          "no valid samples must still return the max_vertices + 1 layout");
  Require(protected_by_count[0].empty(),
          "no valid samples must return no protected indices");
}

void TestPersistentSharpCornerTopologyFrameThreshold() {
  const std::vector<double> right_angle = RightAngleFlat();
  const bbsolver::PropertySamples samples =
      SamplesWithValues({right_angle, right_angle});
  const std::vector<std::vector<int>> protected_by_count =
      bbsolver::PersistentShapeFlatSharpCornerIndicesByVertexCount(
          samples, Config());
  Require(protected_by_count.size() == 4,
          "valid three-vertex samples must allocate through vertex count");
  Require(protected_by_count[3].empty(),
          "topologies below the minimum frame threshold must not be protected");
}

void TestPersistentSharpCornerHitCountThreshold() {
  const std::vector<double> right_angle = RightAngleFlat();
  const std::vector<double> straight = StraightFlat();
  const bbsolver::PropertySamples samples =
      SamplesWithValues({right_angle, right_angle, straight});
  const std::vector<std::vector<int>> protected_by_count =
      bbsolver::PersistentShapeFlatSharpCornerIndicesByVertexCount(
          samples, Config());
  Require(protected_by_count.size() == 4,
          "hit-count threshold must preserve the vertex-count layout");
  Require(protected_by_count[3] == std::vector<int>({0, 2}),
          "only indices with enough persistent hits must be protected");
}

void TestSharpCornerKeyProtection() {
  const std::vector<double> right_angle = RightAngleFlat();
  bbsolver::PropertyKeys keys;
  keys.keys.push_back(KeyWithValue(right_angle));

  bbsolver::SolverConfig config = Config();
  config.path_preserve_sharp_corners = false;
  Require(!bbsolver::ShapeFlatKeyIndexIsProtectedCorner(
              keys, 3, 1, config),
          "disabled sharp-corner preservation must never protect keys");

  config.path_preserve_sharp_corners = true;
  bbsolver::PropertyKeys mismatched_keys;
  mismatched_keys.keys.push_back(KeyWithValue(FourVertexFlat()));
  Require(!bbsolver::ShapeFlatKeyIndexIsProtectedCorner(
              mismatched_keys, 3, 1, config),
          "keys with mismatched vertex counts must be skipped");

  Require(bbsolver::ShapeFlatKeyIndexIsProtectedCorner(keys, 3, 1, config),
          "matching sharp keys must protect the requested index");

  bbsolver::PropertyKeys straight_keys;
  straight_keys.keys.push_back(KeyWithValue(StraightFlat()));
  Require(!bbsolver::ShapeFlatKeyIndexIsProtectedCorner(
              straight_keys, 3, 1, config),
          "matching non-sharp keys must not protect the requested index");
}

void TestSharpCornerValidationDisabledConfig() {
  bbsolver::SolverConfig config = Config();
  config.path_preserve_sharp_corners = false;
  const bbsolver::SharpCornerValidationResult result =
      bbsolver::ValidateSharpCornerPreservation(
          ShapeSamplesWithValues(
              {RightAngleFlat(), RightAngleFlat(), RightAngleFlat()}),
          KeysWithValue(RightAngleFlat()),
          config);
  Require(!result.enabled,
          "disabled sharp-corner preservation must disable validation");
  Require(result.ok, "disabled validation must remain ok");
  Require(result.notes.empty(), "disabled validation must not emit notes");
}

void TestSharpCornerValidationNonShapePath() {
  const bbsolver::SharpCornerValidationResult result =
      bbsolver::ValidateSharpCornerPreservation(
          SamplesWithValues({RightAngleFlat(), RightAngleFlat(), RightAngleFlat()}),
          KeysWithValue(RightAngleFlat()),
          Config());
  Require(!result.enabled, "non-shape paths must disable validation");
  Require(result.ok, "non-shape disabled validation must remain ok");
}

void TestSharpCornerValidationSourceAnchorsDisabled() {
  const bbsolver::SharpCornerValidationResult result =
      bbsolver::ValidateSharpCornerPreservation(
          ShapeSamplesWithValues(
              {RightAngleFlat(), RightAngleFlat(), RightAngleFlat()}),
          KeysWithValue(RightAngleFlat()),
          Config(),
          false);
  Require(!result.enabled,
          "non-semantic source vertices must disable validation");
  Require(result.ok, "source-anchor disabled validation must remain ok");
}

void TestSharpCornerValidationAcceptedMatchingCandidate() {
  const bbsolver::SharpCornerValidationResult result =
      bbsolver::ValidateSharpCornerPreservation(
          ShapeSamplesWithValues(
              {RightAngleFlat(), RightAngleFlat(), RightAngleFlat()}),
          KeysWithValue(RightAngleFlat()),
          Config());
  Require(result.enabled, "shape-flat validation must be enabled");
  Require(result.ok, "matching candidate corners must be accepted");
  Require(result.samples_checked == 3,
          "accepted validation must check persistent source samples");
  Require(result.source_corners == 9,
          "accepted validation must count protected source corners");
  Require(result.candidate_corners == 9,
          "accepted validation must count candidate sharp corners");
  Require(Contains(result.notes, "sharp_corner_preserve=ok"),
          "accepted validation notes must report ok");
  Require(!Contains(result.notes, "missing_sharp_corners="),
          "accepted validation notes must omit rejection-only fields");
}

void TestSharpCornerValidationRejectedMissingCornerAndNotes() {
  const bbsolver::SharpCornerValidationResult result =
      bbsolver::ValidateSharpCornerPreservation(
          ShapeSamplesWithValues(
              {RightAngleFlat(), RightAngleFlat(), RightAngleFlat()}),
          KeysWithValue(FarRightAngleFlat()),
          Config());
  Require(result.enabled, "rejected shape-flat validation must be enabled");
  Require(!result.ok, "far candidate corners must be rejected");
  Require(Contains(result.notes, "sharp_corner_preserve=rejected"),
          "rejected validation notes must report rejection");
  Require(Contains(result.notes, "sharp_corner_angle_deg=90.000000"),
          "validation notes must preserve angle field");
  Require(Contains(result.notes, "sharp_corner_tolerance=1.500000"),
          "validation notes must preserve tolerance field");
  Require(Contains(result.notes, "sharp_corner_samples_checked=3"),
          "validation notes must preserve samples-checked field");
  Require(Contains(result.notes, "source_sharp_landmarks=9"),
          "validation notes must preserve source-corner field");
  Require(Contains(result.notes, "candidate_sharp_corners=9"),
          "validation notes must preserve candidate-corner field");
  Require(Contains(result.notes, "missing_sharp_corners="),
          "rejected validation notes must include missing count");
  Require(Contains(result.notes, "worst_sharp_corner_distance="),
          "rejected validation notes must include worst distance");
  Require(Contains(result.notes, "worst_sharp_corner_t="),
          "rejected validation notes must include worst time");
}

}  // namespace

int main() {
  TestDefaultAndNonFiniteAngle();
  TestAngleClampAndPassThrough();
  TestNonFiniteLockToleranceFallback();
  TestFiniteLockToleranceVersusScaledTolerance();
  TestNonFiniteConfigToleranceAndFloor();
  TestDeflectionInvalidAndDegenerateReturnsZero();
  TestDeflectionOpenEndpointAndRightAngle();
  TestSharpCornerCollectionsAndPoints();
  TestShapeFlatIndexIsSharpCorner();
  TestPersistentSharpCornerNoValidSamples();
  TestPersistentSharpCornerTopologyFrameThreshold();
  TestPersistentSharpCornerHitCountThreshold();
  TestSharpCornerKeyProtection();
  TestSharpCornerValidationDisabledConfig();
  TestSharpCornerValidationNonShapePath();
  TestSharpCornerValidationSourceAnchorsDisabled();
  TestSharpCornerValidationAcceptedMatchingCandidate();
  TestSharpCornerValidationRejectedMissingCornerAndNotes();
  std::cout << "[PASS] test_sharp_corner_policy\n";
  return 0;
}
