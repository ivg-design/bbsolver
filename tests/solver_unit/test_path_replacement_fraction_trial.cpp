#include "bbsolver/path/replacement/path_replacement_fraction_trial.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/shape/shape_flat_topology.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <cstddef>

namespace {

struct ProgressEvent {
  std::string event;
  double local_fraction = 0.0;
  int frame_index = 0;
  int frame_total = 0;
};

void Require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

std::vector<double> ShapeFlat(bool closed, int vertex_count) {
  std::vector<double> flat;
  flat.push_back(closed ? 1.0 : 0.0);
  flat.push_back(static_cast<double>(vertex_count));
  for (int i = 0; i < vertex_count; ++i) {
    flat.push_back(static_cast<double>(i) * 10.0);
    flat.push_back(0.0);
    flat.push_back(0.0);
    flat.push_back(0.0);
    flat.push_back(0.0);
    flat.push_back(0.0);
  }
  return flat;
}

bbsolver::PropertySamples Samples(std::vector<std::vector<double>> frames) {
  bbsolver::PropertySamples samples;
  samples.property.id = "unit/replacement_fraction_trial";
  samples.property.kind = bbsolver::ValueKind::Custom;
  samples.property.units_label = "shape_flat";
  samples.property.dimensions =
      frames.empty() ? 2 : static_cast<int>(frames.front().size());
  samples.samples_per_frame = 1;
  for (std::size_t i = 0; i < frames.size(); ++i) {
    bbsolver::Sample sample;
    sample.t_sec = static_cast<double>(i) / 24.0;
    sample.v = std::move(frames[i]);
    samples.samples.push_back(std::move(sample));
  }
  return samples;
}

void TestReplaysFractionsAndEmitsProgress() {
  bbsolver::PathFrameFitOptions options;
  options.outline_tolerance = 100.0;
  std::vector<ProgressEvent> progress_events;

  const bbsolver::ReplacementFractionTrialResult result =
      bbsolver::TryReplacementFractionLayout(
          Samples({ShapeFlat(false, 5), ShapeFlat(false, 5)}),
          {0.0, 0.5, 1.0},
          options,
          [&](const char* event,
              const std::string&,
              double local_fraction,
              int frame_index,
              int frame_total) {
            progress_events.push_back(
                {event, local_fraction, frame_index, frame_total});
          });

  Require(result.ok, "valid fraction replay should be accepted");
  Require(result.samples.samples.size() == 2,
          "accepted replay should preserve sample count");
  Require(result.fraction_count == 3, "fraction count changed");
  Require(result.fractions == std::vector<double>({0.0, 0.5, 1.0}),
          "winning fractions should be preserved");
  Require(bbsolver::ShapeFlatVertexCount(result.samples.samples[0].v) == 3,
          "accepted replay should produce target vertex count");
  Require(!progress_events.empty(), "fraction replay should emit progress");
  Require(progress_events.back().event ==
              "path_replacement_target_layout_progress",
          "fraction replay progress event changed");
  Require(progress_events.back().frame_index == 2,
          "fraction replay final frame index changed");
}

void TestRejectsInvalidFractionsWithoutAcceptedSamples() {
  bbsolver::PathFrameFitOptions options;
  options.outline_tolerance = 100.0;

  const bbsolver::ReplacementFractionTrialResult result =
      bbsolver::TryReplacementFractionLayout(
          Samples({ShapeFlat(false, 5)}),
          {0.5, 0.25},
          options);

  Require(!result.ok, "invalid fraction order should be rejected");
  Require(result.samples.samples.empty(),
          "rejected first frame should not keep accepted samples");
}

}  // namespace

int main() {
  TestReplaysFractionsAndEmitsProgress();
  TestRejectsInvalidFractionsWithoutAcceptedSamples();
  std::cout << "[PASS] test_path_replacement_fraction_trial\n";
  return 0;
}
