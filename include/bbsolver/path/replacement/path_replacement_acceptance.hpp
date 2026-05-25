#pragma once

#include "bbsolver/domain.hpp"

#include <string>

namespace bbsolver {

struct PathTemporalValidationResult;
struct SharpCornerValidationResult;

// Outcome of EvaluateReplacementAcceptance, with a note string that explains
// the decision and should be appended to the chosen result's .notes field.
//
// Acceptance rule (conservative):
//   1. If original is not a valid fallback, use a valid candidate. If both
//      results are invalid, keep the less-bad result and report that explicitly.
//   2. If candidate_keys < original_keys: use candidate (strictly better on keys).
//   3. If candidate_keys == original_keys AND candidate has fewer fitted
//      vertices than the source: use candidate (same keys, cheaper to writeback
//      and verify, better temporal-solver input for future bakes).
//   4. Vertex-priority UI no longer accepts key growth. Vertex pruning is
//      handled as a guarded second pass after the temporal solve.
//   5. Otherwise (candidate_keys > original_keys, or equal keys with no vertex
//      improvement): fallback to original.
struct ReplacementAcceptanceVerdict {
  bool use_candidate = false;
  // Append to the chosen result's .notes (both acceptance and fallback cases).
  std::string decision_note;
};

struct ReplacementValidationSummary {
  bool candidate_converged = false;
  double candidate_max_err = 0.0;
};

struct ReplacementRetryEligibilityInput {
  bool verdict_use_candidate = false;
  int candidate_key_count = 0;
  int original_key_count = 0;
  int validation_samples_checked = 0;
  bool source_validation_ok = false;
  bool sharp_validation_ok = false;
  int fitted_vertices = 0;
  int source_min_vertices = 0;
};

struct ReplacementRetryEligibility {
  bool retry_key_gate = false;
  bool failed_only_sharp_gate = false;
  bool should_retry = false;
};

ReplacementRetryEligibilityInput BuildReplacementRetryEligibilityInput(
    bool verdict_use_candidate,
    int candidate_key_count,
    int original_key_count,
    const PathTemporalValidationResult& validation,
    const SharpCornerValidationResult& sharp_validation,
    int fitted_vertices,
    int source_min_vertices);

ReplacementValidationSummary SummarizeReplacementCandidateValidation(
    const PathTemporalValidationResult& validation,
    const SharpCornerValidationResult& sharp_validation,
    const PropertyKeys& candidate_keys);

bool ApplyReplacementValidationSummaryToKeys(
    const PathTemporalValidationResult& validation,
    const ReplacementValidationSummary& summary,
    PropertyKeys* keys);

ReplacementRetryEligibility EvaluateReplacementRetryEligibility(
    const ReplacementRetryEligibilityInput& input);

// Evaluate whether to accept a replacement path candidate or fall back to the
// original plain solve. All arguments are plain scalars so this function is
// independently unit-testable without running the full solver.
//
// candidate_keys            number of keys the candidate temporal solve produced
// candidate_max_err         L-inf error of the candidate solve
// candidate_converged       whether the candidate solve converged
// candidate_fitted_vertices vertex count of the replacement regime (< source)
// original_keys             number of keys the original SolvePlainProperty produced
// original_max_err          L-inf error of the original solve
// original_source_vertices  vertex count of the original (unfitted) path
// original_converged        whether the original solve converged
// tolerance                 solver L-inf tolerance threshold
// prefer_vertices           request guarded post-temporal vertex pruning; this
//                           acceptance gate still rejects key growth
// max_key_growth_ratio      candidate_keys/original_keys limit in debug mode
// min_vertex_reduction_ratio required vertex drop ratio in debug mode
// original_sample_count     raw frame/sample count; 0 disables raw-key gate
ReplacementAcceptanceVerdict EvaluateReplacementAcceptance(
    int    candidate_keys,
    double candidate_max_err,
    bool   candidate_converged,
    int    candidate_fitted_vertices,
    int    original_keys,
    double original_max_err,
    int    original_source_vertices,
    bool   original_converged,
    double tolerance,
    bool   prefer_vertices = false,
    double max_key_growth_ratio = 4.0,
    double min_vertex_reduction_ratio = 0.20,
    int    original_sample_count = 0);

}  // namespace bbsolver
