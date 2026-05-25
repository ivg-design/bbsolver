#include "bbsolver/solve/solve_property_temporal_prelude.hpp"
#include "bbsolver/domain.hpp"

#include <cassert>
#include <string>

#include "bbsolver/progress/progress.hpp"
#include "bbsolver/motion_smooth/motion_smooth_reduction_gate.hpp"

namespace {

bbsolver::PropertySamples MakeScalarSamples() {
  bbsolver::PropertySamples samples;
  samples.property.id = "opacity";
  samples.property.display_name = "Opacity";
  samples.property.kind = bbsolver::ValueKind::Scalar;
  samples.property.dimensions = 1;
  samples.samples.resize(4);
  samples.samples[0].t_sec = 0.0;
  samples.samples[0].v = {0.0};
  samples.samples[1].t_sec = 1.0;
  samples.samples[1].v = {100.0};
  samples.samples[2].t_sec = 2.0;
  samples.samples[2].v = {100.0};
  samples.samples[3].t_sec = 3.0;
  samples.samples[3].v = {100.0};
  samples.t_start_sec = 0.0;
  samples.t_end_sec = 3.0;
  return samples;
}

void TestFinalStaticSuffixIsTrimmedForSolvePrelude() {
  bbsolver::PropertySamples original = MakeScalarSamples();
  bbsolver::PropertySamples property = original;
  bbsolver::SolverConfig config;
  bbsolver::CompInfo comp;
  comp.fps = 24.0;
  const bbsolver::ProgressWriter progress(-1);
  bbsolver::ShapeFlatNearOptimalResult near_optimal;
  std::string path_fit_note;

  bbsolver::PropertyTemporalPreludeRequest request;
  request.original_property_samples = &original;
  request.property_samples = &property;
  request.config = &config;
  request.comp = &comp;
  request.progress = &progress;
  request.near_optimal_fast_path = &near_optimal;
  request.path_fit_note = &path_fit_note;
  request.property_count = 1;

  const bbsolver::PropertyTemporalPreludeState state =
      bbsolver::PreparePropertyTemporalPrelude(request);

  assert(state.temporal_source_samples.samples.size() == 2);
  assert(state.temporal_property_samples.samples.size() == 2);
  assert(state.temporal_source_samples.t_end_sec == 1.0);
  assert(state.temporal_property_samples.t_end_sec == 1.0);
  assert(state.final_static_trim_note.find(
             "final_static_suffix_trim_for_solve=true") !=
         std::string::npos);
  assert(state.final_static_trim_note.find(
             "final_static_boundary_sample=1") != std::string::npos);
  assert(state.final_static_trim_note.find(
             "final_static_boundary_frame=24") != std::string::npos);
  assert(state.motion_smooth_enabled == false);
  assert(state.temporal_optimization_enabled == true);
  assert(state.path_temporal_reduced_by_fit == false);
  assert(state.replacement_temporal_max_gap == 0);
  assert(path_fit_note.empty());
  assert(state.property_solve_route ==
         bbsolver::PropertySolveRoute::PlainTemporal);
}

}  // namespace

int main() {
  TestFinalStaticSuffixIsTrimmedForSolvePrelude();
  return 0;
}
