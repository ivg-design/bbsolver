// bbsolver post-cleanup temporal key reduction (refit).
//
// Design owner: temporal-refit lane.
// Orchestration lives in temporal_refit.cpp; helper policy lives in compact
// temporal_refit_* modules.
//
// PURPOSE
//   Given accepted bake keys K1 and original source samples S, try to
//   find K2 with |K2| < |K1| while staying within the operator-set
//   L∞ error budget against S. If K2 cannot be found, return K1
//   unchanged with a clear rejection reason.
//
// CONTRACT
//   * Validation is always performed against the ORIGINAL source S,
//     never against an intermediate resampled stream. This keeps the
//     end-to-end error bound the operator agreed to (the bbsm
//     tolerance) regardless of how many post-cleanup passes ran.
//   * Endpoints are pinned: K2.front().t_sec == S.front().t_sec and
//     K2.back().t_sec == S.back().t_sec. Refit may not extend or
//     truncate the property's time range.
//   * |K2| < |K1| is a hard precondition for acceptance. A "same
//     count, lower error" outcome is reported as `no_gain` and
//     discards K2 in favor of K1.
//   * Cancellation must be observed at coarse intervals (per outer
//     DP step and per parallel-batch boundary) so the operator
//     cancel-file sentinel responds within a frame or two.
//   * Deterministic output under any --jobs value: candidate selection
//     inside the parallel DP breaks ties by source sample index.
//
// PLACEMENT IN PIPELINE
//   Refit runs after vertex-only cleanup and static-key-run collapse.
//   It is skipped for motion-smooth, which already emits a sparse
//   trajectory representation.

#pragma once

#include "bbsolver/domain.hpp"

#include <functional>
#include <string>

namespace bbsolver {

struct PlacementProgress;

// ---------------------------------------------------------------------------
// TemporalRefitOptions
// ---------------------------------------------------------------------------
//
// Controls the budget mode, the DP search horizon, and the cancel /
// progress callbacks. All callbacks are optional; nullptr / empty
// std::function means "no callback".
struct TemporalRefitOptions {
  // Two budget modes for the validation gate.
  //
  // Strict   — full operator tolerance is available to the refit.
  //            Reinvests any headroom that the primary did not
  //            consume (e.g., when vertex prune lowered max_err
  //            below the original tolerance). DEFAULT.
  //
  // Relative — refit is forbidden from worsening the achieved error
  //            beyond `err(K1, S) + relative_eps`. Conservative for
  //            users who saw an acceptable primary preview and want
  //            to guarantee the refit cannot make it visually worse.
  enum class BudgetMode { Strict, Relative };
  BudgetMode budget_mode = BudgetMode::Strict;

  // Slack above K1's achieved error allowed when budget_mode ==
  // Relative. Ignored in Strict mode.
  double relative_eps = 1e-6;

  // DP search horizon (segments wider than this are not considered).
  // 0 = inherit a sensible default from comp.fps (the dp_placer
  // already implements this fallback).
  int max_gap_samples = 0;

  // Optional cancel and progress callbacks. The cancel function must
  // be cheap (a stat on a sentinel path, or an atomic flag read) and
  // is polled at outer-loop boundaries; it is also read inside the
  // parallel candidate-evaluation lambda to short-circuit in-flight
  // workers.
  std::function<bool()> cancel_fn;
  std::function<void(const PlacementProgress&)> progress_fn;

  // Base progress fraction the refit can claim. The pass owns roughly
  // a 10 % progress span at the end of the cleanup phase; callers
  // pass the base offset so the emitted events compose cleanly with
  // the surrounding pipeline. Out-of-range values are clamped at
  // emission time.
  double progress_base = 0.0;
  double progress_span = 0.10;
};

// ---------------------------------------------------------------------------
// TemporalRefitResult
// ---------------------------------------------------------------------------
//
// Returned by TryTemporalRefitKeyReduction. On the success path, `keys`
// holds K2; on every rejection path, `keys` holds K1 (the caller can
// always swap `result.keys` into the property bundle unconditionally).
struct TemporalRefitResult {
  // True if the refit pass ran at all (false only when the pass was
  // gated off, the input was structurally ineligible, or the input
  // already had < 3 keys).
  bool attempted = false;

  // True iff K2 was accepted and `keys` holds it.
  bool accepted = false;

  // One of:
  //   ""                — accepted=true
  //   "no_gain"         — refit produced |K2| >= |K1|
  //   "over_budget"     — refit produced |K2| < |K1| but error vs S exceeded budget
  //   "cancelled"       — cancel_fn returned true during the pass
  //   "degenerate"      — input has < 3 keys, refit has nothing to reduce
  //   "ineligible_*"    — property kind / sampling mode / topology not supported
  //                       by the current temporal-refit implementation. Notable shape/custom cases:
  //                       ineligible_custom_property,
  //                       ineligible_shape_flat_source_malformed,
  //                       ineligible_shape_flat_key_topology.
  //   "not_implemented" — reserved for older callers; current code does not emit this
  std::string rejection_reason;

  // Always populated. On rejection, this is exactly the input K1.
  PropertyKeys keys;

  // Bookkeeping for the operator-visible note channel and benchmark
  // harness. input_key_count == |K1|; output_key_count == |keys|
  // (== |K1| when !accepted, == |K2| when accepted).
  int input_key_count = 0;
  int output_key_count = 0;

  // Maximum L∞ error of `keys` evaluated against `source` at every
  // source sample time. Reported even on rejection so operators can
  // see the validation outcome.
  double max_err = 0.0;
  double max_err_screen_px = 0.0;

  // Compact operator-visible note tokens (see plan §5 for the
  // shape). Caller appends this to PropertyKeys.notes via the same
  // semicolon-separated convention used elsewhere.
  std::string notes;
};

// ---------------------------------------------------------------------------
// TryTemporalRefitKeyReduction
// ---------------------------------------------------------------------------
//
// Entry point. Idempotent (running it twice on the same accepted_keys
// is a no-op the second time because the first call already minimised
// the key count). Safe to call on every property in a bundle. The current
// implementation supports fixed-dimension non-Custom numeric streams (Scalar, vector,
// and spatial variants with samples_per_frame == 1) plus Custom
// `shape_flat` streams whose accepted keys have stable candidate
// topology. shape_flat candidates are always validated against the
// original source samples through path/source-outline temporal
// validation, not scalar ValidateKeys. Other custom streams and
// sub-frame sample bundles return attempted=false with an explicit
// ineligible_* rejection reason.
//
// PARAMETERS
//   source         — original PropertySamples as ingested by the
//                    solver. Refit validates exclusively against this
//                    stream; the value here MUST be the source bbsm
//                    samples, never an intermediate resample.
//   accepted_keys  — K1, the primary's accepted output after vertex
//                    cleanup and static-key-run collapse. The keys'
//                    t_sec values must lie within
//                    [source.front().t_sec, source.back().t_sec].
//   config         — solver config; refit consults `tolerance`,
//                    `tolerance_screen_px`, and the path-replacement
//                    /shape-temporal/influence settings already
//                    governing the primary fit.
//   comp           — composition info (fps, layer transform) for
//                    screen-px validation.
//   options        — refit-specific knobs and callbacks.
//
// RETURNS
//   See TemporalRefitResult above. The caller can always do:
//     property_keys = std::move(result.keys);
//     AppendNote(property_keys.notes, result.notes);
//   regardless of accepted=true/false.
//
// THREAD SAFETY
//   This function is reentrant for distinct (source, accepted_keys)
//   pairs. Concurrent calls on the same property are unsafe (no
//   internal locking). The current pipeline calls refit serially per
//   property; the parallelism it exploits internally is TBB
//   parallel_for inside the DP candidate-fit evaluation, gated by
//   the same kParallelJobsHardCap / --jobs plumbing as bridge prune.
TemporalRefitResult TryTemporalRefitKeyReduction(
    const PropertySamples& source,
    const PropertyKeys& accepted_keys,
    const SolverConfig& config,
    const CompInfo& comp,
    const TemporalRefitOptions& options = {});

// ---------------------------------------------------------------------------
// Internal-but-documented helpers (free functions, not exposed via API)
//
// Declared here only so they are unit-testable from
// solver/tests/solver_unit/test_temporal_refit.cpp without going through the
// full pipeline. Implementations live in temporal_refit_* helper modules.
// ---------------------------------------------------------------------------

// Resample the curve defined by `accepted_keys` at every timestamp in
// `source_times`. Produces a synthetic dense stream with identical
// timestamps to the source but values reconstructed from K1.
//
// Used internally as the input to the refit's DP placement. The
// output stream's property metadata (id, kind, dimensions, units)
// is copied from `source_template`. For shape_flat, dimensions are
// taken from the accepted key topology rather than the original source
// topology so reduced-path candidates can be refit and then validated
// against the original outline. Unsupported custom properties return
// an empty sample stream.
PropertySamples ResampleAcceptedAtSourceTimes(
    const PropertyKeys& accepted_keys,
    const PropertySamples& source_template);

// Validate `candidate` against `source` and report the per-sample
// property/screen error for fixed-dimension non-Custom properties, or
// source-outline error for Custom `shape_flat` properties. The
// validation runs at every source sample, never at an intermediate
// stream's timestamps. Unsupported custom streams return ok=false.
//
// `max_err_out` / `max_err_screen_px_out` are always populated. The
// return value `bool ok` is the gate verdict against the configured
// tolerance under `budget_mode` and `budget_relative_ceiling`.
//
// For `budget_mode == Relative`, `budget_relative_ceiling` MUST be
// `err(K1, S) + options.relative_eps`. For `Strict`, the ceiling is
// the operator's `tolerance` (the function ignores the parameter).
bool ValidateRefitAgainstSource(
    const PropertyKeys& candidate,
    const PropertySamples& source,
    const SolverConfig& config,
    const CompInfo& comp,
    TemporalRefitOptions::BudgetMode budget_mode,
    double budget_relative_ceiling,
    double* max_err_out,
    double* max_err_screen_px_out);

}  // namespace bbsolver
