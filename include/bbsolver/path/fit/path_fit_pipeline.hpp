#pragma once

#include "bbsolver/domain.hpp"

#include <string>
#include <vector>

namespace bbsolver {

// One contiguous run of shape_flat samples with constant vertex count and closed
// flag. Ready for direct input to DecomposePathBundle or the temporal solver.
struct StablePathRegime {
  int start_sample_idx = 0;  // inclusive index into the source PropertySamples
  int end_sample_idx   = 0;  // inclusive
  int vertex_count     = 0;
  int dimensions       = 0;  // == 2 + 6 * vertex_count
  bool closed          = false;

  // Stable sub-stream. property.dimensions == this regime's dimensions (not the
  // original 314+ from a 52-vertex stream). Suitable for direct input to
  // DecomposePathBundle.
  PropertySamples samples;
};

struct PathCorrespondenceResult {
  bool ok         = false;
  bool all_stable = false;  // true when exactly one regime spans all samples
  std::string notes;
  std::vector<StablePathRegime> regimes;
};

// Partition a shape_flat PropertySamples stream into stable topology regimes.
//
// Contracts enforced:
//   - The closed flag must be identical across every sample; a flip sets ok=false
//     (caller must reject the property before reaching the solver).
//   - A new regime begins whenever the vertex count changes between consecutive
//     samples (conservative split; no reconciliation across boundaries).
//   - Each StablePathRegime.samples.property.dimensions == 2 + 6 * vertex_count,
//     replacing the raw 314+ dimension count from a 52-vertex stream.
//
// Returns ok=false for: non-shape_flat input, empty stream, malformed headers,
// or closed-flag flips. When regimes.size() > 1, the caller is responsible for
// inserting Hold keys at regime-boundary sample times.
PathCorrespondenceResult BuildStableRegimes(const PropertySamples& ps);

// Options for single-regime enforcement. Extend this when FitShapeFlatFrame
// gains a target_vertex_count option; set target_vertex_count here to verify
// the fitter produced a fixed-K stream before handing it to the temporal solver.
struct SingleRegimeOptions {
  // When > 0, the regime must have exactly this vertex count; otherwise
  // the result is not ok. Set to FitShapeFlatFrame's target_vertex_count once
  // that option exists. Default 0 = unconstrained.
  int target_vertex_count = 0;
};

// Focused single-regime contract. Accepts a fitted per-frame shape_flat stream
// and either reports one stable regime (ok=true) or a reason it must fall back
// (ok=false, reason set). Delegates to BuildStableRegimes and adds:
//   - all_stable enforcement: multi-regime streams set ok=false
//   - optional target_vertex_count check: if opts.target_vertex_count > 0,
//     the regime vertex count must equal that value
//
// Used by FitReplacementPathProperty (main.cpp) and independently testable.
struct SingleRegimeResult {
  bool ok = false;
  std::string reason;       // populated when !ok
  StablePathRegime regime;  // populated when ok
};

SingleRegimeResult BuildSingleStableRegime(const PropertySamples& ps,
                                           const SingleRegimeOptions& opts = {});

// ---------------------------------------------------------------------------
// Temporal-solve pre-assessment
// ---------------------------------------------------------------------------

// Greedy Linear keyframe estimate for a PropertySamples stream within the
// given L-inf tolerance.
//
// Algorithm: greedily extend each segment as far as possible while every
// intermediate sample can be linearly interpolated (in L-inf) from the segment
// endpoints within tolerance. Returns N for empty or single-sample streams.
//
// Runtime: O(N^2 x D) where N = sample count, D = dimensions of the flat
// vector. For N=73, D=260 this is ~1 M ops - safe to call before any solve.
//
// Note: this uses a pure L-inf Linear check without velocity/acceleration
// weights. It is a cheap pre-solve proxy, not a proof of the final Bezier key
// count. The hard skip is intentionally limited to candidates that are already
// estimated at roughly one key per sample.
int EstimateLinearKeyCount(const PropertySamples& ps, double tolerance);

// Pre-assessment of whether a replacement candidate regime is worth passing
// to the full temporal solver (SolvePathDecomposedProperty). Call this
// BEFORE launching the expensive per-channel Bezier fitting.
struct ReplacementCandidateAssessment {
  bool worth_attempting = false;

  // Greedy Linear key estimates (pure L-inf pre-solve proxies).
  int estimated_candidate_keys = 0;
  int estimated_original_keys  = 0;

  // estimated_candidate_keys / estimated_original_keys.
  // Values < 1.0 are improvements; >= 1.0 mean the candidate is not expected
  // to win the main.cpp key-count comparison.
  double key_reduction_ratio = 1.0;

  // Proxy for child-channel work = n_samples * vertex_count.
  // Large values warn that SolvePathDecomposedProperty will be slow.
  int decompose_cost = 0;

  // Human-readable explanation for the worth_attempting decision.
  std::string reason;
};

// Lightweight pre-check: should the replacement candidate regime go through
// SolvePathDecomposedProperty?
//
// Returns worth_attempting=false only when the candidate is estimated to need
// roughly one key per sample, does not beat the original estimate, and does not
// materially reduce source vertex density. This catches the obvious case where
// per-frame independent fitting broke temporal coherence while still letting
// borderline candidates and stage-1 vertex-reduction candidates reach the final
// solve.
//
// Worth_attempting=true does not guarantee improvement - it only means the
// pre-check did not rule it out. The full solve comparison in main.cpp
// (candidate_keys vs original_keys from SolvePlainProperty) is the final gate.
ReplacementCandidateAssessment AssessReplacementCandidate(
    const StablePathRegime& candidate,
    const PropertySamples& original,
    double tolerance);

}  // namespace bbsolver
