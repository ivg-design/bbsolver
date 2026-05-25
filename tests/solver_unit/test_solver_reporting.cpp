#include "bbsolver/solve/solver_reporting.hpp"
#include "bbsolver/domain.hpp"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    std::abort();
  }
}

void TestAppendSolverNoteIgnoresEmpty() {
  bbsolver::PropertyKeys keys;
  bbsolver::AppendSolverNote(keys, "");
  Require(keys.notes.empty(), "empty note must leave notes untouched");
}

void TestAppendSolverNoteSetsFirstNote() {
  bbsolver::PropertyKeys keys;
  bbsolver::AppendSolverNote(keys, "first");
  Require(keys.notes == "first", "first note should set notes verbatim");
}

void TestAppendSolverNoteJoinsWithDelimiter() {
  bbsolver::PropertyKeys keys;
  keys.notes = "first";
  bbsolver::AppendSolverNote(keys, "second");
  Require(keys.notes == "first; second",
          "subsequent notes must join with '; '");
}

void TestAppendSolverNoteDedupesSubstringMatch() {
  bbsolver::PropertyKeys keys;
  keys.notes = "alpha; beta";
  bbsolver::AppendSolverNote(keys, "beta");
  Require(keys.notes == "alpha; beta",
          "duplicate notes must be ignored");
}

void TestAppendJoinedNoteIgnoresEmpty() {
  std::string notes = "first";
  bbsolver::AppendJoinedNote(notes, "");
  Require(notes == "first", "empty joined note must leave notes untouched");
}

void TestAppendJoinedNotePreservesDuplicates() {
  std::string notes = "same";
  bbsolver::AppendJoinedNote(notes, "same");
  Require(notes == "same; same",
          "plain joined notes must preserve duplicate text");
}

void TestAppendJoinedNoteJoinsWithDelimiter() {
  std::string notes;
  bbsolver::AppendJoinedNote(notes, "first");
  bbsolver::AppendJoinedNote(notes, "second");
  Require(notes == "first; second",
          "plain joined notes must join with '; '");
}

void TestAppendSampleTimingNoteSkipsZeroKeys() {
  std::string note = "prefix";
  bbsolver::AppendSampleTimingNote(note, 0, 0);
  Require(note == "prefix", "zero keys must skip annotation");
}

void TestAppendSampleTimingNoteFullPreservation() {
  std::string note = "prefix";
  bbsolver::AppendSampleTimingNote(note, 4, 4);
  Require(note == "prefix; source_key_timing_preserved=true",
          "full preservation must emit preserved=true");
}

void TestAppendSampleTimingNotePartialPreservation() {
  std::string note;
  bbsolver::AppendSampleTimingNote(note, 4, 3);
  Require(note == "; source_key_timing_preserved_partial=3/4",
          "partial preservation must emit ratio");
}

void TestAppendSampleTimingNoteMissingTiming() {
  std::string note;
  bbsolver::AppendSampleTimingNote(note, 4, 0);
  Require(note == "; source_key_timing_missing=true",
          "zero preserved must emit missing=true");
}

void TestAccuracyGateNoteSkipsCancelled() {
  bbsolver::PropertySamples src;
  src.samples.resize(20);
  bbsolver::PropertyKeys keys;
  keys.converged = true;
  keys.notes = "cancelled";
  keys.keys.resize(20);
  bbsolver::SolverConfig cfg;
  Require(bbsolver::AccuracyGateOptimizationNote(src, cfg, keys).empty(),
          "cancelled notes must short-circuit");
}

void TestAccuracyGateNoteSkipsUnconverged() {
  bbsolver::PropertySamples src;
  src.samples.resize(20);
  bbsolver::PropertyKeys keys;
  keys.converged = false;
  keys.keys.resize(20);
  bbsolver::SolverConfig cfg;
  Require(bbsolver::AccuracyGateOptimizationNote(src, cfg, keys).empty(),
          "unconverged must short-circuit");
}

void TestAccuracyGateNoteSkipsLowSourceCount() {
  bbsolver::PropertySamples src;
  src.samples.resize(3);
  bbsolver::PropertyKeys keys;
  keys.converged = true;
  keys.keys.resize(3);
  bbsolver::SolverConfig cfg;
  Require(bbsolver::AccuracyGateOptimizationNote(src, cfg, keys).empty(),
          "source_count < 4 must short-circuit");
}

void TestAccuracyGateNoteSkipsLargeReduction() {
  bbsolver::PropertySamples src;
  src.samples.resize(20);
  bbsolver::PropertyKeys keys;
  keys.converged = true;
  // 5*10 = 50 < 20*9 = 180 -> short-circuit (real reduction is large enough).
  keys.keys.resize(5);
  bbsolver::SolverConfig cfg;
  Require(bbsolver::AccuracyGateOptimizationNote(src, cfg, keys).empty(),
          "large reduction must short-circuit (no advisory needed)");
}

void TestAccuracyGateNoteNoPracticalReduction() {
  bbsolver::PropertySamples src;
  src.samples.resize(20);
  bbsolver::PropertyKeys keys;
  keys.converged = true;
  // key_count >= source_count -> no_practical branch.
  keys.keys.resize(20);
  bbsolver::SolverConfig cfg;
  cfg.tolerance = 0.5;
  cfg.tolerance_screen_px = 3.0;
  const std::string note =
      bbsolver::AccuracyGateOptimizationNote(src, cfg, keys);
  Require(note.find("no_practical_optimization_at_accuracy_gate=true") !=
              std::string::npos,
          "no-reduction case must say no_practical");
  Require(note.find("optimization_blocker=accuracy_gate") !=
              std::string::npos,
          "note must include accuracy_gate blocker");
  Require(note.find("output_keys=20") != std::string::npos,
          "note must report output_keys=20");
  Require(note.find("source_samples=20") != std::string::npos,
          "note must report source_samples=20");
}

void TestAccuracyGateNoteMarginalReduction() {
  bbsolver::PropertySamples src;
  src.samples.resize(20);
  bbsolver::PropertyKeys keys;
  keys.converged = true;
  // 19*10 = 190 >= 20*9 = 180 -> marginal advisory.
  keys.keys.resize(19);
  bbsolver::SolverConfig cfg;
  const std::string note =
      bbsolver::AccuracyGateOptimizationNote(src, cfg, keys);
  Require(note.find("marginal_optimization_at_accuracy_gate=true") !=
              std::string::npos,
          "near-source-count case must say marginal");
  Require(note.find("key_reduction=1") != std::string::npos,
          "marginal note must report key_reduction=1");
}

void TestUnifiedSpatialWarningSkipsNonSpatial() {
  bbsolver::PropertySamples samples;
  samples.property.is_spatial = false;
  samples.property.is_separated = false;
  std::ostringstream out;
  bbsolver::WarnUnifiedSpatialPropertyIfNeeded(samples, out);
  Require(out.str().empty(), "non-spatial properties must not warn");
}

void TestUnifiedSpatialWarningSkipsSeparatedSpatial() {
  bbsolver::PropertySamples samples;
  samples.property.is_spatial = true;
  samples.property.is_separated = true;
  std::ostringstream out;
  bbsolver::WarnUnifiedSpatialPropertyIfNeeded(samples, out);
  Require(out.str().empty(), "separated spatial properties must not warn");
}

void TestUnifiedSpatialWarningIncludesPropertyId() {
  bbsolver::PropertySamples samples;
  samples.property.id = "Layer/Position";
  samples.property.is_spatial = true;
  samples.property.is_separated = false;
  std::ostringstream out;
  bbsolver::WarnUnifiedSpatialPropertyIfNeeded(samples, out);
  Require(out.str().find("spatial property 'Layer/Position'") !=
              std::string::npos,
          "warning must include property id");
  Require(out.str().find("unified AE spatial fitting") != std::string::npos,
          "warning must preserve operator-facing wording");
}

void TestFinalStaticTrimNoteSkipsEmptyNote() {
  bbsolver::PropertyKeys keys;
  keys.keys.resize(1);
  keys.keys.back().interp_out = bbsolver::InterpType::Linear;
  bbsolver::ApplyFinalStaticTrimNote(keys, "");
  Require(keys.notes.empty(), "empty final static trim note must not append");
  Require(keys.keys.back().interp_out == bbsolver::InterpType::Linear,
          "empty final static trim note must not mutate interpolation");
}

void TestFinalStaticTrimNoteAppendsWithoutKeys() {
  bbsolver::PropertyKeys keys;
  bbsolver::ApplyFinalStaticTrimNote(keys, "final_static_trim=true");
  Require(keys.notes == "final_static_trim=true",
          "final static trim note must append without keys");
}

void TestFinalStaticTrimNoteHoldsLastKeyOut() {
  bbsolver::PropertyKeys keys;
  keys.keys.resize(2);
  keys.keys[0].interp_out = bbsolver::InterpType::Linear;
  keys.keys[1].interp_out = bbsolver::InterpType::Bezier;
  bbsolver::ApplyFinalStaticTrimNote(keys, "final_static_trim=true");
  Require(keys.notes == "final_static_trim=true",
          "final static trim note must append");
  Require(keys.keys[0].interp_out == bbsolver::InterpType::Linear,
          "final static trim note must not mutate earlier keys");
  Require(keys.keys[1].interp_out == bbsolver::InterpType::Hold,
          "final static trim note must hold the final key out interpolation");
}

}  // namespace

int main() {
  TestAppendSolverNoteIgnoresEmpty();
  TestAppendSolverNoteSetsFirstNote();
  TestAppendSolverNoteJoinsWithDelimiter();
  TestAppendSolverNoteDedupesSubstringMatch();
  TestAppendJoinedNoteIgnoresEmpty();
  TestAppendJoinedNotePreservesDuplicates();
  TestAppendJoinedNoteJoinsWithDelimiter();
  TestAppendSampleTimingNoteSkipsZeroKeys();
  TestAppendSampleTimingNoteFullPreservation();
  TestAppendSampleTimingNotePartialPreservation();
  TestAppendSampleTimingNoteMissingTiming();
  TestAccuracyGateNoteSkipsCancelled();
  TestAccuracyGateNoteSkipsUnconverged();
  TestAccuracyGateNoteSkipsLowSourceCount();
  TestAccuracyGateNoteSkipsLargeReduction();
  TestAccuracyGateNoteNoPracticalReduction();
  TestAccuracyGateNoteMarginalReduction();
  TestUnifiedSpatialWarningSkipsNonSpatial();
  TestUnifiedSpatialWarningSkipsSeparatedSpatial();
  TestUnifiedSpatialWarningIncludesPropertyId();
  TestFinalStaticTrimNoteSkipsEmptyNote();
  TestFinalStaticTrimNoteAppendsWithoutKeys();
  TestFinalStaticTrimNoteHoldsLastKeyOut();
  std::cout << "[PASS] test_solver_reporting\n";
  return 0;
}
