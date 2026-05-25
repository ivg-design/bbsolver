#include "bbsolver/solve/solve_property_post_processing.hpp"
#include "bbsolver/domain.hpp"

#include <cassert>
#include <string>

#include "bbsolver/progress/progress.hpp"

namespace {

bbsolver::PropertySamples MakeScalarSamples() {
  bbsolver::PropertySamples samples;
  samples.property.id = "opacity";
  samples.property.display_name = "Opacity";
  samples.property.kind = bbsolver::ValueKind::Scalar;
  samples.property.dimensions = 1;
  samples.samples.resize(2);
  samples.samples[0].t_sec = 0.0;
  samples.samples[0].v = {0.0};
  samples.samples[1].t_sec = 1.0;
  samples.samples[1].v = {100.0};
  return samples;
}

void TestPathFitNoteIsAppendedWithoutShapeSideEffects() {
  bbsolver::PropertySamples original = MakeScalarSamples();
  bbsolver::PropertySamples property = original;
  bbsolver::PropertyKeys keys;
  keys.property_id = original.property.id;
  keys.converged = true;
  keys.max_err = 0.0;
  keys.max_err_screen_px = 0.0;
  keys.keys.resize(2);
  keys.keys[0].t_sec = 0.0;
  keys.keys[0].v = {0.0};
  keys.keys[1].t_sec = 1.0;
  keys.keys[1].v = {100.0};

  bbsolver::SolverConfig config;
  bbsolver::CompInfo comp;
  const bbsolver::ProgressWriter progress(-1);
  std::string path_fit_note = "path_fit_note=true";

  bbsolver::PropertyPostSolveProcessingRequest request;
  request.original_property_samples = &original;
  request.property_samples = &property;
  request.property_keys = &keys;
  request.config = &config;
  request.comp = &comp;
  request.progress = &progress;
  request.path_fit_note = &path_fit_note;
  request.cancel_fn = []() { return false; };
  request.property_count = 1;
  request.temporal_optimization_enabled = false;

  const bbsolver::PropertyPostSolveProcessingResult result =
      bbsolver::ProcessSolvedPropertyPostSolve(request);

  assert(!result.cancelled);
  assert(result.cancel_phase.empty());
  assert(keys.notes == "path_fit_note=true");
  assert(property.property.id == "opacity");
  assert(keys.converged);
}

}  // namespace

int main() {
  TestPathFitNoteIsAppendedWithoutShapeSideEffects();
  return 0;
}
