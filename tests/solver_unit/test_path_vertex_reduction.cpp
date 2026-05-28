#include "bbsolver/path/reduction/path_vertex_reduction.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/shape/shape_flat_topology.hpp"
#include "bbsolver/path/reduction/path_bridge_refit.hpp"

#include <cmath>
#include <cstdlib>
#include <functional>
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

std::vector<double> Flat(const std::vector<bbsolver::ShapeFlatVertex>& vertices,
                         bool closed = true) {
  return bbsolver::ShapeFlatFromVertices(vertices, closed);
}

std::vector<bbsolver::ShapeFlatVertex> SquareVertices() {
  return {
      {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      {1.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      {1.0, 1.0, 0.0, 0.0, 0.0, 0.0},
      {0.0, 1.0, 0.0, 0.0, 0.0, 0.0},
  };
}

std::vector<bbsolver::ShapeFlatVertex> DuplicateTerminalVertices(
    double terminal_x,
    double terminal_y) {
  return {
      {0.0, 0.0, 1.0, 2.0, 3.0, 4.0},
      {1.0, 0.0, 5.0, 6.0, 7.0, 8.0},
      {1.0, 1.0, 9.0, 10.0, 11.0, 12.0},
      {terminal_x, terminal_y, 13.0, 14.0, 15.0, 16.0},
  };
}

bbsolver::Key KeyWithFlat(const std::vector<double>& flat) {
  bbsolver::Key key;
  key.v = flat;
  return key;
}

bbsolver::Sample SampleWithFlat(const std::vector<double>& flat) {
  bbsolver::Sample sample;
  sample.v = flat;
  return sample;
}

bbsolver::PropertyKeys KeysWithFlats(
    const std::vector<std::vector<double>>& flats) {
  bbsolver::PropertyKeys keys;
  for (const std::vector<double>& flat: flats) {
    keys.keys.push_back(KeyWithFlat(flat));
  }
  return keys;
}

bbsolver::PropertySamples SamplesWithFlats(
    const std::vector<std::vector<double>>& flats) {
  bbsolver::PropertySamples samples;
  for (const std::vector<double>& flat: flats) {
    samples.samples.push_back(SampleWithFlat(flat));
  }
  return samples;
}

std::vector<double> TriangleFlat(bool closed = true) {
  return Flat({
      {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      {1.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      {0.0, 1.0, 0.0, 0.0, 0.0, 0.0},
  }, closed);
}

std::vector<double> FiveVertexFlat(bool closed = true) {
  return Flat({
      {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      {1.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      {2.0, 0.5, 0.0, 0.0, 0.0, 0.0},
      {1.0, 1.0, 0.0, 0.0, 0.0, 0.0},
      {0.0, 1.0, 0.0, 0.0, 0.0, 0.0},
  }, closed);
}

void TestRejectsOpenShape() {
  const std::vector<double> open = Flat(SquareVertices(), false);
  Require(bbsolver::BridgeRefitRemoveShapeFlatVertex(open, 1).empty(),
          "bridge refit must reject open shapes");
}

void TestRejectsSmallShapeAndInvalidIndexes() {
  const std::vector<double> triangle = Flat({
      {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      {1.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      {0.0, 1.0, 0.0, 0.0, 0.0, 0.0},
  });
  Require(bbsolver::BridgeRefitRemoveShapeFlatVertex(triangle, 1).empty(),
          "bridge refit must reject shapes with fewer than four vertices");

  const std::vector<double> square = Flat(SquareVertices());
  Require(bbsolver::BridgeRefitRemoveShapeFlatVertex(square, 0).empty(),
          "bridge refit must reject removing the first vertex");
  Require(bbsolver::BridgeRefitRemoveShapeFlatVertex(square, 4).empty(),
          "bridge refit must reject indexes past the vertex count");
}

void TestDegenerateBridgeZeroesAdjacentHandlesAndErasesVertex() {
  const std::vector<double> flat = Flat({
      {2.0, 3.0, 0.0, 0.0, 0.0, 0.0},
      {2.0, 3.0, 0.0, 0.0, 0.0, 0.0},
      {2.0, 3.0, 0.0, 0.0, 0.0, 0.0},
      {2.0, 3.0, 0.0, 0.0, 4.0, 5.0},
  });
  const std::vector<double> reduced =
      bbsolver::BridgeRefitRemoveShapeFlatVertex(flat, 1);
  Require(!reduced.empty(),
          "degenerate bridges must still return a reduced shape");
  Require(bbsolver::ShapeFlatClosed(reduced),
          "degenerate bridge refit must preserve the closed flag");
  Require(bbsolver::ShapeFlatVertexCount(reduced) == 3,
          "degenerate bridge refit must erase the removed vertex");
  const std::vector<bbsolver::ShapeFlatVertex> vertices =
      bbsolver::ShapeFlatVertices(reduced);
  Require(vertices[0].out_x == 0.0 && vertices[0].out_y == 0.0,
          "degenerate bridge refit must zero the previous out handle");
  Require(vertices[1].in_x == 0.0 && vertices[1].in_y == 0.0,
          "degenerate bridge refit must zero the next in handle");
}

void TestNonDegenerateBridgeRefitRemovesRequestedVertex() {
  const std::vector<double> reduced =
      bbsolver::BridgeRefitRemoveShapeFlatVertex(Flat(SquareVertices()), 1);
  Require(!reduced.empty(),
          "non-degenerate bridge refit must return a reduced shape");
  Require(bbsolver::ShapeFlatClosed(reduced),
          "non-degenerate bridge refit must preserve the closed flag");
  Require(bbsolver::ShapeFlatVertexCount(reduced) == 3,
          "non-degenerate bridge refit must erase one vertex");

  const std::vector<bbsolver::ShapeFlatVertex> vertices =
      bbsolver::ShapeFlatVertices(reduced);
  Require(vertices[0].x == 0.0 && vertices[0].y == 0.0,
          "bridge refit must keep the previous vertex position");
  Require(vertices[1].x == 1.0 && vertices[1].y == 1.0,
          "bridge refit must keep the next vertex after removal");
  Require(vertices[2].x == 0.0 && vertices[2].y == 1.0,
          "bridge refit must keep trailing vertices after removal");
  Require(std::isfinite(vertices[0].out_x) && std::isfinite(vertices[0].out_y),
          "bridge refit must solve finite previous out handles");
  Require(std::isfinite(vertices[1].in_x) && std::isfinite(vertices[1].in_y),
          "bridge refit must solve finite next in handles");
}

void TestDuplicateTerminalClosureDetectionTolerance() {
  const std::vector<double> within_floor =
      Flat(DuplicateTerminalVertices(5e-7, 0.0));
  Require(bbsolver::ShapeFlatHasDuplicateTerminalClosure(within_floor, 0.0),
          "duplicate detection must use the 1e-6 minimum tolerance");

  const std::vector<double> outside_floor =
      Flat(DuplicateTerminalVertices(2e-6, 0.0));
  Require(!bbsolver::ShapeFlatHasDuplicateTerminalClosure(outside_floor, 0.0),
          "duplicate detection must reject distances outside the tolerance");

  const std::vector<double> within_configured =
      Flat(DuplicateTerminalVertices(0.25, 0.0));
  Require(bbsolver::ShapeFlatHasDuplicateTerminalClosure(
              within_configured, 0.5),
          "duplicate detection must honor configured tolerance");
}

void TestDuplicateTerminalClosureRejectsOpenAndSmallShapes() {
  Require(!bbsolver::ShapeFlatHasDuplicateTerminalClosure(
              Flat(DuplicateTerminalVertices(0.0, 0.0), false), 1.0),
          "duplicate detection must reject open shapes");

  const std::vector<double> closed_triangle = Flat({
      {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      {1.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
  });
  Require(!bbsolver::ShapeFlatHasDuplicateTerminalClosure(
              closed_triangle, 1.0),
          "duplicate detection must reject shapes with fewer than four vertices");
}

void TestDropDuplicateTerminalClosureKeepsSmallInputsUnchanged() {
  const std::vector<double> one_vertex = Flat({
      {3.0, 4.0, 5.0, 6.0, 7.0, 8.0},
  });
  Require(bbsolver::DropShapeFlatDuplicateTerminalClosure(one_vertex) ==
              one_vertex,
          "duplicate drop must return n <= 1 vectors unchanged");

  const std::vector<double> malformed = {1.0, 2.0, 3.0};
  Require(bbsolver::DropShapeFlatDuplicateTerminalClosure(malformed) ==
              malformed,
          "duplicate drop must return malformed zero-count vectors unchanged");
}

void TestDropDuplicateTerminalClosureReducesAndTransfersIncomingTangent() {
  const std::vector<double> flat =
      Flat(DuplicateTerminalVertices(0.0, 0.0));
  const std::vector<double> dropped =
      bbsolver::DropShapeFlatDuplicateTerminalClosure(flat);
  Require(bbsolver::ShapeFlatClosed(dropped),
          "duplicate drop must preserve the closed flag");
  Require(bbsolver::ShapeFlatVertexCount(dropped) == 3,
          "duplicate drop must reduce the vertex count by one");

  const std::vector<bbsolver::ShapeFlatVertex> vertices =
      bbsolver::ShapeFlatVertices(dropped);
  Require(vertices[0].x == 0.0 && vertices[0].y == 0.0,
          "duplicate drop must preserve vertex zero position");
  Require(vertices[0].in_x == 13.0 && vertices[0].in_y == 14.0,
          "duplicate drop must transfer terminal incoming tangent to vertex zero");
  Require(vertices[0].out_x == 3.0 && vertices[0].out_y == 4.0,
          "duplicate drop must preserve vertex zero outgoing tangent");
  Require(vertices[2].x == 1.0 && vertices[2].y == 1.0,
          "duplicate drop must keep the first n-1 vertex payloads");
}

void TestMinShapeFlatVertexCountEmptyMalformedAndValid() {
  Require(bbsolver::MinShapeFlatVertexCount(SamplesWithFlats({})) == 0,
          "sample min vertex count must return zero for empty samples");
  Require(bbsolver::MinShapeFlatVertexCount(
              SamplesWithFlats({FiveVertexFlat(), {}, TriangleFlat()})) == 0,
          "sample min vertex count must return zero when any sample is malformed");
  Require(bbsolver::MinShapeFlatVertexCount(
              SamplesWithFlats({FiveVertexFlat(), TriangleFlat(),
                                Flat(SquareVertices())})) == 3,
          "sample min vertex count must return the lowest valid count");
}

void TestMaxShapeFlatSampleVertexCountEmptyMalformedAndValid() {
  Require(bbsolver::MaxShapeFlatSampleVertexCount(SamplesWithFlats({})) == 0,
          "sample max vertex count must return zero for empty samples");
  Require(bbsolver::MaxShapeFlatSampleVertexCount(
              SamplesWithFlats({TriangleFlat(), {1.0, 2.0, 3.0}})) == 0,
          "sample max vertex count must return zero when any sample is malformed");
  Require(bbsolver::MaxShapeFlatSampleVertexCount(
              SamplesWithFlats({TriangleFlat(), Flat(SquareVertices()),
                                FiveVertexFlat()})) == 5,
          "sample max vertex count must return the highest valid count");
}

void TestMinShapeFlatKeyVertexCountEmptyMalformedAndValid() {
  Require(bbsolver::MinShapeFlatKeyVertexCount(KeysWithFlats({})) == 0,
          "key min vertex count must return zero for empty keys");
  Require(bbsolver::MinShapeFlatKeyVertexCount(
              KeysWithFlats({FiveVertexFlat(), {}, TriangleFlat()})) == 0,
          "key min vertex count must return zero when any key is malformed");
  Require(bbsolver::MinShapeFlatKeyVertexCount(
              KeysWithFlats({FiveVertexFlat(), TriangleFlat(),
                             Flat(SquareVertices())})) == 3,
          "key min vertex count must return the lowest valid count");
}

void TestUniformShapeFlatKeyTopologyCases() {
  Require(!bbsolver::UniformShapeFlatKeyTopology(KeysWithFlats({})),
          "uniform topology must reject empty keys");
  Require(!bbsolver::UniformShapeFlatKeyTopology(
              KeysWithFlats({{}, Flat(SquareVertices())})),
          "uniform topology must reject invalid first keys");
  Require(bbsolver::UniformShapeFlatKeyTopology(
              KeysWithFlats({Flat(SquareVertices()),
                             Flat(SquareVertices())})),
          "uniform topology must accept matching vertex counts and closed flags");
  Require(!bbsolver::UniformShapeFlatKeyTopology(
              KeysWithFlats({Flat(SquareVertices()), FiveVertexFlat()})),
          "uniform topology must reject mixed vertex counts");
  Require(!bbsolver::UniformShapeFlatKeyTopology(
              KeysWithFlats({Flat(SquareVertices()),
                             Flat(SquareVertices(), false)})),
          "uniform topology must reject mixed closed flags");
}

void TestMaxShapeFlatKeyVertexCountEarlyZeroForMalformedKey() {
  const bbsolver::PropertyKeys keys =
      KeysWithFlats({Flat(SquareVertices()), {}, FiveVertexFlat()});
  Require(bbsolver::MaxShapeFlatKeyVertexCount(keys) == 0,
          "max vertex count must return zero when any key is malformed");
}

void TestMaxShapeFlatKeyVertexCountReturnsMaxCount() {
  const bbsolver::PropertyKeys keys =
      KeysWithFlats({TriangleFlat(), Flat(SquareVertices()), FiveVertexFlat()});
  Require(bbsolver::MaxShapeFlatKeyVertexCount(keys) == 5,
          "max vertex count must return the highest valid key vertex count");
}

void TestDominantClosedShapeFlatKeyVertexCountFilterAndTieBreaker() {
  const bbsolver::PropertyKeys keys = KeysWithFlats({
      TriangleFlat(),
      TriangleFlat(),
      TriangleFlat(),
      Flat(SquareVertices()),
      Flat(SquareVertices()),
      FiveVertexFlat(),
      FiveVertexFlat(),
      FiveVertexFlat(false),
  });
  Require(bbsolver::DominantClosedShapeFlatKeyVertexCount(keys, 3) == 5,
          "dominant closed count must ignore min-target/open keys and tie-break high");
  Require(bbsolver::DominantClosedShapeFlatKeyVertexCount(keys, 4) == 5,
          "dominant closed count must filter counts at or below the target");
  Require(bbsolver::DominantClosedShapeFlatKeyVertexCount(keys, 5) == 0,
          "dominant closed count must return zero when no counts are eligible");
}

void TestBridgePruneShapeFlatKeyClassGuards() {
  int affected = 123;
  bbsolver::PropertyKeys candidate = KeysWithFlats({Flat(SquareVertices())});
  Require(!bbsolver::BridgePruneShapeFlatKeyClass(
              nullptr, 4, 1, &affected),
          "bridge prune must reject null candidate");
  Require(!bbsolver::BridgePruneShapeFlatKeyClass(
              &candidate, 0, 1, &affected),
          "bridge prune must reject non-positive target vertex counts");
  Require(!bbsolver::BridgePruneShapeFlatKeyClass(
              &candidate, 4, 1, nullptr),
          "bridge prune must reject null affected-key output");
}

void TestBridgePruneShapeFlatKeyClassNoMatchingKey() {
  bbsolver::PropertyKeys candidate = KeysWithFlats({TriangleFlat()});
  int affected = 42;
  Require(!bbsolver::BridgePruneShapeFlatKeyClass(
              &candidate, 4, 1, &affected),
          "bridge prune must return false when no keys match the target count");
  Require(affected == 0,
          "bridge prune must zero affected count when no keys match");
  Require(candidate.keys[0].v == TriangleFlat(),
          "bridge prune must leave nonmatching keys unchanged");
}

void TestBridgePruneShapeFlatKeyClassSuccessTouchesOnlyMatchingKeys() {
  const std::vector<double> square = Flat(SquareVertices());
  const std::vector<double> triangle = TriangleFlat();
  bbsolver::PropertyKeys candidate = KeysWithFlats({square, triangle});
  int affected = 0;
  Require(bbsolver::BridgePruneShapeFlatKeyClass(
              &candidate, 4, 1, &affected),
          "bridge prune must succeed when at least one matching key is pruned");
  Require(affected == 1,
          "bridge prune must count only matching pruned keys");
  Require(bbsolver::ShapeFlatVertexCount(candidate.keys[0].v) == 3,
          "bridge prune must reduce matching keys by one vertex");
  Require(candidate.keys[1].v == triangle,
          "bridge prune must not change nonmatching keys");
}

void TestBridgePruneShapeFlatKeyClassFailsOnPruneImpossibleMatch() {
  bbsolver::PropertyKeys candidate =
      KeysWithFlats({Flat(SquareVertices(), false)});
  int affected = 99;
  Require(!bbsolver::BridgePruneShapeFlatKeyClass(
              &candidate, 4, 1, &affected),
          "bridge prune must fail when a matching key cannot be pruned");
  Require(affected == 0,
          "bridge prune must leave affected count zero on prune failure");
}

void TestBridgePruneCandidateEvaluationDefaults() {
  const bbsolver::BridgePruneCandidateEvaluation evaluation;
  Require(evaluation.removed_index == 0,
          "default candidate evaluation must zero removed index");
  Require(evaluation.affected_keys == 0,
          "default candidate evaluation must zero affected keys");
  Require(evaluation.result_vertices == 0,
          "default candidate evaluation must zero result vertices");
  Require(!evaluation.fit_ok && !evaluation.validation_ok &&
              !evaluation.sharp_ok && !evaluation.accepted,
          "default candidate evaluation must initialize flags false");
  Require(std::isinf(evaluation.max_err),
          "default candidate evaluation must initialize max error to infinity");
  Require(std::isinf(evaluation.max_err_screen_px),
          "default candidate evaluation must initialize screen error to infinity");
  Require(evaluation.fit_wall_ms == 0.0 &&
              evaluation.validation_wall_ms == 0.0 &&
              evaluation.sharp_wall_ms == 0.0,
          "default candidate evaluation must zero timing fields");
  Require(evaluation.candidate.keys.empty(),
          "default candidate evaluation must contain empty candidate keys");
  Require(evaluation.failure_note.empty(),
          "default candidate evaluation must contain empty failure note");
}

void TestBridgePruneCancelledBehavior() {
  Require(!bbsolver::BridgePruneCancelled(std::function<bool()>{}),
          "empty cancellation callback must not cancel");
  Require(!bbsolver::BridgePruneCancelled([] { return false; }),
          "false cancellation callback must not cancel");
  Require(bbsolver::BridgePruneCancelled([] { return true; }),
          "true cancellation callback must cancel");
}

bbsolver::BridgePruneCandidateEvaluation ComparableEvaluation() {
  bbsolver::BridgePruneCandidateEvaluation evaluation;
  evaluation.max_err = 1.0;
  evaluation.max_err_screen_px = 1.0;
  evaluation.result_vertices = 4;
  evaluation.removed_index = 2;
  return evaluation;
}

void TestBridgePruneCandidateComparatorMaxErrorPrecedence() {
  bbsolver::BridgePruneCandidateEvaluation candidate =
      ComparableEvaluation();
  bbsolver::BridgePruneCandidateEvaluation incumbent =
      ComparableEvaluation();
  candidate.max_err = 0.5;
  incumbent.max_err = 1.0;
  Require(bbsolver::BridgePruneCandidateIsBetter(candidate, incumbent),
          "candidate with lower max error must be better");
  Require(!bbsolver::BridgePruneCandidateIsBetter(incumbent, candidate),
          "candidate with higher max error must not be better");
}

void TestBridgePruneCandidateComparatorScreenErrorPrecedence() {
  bbsolver::BridgePruneCandidateEvaluation candidate =
      ComparableEvaluation();
  bbsolver::BridgePruneCandidateEvaluation incumbent =
      ComparableEvaluation();
  candidate.max_err_screen_px = 0.5;
  incumbent.max_err_screen_px = 1.0;
  Require(bbsolver::BridgePruneCandidateIsBetter(candidate, incumbent),
          "candidate with lower screen error must be better after max-error tie");
  Require(!bbsolver::BridgePruneCandidateIsBetter(incumbent, candidate),
          "candidate with higher screen error must not be better");
}

void TestBridgePruneCandidateComparatorResultVerticesPrecedence() {
  bbsolver::BridgePruneCandidateEvaluation candidate =
      ComparableEvaluation();
  bbsolver::BridgePruneCandidateEvaluation incumbent =
      ComparableEvaluation();
  candidate.result_vertices = 3;
  incumbent.result_vertices = 4;
  Require(bbsolver::BridgePruneCandidateIsBetter(candidate, incumbent),
          "candidate with fewer result vertices must be better after error ties");
  Require(!bbsolver::BridgePruneCandidateIsBetter(incumbent, candidate),
          "candidate with more result vertices must not be better");
}

void TestBridgePruneCandidateComparatorRemovedIndexPrecedence() {
  bbsolver::BridgePruneCandidateEvaluation candidate =
      ComparableEvaluation();
  bbsolver::BridgePruneCandidateEvaluation incumbent =
      ComparableEvaluation();
  candidate.removed_index = 1;
  incumbent.removed_index = 2;
  Require(bbsolver::BridgePruneCandidateIsBetter(candidate, incumbent),
          "candidate with lower removed index must be better after other ties");
  Require(!bbsolver::BridgePruneCandidateIsBetter(incumbent, candidate),
          "candidate with higher removed index must not be better");
}

void TestBridgePruneCandidateComparatorEpsilonTieBehavior() {
  bbsolver::BridgePruneCandidateEvaluation candidate =
      ComparableEvaluation();
  bbsolver::BridgePruneCandidateEvaluation incumbent =
      ComparableEvaluation();
  candidate.max_err = 1.0;
  incumbent.max_err = 1.0 + 0.5e-9;
  candidate.removed_index = 3;
  incumbent.removed_index = 2;
  Require(!bbsolver::BridgePruneCandidateIsBetter(candidate, incumbent),
          "max-error differences within epsilon must fall through to later ties");

  candidate = ComparableEvaluation();
  incumbent = ComparableEvaluation();
  candidate.max_err_screen_px = 1.0;
  incumbent.max_err_screen_px = 1.0 + 0.5e-9;
  candidate.removed_index = 3;
  incumbent.removed_index = 2;
  Require(!bbsolver::BridgePruneCandidateIsBetter(candidate, incumbent),
          "screen-error differences within epsilon must fall through to later ties");
}

void TestPostTemporalBridgePrunePassBudgetAvailabilityAndCaps() {
  bbsolver::SolverConfig config;
  Require(bbsolver::PostTemporalBridgePrunePassBudget(config, 10, 10) == 0,
          "bridge-prune pass budget must return zero with no available vertices");
  Require(bbsolver::PostTemporalBridgePrunePassBudget(config, 10, 12) == 0,
          "bridge-prune pass budget must return zero with negative availability");
  Require(bbsolver::PostTemporalBridgePrunePassBudget(config, 10, 8) == 2,
          "bridge-prune pass budget must cap to available vertices");
  Require(bbsolver::PostTemporalBridgePrunePassBudget(config, 100, 0) == 16,
          "bridge-prune pass budget must use the default tolerance budget");
}

void TestPostTemporalBridgePrunePassBudgetToleranceThresholds() {
  bbsolver::SolverConfig config;
  config.tolerance = 1.0;
  Require(bbsolver::PostTemporalBridgePrunePassBudget(config, 100, 0) == 24,
          "bridge-prune pass budget must use the >= 1 tolerance tier");
  config.tolerance = 2.0;
  Require(bbsolver::PostTemporalBridgePrunePassBudget(config, 100, 0) == 32,
          "bridge-prune pass budget must use the >= 2 tolerance tier");
  config.tolerance = 3.0;
  Require(bbsolver::PostTemporalBridgePrunePassBudget(config, 100, 0) == 48,
          "bridge-prune pass budget must use the >= 3 tolerance tier");
  config.tolerance = 5.0;
  Require(bbsolver::PostTemporalBridgePrunePassBudget(config, 100, 0) == 64,
          "bridge-prune pass budget must use the >= 5 tolerance tier");
}

void TestPostTemporalBridgePrunePassBudgetFiniteToleranceBranches() {
  bbsolver::SolverConfig config;
  config.tolerance = 0.5;
  config.tolerance_screen_px = 3.0;
  Require(bbsolver::PostTemporalBridgePrunePassBudget(config, 100, 0) == 48,
          "bridge-prune pass budget must consider finite screen tolerance");

  config.tolerance = std::numeric_limits<double>::infinity();
  config.tolerance_screen_px = 2.0;
  Require(bbsolver::PostTemporalBridgePrunePassBudget(config, 100, 0) == 32,
          "bridge-prune pass budget must ignore non-finite tolerance");

  config.tolerance = std::numeric_limits<double>::infinity();
  config.tolerance_screen_px = std::numeric_limits<double>::quiet_NaN();
  Require(bbsolver::PostTemporalBridgePrunePassBudget(config, 100, 0) == 16,
          "bridge-prune pass budget must ignore non-finite tolerance values");
}

void TestBridgePruneLocalProgressClampAndCap() {
  Require(bbsolver::BridgePruneLocalProgress(10, 5, 10, 0.0) == 0.812,
          "bridge-prune local progress must start at the bridge-prune base");
  Require(bbsolver::BridgePruneLocalProgress(10, 5, 10, -1.0) == 0.812,
          "bridge-prune local progress must clamp negative candidate fractions");

  const double partial =
      bbsolver::BridgePruneLocalProgress(10, 5, 10, 2.0);
  Require(std::abs(partial - 0.828) < 1e-12,
          "bridge-prune local progress must clamp candidate fractions to one");

  Require(bbsolver::BridgePruneLocalProgress(10, 5, 0, 1.0) == 0.892,
          "bridge-prune local progress must clamp completed vertices to full");
  Require(bbsolver::BridgePruneLocalProgress(100, 0, -100, 1.0) == 0.892,
          "bridge-prune local progress must keep full progress below the cap");
  Require(bbsolver::BridgePruneLocalProgress(10, 10, 0, 1.0) == 0.892,
          "bridge-prune local progress must use one reducible vertex minimum");
}

void TestBridgePruneProgressChunkSizeClampBehavior() {
  Require(bbsolver::BridgePruneProgressChunkSize(0, 4) == 1,
          "bridge-prune chunk size must return one for zero candidates");
  Require(bbsolver::BridgePruneProgressChunkSize(1, 4) == 1,
          "bridge-prune chunk size must return one for one candidate");
  Require(bbsolver::BridgePruneProgressChunkSize(2, -10) == 2,
          "bridge-prune chunk size must clamp non-positive jobs and cap to count");
  Require(bbsolver::BridgePruneProgressChunkSize(5, 1) == 2,
          "bridge-prune chunk size must use a minimum chunk of two");
  Require(bbsolver::BridgePruneProgressChunkSize(7, 99) == 7,
          "bridge-prune chunk size must not exceed candidate count");
  Require(bbsolver::BridgePruneProgressChunkSize(20, 99) == 8,
          "bridge-prune chunk size must cap jobs at eight");
}

void TestReplacementTargetLadderStableTopologyOrderingAndDedupe() {
  bbsolver::SolverConfig config;
  config.path_replacement_min_vertices = 4;
  config.path_replacement_max_vertices = 0;

  std::vector<int> targets =
      bbsolver::BuildReplacementTargetLadder(9, 30, 30, config);
  Require(targets == std::vector<int>({9, 10, 12, 14, 16, 18}),
          "stable target ladder must sort auto fallback with low even targets");

  targets = bbsolver::BuildReplacementTargetLadder(18, 30, 30, config);
  Require(targets == std::vector<int>({18}),
          "stable target ladder must dedupe low-end and auto fallback targets");
}

void TestReplacementTargetLadderVariableTopologyUsesSourceMax() {
  bbsolver::SolverConfig config;
  config.path_replacement_min_vertices = 4;
  config.path_replacement_max_vertices = 0;

  std::vector<int> stable =
      bbsolver::BuildReplacementTargetLadder(18, 12, 12, config);
  std::vector<int> variable =
      bbsolver::BuildReplacementTargetLadder(18, 12, 20, config);
  Require(stable == std::vector<int>({11}),
          "stable topology must strictly reduce from the source minimum");
  Require(variable == std::vector<int>({18}),
          "variable topology must allow targets below the source maximum");
}

void TestReplacementTargetLadderCapsAndFallbacks() {
  bbsolver::SolverConfig config;
  config.path_replacement_min_vertices = 6;
  config.path_replacement_max_vertices = 12;

  std::vector<int> targets =
      bbsolver::BuildReplacementTargetLadder(4, 20, 20, config);
  Require(targets == std::vector<int>({6, 10, 12}),
          "target ladder must clamp auto fallback to min legal and cap max legal");

  targets = bbsolver::BuildReplacementTargetLadder(10, 5, 5, config);
  Require(targets.empty(),
          "target ladder must reject min legal values above max legal");

  config.path_replacement_min_vertices = 4;
  config.path_replacement_max_vertices = 0;
  targets = bbsolver::BuildReplacementTargetLadder(10, 1, 10, config);
  Require(targets.empty(),
          "target ladder must reject source minima at or below one vertex");
}

void TestJoinIntsCommaJoinsWithoutSpaces() {
  Require(bbsolver::JoinInts({}).empty(),
          "integer joins must return empty for empty vectors");
  Require(bbsolver::JoinInts({4, 8, 12}) == "4,8,12",
          "integer joins must comma-join without spaces");
}

void TestJoinNotesSkipsEmptyAndUsesPipeSeparator() {
  Require(bbsolver::JoinNotes({}).empty(),
          "note joins must return empty for empty vectors");
  Require(bbsolver::JoinNotes({"", "first", "", "second"}) ==
              "first | second",
          "note joins must skip empty notes and use pipe separators");
}

void TestBridgePruneTelemetryNotesTokenSpelling() {
  const std::string notes =
      bbsolver::BridgePruneTelemetryNotes(1, 2, 3, 4);
  Require(notes ==
              "; bridge_prune_fit_failures=1"
              "; bridge_prune_validation_failures=2"
              "; bridge_prune_sharp_failures=3"
              "; bridge_prune_accepted_candidates=4",
          "bridge-prune telemetry notes must preserve exact token spelling");
}

}  // namespace

int main() {
  TestRejectsOpenShape();
  TestRejectsSmallShapeAndInvalidIndexes();
  TestDegenerateBridgeZeroesAdjacentHandlesAndErasesVertex();
  TestNonDegenerateBridgeRefitRemovesRequestedVertex();
  TestDuplicateTerminalClosureDetectionTolerance();
  TestDuplicateTerminalClosureRejectsOpenAndSmallShapes();
  TestDropDuplicateTerminalClosureKeepsSmallInputsUnchanged();
  TestDropDuplicateTerminalClosureReducesAndTransfersIncomingTangent();
  TestMinShapeFlatVertexCountEmptyMalformedAndValid();
  TestMaxShapeFlatSampleVertexCountEmptyMalformedAndValid();
  TestMinShapeFlatKeyVertexCountEmptyMalformedAndValid();
  TestUniformShapeFlatKeyTopologyCases();
  TestMaxShapeFlatKeyVertexCountEarlyZeroForMalformedKey();
  TestMaxShapeFlatKeyVertexCountReturnsMaxCount();
  TestDominantClosedShapeFlatKeyVertexCountFilterAndTieBreaker();
  TestBridgePruneShapeFlatKeyClassGuards();
  TestBridgePruneShapeFlatKeyClassNoMatchingKey();
  TestBridgePruneShapeFlatKeyClassSuccessTouchesOnlyMatchingKeys();
  TestBridgePruneShapeFlatKeyClassFailsOnPruneImpossibleMatch();
  TestBridgePruneCandidateEvaluationDefaults();
  TestBridgePruneCancelledBehavior();
  TestBridgePruneCandidateComparatorMaxErrorPrecedence();
  TestBridgePruneCandidateComparatorScreenErrorPrecedence();
  TestBridgePruneCandidateComparatorResultVerticesPrecedence();
  TestBridgePruneCandidateComparatorRemovedIndexPrecedence();
  TestBridgePruneCandidateComparatorEpsilonTieBehavior();
  TestPostTemporalBridgePrunePassBudgetAvailabilityAndCaps();
  TestPostTemporalBridgePrunePassBudgetToleranceThresholds();
  TestPostTemporalBridgePrunePassBudgetFiniteToleranceBranches();
  TestBridgePruneLocalProgressClampAndCap();
  TestBridgePruneProgressChunkSizeClampBehavior();
  TestReplacementTargetLadderStableTopologyOrderingAndDedupe();
  TestReplacementTargetLadderVariableTopologyUsesSourceMax();
  TestReplacementTargetLadderCapsAndFallbacks();
  TestJoinIntsCommaJoinsWithoutSpaces();
  TestJoinNotesSkipsEmptyAndUsesPipeSeparator();
  TestBridgePruneTelemetryNotesTokenSpelling();
  std::cout << "[PASS] test_path_vertex_reduction\n";
  return 0;
}
