#pragma once

#include "bbsolver/domain.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

namespace bbsolver {

// True when two value vectors have identical length and every element pair
// differs by no more than `eps`.
inline bool KeyValuesEqualWithin(const std::vector<double>& a,
                                 const std::vector<double>& b,
                                 double eps = 1e-9) {
  if (a.size() != b.size()) {
    return false;
  }
  for (std::size_t idx = 0; idx < a.size(); ++idx) {
    if (std::abs(a[idx] - b[idx]) > eps) {
      return false;
    }
  }
  return true;
}

struct StaticKeyRunCollapseResult {
  bool attempted = false;
  bool accepted = false;
  int runs_collapsed = 0;
  int keys_removed = 0;
  double max_err = 0.0;
  double max_err_screen_px = 0.0;
};

// Collapse runs of consecutive keys with equal values into hold-bounded
// endpoints. Suffix runs become a single Hold-out key, interior runs of
// length > 2 become Hold-out/Hold-in endpoints; runs of length 1 or 2 are
// left untouched. Mutations apply only when the validator confirms the
// candidate stays within the existing per-property and screen-space error
// budgets.
StaticKeyRunCollapseResult CollapseRedundantStaticKeyRuns(
    const PropertySamples& source_samples,
    const SolverConfig& config,
    const CompInfo& comp,
    PropertyKeys& keys);

struct FinalStaticBoundaryAnchorResult {
  bool attempted = false;
  bool accepted = false;
  int boundary_sample = -1;
  int suffix_samples = 0;
  int tail_keys_removed = 0;
  double boundary_t_sec = 0.0;
  double max_err = 0.0;
  double max_err_screen_px = 0.0;
};

// Locate the first sample index that begins a run of equal-valued samples
// ending at the last sample, or -1 when no such suffix exists.
int FindFinalStaticSuffixStartSample(const std::vector<Sample>& samples);

// Drop samples whose timestamp lies strictly past `end_t_sec` (with a
// small epsilon), update `t_end_sec`, and return the number of samples
// removed.
int TrimSamplesAfterTime(PropertySamples& samples, double end_t_sec);

// Replace any keys past the source's final-static boundary with a single
// Hold anchor at the boundary time. Mutations apply only when the
// validator confirms the candidate respects the configured tolerance and
// screen-space gate.
FinalStaticBoundaryAnchorResult AnchorFinalStaticBoundary(
    const PropertySamples& source_samples,
    const SolverConfig& config,
    const CompInfo& comp,
    PropertyKeys& keys);

}  // namespace bbsolver
