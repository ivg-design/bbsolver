#include "bbsolver/path/replacement/path_replacement_feature_layout_trial.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

#include <cmath>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

constexpr double kPi = 3.14159265358979323846;

struct Point {
  double x = 0.0;
  double y = 0.0;
};

std::vector<Point> OpenCurvedWristPolyline(int wrist_step) {
  std::vector<Point> points;
  const int steps = 40;
  for (int i = 0; i <= steps; ++i) {
    const double x = 200.0 * static_cast<double>(i) / static_cast<double>(steps);
    double y = 25.0 * std::sin(kPi * x / 200.0);
    if (i == wrist_step) {
      y += 1.7;
    }
    points.push_back({x, y});
  }
  return points;
}

std::vector<double> ShapeFlatWithSmoothTangents(
    bool closed,
    const std::vector<Point>& vertices) {
  std::vector<double> flat;
  flat.push_back(closed ? 1.0: 0.0);
  flat.push_back(static_cast<double>(vertices.size()));
  for (std::size_t i = 0; i < vertices.size(); ++i) {
    Point in_tangent{0.0, 0.0};
    Point out_tangent{0.0, 0.0};
    if (vertices.size() > 1) {
      const bool has_prev = closed || i > 0;
      const bool has_next = closed || i + 1 < vertices.size();
      if (has_prev && has_next) {
        const Point prev = vertices[(i + vertices.size() - 1) % vertices.size()];
        const Point next = vertices[(i + 1) % vertices.size()];
        in_tangent = {(prev.x - next.x) / 6.0, (prev.y - next.y) / 6.0};
        out_tangent = {(next.x - prev.x) / 6.0, (next.y - prev.y) / 6.0};
      }
    }
    flat.push_back(vertices[i].x);
    flat.push_back(vertices[i].y);
    flat.push_back(in_tangent.x);
    flat.push_back(in_tangent.y);
    flat.push_back(out_tangent.x);
    flat.push_back(out_tangent.y);
  }
  return flat;
}

std::vector<std::vector<double>> FeatureFrames() {
  std::vector<std::vector<double>> frames;
  for (int wrist_step: {19, 20, 21}) {
    frames.push_back(ShapeFlatWithSmoothTangents(
        false, OpenCurvedWristPolyline(wrist_step)));
  }
  return frames;
}

void TestAcceptsFirstReplayableFeatureLayout() {
  bbsolver::SolverConfig config;
  bbsolver::PathFrameFitOptions options;
  int callback_count = 0;

  const bbsolver::ReplacementFeatureLayoutTrialResult result =
      bbsolver::TryReplacementFeatureFractionLayout(
          FeatureFrames(),
          9,
          41,
          config,
          options,
          [&](const std::vector<double>& fractions,
              int seed_idx,
              int adaptive_count) {
            ++callback_count;
            Require(seed_idx == -1, "feature layout seed index changed");
            Require(adaptive_count == 0,
                    "feature layout adaptive count changed");
            return fractions.size() == 9;
          });

  Require(result.applied, "feature layout should apply when callback accepts");
  Require(result.targets_tried == 1, "accepted first target should stop search");
  Require(result.target_vertices == 9, "accepted target vertices changed");
  Require(callback_count == 1, "feature layout callback count changed");
}

void TestRejectedCallbackContinuesBoundedTargetRange() {
  bbsolver::SolverConfig config;
  bbsolver::PathFrameFitOptions options;
  int callback_count = 0;

  const bbsolver::ReplacementFeatureLayoutTrialResult result =
      bbsolver::TryReplacementFeatureFractionLayout(
          FeatureFrames(),
          9,
          41,
          config,
          options,
          [&](const std::vector<double>&, int, int) {
            ++callback_count;
            return false;
          });

  Require(!result.applied, "rejecting callback should not apply");
  Require(result.targets_tried == 3,
          "feature layout should try target through +2 bound");
  Require(callback_count == 3,
          "callback should run once per replayable target");
}

void TestEmptyFramesSkipWithoutCallback() {
  bbsolver::SolverConfig config;
  bbsolver::PathFrameFitOptions options;
  bool callback_called = false;

  const bbsolver::ReplacementFeatureLayoutTrialResult result =
      bbsolver::TryReplacementFeatureFractionLayout(
          {},
          4,
          8,
          config,
          options,
          [&](const std::vector<double>&, int, int) {
            callback_called = true;
            return true;
          });

  Require(!result.applied, "empty feature frames should not apply");
  Require(result.targets_tried == 0, "empty frames should try no targets");
  Require(!callback_called, "empty frames should not invoke callback");
}

}  // namespace

int main() {
  TestAcceptsFirstReplayableFeatureLayout();
  TestRejectedCallbackContinuesBoundedTargetRange();
  TestEmptyFramesSkipWithoutCallback();
  std::cout << "[PASS] test_path_replacement_feature_layout_trial\n";
  return 0;
}
