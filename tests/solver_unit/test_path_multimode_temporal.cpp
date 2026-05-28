#include "bbsolver/path/multimode/path_multimode_temporal.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"

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

bool AlmostEqual(double a, double b, double eps = 1e-9) {
  return std::abs(a - b) <= eps;
}

std::vector<double> Flat(int vertex_count) {
  std::vector<double> out;
  out.push_back(1.0);
  out.push_back(static_cast<double>(vertex_count));
  for (int vertex = 0; vertex < vertex_count; ++vertex) {
    out.push_back(static_cast<double>(vertex));
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
  }
  return out;
}

bbsolver::PropertySamples Samples(int sample_count, int vertex_count) {
  bbsolver::PropertySamples ps;
  ps.property.kind = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions = 2 + vertex_count * 6;
  for (int idx = 0; idx < sample_count; ++idx) {
    bbsolver::Sample sample;
    sample.t_sec = static_cast<double>(idx);
    sample.v = Flat(vertex_count);
    ps.samples.push_back(sample);
  }
  return ps;
}

void TestEaseAndClampHelpers() {
  const std::vector<bbsolver::TemporalEase> neutral =
      bbsolver::path_multimode::NeutralEase();
  Require(neutral.size() == 1 && neutral[0].speed == 0.0 &&
              neutral[0].influence == 33.3,
          "neutral ease must preserve the default single shape ease");

  Require(bbsolver::path_multimode::ShapeEase(150.0).front().influence ==
              100.0,
          "shape ease must clamp high influence to 100");
  Require(bbsolver::path_multimode::ShapeEase(-1.0).front().influence ==
              0.1,
          "shape ease must clamp low influence to 0.1");
  Require(bbsolver::path_multimode::ShapeEase(
              std::numeric_limits<double>::infinity()).front().influence ==
              33.3,
          "non-finite shape ease must fall back to 33.3");

  bbsolver::ShapeMorphProgressBandOptions options;
  options.min_bezier_influence = 20.0;
  options.max_bezier_influence = 80.0;
  Require(bbsolver::path_multimode::ClampInfluence(5.0, options) == 20.0,
          "influence clamp must honor configured lower bound");
  Require(bbsolver::path_multimode::ClampInfluence(95.0, options) == 80.0,
          "influence clamp must honor configured upper bound");
}

void TestProgressValues() {
  const bbsolver::PropertySamples ps = Samples(3, 1);
  const bbsolver::ShapeMorphProgressBandOptions options;
  const bbsolver::TemporalEase neutral{0.0, 33.3};
  const std::vector<double> linear =
      bbsolver::path_multimode::SegmentProgressValues(
          ps, 0, 2, false, neutral, neutral, options);
  Require(linear.size() == 3 &&
              AlmostEqual(linear[0], 0.0) &&
              AlmostEqual(linear[1], 0.5) &&
              AlmostEqual(linear[2], 1.0),
          "linear segment progress must use normalized sample time");

  const std::vector<double> bezier =
      bbsolver::path_multimode::SegmentProgressValues(
          ps, 0, 2, true, neutral, neutral, options);
  Require(bezier.size() == 3 &&
              AlmostEqual(bezier[0], 0.0, 1e-6) &&
              AlmostEqual(bezier[2], 1.0, 1e-6) &&
              bezier[1] > 0.0 && bezier[1] < 1.0,
          "bezier segment progress must remain bounded between endpoints");
  Require(bbsolver::path_multimode::SegmentProgressValues(
              ps, 2, 1, false, neutral, neutral, options).empty(),
          "invalid segment progress request must return empty");
}

void TestLandmarkInfluencePairsAndSearchGate() {
  bbsolver::ShapeMorphProgressBandOptions options;
  bbsolver::ShapeMorphProgressBandResult strict;
  strict.fitted_bezier_pairs_tried = 1;
  strict.max_fitted_bezier_error = 0.25;
  strict.fitted_bezier_out_influence = 72.0;
  strict.fitted_bezier_in_influence = 18.0;
  const std::vector<bbsolver::path_multimode::LandmarkInfluencePair> pairs =
      bbsolver::path_multimode::BuildLandmarkInfluencePairs(options, strict);
  Require(!pairs.empty() && pairs.size() <= 12,
          "landmark influence search must cap candidate pairs");
  bool found_strict = false;
  for (const auto& pair: pairs) {
    if (AlmostEqual(pair.out_influence, 72.0) &&
        AlmostEqual(pair.in_influence, 18.0)) {
      found_strict = true;
    }
  }
  Require(found_strict,
          "landmark influence search must include fitted strict pair");
  Require(bbsolver::path_multimode::SameInfluencePair(
              {33.3, 33.3}, {33.3000001, 33.3000001}),
          "influence pair comparison must use epsilon tolerance");

  Require(bbsolver::path_multimode::CanRunExtendedRelaxedBezierSearch(
              Samples(4, 2), 0, 3),
          "small two-vertex regions must allow extended relaxed search");
  Require(!bbsolver::path_multimode::CanRunExtendedRelaxedBezierSearch(
              Samples(4, 3), 0, 3),
          "larger vertex regions must skip extended relaxed search");
}

void TestLandmarkBandOptions() {
  const bbsolver::ShapeMorphProgressBandOptions options =
      bbsolver::path_multimode::LandmarkBandOptions(-2.0, 3);
  Require(options.frame_fit_options.outline_tolerance == 0.0,
          "negative landmark tolerance must clamp to zero");
  Require(options.frame_fit_options.max_subdivisions_per_segment == 8,
          "landmark band options must keep subdivision contract");
  Require(options.progress_steps == 16 &&
              options.max_window_samples == 4 &&
              options.max_evaluations == 256,
          "landmark band options must preserve progress/window budgets");
  Require(options.fit_bezier_influence_pairs &&
              options.max_bezier_influence_pairs == 8,
          "landmark band options must enable bounded influence fitting");
}

}  // namespace

int main() {
  TestEaseAndClampHelpers();
  TestProgressValues();
  TestLandmarkInfluencePairsAndSearchGate();
  TestLandmarkBandOptions();
  std::cout << "[PASS] test_path_multimode_temporal\n";
  return 0;
}
