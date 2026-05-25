#include "bbsolver/path/replacement/path_replacement_phase2_fit.hpp"
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
  std::string phase;
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
    flat.push_back(i == 1 ? 2.0 : 0.0);
    flat.push_back(0.0);
    flat.push_back(0.0);
    flat.push_back(0.0);
    flat.push_back(0.0);
  }
  return flat;
}

bbsolver::PropertySamples Samples(std::vector<std::vector<double>> frames) {
  bbsolver::PropertySamples samples;
  samples.property.id = "unit/replacement_phase2";
  samples.property.kind = bbsolver::ValueKind::Custom;
  samples.property.units_label = "shape_flat";
  samples.property.dimensions =
      frames.empty() ? 2 : static_cast<int>(frames.front().size());
  samples.samples_per_frame = 1;
  samples.t_start_sec = 0.0;
  samples.t_end_sec = frames.empty()
                          ? 0.0
                          : static_cast<double>(frames.size() - 1) / 24.0;
  for (std::size_t i = 0; i < frames.size(); ++i) {
    bbsolver::Sample sample;
    sample.t_sec = static_cast<double>(i) / 24.0;
    sample.v = std::move(frames[i]);
    samples.samples.push_back(std::move(sample));
  }
  return samples;
}

void TestAcceptsAlreadyTargetedFramesAndEmitsProgress() {
  const bbsolver::PropertySamples samples =
      Samples({ShapeFlat(false, 4), ShapeFlat(false, 4)});
  bbsolver::PathFrameFitOptions options;
  options.outline_tolerance = 100.0;
  options.target_vertex_count = 4;

  std::vector<ProgressEvent> progress_events;
  const bbsolver::ReplacementPhase2FitResult result =
      bbsolver::FitReplacementPhase2Records(
          samples,
          options,
          4,
          [&](const char* event,
              const std::string& phase,
              double local_fraction,
              int frame_index,
              int frame_total) {
            progress_events.push_back(
                {event, phase, local_fraction, frame_index, frame_total});
          });

  Require(result.ok, "already targeted frames should be accepted");
  Require(result.records.size() == 2, "accepted records should match samples");
  Require(result.records[0].t_sec == 0.0, "record should preserve sample time");
  Require(bbsolver::ShapeFlatVertexCount(result.records[0].fitted) == 4,
          "record should preserve target vertex count");
  Require(result.records[0].outline_fractions.size() == 4,
          "record should preserve outline fractions");
  Require(!progress_events.empty(), "progress callback should be used");
  Require(progress_events.front().event ==
              "path_replacement_target_phase2_start",
          "first progress event should be phase2 start");
  Require(progress_events.back().event ==
              "path_replacement_target_phase2_done",
          "last progress event should be phase2 done");
  Require(progress_events.back().frame_index == 2,
          "done progress should report accepted record count");
}

void TestRejectsMalformedFrameWithWarning() {
  const bbsolver::PropertySamples samples =
      Samples({{0.0, 4.0, 1.0, 2.0}});
  bbsolver::PathFrameFitOptions options;
  options.target_vertex_count = 4;

  const bbsolver::ReplacementPhase2FitResult result =
      bbsolver::FitReplacementPhase2Records(samples, options, 4);

  Require(!result.ok, "malformed shape_flat should be rejected");
  Require(result.records.empty(), "rejected first frame should emit no records");
  Require(result.warning.find("phase2_target_fit_failed") != std::string::npos,
          "malformed rejection should retain phase2 warning prefix");
}

}  // namespace

int main() {
  TestAcceptsAlreadyTargetedFramesAndEmitsProgress();
  TestRejectsMalformedFrameWithWarning();
  std::cout << "[PASS] test_path_replacement_phase2_fit\n";
  return 0;
}
