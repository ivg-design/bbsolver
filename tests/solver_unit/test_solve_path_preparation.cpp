#include "bbsolver/solve/solve_path_preparation.hpp"
#include "bbsolver/domain.hpp"

#include <cassert>

#include "bbsolver/progress/progress.hpp"

namespace {

void TestPlainScalarPreparationIsNoop() {
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

  bbsolver::SolverConfig config;
  bbsolver::CompInfo comp;
  const bbsolver::ProgressWriter progress(-1);

  bbsolver::PathSolvePreparationRequest request;
  request.source_property = &samples;
  request.config = &config;
  request.comp = &comp;
  request.progress = &progress;
  request.property_count = 1;

  bbsolver::PathSolvePreparationState state =
      bbsolver::PreparePathSolveInputs(request);

  assert(state.original_property_samples.property.id == "opacity");
  assert(state.property_samples.property.id == "opacity");
  assert(!state.visible_outline_reference);
  assert(!state.near_optimal_fast_path.applied);
  assert(!state.replacement_path_applied);
  assert(!state.canonical_path_applied);
  assert(state.path_fit_note.empty());
  assert(state.replacement_fitted_vertices == 0);
  assert(state.replacement_frame_fit_error == 0.0);
}

}  // namespace

int main() {
  TestPlainScalarPreparationIsNoop();
  return 0;
}
