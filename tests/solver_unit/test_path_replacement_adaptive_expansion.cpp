#include "bbsolver/path/replacement/path_replacement_adaptive_expansion.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/path/replacement/path_replacement_fraction_layout.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Point {
  double x = 0.0;
  double y = 0.0;
};

void Require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void PushUnique(std::vector<Point>& points, Point p) {
  if (!points.empty()) {
    const double dx = points.back().x - p.x;
    const double dy = points.back().y - p.y;
    if (std::sqrt(dx * dx + dy * dy) < 1e-9) {
      return;
    }
  }
  points.push_back(p);
}

std::vector<double> ShapeFlat(bool closed, const std::vector<Point>& vertices) {
  std::vector<double> flat;
  flat.push_back(closed ? 1.0: 0.0);
  flat.push_back(static_cast<double>(vertices.size()));
  for (const Point& vertex: vertices) {
    flat.push_back(vertex.x);
    flat.push_back(vertex.y);
    flat.push_back(0.0);
    flat.push_back(0.0);
    flat.push_back(0.0);
    flat.push_back(0.0);
  }
  return flat;
}

std::vector<Point> DeformedSmoothOvalPolyline(double rx,
                                              double ry,
                                              double wobble) {
  std::vector<Point> points;
  const int steps = 96;
  const double cx = 120.0;
  const double cy = 90.0;
  for (int i = 0; i < steps; ++i) {
    const double a = 2.0 * kPi * static_cast<double>(i) /
                     static_cast<double>(steps);
    const double r = 1.0 + wobble * std::sin(3.0 * a);
    PushUnique(points, {cx + std::cos(a) * rx * r,
                        cy + std::sin(a) * ry *
                                 (1.0 - 0.5 * wobble * std::cos(2.0 * a))});
  }
  return points;
}

std::vector<double> UniformClosedFractions(int count) {
  std::vector<double> fractions;
  for (int i = 0; i < count; ++i) {
    fractions.push_back(static_cast<double>(i) / static_cast<double>(count));
  }
  return fractions;
}

bbsolver::ReplacementFrameFitRecord Record(std::vector<double> fractions) {
  bbsolver::ReplacementFrameFitRecord record;
  record.outline_fractions = std::move(fractions);
  return record;
}

std::vector<std::vector<double>> ExpansionFrames() {
  return {
      ShapeFlat(true, DeformedSmoothOvalPolyline(76.0, 36.0, 0.05)),
      ShapeFlat(true, DeformedSmoothOvalPolyline(70.0, 42.0, 0.10)),
      ShapeFlat(true, DeformedSmoothOvalPolyline(82.0, 32.0, 0.08)),
  };
}

void TestAdaptiveExpansionCallsAcceptedLayout() {
  bbsolver::SolverConfig config;
  bbsolver::PathFrameFitOptions options;
  options.outline_tolerance = 0.45;
  bool callback_called = false;
  int callback_seed = -1;
  int callback_insertions = 0;

  const bbsolver::ReplacementAdaptiveExpansionResult result =
      bbsolver::TryReplacementAdaptiveFractionExpansion(
          ExpansionFrames(),
          {Record(UniformClosedFractions(6))},
          {0},
          config,
          options,
          18,
          6,
          [&](const std::vector<double>& fractions,
              int seed_idx,
              int adaptive_count) {
            callback_called = true;
            callback_seed = seed_idx;
            callback_insertions = adaptive_count;
            return fractions.size() > 6;
          });

  Require(result.applied, "accepted adaptive expansion should apply");
  Require(callback_called, "adaptive expansion should invoke callback");
  Require(callback_seed == 0, "adaptive expansion seed index changed");
  Require(callback_insertions > 0, "adaptive insertion count should be reported");
  Require(result.evaluations > 0, "adaptive evaluations should be counted");
  Require(result.best_attempt_error <= options.outline_tolerance,
          "adaptive best error should track accepted expansion");
}

void TestAdaptiveExpansionSkipsWhenBoundCannotGrow() {
  bbsolver::SolverConfig config;
  bbsolver::PathFrameFitOptions options;
  bool callback_called = false;

  const bbsolver::ReplacementAdaptiveExpansionResult result =
      bbsolver::TryReplacementAdaptiveFractionExpansion(
          ExpansionFrames(),
          {Record(UniformClosedFractions(6))},
          {0},
          config,
          options,
          7,
          6,
          [&](const std::vector<double>&, int, int) {
            callback_called = true;
            return true;
          });

  Require(!result.applied, "non-growing adaptive bound should not apply");
  Require(result.evaluations == 0, "non-growing bound should run no candidates");
  Require(!callback_called, "non-growing bound should not invoke callback");
}

void TestAdaptiveExpansionSkipsEmptySeeds() {
  bbsolver::SolverConfig config;
  bbsolver::PathFrameFitOptions options;
  bool callback_called = false;

  const bbsolver::ReplacementAdaptiveExpansionResult result =
      bbsolver::TryReplacementAdaptiveFractionExpansion(
          ExpansionFrames(),
          {Record(UniformClosedFractions(6))},
          {},
          config,
          options,
          18,
          6,
          [&](const std::vector<double>&, int, int) {
            callback_called = true;
            return true;
          });

  Require(!result.applied, "empty seeds should not apply");
  Require(result.evaluations == 0, "empty seeds should run no candidates");
  Require(!callback_called, "empty seeds should not invoke callback");
}

}  // namespace

int main() {
  TestAdaptiveExpansionCallsAcceptedLayout();
  TestAdaptiveExpansionSkipsWhenBoundCannotGrow();
  TestAdaptiveExpansionSkipsEmptySeeds();
  std::cout << "[PASS] test_path_replacement_adaptive_expansion\n";
  return 0;
}
