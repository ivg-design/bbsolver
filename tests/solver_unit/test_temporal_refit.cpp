#include <cassert>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "bbsolver/domain.hpp"
#include "bbsolver/temporal/refit/temporal_refit_budget.hpp"  // IWYU pragma: keep
#include "bbsolver/temporal/refit/temporal_refit_candidate.hpp"
#include "bbsolver/temporal/refit/temporal_refit_dimensions.hpp"  // IWYU pragma: keep
#include "bbsolver/temporal/refit/temporal_refit.hpp"
#include "bbsolver/temporal/refit/temporal_refit_shape.hpp"
#include "bbsolver/temporal/refit/temporal_refit_structural.hpp"  // IWYU pragma: keep
#include "bbsolver/temporal/refit/temporal_refit_support.hpp"
#include "bbsolver/temporal/refit/temporal_refit_validation.hpp"
#include "bbsolver/dp/dp_placer.hpp"

namespace {

bbsolver::PropertySamples MakeSource(const std::vector<double>& values);
bbsolver::PropertyKeys MakeKeys(const std::vector<double>& values);

bbsolver::PropertySamples MakeSource(int sample_count) {
  std::vector<double> values;
  values.reserve(static_cast<std::size_t>(sample_count));
  for (int i = 0; i < sample_count; ++i) {
    values.push_back(static_cast<double>(i));
  }
  return MakeSource(values);
}

bbsolver::PropertySamples MakeSource(const std::vector<double>& values) {
  bbsolver::PropertySamples source;
  source.property.id = "temporal-refit-test";
  source.property.kind = bbsolver::ValueKind::Scalar;
  source.property.dimensions = 1;
  source.t_start_sec = 0.0;
  source.t_end_sec =
      values.empty() ? 0.0 : static_cast<double>(values.size() - 1);
  for (std::size_t i = 0; i < values.size(); ++i) {
    bbsolver::Sample sample;
    sample.t_sec = static_cast<double>(i);
    sample.v = {values[i]};
    source.samples.push_back(sample);
  }
  return source;
}

bbsolver::PropertyKeys MakeKeys(int key_count) {
  std::vector<double> values;
  values.reserve(static_cast<std::size_t>(key_count));
  for (int i = 0; i < key_count; ++i) {
    values.push_back(static_cast<double>(i));
  }
  return MakeKeys(values);
}

bbsolver::PropertyKeys MakeKeys(const std::vector<double>& values) {
  bbsolver::PropertyKeys keys;
  keys.property_id = "temporal-refit-test";
  for (std::size_t i = 0; i < values.size(); ++i) {
    bbsolver::Key key;
    key.t_sec = static_cast<double>(i);
    key.v = {values[i]};
    key.interp_in = bbsolver::InterpType::Linear;
    key.interp_out = bbsolver::InterpType::Linear;
    keys.keys.push_back(key);
  }
  return keys;
}

bool Near(double a, double b) {
  return std::abs(a - b) <= 1e-9;
}

void AssertKeysPreserved(const bbsolver::PropertyKeys& actual,
                         const bbsolver::PropertyKeys& expected) {
  assert(actual.property_id == expected.property_id);
  assert(actual.keys.size() == expected.keys.size());
  for (std::size_t i = 0; i < expected.keys.size(); ++i) {
    assert(Near(actual.keys[i].t_sec, expected.keys[i].t_sec));
    assert(actual.keys[i].v == expected.keys[i].v);
    assert(actual.keys[i].interp_in == expected.keys[i].interp_in);
    assert(actual.keys[i].interp_out == expected.keys[i].interp_out);
  }
}

void AssertNoAttemptRejectedWithoutProgress(
    const bbsolver::TemporalRefitResult& result,
    const bbsolver::PropertyKeys& expected_keys,
    const std::vector<bbsolver::PlacementProgress>& events,
    const std::string& rejection_reason) {
  assert(!result.attempted);
  assert(!result.accepted);
  assert(result.rejection_reason == rejection_reason);
  assert(result.input_key_count == static_cast<int>(expected_keys.keys.size()));
  assert(result.output_key_count == static_cast<int>(expected_keys.keys.size()));
  AssertKeysPreserved(result.keys, expected_keys);
  assert(events.empty());
  assert(result.notes.find("temporal_refit_attempted=false") !=
         std::string::npos);
  const std::string input_token =
      "temporal_refit_input_keys=" +
      std::to_string(expected_keys.keys.size());
  const std::string output_token =
      "temporal_refit_output_keys=" +
      std::to_string(expected_keys.keys.size());
  assert(result.notes.find(input_token) != std::string::npos);
  assert(result.notes.find(output_token) != std::string::npos);
  const std::string token = "temporal_refit_rejected=" + rejection_reason;
  assert(result.notes.find(token) != std::string::npos);
}

void AssertAttemptedRejectedWithValidationProgress(
    const bbsolver::TemporalRefitResult& result,
    const bbsolver::PropertyKeys& expected_keys,
    const std::vector<bbsolver::PlacementProgress>& events,
    const std::string& rejection_reason) {
  assert(result.attempted);
  assert(!result.accepted);
  assert(result.rejection_reason == rejection_reason);
  assert(result.input_key_count == static_cast<int>(expected_keys.keys.size()));
  assert(result.output_key_count == static_cast<int>(expected_keys.keys.size()));
  AssertKeysPreserved(result.keys, expected_keys);
  assert(result.notes.find("temporal_refit_attempted=true") !=
         std::string::npos);
  const std::string input_token =
      "temporal_refit_input_keys=" +
      std::to_string(expected_keys.keys.size());
  const std::string output_token =
      "temporal_refit_output_keys=" +
      std::to_string(expected_keys.keys.size());
  assert(result.notes.find(input_token) != std::string::npos);
  assert(result.notes.find(output_token) != std::string::npos);
  const std::string token = "temporal_refit_rejected=" + rejection_reason;
  assert(result.notes.find(token) != std::string::npos);
  assert(result.notes.find("temporal_refit_max_err=") != std::string::npos);
  assert(result.notes.find("temporal_refit_max_err_screen_px=") !=
         std::string::npos);
  assert(events.size() >= 3);
  assert(events.front().stage == "temporal_refit_start");
  assert(events.front().step_index == 0);
  assert(events.front().step_total == 1);
  int start_count = 0;
  int validate_index = -1;
  int validate_count = 0;
  int done_count = 0;
  for (std::size_t i = 0; i < events.size(); ++i) {
    assert(events[i].stage == "temporal_refit_start" ||
           events[i].stage == "temporal_refit_progress" ||
           events[i].stage == "temporal_refit_validate" ||
           events[i].stage == "temporal_refit_done");
    if (events[i].stage == "temporal_refit_start") {
      ++start_count;
    }
    if (events[i].stage == "temporal_refit_validate") {
      ++validate_count;
      validate_index = static_cast<int>(i);
      assert(events[i].step_index == 1);
      assert(events[i].step_total == 2);
    }
    if (events[i].stage == "temporal_refit_done") {
      ++done_count;
    }
  }
  assert(start_count == 1);
  assert(validate_count == 1);
  assert(done_count == 1);
  assert(validate_index > 0);
  assert(validate_index < static_cast<int>(events.size() - 1));
  assert(events.back().stage == "temporal_refit_done");
  assert(events.back().step_index == 1);
  assert(events.back().step_total == 1);
}

void AssertAcceptedWithValidationProgress(
    const bbsolver::TemporalRefitResult& result,
    const bbsolver::PropertyKeys& input_keys,
    const std::vector<bbsolver::PlacementProgress>& events) {
  assert(result.attempted);
  assert(result.accepted);
  assert(result.rejection_reason.empty());
  assert(result.input_key_count == static_cast<int>(input_keys.keys.size()));
  assert(result.output_key_count == static_cast<int>(result.keys.keys.size()));
  assert(result.output_key_count < result.input_key_count);
  assert(result.notes.find("temporal_refit_attempted=true") !=
         std::string::npos);
  assert(result.notes.find("temporal_refit_accepted=true") !=
         std::string::npos);
  const std::string input_token =
      "temporal_refit_input_keys=" +
      std::to_string(input_keys.keys.size());
  const std::string output_token =
      "temporal_refit_output_keys=" +
      std::to_string(result.keys.keys.size());
  const std::string reduction_token =
      "temporal_refit_reduction=" +
      std::to_string(input_keys.keys.size() - result.keys.keys.size());
  assert(result.notes.find(input_token) != std::string::npos);
  assert(result.notes.find(output_token) != std::string::npos);
  assert(result.notes.find(reduction_token) != std::string::npos);
  assert(result.notes.find("temporal_refit_max_err=") != std::string::npos);
  assert(result.notes.find("temporal_refit_max_err_screen_px=") !=
         std::string::npos);
  assert(events.size() >= 3);
  assert(events.front().stage == "temporal_refit_start");
  assert(events.front().step_index == 0);
  assert(events.front().step_total == 1);
  int validate_index = -1;
  int start_count = 0;
  int validate_count = 0;
  int done_count = 0;
  for (std::size_t i = 0; i < events.size(); ++i) {
    assert(events[i].stage == "temporal_refit_start" ||
           events[i].stage == "temporal_refit_progress" ||
           events[i].stage == "temporal_refit_validate" ||
           events[i].stage == "temporal_refit_done");
    if (events[i].stage == "temporal_refit_start") {
      ++start_count;
    }
    if (events[i].stage == "temporal_refit_validate") {
      ++validate_count;
      validate_index = static_cast<int>(i);
      assert(events[i].step_index == 1);
      assert(events[i].step_total == 2);
    }
    if (events[i].stage == "temporal_refit_done") {
      ++done_count;
    }
  }
  assert(start_count == 1);
  assert(validate_count == 1);
  assert(done_count == 1);
  assert(validate_index > 0);
  assert(validate_index < static_cast<int>(events.size() - 1));
  assert(events.back().stage == "temporal_refit_done");
  assert(events.back().step_index == 1);
  assert(events.back().step_total == 1);
}

std::vector<double> ShapeFlat(const std::vector<std::pair<double, double>>& vertices) {
  std::vector<double> flat;
  flat.reserve(2 + vertices.size() * 6);
  flat.push_back(1.0);
  flat.push_back(static_cast<double>(vertices.size()));
  for (const auto& vertex : vertices) {
    flat.push_back(vertex.first);
    flat.push_back(vertex.second);
    flat.push_back(0.0);
    flat.push_back(0.0);
    flat.push_back(0.0);
    flat.push_back(0.0);
  }
  return flat;
}

std::vector<double> ShapeFlatRect(double x, double y, double w, double h) {
  return ShapeFlat({
      {x, y},
      {x + w, y},
      {x + w, y + h},
      {x, y + h},
  });
}

std::vector<double> RedundantShapeFlatRect(double x, double y, double w, double h) {
  return ShapeFlat({
      {x, y},
      {x + w * 0.5, y},
      {x + w, y},
      {x + w, y + h * 0.5},
      {x + w, y + h},
      {x + w * 0.5, y + h},
      {x, y + h},
      {x, y + h * 0.5},
  });
}

bbsolver::PropertySamples MakeShapeFlatSource(int sample_count) {
  bbsolver::PropertySamples source;
  source.property.id = "temporal-refit-shape-test";
  source.property.kind = bbsolver::ValueKind::Custom;
  source.property.units_label = "shape_flat";
  source.property.dimensions =
      static_cast<int>(RedundantShapeFlatRect(0.0, 0.0, 10.0, 10.0).size());
  source.samples_per_frame = 1;
  source.t_start_sec = 0.0;
  source.t_end_sec = static_cast<double>(sample_count - 1);
  for (int i = 0; i < sample_count; ++i) {
    bbsolver::Sample sample;
    sample.t_sec = static_cast<double>(i);
    sample.v = RedundantShapeFlatRect(static_cast<double>(i), 0.0, 10.0, 10.0);
    source.samples.push_back(std::move(sample));
  }
  return source;
}

bbsolver::Key ShapeKey(double t_sec, const std::vector<double>& flat) {
  bbsolver::Key key;
  key.t_sec = t_sec;
  key.v = flat;
  key.interp_in = bbsolver::InterpType::Linear;
  key.interp_out = bbsolver::InterpType::Linear;
  key.temporal_ease_in = {{0.0, 33.3}};
  key.temporal_ease_out = {{0.0, 33.3}};
  return key;
}

bbsolver::PropertyKeys MakeReducedShapeKeys() {
  bbsolver::PropertyKeys keys;
  keys.property_id = "temporal-refit-shape-test";
  keys.keys.push_back(ShapeKey(0.0, ShapeFlatRect(0.0, 0.0, 10.0, 10.0)));
  keys.keys.push_back(ShapeKey(2.0, ShapeFlatRect(2.0, 0.0, 10.0, 10.0)));
  keys.keys.push_back(ShapeKey(4.0, ShapeFlatRect(4.0, 0.0, 10.0, 10.0)));
  return keys;
}

}  // namespace

int main() {
  {
    bbsolver::PropertySamples source = MakeSource(0);
    source.property.dimensions = 0;
    assert(bbsolver::TemporalRefitExpectedDimensions(source) == 1);
    assert(bbsolver::TemporalRefitValuesMatchDimensions({0.0}, source));
    assert(!bbsolver::TemporalRefitValuesMatchDimensions({}, source));

    source.property.dimensions = 2;
    assert(bbsolver::TemporalRefitExpectedDimensions(source) == 2);
    assert(bbsolver::TemporalRefitValuesMatchDimensions({1.0, 2.0}, source));
    assert(!bbsolver::TemporalRefitValuesMatchDimensions({1.0}, source));

    bbsolver::PropertyKeys keys;
    assert(!bbsolver::AllTemporalRefitCandidateKeysMatchDimensions(keys,
                                                                   source));
    bbsolver::Key key;
    key.v = {1.0, 2.0};
    keys.keys.push_back(key);
    assert(bbsolver::AllTemporalRefitCandidateKeysMatchDimensions(keys,
                                                                  source));
    keys.keys.back().v = {1.0};
    assert(!bbsolver::AllTemporalRefitCandidateKeysMatchDimensions(keys,
                                                                   source));
  }

  {
    bbsolver::SolverConfig cfg;
    cfg.tolerance = -2.0;
    assert(Near(bbsolver::StrictPropertyCeiling(cfg), 0.0));
    cfg.tolerance = 3.5;
    assert(Near(bbsolver::StrictPropertyCeiling(cfg), 3.5));

    cfg.tolerance_screen_px = 0.0;
    cfg.weight_screen = 0.0;
    assert(!bbsolver::TemporalRefitScreenGateEnabled(cfg));
    cfg.weight_screen = 1.0;
    assert(bbsolver::TemporalRefitScreenGateEnabled(cfg));
    cfg.weight_screen = 0.0;
    cfg.tolerance_screen_px = 2.25;
    assert(bbsolver::TemporalRefitScreenGateEnabled(cfg));

    cfg.tolerance = 0.75;
    cfg.tolerance_screen_px = 0.0;
    assert(Near(bbsolver::StrictScreenCeiling(cfg), 0.75));
    cfg.tolerance_screen_px = 1.25;
    assert(Near(bbsolver::StrictScreenCeiling(cfg), 1.25));

    cfg.tolerance_screen_px = 0.0;
    cfg.weight_screen = 0.0;
    assert(Near(bbsolver::RelativeCeilingFromBaseline(1.0, 5.0, cfg, -7.0),
                1.0));
    cfg.weight_screen = 1.0;
    assert(Near(bbsolver::RelativeCeilingFromBaseline(1.0, 5.0, cfg, 0.5),
                5.5));
  }

  {
    bbsolver::PropertySamples empty = MakeSource(0);
    empty.property.id = "empty-endpoint";
    const bbsolver::PropertyKeys empty_candidate =
        bbsolver::TwoEndpointCandidate(empty);
    assert(empty_candidate.property_id == "empty-endpoint");
    assert(empty_candidate.keys.empty());

    bbsolver::PropertySamples single = MakeSource(std::vector<double>{7.0});
    single.property.id = "single-endpoint";
    const bbsolver::PropertyKeys single_candidate =
        bbsolver::TwoEndpointCandidate(single);
    assert(single_candidate.property_id == "single-endpoint");
    assert(single_candidate.keys.empty());

    bbsolver::PropertySamples source =
        MakeSource(std::vector<double>{2.0, 4.0, 8.0});
    source.property.id = "endpoint-candidate";
    bbsolver::PropertyKeys candidate = bbsolver::TwoEndpointCandidate(source);
    assert(candidate.property_id == "endpoint-candidate");
    assert(candidate.keys.size() == 2);
    assert(Near(candidate.keys.front().t_sec, 0.0));
    assert(Near(candidate.keys.back().t_sec, 2.0));
    assert(candidate.keys.front().v == std::vector<double>{2.0});
    assert(candidate.keys.back().v == std::vector<double>{8.0});
    assert(candidate.keys.front().interp_in == bbsolver::InterpType::Linear);
    assert(candidate.keys.front().interp_out == bbsolver::InterpType::Linear);
    assert(candidate.keys.back().interp_in == bbsolver::InterpType::Linear);
    assert(candidate.keys.back().interp_out == bbsolver::InterpType::Linear);

    source.samples.front().v = {999.0};
    assert(candidate.keys.front().v == std::vector<double>{2.0});
  }

  {
    bbsolver::TemporalRefitOptions options;
    assert(!bbsolver::TemporalRefitCancelled(options));

    int cancel_checks = 0;
    options.cancel_fn = [&]() {
      ++cancel_checks;
      return cancel_checks == 1;
    };
    assert(bbsolver::TemporalRefitCancelled(options));
    assert(cancel_checks == 1);
  }

  {
    std::vector<bbsolver::PlacementProgress> events;
    bbsolver::TemporalRefitOptions options;
    options.progress_fn = [&](const bbsolver::PlacementProgress& progress) {
      events.push_back(progress);
    };

    bbsolver::EmitTemporalRefitProgress(options, "temporal_refit_probe", 2, 5);

    assert(events.size() == 1);
    assert(events.front().stage == "temporal_refit_probe");
    assert(events.front().step_index == 2);
    assert(events.front().step_total == 5);
    assert(events.front().sample_index == -1);
    assert(events.front().samples == 0);
    assert(events.front().segments_tried == 0);
    assert(events.front().segments_feasible == 0);
  }

  {
    bbsolver::TemporalRefitOptions options;
    bbsolver::EmitTemporalRefitProgress(options, "temporal_refit_noop", 1, 1);
  }

  {
    bbsolver::TemporalRefitResult neutral;
    neutral.input_key_count = 3;
    neutral.output_key_count = 3;
    const std::string neutral_notes =
        bbsolver::BuildTemporalRefitNotes(neutral);
    assert(neutral_notes ==
           "temporal_refit_attempted=false; temporal_refit_input_keys=3; "
           "temporal_refit_output_keys=3");

    bbsolver::TemporalRefitResult accepted;
    accepted.attempted = true;
    accepted.accepted = true;
    accepted.input_key_count = 5;
    accepted.output_key_count = 2;
    accepted.max_err = 0.125;
    accepted.max_err_screen_px = 0.25;
    const std::string accepted_notes =
        bbsolver::BuildTemporalRefitNotes(accepted);
    assert(accepted_notes ==
           "temporal_refit_attempted=true; temporal_refit_input_keys=5; "
           "temporal_refit_output_keys=2; temporal_refit_accepted=true; "
           "temporal_refit_reduction=3; temporal_refit_max_err=0.125000; "
           "temporal_refit_max_err_screen_px=0.250000");

    bbsolver::TemporalRefitResult rejected;
    rejected.attempted = true;
    rejected.rejection_reason = "over_budget";
    rejected.input_key_count = 4;
    rejected.output_key_count = 4;
    const std::string rejected_notes =
        bbsolver::BuildTemporalRefitNotes(rejected);
    assert(rejected_notes ==
           "temporal_refit_attempted=true; temporal_refit_input_keys=4; "
           "temporal_refit_output_keys=4; "
           "temporal_refit_rejected=over_budget; "
           "temporal_refit_max_err=0.000000; "
           "temporal_refit_max_err_screen_px=0.000000");
  }

  {
    bbsolver::PropertySamples numeric = MakeSource(1);
    assert(bbsolver::TemporalRefitValidationNote(numeric) ==
           "; temporal_refit_validation=numeric_linf");

    bbsolver::PropertySamples custom = numeric;
    custom.property.kind = bbsolver::ValueKind::Custom;
    custom.property.units_label = "unsupported_blob";
    assert(bbsolver::TemporalRefitValidationNote(custom) ==
           "; temporal_refit_validation=unsupported_custom");

    bbsolver::PropertySamples shape = custom;
    shape.property.units_label = "shape_flat";
    assert(bbsolver::TemporalRefitValidationNote(shape) ==
           "; temporal_refit_validation=shape_outline");
  }

  {
    bbsolver::PropertySamples custom = MakeSource(3);
    custom.property.kind = bbsolver::ValueKind::Custom;
    custom.property.dimensions = 3;
    custom.property.units_label = "unsupported_blob";
    custom.property.id = "custom-resample";
    custom.t_start_sec = -0.25;
    custom.t_end_sec = 1.25;
    custom.samples_per_frame = 3;
    custom.hash_of_expression = "unsupported-custom-expression";
    const bbsolver::PropertySamples resampled =
        bbsolver::ResampleAcceptedAtSourceTimes(MakeKeys(3), custom);
    assert(resampled.property.id == "custom-resample");
    assert(resampled.property.kind == bbsolver::ValueKind::Custom);
    assert(resampled.property.dimensions == custom.property.dimensions);
    assert(resampled.property.units_label == "unsupported_blob");
    assert(Near(resampled.t_start_sec, custom.t_start_sec));
    assert(Near(resampled.t_end_sec, custom.t_end_sec));
    assert(resampled.samples_per_frame == custom.samples_per_frame);
    assert(resampled.hash_of_expression == custom.hash_of_expression);
    assert(resampled.samples.empty());
  }

  {
    const std::vector<double> valid = ShapeFlatRect(0.0, 0.0, 10.0, 10.0);
    assert(bbsolver::IsValidTemporalRefitShapeFlatValue(valid));
    assert(!bbsolver::IsValidTemporalRefitShapeFlatValue({1.0}));

    std::vector<double> nonfinite = valid;
    nonfinite[2] = std::numeric_limits<double>::infinity();
    assert(!bbsolver::IsValidTemporalRefitShapeFlatValue(nonfinite));

    std::vector<double> wrong_size = valid;
    wrong_size.pop_back();
    assert(!bbsolver::IsValidTemporalRefitShapeFlatValue(wrong_size));

    std::vector<double> open = valid;
    open[0] = 0.0;
    assert(!bbsolver::TemporalRefitShapeFlatTopologyMatches(valid, open));
    assert(bbsolver::TemporalRefitShapeFlatTopologyMatches(valid, valid));

    bbsolver::PropertyKeys stable;
    stable.keys.push_back(ShapeKey(0.0, valid));
    stable.keys.push_back(ShapeKey(1.0, valid));
    assert(bbsolver::AllTemporalRefitShapeFlatKeysHaveStableTopology(stable));
    stable.keys.back().v = RedundantShapeFlatRect(0.0, 0.0, 10.0, 10.0);
    assert(!bbsolver::AllTemporalRefitShapeFlatKeysHaveStableTopology(stable));
  }

  {
    bbsolver::PropertySamples source = MakeSource(4);
    bbsolver::PropertyKeys keys = MakeKeys(4);
    assert(bbsolver::RefitStructuralRejection(source, keys).empty());

    source.samples_per_frame = 2;
    assert(bbsolver::RefitStructuralRejection(source, keys) ==
           "ineligible_subframe_samples");

    source = MakeSource(4);
    keys.keys.back().t_sec += 0.01;
    assert(bbsolver::RefitStructuralRejection(source, keys) ==
           "ineligible_endpoint_mismatch");

    source = MakeSource(2);
    keys = MakeKeys(2);
    assert(bbsolver::RefitStructuralRejection(source, keys) == "degenerate");
  }

  {
    bbsolver::PropertySamples source = MakeSource(4);
    source.samples_per_frame = 2;
    const bbsolver::PropertyKeys keys = MakeKeys(4);
    std::vector<bbsolver::PlacementProgress> events;
    bbsolver::TemporalRefitOptions options;
    options.progress_fn = [&](const bbsolver::PlacementProgress& progress) {
      events.push_back(progress);
    };

    const bbsolver::TemporalRefitResult result =
        bbsolver::TryTemporalRefitKeyReduction(
            source, keys, bbsolver::SolverConfig{}, bbsolver::CompInfo{},
            options);

    AssertNoAttemptRejectedWithoutProgress(
        result, keys, events, "ineligible_subframe_samples");
  }

  {
    const bbsolver::PropertySamples source = MakeSource(4);
    bbsolver::PropertyKeys keys = MakeKeys(4);
    keys.keys.back().t_sec += 0.01;
    std::vector<bbsolver::PlacementProgress> events;
    bbsolver::TemporalRefitOptions options;
    options.progress_fn = [&](const bbsolver::PlacementProgress& progress) {
      events.push_back(progress);
    };

    const bbsolver::TemporalRefitResult result =
        bbsolver::TryTemporalRefitKeyReduction(
            source, keys, bbsolver::SolverConfig{}, bbsolver::CompInfo{},
            options);

    AssertNoAttemptRejectedWithoutProgress(
        result, keys, events, "ineligible_endpoint_mismatch");
  }

  {
    bbsolver::PropertySamples source = MakeSource(4);
    source.property.dimensions = 1;
    bbsolver::PropertyKeys keys = MakeKeys(4);
    keys.keys[1].v = {1.0, 2.0};
    assert(bbsolver::RefitStructuralRejection(source, keys) ==
           "ineligible_dimensions");

    std::vector<bbsolver::PlacementProgress> events;
    bbsolver::TemporalRefitOptions options;
    options.progress_fn = [&](const bbsolver::PlacementProgress& progress) {
      events.push_back(progress);
    };

    const bbsolver::TemporalRefitResult result =
        bbsolver::TryTemporalRefitKeyReduction(
            source, keys, bbsolver::SolverConfig{}, bbsolver::CompInfo{},
            options);

    AssertNoAttemptRejectedWithoutProgress(
        result, keys, events, "ineligible_dimensions");

    bbsolver::PropertyKeys endpoint_keys;
    endpoint_keys.property_id = source.property.id;
    bbsolver::Key first;
    first.t_sec = source.samples.front().t_sec;
    first.v = {0.0, 0.0};
    first.interp_in = bbsolver::InterpType::Linear;
    first.interp_out = bbsolver::InterpType::Linear;
    bbsolver::Key last = first;
    last.t_sec = source.samples.back().t_sec;
    last.v = {3.0, 3.0};
    endpoint_keys.keys = {first, last};

    const bbsolver::PropertySamples resampled =
        bbsolver::ResampleAcceptedAtSourceTimes(endpoint_keys, source);
    assert(resampled.samples.empty());
  }

  {
    bbsolver::SolverConfig cfg;
    cfg.tolerance = 0.25;
    assert(bbsolver::StrictPropertyCeiling(cfg) == 0.25);
    cfg.tolerance = -1.0;
    assert(bbsolver::StrictPropertyCeiling(cfg) == 0.0);

    cfg.tolerance = 0.5;
    cfg.tolerance_screen_px = 0.0;
    cfg.weight_screen = 0.0;
    assert(!bbsolver::TemporalRefitScreenGateEnabled(cfg));
    assert(bbsolver::StrictScreenCeiling(cfg) == 0.5);

    cfg.tolerance_screen_px = 1.25;
    assert(bbsolver::TemporalRefitScreenGateEnabled(cfg));
    assert(bbsolver::StrictScreenCeiling(cfg) == 1.25);
    assert(bbsolver::RelativeCeilingFromBaseline(0.2, 0.9, cfg, 0.05) ==
           0.95);
    assert(bbsolver::RelativeCeilingFromBaseline(0.2, 0.9, cfg, -1.0) ==
           0.9);
  }

  {
    const bbsolver::PropertyKeys empty =
        bbsolver::TwoEndpointCandidate(MakeSource(1));
    assert(empty.keys.empty());

    const bbsolver::PropertySamples source = MakeSource({4.0, 5.0, 9.0});
    const bbsolver::PropertyKeys endpoints =
        bbsolver::TwoEndpointCandidate(source);
    assert(endpoints.property_id == source.property.id);
    assert(endpoints.keys.size() == 2);
    assert(Near(endpoints.keys.front().t_sec, source.samples.front().t_sec));
    assert(Near(endpoints.keys.back().t_sec, source.samples.back().t_sec));
    assert(Near(endpoints.keys.front().v[0], 4.0));
    assert(Near(endpoints.keys.back().v[0], 9.0));
    assert(endpoints.keys.front().interp_in == bbsolver::InterpType::Linear);
    assert(endpoints.keys.front().interp_out == bbsolver::InterpType::Linear);
    assert(endpoints.keys.back().interp_in == bbsolver::InterpType::Linear);
    assert(endpoints.keys.back().interp_out == bbsolver::InterpType::Linear);
  }

  {
    bbsolver::PropertySamples source = MakeSource({0.0, 0.0, 0.0, 0.0, 0.0});
    source.property.id = "resample-source-time-metadata";
    source.property.kind = bbsolver::ValueKind::Scalar;
    source.property.dimensions = 1;
    source.property.units_label = "px";
    source.t_start_sec = 0.0;
    for (std::size_t i = 0; i < source.samples.size(); ++i) {
      source.samples[i].t_sec = static_cast<double>(i) * 0.25;
    }
    source.t_end_sec = source.samples.back().t_sec;

    bbsolver::PropertyKeys accepted;
    accepted.property_id = "temporal-refit-test";
    bbsolver::Key first;
    first.t_sec = 0.0;
    first.v = {0.0};
    first.interp_in = bbsolver::InterpType::Linear;
    first.interp_out = bbsolver::InterpType::Linear;
    bbsolver::Key last = first;
    last.t_sec = 1.0;
    last.v = {4.0};
    accepted.keys = {first, last};

    const bbsolver::PropertySamples resampled =
        bbsolver::ResampleAcceptedAtSourceTimes(accepted, source);

    assert(resampled.property.id == source.property.id);
    assert(resampled.property.kind == source.property.kind);
    assert(resampled.property.dimensions == source.property.dimensions);
    assert(resampled.property.units_label == source.property.units_label);
    assert(Near(resampled.t_start_sec, source.t_start_sec));
    assert(Near(resampled.t_end_sec, source.t_end_sec));
    assert(resampled.samples.size() == source.samples.size());
    for (std::size_t i = 0; i < source.samples.size(); ++i) {
      assert(Near(resampled.samples[i].t_sec, source.samples[i].t_sec));
    }
    assert(Near(resampled.samples[0].v[0], 0.0));
    assert(Near(resampled.samples[2].v[0], 2.0));
    assert(Near(resampled.samples[4].v[0], 4.0));
  }

  {
    const bbsolver::PropertySamples source = MakeSource(3);
    const bbsolver::PropertyKeys exact = MakeKeys({0.0, 2.0});
    bbsolver::SolverConfig cfg;
    cfg.tolerance = 0.01;
    double max_err = -1.0;
    double max_err_screen_px = -1.0;

    const bool exact_ok = bbsolver::ValidateRefitAgainstSource(
        exact, source, cfg, bbsolver::CompInfo{},
        bbsolver::TemporalRefitOptions::BudgetMode::Strict, 0.0,
        &max_err, &max_err_screen_px);

    assert(exact_ok);
    assert(Near(max_err, 0.0));
    assert(Near(max_err_screen_px, 0.0));

    const bbsolver::PropertyKeys miss = MakeKeys({0.0, 4.0});
    const bool strict_ok = bbsolver::ValidateRefitAgainstSource(
        miss, source, cfg, bbsolver::CompInfo{},
        bbsolver::TemporalRefitOptions::BudgetMode::Strict, 0.0,
        &max_err, &max_err_screen_px);
    assert(!strict_ok);
    assert(max_err > 0.01);

    const bool relative_ok = bbsolver::ValidateRefitAgainstSource(
        miss, source, cfg, bbsolver::CompInfo{},
        bbsolver::TemporalRefitOptions::BudgetMode::Relative, max_err + 0.01,
        &max_err, &max_err_screen_px);
    assert(relative_ok);
  }

  {
    const bbsolver::PropertySamples source = MakeSource(3);
    const bbsolver::PropertyKeys keys = MakeKeys(3);

    std::vector<bbsolver::PlacementProgress> events;
    bbsolver::TemporalRefitOptions options;
    options.progress_fn = [&](const bbsolver::PlacementProgress& progress) {
      events.push_back(progress);
    };

    const bbsolver::TemporalRefitResult result =
        bbsolver::TryTemporalRefitKeyReduction(
            source, keys, bbsolver::SolverConfig{}, bbsolver::CompInfo{},
            options);

    assert(result.attempted);
    assert(result.accepted);
    assert(result.rejection_reason.empty());
    assert(result.input_key_count == 3);
    assert(result.output_key_count == 2);
    assert(result.keys.keys.size() == 2);
    assert(result.keys.converged);
    assert(Near(result.keys.max_err, result.max_err));
    assert(Near(result.keys.max_err_screen_px, result.max_err_screen_px));
    assert(Near(result.keys.keys.front().t_sec, source.samples.front().t_sec));
    assert(Near(result.keys.keys.back().t_sec, source.samples.back().t_sec));
    AssertAcceptedWithValidationProgress(result, keys, events);
    assert(result.notes.find("temporal_refit_validation=numeric_linf") !=
           std::string::npos);
  }

  {
    const bbsolver::PropertySamples source = MakeSource({0.0, 10.0, 0.0});
    const bbsolver::PropertyKeys keys = MakeKeys({0.0, 0.0, 0.0});
    std::vector<bbsolver::PlacementProgress> events;
    bbsolver::TemporalRefitOptions options;
    options.progress_fn = [&](const bbsolver::PlacementProgress& progress) {
      events.push_back(progress);
    };

    const bbsolver::TemporalRefitResult result =
        bbsolver::TryTemporalRefitKeyReduction(
            source, keys, bbsolver::SolverConfig{}, bbsolver::CompInfo{},
            options);

    AssertAttemptedRejectedWithValidationProgress(
        result, keys, events, "over_budget");
  }

  {
    const bbsolver::PropertySamples source = MakeSource({0.0, 10.0, 0.0});
    const bbsolver::PropertyKeys keys = MakeKeys({0.0, 10.0, 0.0});
    std::vector<bbsolver::PlacementProgress> events;
    bbsolver::TemporalRefitOptions options;
    options.progress_fn = [&](const bbsolver::PlacementProgress& progress) {
      events.push_back(progress);
    };

    const bbsolver::TemporalRefitResult result =
        bbsolver::TryTemporalRefitKeyReduction(
            source, keys, bbsolver::SolverConfig{}, bbsolver::CompInfo{},
            options);

    AssertAttemptedRejectedWithValidationProgress(
        result, keys, events, "no_gain");
  }

  {
    const bbsolver::PropertySamples source = MakeSource(2);
    const bbsolver::PropertyKeys keys = MakeKeys(2);
    std::vector<bbsolver::PlacementProgress> events;
    bbsolver::TemporalRefitOptions options;
    options.progress_fn = [&](const bbsolver::PlacementProgress& progress) {
      events.push_back(progress);
    };

    const bbsolver::TemporalRefitResult result =
        bbsolver::TryTemporalRefitKeyReduction(
            source, keys, bbsolver::SolverConfig{}, bbsolver::CompInfo{},
            options);

    AssertNoAttemptRejectedWithoutProgress(result, keys, events, "degenerate");
    assert(result.notes.find("temporal_refit_attempted=false") !=
           std::string::npos);
  }

  {
    bbsolver::PropertySamples source = MakeSource(3);
    source.property.kind = bbsolver::ValueKind::Custom;
    source.property.units_label = "unsupported_blob";
    const bbsolver::PropertyKeys keys = MakeKeys(3);
    std::vector<bbsolver::PlacementProgress> events;
    bbsolver::TemporalRefitOptions options;
    options.progress_fn = [&](const bbsolver::PlacementProgress& progress) {
      events.push_back(progress);
    };

    const bbsolver::TemporalRefitResult result =
        bbsolver::TryTemporalRefitKeyReduction(
            source, keys, bbsolver::SolverConfig{}, bbsolver::CompInfo{},
            options);

    AssertNoAttemptRejectedWithoutProgress(
        result, keys, events, "ineligible_custom_property");
    assert(result.notes.find("temporal_refit_validation=unsupported_custom") !=
           std::string::npos);
  }

  {
    bbsolver::PropertySamples source = MakeShapeFlatSource(3);
    source.samples[1].v = {1.0};
    const bbsolver::PropertyKeys keys = MakeReducedShapeKeys();
    std::vector<bbsolver::PlacementProgress> events;
    bbsolver::TemporalRefitOptions options;
    options.progress_fn = [&](const bbsolver::PlacementProgress& progress) {
      events.push_back(progress);
    };

    const bbsolver::TemporalRefitResult result =
        bbsolver::TryTemporalRefitKeyReduction(
            source, keys, bbsolver::SolverConfig{}, bbsolver::CompInfo{},
            options);

    AssertNoAttemptRejectedWithoutProgress(
        result, keys, events, "ineligible_shape_flat_source_malformed");
    assert(result.notes.find("temporal_refit_validation=shape_outline") !=
           std::string::npos);
  }

  {
    const bbsolver::PropertySamples source = MakeShapeFlatSource(5);
    bbsolver::PropertyKeys keys = MakeReducedShapeKeys();
    keys.keys[1].v = RedundantShapeFlatRect(1.0, 0.0, 10.0, 10.0);

    assert(bbsolver::RefitStructuralRejection(source, keys) ==
           "ineligible_shape_flat_key_topology");

    std::vector<bbsolver::PlacementProgress> events;
    bbsolver::TemporalRefitOptions options;
    options.progress_fn = [&](const bbsolver::PlacementProgress& progress) {
      events.push_back(progress);
    };

    const bbsolver::TemporalRefitResult result =
        bbsolver::TryTemporalRefitKeyReduction(
            source, keys, bbsolver::SolverConfig{}, bbsolver::CompInfo{},
            options);

    AssertNoAttemptRejectedWithoutProgress(
        result, keys, events, "ineligible_shape_flat_key_topology");
    assert(result.notes.find("temporal_refit_validation=shape_outline") !=
           std::string::npos);
  }

  {
    const bbsolver::PropertySamples source = MakeShapeFlatSource(5);
    bbsolver::PropertyKeys candidate = MakeReducedShapeKeys();
    candidate.keys[1].v = RedundantShapeFlatRect(1.0, 0.0, 10.0, 10.0);
    bbsolver::SolverConfig cfg;
    cfg.tolerance = 0.01;
    double max_err = -1.0;
    double max_err_screen_px = -1.0;

    const bool ok = bbsolver::ValidateShapeRefitAgainstSource(
        candidate, source, cfg,
        bbsolver::TemporalRefitOptions::BudgetMode::Strict, 0.0,
        &max_err, &max_err_screen_px);

    assert(!ok);
    assert(std::isinf(max_err));
    assert(std::isinf(max_err_screen_px));
  }

  {
    const bbsolver::PropertySamples source = MakeShapeFlatSource(5);
    const bbsolver::PropertyKeys accepted = MakeReducedShapeKeys();
    bbsolver::SolverConfig cfg;
    cfg.tolerance = 0.01;
    double max_err = -1.0;
    double max_err_screen_px = -1.0;

    const bbsolver::PropertySamples resampled =
        bbsolver::ResampleAcceptedAtSourceTimes(accepted, source);
    assert(resampled.samples.size() == source.samples.size());
    assert(resampled.property.kind == bbsolver::ValueKind::Custom);
    assert(resampled.property.units_label == "shape_flat");
    assert(resampled.property.dimensions ==
           static_cast<int>(accepted.keys.front().v.size()));
    assert(resampled.samples.front().v.size() != source.samples.front().v.size());

    const bool baseline_ok = bbsolver::ValidateRefitAgainstSource(
        accepted, source, cfg, bbsolver::CompInfo{},
        bbsolver::TemporalRefitOptions::BudgetMode::Strict, 0.0,
        &max_err, &max_err_screen_px);
    assert(baseline_ok);
    assert(max_err <= cfg.tolerance + 1e-9);
    assert(Near(max_err, max_err_screen_px));

    std::vector<bbsolver::PlacementProgress> events;
    bbsolver::TemporalRefitOptions options;
    options.progress_fn = [&](const bbsolver::PlacementProgress& progress) {
      events.push_back(progress);
    };

    const bbsolver::TemporalRefitResult result =
        bbsolver::TryTemporalRefitKeyReduction(
            source, accepted, cfg, bbsolver::CompInfo{}, options);

    assert(result.attempted);
    assert(result.accepted);
    assert(result.rejection_reason.empty());
    assert(result.input_key_count == 3);
    assert(result.output_key_count == 2);
    assert(result.keys.keys.size() == 2);
    assert(result.keys.converged);
    assert(Near(result.keys.max_err, result.max_err));
    assert(Near(result.keys.max_err_screen_px, result.max_err_screen_px));
    assert(result.max_err <= cfg.tolerance + 1e-9);
    AssertAcceptedWithValidationProgress(result, accepted, events);
    assert(result.notes.find("temporal_refit_validation=shape_outline") !=
           std::string::npos);
    assert(events.size() == 3);
    assert(events[0].stage == "temporal_refit_start");
    assert(events[0].step_index == 0);
    assert(events[0].step_total == 1);
    assert(events[1].stage == "temporal_refit_validate");
    assert(events[1].step_index == 1);
    assert(events[1].step_total == 2);
    assert(events[2].stage == "temporal_refit_done");
    assert(events[2].step_index == 1);
    assert(events[2].step_total == 1);
  }

  {
    const bbsolver::PropertyKeys keys = MakeKeys(3);
    std::vector<bbsolver::PlacementProgress> events;
    bbsolver::TemporalRefitOptions options;
    options.cancel_fn = []() { return true; };
    options.progress_fn = [&](const bbsolver::PlacementProgress& progress) {
      events.push_back(progress);
    };

    const bbsolver::TemporalRefitResult result =
        bbsolver::TryTemporalRefitKeyReduction(
            MakeSource(3), keys, bbsolver::SolverConfig{},
            bbsolver::CompInfo{}, options);

    AssertNoAttemptRejectedWithoutProgress(result, keys, events, "cancelled");
  }

  {
    const bbsolver::PropertyKeys keys = MakeKeys(4);
    int cancel_checks = 0;
    std::vector<bbsolver::PlacementProgress> events;
    bbsolver::TemporalRefitOptions options;
    options.cancel_fn = [&]() {
      ++cancel_checks;
      return cancel_checks >= 2;
    };
    options.progress_fn = [&](const bbsolver::PlacementProgress& progress) {
      events.push_back(progress);
    };

    const bbsolver::TemporalRefitResult result =
        bbsolver::TryTemporalRefitKeyReduction(
            MakeSource(4), keys, bbsolver::SolverConfig{},
            bbsolver::CompInfo{}, options);

    assert(result.attempted);
    assert(!result.accepted);
    assert(result.rejection_reason == "cancelled");
    assert(result.output_key_count == 4);
    AssertKeysPreserved(result.keys, keys);
    assert(result.notes.find("temporal_refit_attempted=true") !=
           std::string::npos);
    assert(result.notes.find("temporal_refit_rejected=cancelled") !=
           std::string::npos);
    assert(events.size() == 2);
    assert(events[0].stage == "temporal_refit_start");
    assert(events[0].step_index == 0);
    assert(events[0].step_total == 1);
    assert(events[1].stage == "temporal_refit_done");
    assert(events[1].step_index == 1);
    assert(events[1].step_total == 1);
  }

  return 0;
}
