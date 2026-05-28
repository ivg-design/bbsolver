#include "bbsolver/path/replacement/path_replacement_initial_scan.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>
#include <cstddef>

namespace {

void Require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

std::vector<double> ShapeFlat(bool closed, int vertex_count) {
  std::vector<double> flat;
  flat.push_back(closed ? 1.0: 0.0);
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
  samples.property.id = "unit/replacement_initial_scan";
  samples.property.kind = bbsolver::ValueKind::Custom;
  samples.property.units_label = "shape_flat";
  samples.property.dimensions =
      frames.empty() ? 2: static_cast<int>(frames.front().size());
  samples.samples_per_frame = 1;
  for (std::size_t i = 0; i < frames.size(); ++i) {
    bbsolver::Sample sample;
    sample.t_sec = static_cast<double>(i) / 24.0;
    sample.v = std::move(frames[i]);
    samples.samples.push_back(std::move(sample));
  }
  return samples;
}

void TestScansSourceAndAutoVertexRanges() {
  bbsolver::PathFrameFitOptions options;
  options.outline_tolerance = 0.5;

  const bbsolver::ReplacementInitialFrameScan scan =
      bbsolver::ScanReplacementInitialFrames(
          Samples({ShapeFlat(false, 4), ShapeFlat(false, 6)}), options);

  Require(scan.ok, "valid frames should scan successfully");
  Require(scan.source_min_vertices == 4, "source min vertices changed");
  Require(scan.source_max_vertices == 6, "source max vertices changed");
  Require(scan.auto_min_vertices > 0, "auto min should be populated");
  Require(scan.auto_max_vertices >= scan.auto_min_vertices,
          "auto range should be ordered");
}

void TestRejectsMalformedShapeFlatWithOriginalNotePrefix() {
  bbsolver::PathFrameFitOptions options;
  const bbsolver::ReplacementInitialFrameScan scan =
      bbsolver::ScanReplacementInitialFrames(Samples({{0.0, 4.0, 1.0}}),
                                             options);

  Require(!scan.ok, "malformed shape_flat should be rejected");
  Require(scan.warning.find("path_replacement_fit skipped: malformed "
                            "shape_flat sample at t=") == 0,
          "malformed warning prefix changed");
}

}  // namespace

int main() {
  TestScansSourceAndAutoVertexRanges();
  TestRejectsMalformedShapeFlatWithOriginalNotePrefix();
  std::cout << "[PASS] test_path_replacement_initial_scan\n";
  return 0;
}
