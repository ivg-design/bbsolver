#pragma once

#include "bbsolver/domain.hpp"

#include <cstddef>
#include <functional>

namespace bbsolver {

class ProgressWriter;

struct PropertyOutputRequest {
  bool emit_landmark_subpaths = false;
  bool replacement_output_accepted = false;
  const PropertySamples* property_samples = nullptr;
  PropertyKeys* property_keys = nullptr;
  KeyBundle* keys = nullptr;
  const SolverConfig* config = nullptr;
  const ProgressWriter* progress = nullptr;
  std::function<bool()> cancel_fn;
  std::size_t property_idx = 0;
  std::size_t property_count = 0;
  double prop_ms = 0.0;
};

void AppendSolvedPropertyOutput(const PropertyOutputRequest& request);

}  // namespace bbsolver
