#include "bbsolver/motion_smooth/motion_path_smooth_spatial_trajectory.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/motion_smooth/motion_path_smooth_fairing.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    std::abort();
  }
}

bool Contains(const std::string& text, const std::string& needle) {
  return text.find(needle) != std::string::npos;
}

double ComponentOrZero(const std::vector<double>& value, std::size_t idx) {
  return idx < value.size() ? value[idx]: 0.0;
}

double PointDistance(const std::vector<double>& a,
                     const std::vector<double>& b,
                     int dims) {
  double sum = 0.0;
  for (int d = 0; d < dims; ++d) {
    const std::size_t sd = static_cast<std::size_t>(d);
    const double delta = ComponentOrZero(a, sd) - ComponentOrZero(b, sd);
    sum += delta * delta;
  }
  return std::sqrt(sum);
}

double PathLength(const std::vector<std::vector<double>>& points, int dims) {
  double length = 0.0;
  for (std::size_t i = 1; i < points.size(); ++i) {
    length += PointDistance(points[i - 1], points[i], dims);
  }
  return length;
}

struct Bounds {
  double min_x = 0.0;
  double max_x = 0.0;
  double min_y = 0.0;
  double max_y = 0.0;
};

Bounds PathBounds(const std::vector<std::vector<double>>& points) {
  Bounds bounds;
  if (points.empty()) {
    return bounds;
  }
  bounds.min_x = bounds.max_x = ComponentOrZero(points.front(), 0);
  bounds.min_y = bounds.max_y = ComponentOrZero(points.front(), 1);
  for (const std::vector<double>& point: points) {
    const double x = ComponentOrZero(point, 0);
    const double y = ComponentOrZero(point, 1);
    bounds.min_x = std::min(bounds.min_x, x);
    bounds.max_x = std::max(bounds.max_x, x);
    bounds.min_y = std::min(bounds.min_y, y);
    bounds.max_y = std::max(bounds.max_y, y);
  }
  return bounds;
}

double BoundsMaxDeviation(const Bounds& source, const Bounds& candidate) {
  return std::max({std::abs(candidate.min_x - source.min_x),
                   std::abs(candidate.max_x - source.max_x),
                   std::abs(candidate.min_y - source.min_y),
                   std::abs(candidate.max_y - source.max_y)});
}

double TurnAngleDeg(const std::vector<double>& prev,
                    const std::vector<double>& cur,
                    const std::vector<double>& next,
                    int dims) {
  double dot = 0.0;
  double a_len_sq = 0.0;
  double b_len_sq = 0.0;
  for (int d = 0; d < dims; ++d) {
    const std::size_t sd = static_cast<std::size_t>(d);
    const double ax = ComponentOrZero(cur, sd) - ComponentOrZero(prev, sd);
    const double bx = ComponentOrZero(next, sd) - ComponentOrZero(cur, sd);
    dot += ax * bx;
    a_len_sq += ax * ax;
    b_len_sq += bx * bx;
  }
  if (a_len_sq <= 1e-12 || b_len_sq <= 1e-12) {
    return 0.0;
  }
  constexpr double kPi = 3.14159265358979323846;
  const double denom = std::sqrt(a_len_sq * b_len_sq);
  return std::acos(std::clamp(dot / denom, -1.0, 1.0)) * 180.0 / kPi;
}

double MaxInteriorTurnDeg(const std::vector<std::vector<double>>& points,
                          int dims) {
  double max_turn = 0.0;
  for (std::size_t i = 1; i + 1 < points.size(); ++i) {
    max_turn = std::max(
        max_turn, TurnAngleDeg(points[i - 1], points[i], points[i + 1], dims));
  }
  return max_turn;
}

std::vector<std::vector<double>> SampleValues(
    const bbsolver::PropertySamples& property) {
  std::vector<std::vector<double>> values;
  values.reserve(property.samples.size());
  for (const bbsolver::Sample& sample: property.samples) {
    values.push_back(sample.v);
  }
  return values;
}

bbsolver::Sample Sample(double t, std::vector<double> value) {
  bbsolver::Sample sample;
  sample.t_sec = t;
  sample.v = std::move(value);
  return sample;
}

bbsolver::PropertySamples PositionProperty(
    const std::vector<bbsolver::Sample>& samples) {
  bbsolver::PropertySamples property;
  property.property.id = "position";
  property.property.match_name = "ADBE Position";
  property.property.display_name = "Position";
  property.property.kind = bbsolver::ValueKind::TwoD_Spatial;
  property.property.dimensions = 2;
  property.property.is_spatial = true;
  property.t_start_sec = samples.front().t_sec;
  property.t_end_sec = samples.back().t_sec;
  property.samples = samples;
  return property;
}

bbsolver::SolverConfig MotionPathConfig() {
  bbsolver::SolverConfig config;
  config.solve_optimization_mode = "motion_path_smooth";
  config.motion_path_smoothing_tolerance = 5.0;
  config.motion_path_accuracy_tolerance = 100.0;
  config.motion_smooth_use_ease = true;
  config.allow_bezier = true;
  return config;
}

void TestBounceImpactStaysSharp() {
  bbsolver::PropertySamples property = PositionProperty({
      Sample(0.00, {0.0, 10.0}),
      Sample(0.25, {0.0, 5.0}),
      Sample(0.50, {0.0, 0.0}),
      Sample(0.75, {0.0, 5.0}),
      Sample(1.00, {0.0, 10.0}),
  });

  bbsolver::SolverConfig config = MotionPathConfig();
  config.motion_path_preserve_sharp_points = true;
  config.motion_path_sharp_angle_deg = 120.0;
  config.motion_path_respect_keyed_frames = false;

  const bbsolver::PropertyKeys keys =
      bbsolver::MotionPathSmoothSpatialTrajectoryKeys(property, config);

  Require(keys.converged, "motion path smooth must converge");
  Require(keys.keys.size() == 3,
          "loose bounce smoothing must keep endpoints plus impact cusp");
  Require(std::abs(keys.keys[1].t_sec - 0.50) < 1e-9,
          "impact cusp must stay at the source impact frame");
  Require(keys.keys[1].v == std::vector<double>({0.0, 0.0}),
          "impact cusp must preserve the raw bounce hit position");
  Require(!keys.keys[1].roving,
          "sharp impact key must not be roving");
  Require(!keys.keys[1].spatial_continuous,
          "sharp impact key must not smooth spatial continuity");
  Require(keys.keys[1].spatial_in == std::vector<double>({0.0, 0.0}) &&
              keys.keys[1].spatial_out == std::vector<double>({0.0, 0.0}),
          "sharp impact key must keep zero spatial tangents");
  Require(Contains(keys.notes, "solve_mode_motion_path_smooth") &&
              Contains(keys.notes,
                       "motion_path_spatial_trajectory_filter=true") &&
              Contains(keys.notes,
                       "motion_path_preserve_sharp_points=true") &&
              Contains(keys.notes, "motion_path_sharp_points=1"),
          "motion path notes must expose sharp-point preservation");
}

void TestKeyedFramesCanBeHardConstraints() {
  bbsolver::PropertySamples property = PositionProperty({
      Sample(0.00, {0.0, 0.0}),
      Sample(0.25, {8.0, 8.0}),
      Sample(0.50, {12.0, 20.0}),
      Sample(0.75, {24.0, 8.0}),
      Sample(1.00, {32.0, 0.0}),
  });
  property.property.source_key_times = {0.0, 0.50, 1.0};

  bbsolver::SolverConfig config = MotionPathConfig();
  config.motion_path_preserve_sharp_points = false;
  config.motion_path_respect_keyed_frames = true;

  const bbsolver::PropertyKeys keys =
      bbsolver::MotionPathSmoothSpatialTrajectoryKeys(property, config);

  Require(keys.keys.size() == 3,
          "loose keyed-frame smoothing must keep keyed endpoints and middle");
  Require(std::abs(keys.keys[1].t_sec - 0.50) < 1e-9,
          "middle source key time must stay locked");
  Require(keys.keys[1].v == std::vector<double>({12.0, 20.0}),
          "middle source key pose must stay raw when keyed frames are locked");
  Require(!keys.keys[1].roving,
          "locked keyed-frame output must not rove");
  Require(Contains(keys.notes, "motion_path_respect_keyed_frames=true") &&
              Contains(keys.notes, "motion_path_keyed_points=3") &&
              Contains(keys.notes, "source_key_count=3"),
          "motion path notes must expose keyed-frame constraints");
}

void TestSmoothArcCanReduceSourceKeySchedule() {
  bbsolver::PropertySamples property = PositionProperty({
      Sample(0.00, {0.0, 0.0}),
      Sample(0.25, {8.0, 1.0}),
      Sample(0.50, {16.0, 2.0}),
      Sample(0.75, {24.0, 1.0}),
      Sample(1.00, {32.0, 0.0}),
  });
  property.property.source_key_times = {0.0, 0.25, 0.50, 0.75, 1.0};

  bbsolver::SolverConfig config = MotionPathConfig();
  config.motion_path_preserve_sharp_points = true;
  config.motion_path_respect_keyed_frames = false;
  config.motion_path_accuracy_tolerance = 8.0;

  const bbsolver::PropertyKeys keys =
      bbsolver::MotionPathSmoothSpatialTrajectoryKeys(property, config);

  Require(keys.keys.size() < property.property.source_key_times.size(),
          "motion path smooth must be able to reduce source key schedules");
  Require(keys.keys.front().t_sec == property.samples.front().t_sec &&
              keys.keys.back().t_sec == property.samples.back().t_sec,
          "motion path smooth must preserve range endpoints");
  Require(Contains(keys.notes,
                   "motion_path_spatial_trajectory_filter=true") &&
              Contains(keys.notes, "smoothed_path_max_err=") &&
              Contains(keys.notes, "raw_source_max_displacement="),
          "motion path notes must distinguish smoothing displacement from fit "
          "accuracy");
}

void TestAccuracyUsesSampleTimeNotOnlyGeometricPathDistance() {
  bbsolver::PropertySamples property = PositionProperty({
      Sample(0.00, {0.0, 0.0}),
      Sample(0.50, {100.0, 0.0}),
      Sample(1.00, {101.0, 0.0}),
  });

  bbsolver::SolverConfig config = MotionPathConfig();
  config.motion_path_smoothing_tolerance = 1.0;
  config.motion_path_accuracy_tolerance = 1.0;
  config.motion_path_preserve_sharp_points = false;
  config.motion_path_respect_keyed_frames = false;
  config.motion_smooth_use_ease = false;
  config.allow_bezier = false;

  const bbsolver::PropertyKeys keys =
      bbsolver::MotionPathSmoothSpatialTrajectoryKeys(property, config);

  Require(keys.keys.size() == 3,
          "motion path accuracy must retain time-critical interior samples");
  Require(std::abs(keys.keys[1].t_sec - 0.50) < 1e-9,
          "motion path accuracy must keep the interior sample at its source "
          "time");
  Require(keys.max_err <= 1.0 + 1e-9,
          "reported smoothed-path error must respect the accuracy tolerance");
  Require(Contains(keys.notes, "motion_path_accuracy_tolerance=1") &&
              Contains(keys.notes, "smoothed_path_max_err="),
          "motion path notes must report accuracy and measured path error");
}

void TestSmoothingStrengthIsClampedToDocumentedRange() {
  bbsolver::PropertySamples property = PositionProperty({
      Sample(0.00, {0.0, 0.0}),
      Sample(0.25, {8.0, 1.0}),
      Sample(0.50, {16.0, 2.0}),
      Sample(0.75, {24.0, 1.0}),
      Sample(1.00, {32.0, 0.0}),
  });

  bbsolver::SolverConfig config = MotionPathConfig();
  config.motion_path_smoothing_tolerance = 100.0;

  const bbsolver::PropertyKeys keys =
      bbsolver::MotionPathSmoothSpatialTrajectoryKeys(property, config);

  Require(Contains(keys.notes, "smoothing_strength=32") &&
              Contains(keys.notes, "smoothing_passes=4096"),
          "motion path smooth must clamp strength to the documented max");
}

void TestMaxSmoothingRemovesUnpreservedSharpChanges() {
  bbsolver::PropertySamples property = PositionProperty({
      Sample(0.00, {0.0, 0.0}),
      Sample(0.10, {10.0, 40.0}),
      Sample(0.20, {20.0, -40.0}),
      Sample(0.30, {30.0, 35.0}),
      Sample(0.40, {40.0, -35.0}),
      Sample(0.50, {50.0, 30.0}),
      Sample(0.60, {60.0, -30.0}),
      Sample(0.70, {70.0, 20.0}),
      Sample(0.80, {80.0, 0.0}),
  });

  bbsolver::SolverConfig config = MotionPathConfig();
  config.motion_path_smoothing_tolerance = 32.0;
  config.motion_path_accuracy_tolerance = 2.0;
  config.motion_path_preserve_bounds = false;
  config.motion_path_preserve_sharp_points = false;
  config.motion_path_respect_keyed_frames = false;

  const std::vector<std::vector<double>> raw = SampleValues(property);
  const bbsolver::MotionPathLocks locks =
      bbsolver::BuildMotionPathLocks(property, raw, config, 2);
  const bbsolver::MotionPathFairingResult fairing =
      bbsolver::FairMotionPathPoints(raw, locks, 32.0, false, 0.0, 2);

  Require(fairing.passes == 4096,
          "max motion path smoothing must use the documented maximum pass "
          "count");
  Require(fairing.smoothed_path_length < PathLength(raw, 2) * 0.45,
          "max smoothing without sharp locks must collapse zig-zag path length");
  Require(MaxInteriorTurnDeg(fairing.points, 2) <= 12.0,
          "max smoothing without sharp locks must remove angular path changes");

  const bbsolver::PropertyKeys keys =
      bbsolver::MotionPathSmoothSpatialTrajectoryKeys(property, config);

  Require(keys.converged, "max motion path smooth must converge");
  Require(Contains(keys.notes, "smoothing_strength=32") &&
              Contains(keys.notes, "smoothing_passes=4096") &&
              Contains(keys.notes, "smoothed_path_length=") &&
              Contains(keys.notes, "source_path_length="),
	          "motion path notes must expose max-strength fairing diagnostics");
}

void TestPreserveBoundsConstrainsFairingFootprint() {
  bbsolver::PropertySamples property = PositionProperty({
      Sample(0.00, {0.0, 0.0}),
      Sample(0.10, {0.0, 120.0}),
      Sample(0.20, {120.0, 120.0}),
      Sample(0.30, {120.0, 0.0}),
      Sample(0.40, {60.0, 60.0}),
      Sample(0.50, {0.0, 0.0}),
  });

  bbsolver::SolverConfig config = MotionPathConfig();
  config.motion_path_smoothing_tolerance = 32.0;
  config.motion_path_accuracy_tolerance = 12.0;
  config.motion_path_preserve_bounds = true;
  config.motion_path_bounds_tolerance = 0.0;
  config.motion_path_preserve_sharp_points = false;
  config.motion_path_respect_keyed_frames = false;

  const std::vector<std::vector<double>> raw = SampleValues(property);
  const Bounds source_bounds = PathBounds(raw);
  const bbsolver::MotionPathLocks locks =
      bbsolver::BuildMotionPathLocks(property, raw, config, 2);
  const bbsolver::MotionPathFairingResult fairing =
      bbsolver::FairMotionPathPoints(raw, locks, 32.0, true, 0.0, 2);
  const Bounds smoothed_bounds = PathBounds(fairing.points);

  Require(fairing.bounds_preserved,
          "preserve-bounds fairing must record the enabled constraint");
  Require(fairing.bounds_max_deviation <= 1e-6,
          "zero bounds tolerance must restore source motion-path bounds");
  Require(BoundsMaxDeviation(source_bounds, smoothed_bounds) <= 1e-6,
          "smoothed target bounds must match source bounds at zero tolerance");
  Require(bbsolver::CountMotionPathLocks(locks.bounds) >= 3,
          "bounds preservation must mark global extrema as retained samples");

  bbsolver::PropertyKeys keys =
      bbsolver::MotionPathSmoothSpatialTrajectoryKeys(property, config);
  Require(Contains(keys.notes, "motion_path_preserve_bounds=true") &&
              Contains(keys.notes, "motion_path_bounds_tolerance=0") &&
              Contains(keys.notes, "motion_path_bounds_points=") &&
              Contains(keys.notes, "bounds_max_deviation="),
          "motion path notes must expose bounds-preservation diagnostics");

  config.motion_path_bounds_tolerance = 20.0;
  const bbsolver::MotionPathFairingResult relaxed =
      bbsolver::FairMotionPathPoints(raw, locks, 32.0, true, 20.0, 2);
  Require(relaxed.bounds_max_deviation <= 20.0 + 1e-6,
          "positive bounds tolerance must cap per-side bounds deviation");
}

}  // namespace

int main() {
  TestBounceImpactStaysSharp();
  TestKeyedFramesCanBeHardConstraints();
  TestSmoothArcCanReduceSourceKeySchedule();
  TestAccuracyUsesSampleTimeNotOnlyGeometricPathDistance();
  TestSmoothingStrengthIsClampedToDocumentedRange();
  TestMaxSmoothingRemovesUnpreservedSharpChanges();
  TestPreserveBoundsConstrainsFairingFootprint();
  std::cout << "[PASS] test_motion_path_smooth_solver\n";
  return 0;
}
