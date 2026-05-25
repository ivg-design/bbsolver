#include "bbsolver/solve/static_key_cleanup.hpp"
#include "bbsolver/domain.hpp"

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

bbsolver::Sample MakeSample(double t, std::vector<double> v) {
  bbsolver::Sample sample;
  sample.t_sec = t;
  sample.v = std::move(v);
  return sample;
}

bbsolver::Key MakeKey(double t, std::vector<double> v) {
  bbsolver::Key key;
  key.t_sec = t;
  key.v = std::move(v);
  return key;
}

void TestKeyValuesEqualWithinSizeMismatch() {
  Require(!bbsolver::KeyValuesEqualWithin({1.0}, {1.0, 2.0}),
          "differing sizes must not compare equal");
}

void TestKeyValuesEqualWithinUsesEps() {
  Require(bbsolver::KeyValuesEqualWithin({1.0}, {1.0 + 1e-12}),
          "tiny diff must compare equal under default eps");
  Require(!bbsolver::KeyValuesEqualWithin({1.0}, {1.0 + 1e-3}),
          "0.001 diff must exceed default eps");
}

void TestFindFinalStaticSuffixStartSampleEmpty() {
  std::vector<bbsolver::Sample> samples;
  Require(bbsolver::FindFinalStaticSuffixStartSample(samples) == -1,
          "empty samples must return -1");
}

void TestFindFinalStaticSuffixStartSampleSingle() {
  std::vector<bbsolver::Sample> samples;
  samples.push_back(MakeSample(0.0, {1.0}));
  Require(bbsolver::FindFinalStaticSuffixStartSample(samples) == -1,
          "single sample cannot form a static suffix");
}

void TestFindFinalStaticSuffixStartSampleNoSuffix() {
  std::vector<bbsolver::Sample> samples;
  samples.push_back(MakeSample(0.0, {1.0}));
  samples.push_back(MakeSample(1.0, {2.0}));
  Require(bbsolver::FindFinalStaticSuffixStartSample(samples) == -1,
          "differing final two values must not yield a suffix");
}

void TestFindFinalStaticSuffixStartSampleAllSame() {
  std::vector<bbsolver::Sample> samples;
  for (int i = 0; i < 4; ++i) {
    samples.push_back(MakeSample(static_cast<double>(i), {7.0}));
  }
  Require(bbsolver::FindFinalStaticSuffixStartSample(samples) == 0,
          "all-equal samples must report suffix starting at index 0");
}

void TestFindFinalStaticSuffixStartSamplePartialSuffix() {
  std::vector<bbsolver::Sample> samples;
  samples.push_back(MakeSample(0.0, {1.0}));
  samples.push_back(MakeSample(1.0, {2.0}));
  samples.push_back(MakeSample(2.0, {3.0}));
  samples.push_back(MakeSample(3.0, {3.0}));
  samples.push_back(MakeSample(4.0, {3.0}));
  Require(bbsolver::FindFinalStaticSuffixStartSample(samples) == 2,
          "tail of three equal samples must start at index 2");
}

void TestTrimSamplesAfterTimeShortInput() {
  bbsolver::PropertySamples ps;
  ps.t_end_sec = 5.0;
  ps.samples.push_back(MakeSample(0.0, {1.0}));
  const int removed = bbsolver::TrimSamplesAfterTime(ps, 0.0);
  Require(removed == 0, "single-sample input must not be trimmed");
  Require(ps.samples.size() == 1, "single sample must be retained");
  Require(ps.t_end_sec == 5.0,
          "t_end_sec must be unchanged when no trim happens");
}

void TestTrimSamplesAfterTimeNoOpInBounds() {
  bbsolver::PropertySamples ps;
  ps.samples.push_back(MakeSample(0.0, {1.0}));
  ps.samples.push_back(MakeSample(1.0, {1.0}));
  ps.samples.push_back(MakeSample(2.0, {1.0}));
  ps.t_end_sec = 2.0;
  Require(bbsolver::TrimSamplesAfterTime(ps, 2.0) == 0,
          "no samples past end_t must yield zero trim");
  Require(ps.samples.size() == 3, "no trim must preserve every sample");
}

void TestTrimSamplesAfterTimeTrimsTail() {
  bbsolver::PropertySamples ps;
  for (int i = 0; i < 5; ++i) {
    ps.samples.push_back(MakeSample(static_cast<double>(i), {1.0}));
  }
  ps.t_end_sec = 4.0;
  const int removed = bbsolver::TrimSamplesAfterTime(ps, 2.0);
  Require(removed == 2, "trim must remove samples strictly past end_t");
  Require(ps.samples.size() == 3, "three samples must remain");
  Require(ps.samples.back().t_sec == 2.0,
          "last sample must equal end_t");
  Require(ps.t_end_sec == 2.0, "t_end_sec must follow the new tail");
}

void TestTrimSamplesAfterTimeHonorsEpsilon() {
  bbsolver::PropertySamples ps;
  ps.samples.push_back(MakeSample(0.0, {1.0}));
  ps.samples.push_back(MakeSample(1.0, {1.0}));
  ps.samples.push_back(MakeSample(2.0, {1.0}));
  ps.t_end_sec = 2.0;
  // 2.0 + 1e-7 (time_eps) > 2.0 - 1e-8, so no sample is past the cutoff.
  Require(bbsolver::TrimSamplesAfterTime(ps, 2.0 - 1e-8) == 0,
          "tail within epsilon must not be trimmed");
}

void TestCollapseNoOpWhenUnconverged() {
  bbsolver::PropertySamples src;
  bbsolver::SolverConfig cfg;
  bbsolver::CompInfo comp;
  bbsolver::PropertyKeys keys;
  keys.converged = false;
  keys.keys.push_back(MakeKey(0.0, {1.0}));
  keys.keys.push_back(MakeKey(1.0, {1.0}));
  const auto result =
      bbsolver::CollapseRedundantStaticKeyRuns(src, cfg, comp, keys);
  Require(!result.attempted, "unconverged input must not attempt collapse");
  Require(!result.accepted, "unconverged input must not accept collapse");
  Require(result.keys_removed == 0, "no keys may be marked removed");
  Require(keys.keys.size() == 2, "keys must be untouched");
  Require(keys.notes.empty(), "notes must remain empty");
}

void TestCollapseNoOpWhenTooFewKeys() {
  bbsolver::PropertySamples src;
  bbsolver::SolverConfig cfg;
  bbsolver::CompInfo comp;
  bbsolver::PropertyKeys keys;
  keys.converged = true;
  keys.keys.push_back(MakeKey(0.0, {1.0}));
  const auto result =
      bbsolver::CollapseRedundantStaticKeyRuns(src, cfg, comp, keys);
  Require(!result.attempted && result.keys_removed == 0,
          "single-key input cannot collapse");
}

void TestCollapseNoOpWhenAllKeysDistinct() {
  bbsolver::PropertySamples src;
  bbsolver::SolverConfig cfg;
  bbsolver::CompInfo comp;
  bbsolver::PropertyKeys keys;
  keys.converged = true;
  keys.keys.push_back(MakeKey(0.0, {1.0}));
  keys.keys.push_back(MakeKey(1.0, {2.0}));
  keys.keys.push_back(MakeKey(2.0, {3.0}));
  const auto result =
      bbsolver::CollapseRedundantStaticKeyRuns(src, cfg, comp, keys);
  Require(!result.attempted, "distinct keys must not trigger validation");
  Require(result.keys_removed == 0,
          "no removable keys when values all differ");
  Require(keys.keys.size() == 3, "keys vector must be unchanged");
}

void TestAnchorNoOpWhenUnconverged() {
  bbsolver::PropertySamples src;
  src.samples.push_back(MakeSample(0.0, {1.0}));
  src.samples.push_back(MakeSample(1.0, {1.0}));
  bbsolver::SolverConfig cfg;
  bbsolver::CompInfo comp;
  bbsolver::PropertyKeys keys;
  keys.converged = false;
  keys.keys.push_back(MakeKey(0.0, {1.0}));
  keys.keys.push_back(MakeKey(1.0, {1.0}));
  const auto result =
      bbsolver::AnchorFinalStaticBoundary(src, cfg, comp, keys);
  Require(!result.attempted, "unconverged input must skip anchoring");
}

void TestAnchorNoOpWhenSourceLacksStaticSuffix() {
  bbsolver::PropertySamples src;
  src.samples.push_back(MakeSample(0.0, {1.0}));
  src.samples.push_back(MakeSample(1.0, {2.0}));
  bbsolver::SolverConfig cfg;
  bbsolver::CompInfo comp;
  bbsolver::PropertyKeys keys;
  keys.converged = true;
  keys.keys.push_back(MakeKey(0.0, {1.0}));
  keys.keys.push_back(MakeKey(1.0, {2.0}));
  const auto result =
      bbsolver::AnchorFinalStaticBoundary(src, cfg, comp, keys);
  Require(!result.attempted,
          "no static suffix in source must skip anchoring");
}

void TestAnchorNoOpWhenKeysAlreadyAtOrBeforeBoundary() {
  bbsolver::PropertySamples src;
  src.samples.push_back(MakeSample(0.0, {1.0}));
  src.samples.push_back(MakeSample(1.0, {2.0}));
  src.samples.push_back(MakeSample(2.0, {2.0}));
  src.samples.push_back(MakeSample(3.0, {2.0}));
  bbsolver::SolverConfig cfg;
  bbsolver::CompInfo comp;
  bbsolver::PropertyKeys keys;
  keys.converged = true;
  // Last key at t=1.0 matches the boundary -> short-circuit before validation.
  keys.keys.push_back(MakeKey(0.0, {1.0}));
  keys.keys.push_back(MakeKey(1.0, {2.0}));
  const auto result =
      bbsolver::AnchorFinalStaticBoundary(src, cfg, comp, keys);
  Require(!result.attempted,
          "keys ending at or before boundary must skip anchoring");
}

bbsolver::Key MakeLinearKey(double t, std::vector<double> v) {
  bbsolver::Key key = MakeKey(t, std::move(v));
  key.interp_in = bbsolver::InterpType::Linear;
  key.interp_out = bbsolver::InterpType::Linear;
  return key;
}

void TestCollapseAcceptedWhenValidatorWithinBudget() {
  // Four constant samples + four equal-valued keys. Collapsing to a single
  // Hold key still evaluates to the same constant everywhere, so the
  // validator returns max_err = 0 and the candidate is accepted.
  bbsolver::PropertySamples src;
  for (int i = 0; i < 4; ++i) {
    src.samples.push_back(MakeSample(static_cast<double>(i), {1.0}));
  }
  bbsolver::SolverConfig cfg;
  bbsolver::CompInfo comp;
  bbsolver::PropertyKeys keys;
  keys.converged = true;
  keys.max_err = 0.0;
  keys.max_err_screen_px = 0.0;
  for (int i = 0; i < 4; ++i) {
    keys.keys.push_back(MakeLinearKey(static_cast<double>(i), {1.0}));
  }

  const auto result =
      bbsolver::CollapseRedundantStaticKeyRuns(src, cfg, comp, keys);

  Require(result.attempted, "all-equal keys must trigger validation");
  Require(result.accepted, "zero-error candidate must be accepted");
  Require(result.runs_collapsed == 1,
          "single suffix run must produce runs_collapsed == 1");
  Require(result.keys_removed == 3,
          "four equal keys must drop three on collapse");
  Require(keys.keys.size() == 1, "accepted collapse must mutate keys to one");
  Require(keys.keys.front().interp_out == bbsolver::InterpType::Hold,
          "remaining key must hold its value forward");
  Require(keys.notes.find("static_key_run_collapse_accepted") !=
              std::string::npos,
          "notes must record the accepted decision");
  Require(keys.notes.find("static_keys_removed=3") != std::string::npos,
          "notes must report static_keys_removed=3");
}

void TestCollapseRejectedWhenValidatorExceedsBudget() {
  // Samples spike mid-range, but all keys claim the same constant value with
  // zero recorded error. Collapsing forces a Hold candidate that mis-fits the
  // spike, so the validator's max_err > keys.max_err + 1e-6 and the
  // candidate is rejected without mutating keys.
  bbsolver::PropertySamples src;
  src.samples.push_back(MakeSample(0.0, {1.0}));
  src.samples.push_back(MakeSample(1.0, {50.0}));
  src.samples.push_back(MakeSample(2.0, {1.0}));
  bbsolver::SolverConfig cfg;
  bbsolver::CompInfo comp;
  bbsolver::PropertyKeys keys;
  keys.converged = true;
  keys.max_err = 0.0;
  keys.max_err_screen_px = 0.0;
  for (int i = 0; i < 3; ++i) {
    keys.keys.push_back(MakeLinearKey(static_cast<double>(i), {1.0}));
  }

  const auto result =
      bbsolver::CollapseRedundantStaticKeyRuns(src, cfg, comp, keys);

  Require(result.attempted,
          "all-equal keys must still trigger the validator call");
  Require(!result.accepted, "spike sample must drive rejection");
  Require(result.runs_collapsed == 1,
          "the would-be suffix run still counts as one collapse attempt");
  Require(result.keys_removed == 2,
          "three equal keys would have removed two on collapse");
  Require(result.max_err >= 1.0,
          "validator must report a non-trivial error against the spike");
  Require(keys.keys.size() == 3,
          "rejected collapse must leave the input keys untouched");
  Require(keys.notes.find("static_key_run_collapse_rejected") !=
              std::string::npos,
          "notes must record the rejected decision");
  Require(keys.notes.find("static_keys_would_remove=2") != std::string::npos,
          "rejected notes must report static_keys_would_remove=2");
}

void TestAnchorAcceptedWhenValidatorWithinTolerance() {
  // Source ramps to 2.0 by t=1 and holds; keys mirror the same shape. The
  // anchor candidate keeps the ramp key and replaces the post-boundary keys
  // with a single Hold at t=1, which still reproduces the samples exactly.
  bbsolver::PropertySamples src;
  src.samples.push_back(MakeSample(0.0, {1.0}));
  src.samples.push_back(MakeSample(1.0, {2.0}));
  src.samples.push_back(MakeSample(2.0, {2.0}));
  src.samples.push_back(MakeSample(3.0, {2.0}));
  bbsolver::SolverConfig cfg;
  bbsolver::CompInfo comp;
  bbsolver::PropertyKeys keys;
  keys.converged = true;
  keys.keys.push_back(MakeLinearKey(0.0, {1.0}));
  keys.keys.push_back(MakeLinearKey(1.0, {2.0}));
  keys.keys.push_back(MakeLinearKey(2.0, {2.0}));
  keys.keys.push_back(MakeLinearKey(3.0, {2.0}));

  const auto result =
      bbsolver::AnchorFinalStaticBoundary(src, cfg, comp, keys);

  Require(result.attempted, "static suffix must trigger anchoring");
  Require(result.accepted, "zero-error candidate must be accepted");
  Require(result.boundary_sample == 1,
          "boundary sample must match the first equal-tail sample");
  Require(result.suffix_samples == 3, "tail of three samples must be reported");
  Require(result.tail_keys_removed == 2,
          "two keys past the boundary must be retired into a single anchor");
  Require(keys.keys.size() == 2,
          "accepted anchor must collapse the tail to a single key");
  Require(keys.keys.back().t_sec == 1.0,
          "anchor key must land at the boundary time");
  Require(keys.keys.back().interp_out == bbsolver::InterpType::Hold,
          "anchor key must hold forward past the boundary");
  Require(keys.notes.find("final_static_boundary_anchor_accepted") !=
              std::string::npos,
          "notes must record the accepted decision");
  Require(keys.notes.find("final_static_boundary_sample=1") !=
              std::string::npos,
          "notes must report final_static_boundary_sample=1");
  Require(keys.notes.find("final_static_tail_keys_removed=2") !=
              std::string::npos,
          "notes must report final_static_tail_keys_removed=2");
}

void TestAnchorRejectedWhenValidatorExceedsTolerance() {
  // Source spikes to 50 mid-range before settling; keys only carry the
  // endpoints, so anchoring at t=2 leaves the validator linearly fitting a
  // 50-unit spike from a (0,0)->(2,5) segment, blowing past tolerance.
  bbsolver::PropertySamples src;
  src.samples.push_back(MakeSample(0.0, {0.0}));
  src.samples.push_back(MakeSample(1.0, {50.0}));
  src.samples.push_back(MakeSample(2.0, {5.0}));
  src.samples.push_back(MakeSample(3.0, {5.0}));
  src.samples.push_back(MakeSample(4.0, {5.0}));
  bbsolver::SolverConfig cfg;  // tolerance = 0.5, screen weighting off
  bbsolver::CompInfo comp;
  bbsolver::PropertyKeys keys;
  keys.converged = true;
  keys.keys.push_back(MakeLinearKey(0.0, {0.0}));
  keys.keys.push_back(MakeLinearKey(4.0, {5.0}));

  const auto result =
      bbsolver::AnchorFinalStaticBoundary(src, cfg, comp, keys);

  Require(result.attempted, "static suffix must trigger anchoring");
  Require(!result.accepted, "spike sample must drive rejection");
  Require(result.boundary_sample == 2,
          "boundary sample must point at the first equal-tail sample");
  Require(result.max_err >= 1.0,
          "validator must report a non-trivial error past tolerance");
  Require(keys.keys.size() == 2,
          "rejected anchor must leave the input keys untouched");
  Require(keys.keys.back().t_sec == 4.0,
          "rejected anchor must not relocate the trailing key");
  Require(keys.notes.find("final_static_boundary_anchor_rejected") !=
              std::string::npos,
          "notes must record the rejected decision");
  Require(keys.notes.find("final_static_boundary_sample=2") !=
              std::string::npos,
          "rejected notes must still report the boundary sample index");
}

}  // namespace

int main() {
  TestKeyValuesEqualWithinSizeMismatch();
  TestKeyValuesEqualWithinUsesEps();
  TestFindFinalStaticSuffixStartSampleEmpty();
  TestFindFinalStaticSuffixStartSampleSingle();
  TestFindFinalStaticSuffixStartSampleNoSuffix();
  TestFindFinalStaticSuffixStartSampleAllSame();
  TestFindFinalStaticSuffixStartSamplePartialSuffix();
  TestTrimSamplesAfterTimeShortInput();
  TestTrimSamplesAfterTimeNoOpInBounds();
  TestTrimSamplesAfterTimeTrimsTail();
  TestTrimSamplesAfterTimeHonorsEpsilon();
  TestCollapseNoOpWhenUnconverged();
  TestCollapseNoOpWhenTooFewKeys();
  TestCollapseNoOpWhenAllKeysDistinct();
  TestAnchorNoOpWhenUnconverged();
  TestAnchorNoOpWhenSourceLacksStaticSuffix();
  TestAnchorNoOpWhenKeysAlreadyAtOrBeforeBoundary();
  TestCollapseAcceptedWhenValidatorWithinBudget();
  TestCollapseRejectedWhenValidatorExceedsBudget();
  TestAnchorAcceptedWhenValidatorWithinTolerance();
  TestAnchorRejectedWhenValidatorExceedsTolerance();
  std::cout << "[PASS] test_static_key_cleanup\n";
  return 0;
}
