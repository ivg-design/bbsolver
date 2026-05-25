#pragma once

#include "bbsolver/app/cli_options.hpp"
#include "bbsolver/domain.hpp"

#include <cstddef>
#include <functional>
#include <string>

namespace bbsolver {

class ProgressWriter;

struct ReplacementBaselineSolveRequest {
  const PropertySamples* original_property_samples = nullptr;
  const SolverConfig* config = nullptr;
  const CompInfo* comp = nullptr;
  const SolveOptions* options = nullptr;
  const ProgressWriter* progress = nullptr;
  std::function<bool()> cancel_fn;
  std::size_t property_idx = 0;
  std::size_t property_count = 1;
};

struct ReplacementBaselineSolveResult {
  PropertyKeys keys;
  bool cancelled = false;
  std::string cancel_phase;
};

ReplacementBaselineSolveResult SolveReplacementBaseline(
    const ReplacementBaselineSolveRequest& request);

}  // namespace bbsolver
