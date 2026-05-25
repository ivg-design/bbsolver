#include "bbsolver/solve/solve_property_output.hpp"
#include "bbsolver/domain.hpp"

#include <cassert>

#include "bbsolver/progress/progress.hpp"

namespace {

bbsolver::PropertySamples MakePropertySamples() {
  bbsolver::PropertySamples samples;
  samples.property.id = "prop";
  samples.property.display_name = "Prop";
  samples.samples.resize(3);
  return samples;
}

void TestAppendSolvedPropertyOutputUpdatesBundleTotals() {
  bbsolver::PropertySamples samples = MakePropertySamples();
  bbsolver::PropertyKeys keys;
  keys.property_id = "prop";
  keys.keys.resize(2);
  keys.max_err = 1.25;
  bbsolver::KeyBundle bundle;
  bbsolver::SolverConfig config;
  const bbsolver::ProgressWriter progress(-1);

  bbsolver::PropertyOutputRequest request;
  request.property_samples = &samples;
  request.property_keys = &keys;
  request.keys = &bundle;
  request.config = &config;
  request.progress = &progress;
  request.property_count = 1;

  bbsolver::AppendSolvedPropertyOutput(request);

  assert(bundle.total_keys == 2);
  assert(bundle.total_samples_input == 3);
  assert(bundle.property_results.size() == 1);
  assert(bundle.property_results[0].property_id == "prop");
  assert(bundle.property_results[0].keys.size() == 2);
  assert(bundle.property_results[0].max_err == 1.25);
}

void TestLandmarkDisabledDoesNotAppendSubpaths() {
  bbsolver::PropertySamples samples = MakePropertySamples();
  bbsolver::PropertyKeys keys;
  keys.property_id = "prop";
  keys.keys.resize(1);
  bbsolver::KeyBundle bundle;
  bbsolver::SolverConfig config;
  const bbsolver::ProgressWriter progress(-1);

  bbsolver::PropertyOutputRequest request;
  request.emit_landmark_subpaths = false;
  request.replacement_output_accepted = true;
  request.property_samples = &samples;
  request.property_keys = &keys;
  request.keys = &bundle;
  request.config = &config;
  request.progress = &progress;
  request.property_count = 1;

  bbsolver::AppendSolvedPropertyOutput(request);

  assert(bundle.property_results.size() == 1);
  assert(bundle.total_keys == 1);
}

}  // namespace

int main() {
  TestAppendSolvedPropertyOutputUpdatesBundleTotals();
  TestLandmarkDisabledDoesNotAppendSubpaths();
  return 0;
}
