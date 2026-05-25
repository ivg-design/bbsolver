#include "bbsolver/path/multimode/path_multimode_input_validation.hpp"
#include "bbsolver/domain.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <cstddef>

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    std::abort();
  }
}

std::vector<double> ShapeFlat(int vertex_count) {
  std::vector<double> out;
  out.reserve(static_cast<std::size_t>(2 + vertex_count * 6));
  out.push_back(1.0);
  out.push_back(static_cast<double>(vertex_count));
  for (int vertex = 0; vertex < vertex_count; ++vertex) {
    out.push_back(static_cast<double>(vertex * 10));
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
  }
  return out;
}

bbsolver::PropertySamples ShapeSamples(
    std::vector<std::vector<double>> values) {
  bbsolver::PropertySamples ps;
  ps.property.id = "unit/path_multimode_input_validation";
  ps.property.kind = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions =
      values.empty() ? 0 : static_cast<int>(values.front().size());
  ps.t_start_sec = 0.0;
  ps.t_end_sec = static_cast<double>(values.empty() ? 0 : values.size() - 1);
  ps.samples_per_frame = 1;
  for (std::size_t idx = 0; idx < values.size(); ++idx) {
    ps.samples.push_back({static_cast<double>(idx), std::move(values[idx])});
  }
  return ps;
}

void TestDefaultValidationResult() {
  const bbsolver::path_multimode::ShapeFlatInputValidation result;
  Require(!result.ok, "default validation must not be ok");
  Require(result.vertex_count == 0, "default validation vertex count is zero");
  Require(result.note.empty(), "default validation note is empty");
}

void TestMultiModeValidInput() {
  const bbsolver::PropertySamples original =
      ShapeSamples({ShapeFlat(2), ShapeFlat(2)});
  const bbsolver::PropertySamples reduced = original;
  const bbsolver::path_multimode::ShapeFlatInputValidation result =
      bbsolver::path_multimode::ValidateShapeFlatMultiModeInputs(original,
                                                                 reduced);
  Require(result.ok, "valid multimode input must pass validation");
  Require(result.vertex_count == 2, "valid input must return vertex count");
  Require(result.note.empty(), "valid multimode input must have no note");
}

void TestMultiModeFailureNotes() {
  bbsolver::PropertySamples original =
      ShapeSamples({ShapeFlat(2), ShapeFlat(2)});
  bbsolver::PropertySamples reduced = original;

  original.property.units_label = "other";
  Require(bbsolver::path_multimode::ValidateShapeFlatMultiModeInputs(
              original, reduced)
              .note == "shape_multimode_requires_shape_flat",
          "non-shape inputs must preserve requires-shape note");

  original = ShapeSamples({});
  reduced = ShapeSamples({ShapeFlat(2)});
  Require(bbsolver::path_multimode::ValidateShapeFlatMultiModeInputs(
              original, reduced)
              .note == "shape_multimode_no_samples",
          "empty inputs must preserve no-samples note");

  original = ShapeSamples({ShapeFlat(2), ShapeFlat(2)});
  reduced = original;
  reduced.samples.back().t_sec += 2e-9;
  Require(bbsolver::path_multimode::ValidateShapeFlatMultiModeInputs(
              original, reduced)
              .note == "shape_multimode_sample_time_mismatch",
          "sample-time mismatch must preserve mismatch note");

  original = ShapeSamples({{1.0, 2.0}});
  reduced = original;
  Require(bbsolver::path_multimode::ValidateShapeFlatMultiModeInputs(
              original, reduced)
              .note == "shape_multimode_malformed_topology",
          "malformed first frame must preserve malformed-topology note");

  original = ShapeSamples({ShapeFlat(2), ShapeFlat(3)});
  reduced = original;
  Require(bbsolver::path_multimode::ValidateShapeFlatMultiModeInputs(
              original, reduced)
              .note == "shape_multimode_unstable_topology",
          "changing topology must preserve unstable-topology note");
}

void TestLandmarkValidationSilentFailures() {
  bbsolver::PropertySamples reduced = ShapeSamples({ShapeFlat(2)});
  bbsolver::path_multimode::ShapeFlatInputValidation result =
      bbsolver::path_multimode::ValidateShapeFlatLandmarkInput(reduced);
  Require(result.ok, "valid landmark input must pass validation");
  Require(result.vertex_count == 2, "landmark validation returns vertex count");
  Require(result.note.empty(), "landmark validation never adds success note");

  reduced.property.units_label = "other";
  result = bbsolver::path_multimode::ValidateShapeFlatLandmarkInput(reduced);
  Require(!result.ok && result.note.empty(),
          "non-shape landmark input must fail silently");

  reduced = ShapeSamples({});
  result = bbsolver::path_multimode::ValidateShapeFlatLandmarkInput(reduced);
  Require(!result.ok && result.note.empty(),
          "empty landmark input must fail silently");

  reduced = ShapeSamples({{1.0, 2.0}});
  result = bbsolver::path_multimode::ValidateShapeFlatLandmarkInput(reduced);
  Require(!result.ok && result.note.empty(),
          "malformed landmark input must fail silently");

  reduced = ShapeSamples({ShapeFlat(2), ShapeFlat(3)});
  result = bbsolver::path_multimode::ValidateShapeFlatLandmarkInput(reduced);
  Require(!result.ok && result.note.empty(),
          "unstable landmark topology must fail silently");
}

}  // namespace

int main() {
  TestDefaultValidationResult();
  TestMultiModeValidInput();
  TestMultiModeFailureNotes();
  TestLandmarkValidationSilentFailures();
  std::cout << "[PASS] test_path_multimode_input_validation\n";
  return 0;
}
