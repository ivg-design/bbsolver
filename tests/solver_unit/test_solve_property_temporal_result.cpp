#include "bbsolver/solve/solve_property_temporal_result.hpp"
#include "bbsolver/domain.hpp"

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string>

#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>

#include "bbsolver/progress/progress.hpp"

#ifndef _WIN32
#include <sys/types.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    std::abort();
  }
}

void RequireNear(double actual, double expected, const std::string& message) {
  if (std::abs(actual - expected) > 1e-12) {
    std::cerr << message << ": expected " << expected << ", got " << actual
              << "\n";
    std::abort();
  }
}

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

bbsolver::PropertyKeys MakeScalarKeys() {
  bbsolver::PropertyKeys keys;
  keys.property_id = "opacity";
  keys.converged = true;
  keys.max_err = 0.125;
  keys.keys.resize(2);
  keys.keys[0].t_sec = 0.0;
  keys.keys[0].v = {0.0};
  keys.keys[1].t_sec = 1.0;
  keys.keys[1].v = {100.0};
  return keys;
}

void TestTemporalSolveDoneProgressEvent() {
#ifndef _WIN32
  const bbsolver::PropertySamples samples = MakeScalarSamples();
  const bbsolver::PropertyKeys keys = MakeScalarKeys();
  int fds[2] = {-1, -1};
  Require(::pipe(fds) == 0, "pipe creation failed");
  const bbsolver::ProgressWriter progress(fds[1]);

  bbsolver::PropertyTemporalSolveResultRequest request;
  request.property_samples = &samples;
  request.property_keys = &keys;
  request.progress = &progress;
  request.cancel_fn = []() { return false; };
  request.property_idx = 1;
  request.property_count = 3;
  request.prop_ms = 12.5;

  const bbsolver::PropertyTemporalSolveResult result =
      bbsolver::ReportPropertyTemporalSolveResult(request);

::close(fds[1]);
  char buffer[512] = {};
  const auto read_count =::read(fds[0], buffer, sizeof(buffer) - 1);
::close(fds[0]);

  Require(!result.cancelled, "non-cancelled solve should not cancel");
  Require(read_count > 0, "temporal result did not emit progress bytes");
  const std::string line(buffer, static_cast<std::size_t>(read_count));
  Require(!line.empty() && line.back() == '\n',
          "temporal result progress must be newline-delimited JSON");
  const nlohmann::json event = nlohmann::json::parse(line);
  Require(event.at("event") == "temporal_solve_done",
          "temporal done event name mismatch");
  Require(event.at("phase") == "Temporal solve finished for Opacity",
          "temporal done phase mismatch");
  RequireNear(event.at("progress").get<double>(),
              bbsolver::SolveProgressForPropertyStage(1, 3, 0.70),
              "temporal done progress mismatch");
  Require(event.at("id") == "opacity", "property id mismatch");
  Require(event.at("display_name") == "Opacity",
          "display name mismatch");
  Require(event.at("i") == 1, "property index mismatch");
  Require(event.at("n") == 3, "property count mismatch");
  Require(event.at("K") == 2, "key count mismatch");
  RequireNear(event.at("max_err").get<double>(), 0.125,
              "max error mismatch");
  RequireNear(event.at("ms").get<double>(), 12.5, "duration mismatch");
#endif
}

void TestCancelledNotesShortCircuitCancelCheck() {
  const bbsolver::PropertySamples samples = MakeScalarSamples();
  bbsolver::PropertyKeys keys = MakeScalarKeys();
  keys.notes = "cancelled";
  const bbsolver::ProgressWriter progress(-1);
  bool cancel_checked = false;

  bbsolver::PropertyTemporalSolveResultRequest request;
  request.property_samples = &samples;
  request.property_keys = &keys;
  request.progress = &progress;
  request.cancel_fn = [&]() {
    cancel_checked = true;
    return false;
  };

  const bbsolver::PropertyTemporalSolveResult result =
      bbsolver::ReportPropertyTemporalSolveResult(request);

  Require(result.cancelled, "cancelled notes should cancel");
  Require(result.cancel_phase == "temporal_solve",
          "cancelled notes should return temporal_solve phase");
  Require(!cancel_checked,
          "cancelled notes should short-circuit external cancel check");
}

void TestExternalCancelCheckCancelsTemporalResult() {
  const bbsolver::PropertySamples samples = MakeScalarSamples();
  const bbsolver::PropertyKeys keys = MakeScalarKeys();
  const bbsolver::ProgressWriter progress(-1);

  bbsolver::PropertyTemporalSolveResultRequest request;
  request.property_samples = &samples;
  request.property_keys = &keys;
  request.progress = &progress;
  request.cancel_fn = []() { return true; };

  const bbsolver::PropertyTemporalSolveResult result =
      bbsolver::ReportPropertyTemporalSolveResult(request);

  Require(result.cancelled, "external cancel should cancel");
  Require(result.cancel_phase == "temporal_solve",
          "external cancel should return temporal_solve phase");
}

}  // namespace

int main() {
  TestTemporalSolveDoneProgressEvent();
  TestCancelledNotesShortCircuitCancelCheck();
  TestExternalCancelCheckCancelsTemporalResult();
  return 0;
}
