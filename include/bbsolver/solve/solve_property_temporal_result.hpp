#pragma once

#include "bbsolver/domain.hpp"

#include <cstddef>
#include <functional>
#include <string>

namespace bbsolver {

class ProgressWriter;

struct PropertyTemporalSolveResultRequest {
  const PropertySamples* property_samples = nullptr;
  const PropertyKeys* property_keys = nullptr;
  const ProgressWriter* progress = nullptr;
  std::function<bool()> cancel_fn;
  std::size_t property_idx = 0;
  std::size_t property_count = 0;
  double prop_ms = 0.0;
};

struct PropertyTemporalSolveResult {
  bool cancelled = false;
  std::string cancel_phase;
};

PropertyTemporalSolveResult ReportPropertyTemporalSolveResult(
    const PropertyTemporalSolveResultRequest& request);

}  // namespace bbsolver
