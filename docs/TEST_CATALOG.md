# bbsolver Test Catalog

This catalog explains what the solver tests are for, when to run them, and how
to interpret valid and invalid output.

It covers the standalone solver test tree:

- `tests/solver_unit/` — C++ unit and focused integration tests built by
  CMake.
- `tests/policies/` — solver-owned Python source-level policy checks.

## Quick Commands

Configure and build every C++ solver test:

```sh
cmake -S . -B build -DBBSOLVER_BUILD_TESTS=ON
cmake --build build -j 8
```

Run every registered C++ solver test:

```sh
ctest --test-dir build --output-on-failure -j 8
```

Run the fast unit sweep used by CI:

```sh
ctest --test-dir build -L unit -LE slow --output-on-failure -j 8
```

That command uses CTest's `unit` label and excludes tests marked `slow`.
To include the slow replacement-temporal solver coverage:

```sh
ctest --test-dir build -L unit --output-on-failure -j 8
```

For preset-based equivalents and the incremental package validator, see
[`VALIDATION_WORKFLOWS.md`](VALIDATION_WORKFLOWS.md).

Run one C++ test:

```sh
cmake --build build --target test_solver_config_io -j 8
./build/test_solver_config_io
```

or:

```sh
ctest --test-dir build -R "^test_solver_config_io$" --output-on-failure
```

Run all solver-owned Python policies:

```sh
for policy in tests/policies/*_policy.py; do
  python3 "$policy" || exit $?
done
```

## How To Read Results

### C++ Unit Tests

Each `tests/solver_unit/test_*.cpp` file builds into one executable with
the same basename. CMake registers each executable with `ctest`.

Valid output:

- exit code `0`
- `ctest` reports `Passed`
- many binaries print nothing on success

Invalid output:

- non-zero exit code
- `assert(...)` failure
- a `Require(...)` failure message from helper assertions
- uncaught exception such as a parser rejecting an unexpected enum
- sanitizer/runtime crash if a sanitizer build is used

Some tests intentionally feed invalid inputs to the solver. That is still a
valid test when the binary exits `0`; the test is checking that the invalid
input is rejected, downgraded, or routed safely.

### Python Policy Checks

Solver-owned policies under `tests/policies/` are source-level checks.
Most do not compile or run the solver. Some smoke policies use `build`
when a binary is available.

Valid output:

```text
[PASS] test_name
summary: N passed, 0 failed
```

Invalid output:

```text
[FAIL] test_name: reason
summary: N passed, M failed
```

or a Python exception / non-zero exit code.

Policy failures usually mean a structural contract moved without updating the
guard. Do not silence a policy by weakening it until you have confirmed the
new structure is intentional.

## When To Run Which Tests

| Situation | Run |
|---|---|
| Editing one implementation module | Build and run that module's closest `test_<area>_*.cpp`, then any affected policy. |
| Editing public headers | Run the focused C++ tests plus `solver_public_header_dependency_policy.py` and `solver_layout_policy.py`. |
| Editing diagnostics/progress/cancellation | Run `test_solver_diagnostics`, `test_solver_diagnostic_events`, `test_progress_events`, `test_solve_lifecycle_reporting`, `test_solve_cancellation`, plus diagnostics/progress policies. |
| Editing JSON or bundle schemas | Run `test_sample_*_io`, `test_key_bundle_io`, `test_solver_config_io`, `test_verify_dump_commands`, and `sample_json_refactor_policy.py`. |
| Editing solve modes or routing | Run `test_solve_mode_policy`, `test_property_solver_routing`, route-specific tests, and `property_solver_routing_policy.py`. |
| Editing motion smoothing | Run all `test_motion_*` binaries plus `motion_smooth_facade_lock_policy.py`. |
| Editing path geometry/replacement/reduction | Run the focused `test_path_*` family for the area plus `solver_layout_policy.py` and affected path policies. |
| Editing temporal refit | Run `test_temporal_refit`, `test_temporal_refit_gate`, and `temporal_refit_refactor_policy.py`. |
| Before integration commit | Run `ctest --test-dir build --output-on-failure -j 8` and every policy under `tests/policies/`. |
| Before standalone release | Run `python3 scripts/validate_standalone_package.py`. It performs the forced local archive build, full C++ tests, all solver policies, install/package smoke, JSON example solve/verify/dump checks, negative bundle checks, install hygiene scan, and clangd sweep when available. |

## C++ Test Families

### App, CLI, Runtime, Solve Lifecycle

Use these when changing process behavior, command options, progress/cancel
behavior, diagnostics lifecycle, reporting, or solve orchestration.

Files:

- `test_cli_options.cpp`
- `test_runtime_env.cpp`
- `test_progress_events.cpp`
- `test_solve_cancellation.cpp`
- `test_solve_command_config.cpp`
- `test_solve_lifecycle_reporting.cpp`
- `test_solve_mode_policy.cpp`
- `test_solve_path_preparation.cpp`
- `test_solve_property_output.cpp`
- `test_solve_property_post_processing.cpp`
- `test_solve_property_temporal_prelude.cpp`
- `test_solve_property_temporal_result.cpp`
- `test_solver_diagnostic_events.cpp`
- `test_solver_diagnostics.cpp`
- `test_solver_observability.cpp`
- `test_solver_reporting.cpp`
- `test_verify_dump_commands.cpp`

Expected valid behavior:

- documented CLI flags parse or reject deterministically
- lifecycle/progress/diagnostic JSON carries required fields
- cancellation writes a partial bundle and returns the documented cancel path
- `main` remains dispatch-only through policy checks, not through these tests

Expected invalid behavior:

- missing or malformed lifecycle fields
- accepted invalid CLI options
- cancellation output marked as fully converged
- command code starting to own leaf algorithm behavior

### Bundle IO, Domain Values, Samples

Use these when changing `SampleBundle`, `KeyBundle`, config parsing, sample
value interpretation, source-key preservation, or raw-frame output.

Files:

- `test_key_bundle_io.cpp`
- `test_raw_frame_keys.cpp`
- `test_sample_bundle_io.cpp`
- `test_sample_json_timing_io.cpp`
- `test_sample_json_value_io.cpp`
- `test_sample_key_timing.cpp`
- `test_sample_property_io.cpp`
- `test_sample_value_helpers.cpp`
- `test_solver_config_io.cpp`
- `test_source_key_preservation.cpp`
- `test_static_key_cleanup.cpp`

Expected valid behavior:

- known JSON fields parse into the expected domain structs
- unknown or null optional fields fall back safely where the parser permits it
- enum strings reject unknown values
- config compatibility aliases parse but canonical snake_case stays preferred
- static cleanup removes only redundant keys

Expected invalid behavior:

- unknown enum strings accepted silently
- required nested objects missing without a controlled failure
- snake_case config fields ignored
- source-key anchors lost when preservation is enabled

### Routing And Basic Property Solving

Use these when changing property classification, route priority, plain solving,
separated position behavior, or fallback behavior.

Files:

- `test_plain_property_solver.cpp`
- `test_property_classification.cpp`
- `test_property_solver_routing.cpp`
- `test_separated_position.cpp`

Expected valid behavior:

- route priority remains explicit and deterministic
- shape-flat, spatial, separated, and plain properties classify correctly
- separated Position streams behave like ordinary scalar/1D child streams

Expected invalid behavior:

- route priority changes without tests changing intentionally
- non-shape data routed to shape-only solvers
- separated streams recombined or interpreted with the wrong dimensions

### DP, Segment Fitting, Metrics

Use these when changing key placement, segment feasibility, Ceres fitting,
analytic Bezier fitting, AE curve evaluation, or error metrics.

Files:

- `test_ae_curve.cpp`
- `test_dp_placement_strategy.cpp`
- `test_segment_fit_bezier.cpp`
- `test_segment_fit_ceres.cpp`
- `test_segment_fit_diagnostic_events.cpp`
- `test_segment_fit_policy.cpp`
- `test_segment_fit_samples.cpp`
- `test_segment_fit_shape_temporal.cpp`
- `test_segment_fit_unified_spatial.cpp`
- `test_segment_fitter.cpp`

Expected valid behavior:

- feasible segments stay within tolerance
- infeasible segments are rejected
- fitter policy chooses the expected fitter by value kind/property shape
- AE-style interpolation evaluates consistently with generated keys

Expected invalid behavior:

- a segment reports feasible while exceeding tolerance
- fitter selection changes for an existing property class without a scoped
  behavior change
- diagnostic event builders lose required fields

### Motion Smooth And Motion Path Smooth

Use these when changing `motion_smooth`, `motion_path_smooth`, smoothing ease,
rove behavior, endpoint preservation, shape-flat smoothing, or motion-smooth
post-reduction gates.

Files:

- `test_motion_path_smooth_solver.cpp`
- `test_motion_smooth_bezier_ease_guards.cpp`
- `test_motion_smooth_curve_and_tangent_lock.cpp`
- `test_motion_smooth_reduction_gate.cpp`
- `test_motion_smooth_rove_schedule.cpp`
- `test_motion_smooth_shape_flat.cpp`
- `test_motion_smooth_shape_flat_closed_loop.cpp`
- `test_motion_smooth_shape_flat_key_emission.cpp`
- `test_motion_smooth_shape_flat_notes.cpp`
- `test_motion_smooth_shape_flat_topology_gate.cpp`
- `test_motion_smooth_shape_loop_adaptive.cpp`
- `test_motion_smooth_shape_loop_schedule.cpp`
- `test_motion_smooth_shape_trajectory_smooth.cpp`
- `test_motion_smooth_solver.cpp`
- `test_motion_smooth_source_key_schedule.cpp`

Expected valid behavior:

- endpoints remain stable
- source-key fidelity locks source poses when enabled
- sharp motion-path points stay sharp when preservation is enabled
- keyed frames can be hard constraints in `motion_path_smooth`
- smoothing notes identify which smoothing path was used
- rove/ease behavior follows mode settings

Expected invalid behavior:

- endpoint deletion
- smoothing a locked bounce/cusp into a rounded turn
- source keyed frames drifting when `motion_path_respect_keyed_frames=true`
- shape-flat topology instability accepted as a stable smooth

### Path Topology, Geometry, Visible Outline

Use these when changing path flattening, feature anchors, topology parsing,
outline error, visible-outline extraction/prepass, and path geometry helpers.

Files:

- `test_path_dense_landmarks.cpp`
- `test_path_feature_anchor.cpp`
- `test_path_feature_cluster.cpp`
- `test_path_fraction_helpers.cpp`
- `test_path_frame_fit.cpp`
- `test_path_frame_fit_decimate.cpp`
- `test_path_geometry_refinement.cpp`
- `test_path_outline_error.cpp`
- `test_path_outline_fraction_expand.cpp`
- `test_path_portability.cpp`
- `test_path_sharp_feature.cpp`
- `test_path_visible_outline_extract.cpp`
- `test_path_visible_outline_prepass.cpp`
- `test_shape_flat_topology.cpp`
- `test_shape_outline_error.cpp`
- `test_sharp_corner_policy.cpp`

Expected valid behavior:

- `shape_flat` payloads parse into stable topology only when dimensions match
- visible-outline helpers report bounded geometry error
- sharp features/corners remain identifiable under tolerated simplification
- frame-fit and decimation preserve required geometry contracts

Expected invalid behavior:

- accepting malformed or changing topology as stable
- losing a protected sharp corner under a loose tolerance
- outline error calculated against the wrong contour or fraction layout

### Path Decompose, Reduction, Bridge Prune

Use these when changing per-channel path decomposition, post-solve vertex
reduction, bridge refitting, or bridge-prune scheduling.

Files:

- `test_path_bridge_prune.cpp`
- `test_path_decompose.cpp`
- `test_path_decomposed_solver.cpp`
- `test_path_post_solve_reduction.cpp`
- `test_path_vertex_reduction.cpp`

Expected valid behavior:

- decomposition emits one parent `PropertyKeys` row after reassembly
- child solves do not leak into final public output
- vertex/bridge reductions keep accepted key times and error budgets safe
- bridge-prune decisions remain deterministic across parallel jobs

Expected invalid behavior:

- changing final property ids during reassembly
- accepting a vertex removal that exceeds geometry or temporal tolerance
- parallel bridge-prune output differing from serial output

### Path Fit, Replacement, Multimode

Use these when changing fitted replacement path topology, target ladders,
fraction layouts, region/multimode logic, landmark subpaths, or replacement
temporal integration.

Files:

- `test_path_fit.cpp`
- `test_path_fit_pipeline.cpp`
- `test_path_gap_policy.cpp`
- `test_path_multimode_geometry.cpp`
- `test_path_multimode_input_validation.cpp`
- `test_path_multimode_landmark_diagnostics.cpp`
- `test_path_multimode_landmark_emission.cpp`
- `test_path_multimode_landmark_options.cpp`
- `test_path_multimode_landmark_output.cpp`
- `test_path_multimode_landmark_partition.cpp`
- `test_path_multimode_landmark_segment_fit.cpp`
- `test_path_multimode_landmark_temporal_solve.cpp`
- `test_path_multimode_notes.cpp`
- `test_path_multimode_recombined_temporal.cpp`
- `test_path_multimode_reconstruction.cpp`
- `test_path_multimode_region_candidate.cpp`
- `test_path_multimode_solver.cpp`
- `test_path_multimode_solver_notes.cpp`
- `test_path_multimode_temporal.cpp`
- `test_path_multimode_visible_probe.cpp`
- `test_path_progress_band_oracle.cpp`
- `test_path_replacement_adaptive_expansion.cpp`
- `test_path_replacement_feature_layout_trial.cpp`
- `test_path_replacement_fraction_layout.cpp`
- `test_path_replacement_fraction_trial.cpp`
- `test_path_replacement_initial_scan.cpp`
- `test_path_replacement_notes.cpp`
- `test_path_replacement_phase2_fit.cpp`
- `test_path_replacement_progress.cpp`
- `test_path_replacement_seed_selection.cpp`
- `test_path_replacement_target_ladder.cpp`
- `test_path_solver_config.cpp`
- `test_path_temporal_band_helpers.cpp`
- `test_path_temporal_progress.cpp`
- `test_replacement_temporal_solver.cpp`

Expected valid behavior:

- replacement candidates must beat the configured acceptance criteria before
  replacing source topology
- target ladders and fraction layouts remain deterministic
- multimode/landmark outputs carry expected notes and grouping metadata
- region and temporal validation reject unsafe candidates
- path-specific config tightens error budgets in the expected places

Expected invalid behavior:

- accepting a replacement with worse keys/vertices outside configured policy
- missing diagnostic notes for advanced path outputs
- emitting landmark outputs as if they were ordinary source replacements
- region candidates bypassing validation or budget gates

### Temporal Refit

Use these when changing post-solve temporal refit eligibility, candidate
generation, resampling, validation, or acceptance.

Files:

- `test_temporal_refit.cpp`
- `test_temporal_refit_gate.cpp`

Expected valid behavior:

- only eligible property classes enter refit
- accepted refits reduce or preserve key count while staying inside tolerance
- rejected candidates leave the original solved keys unchanged

Expected invalid behavior:

- refit changing unsupported custom/shape properties
- accepting a refit that fails replay validation
- mutating the original output after a rejected candidate

## Solver-Owned Policy Catalog

| Policy | Purpose | Valid Output | Invalid Output |
|---|---|---|---|
| `solver_layout_policy.py` | Locks public/include and private/src folder placement, test-tree placement, archive fallback layout, standalone CI, and repository-sync safety. | `summary: 50 passed, 0 failed` currently. | Missing/misplaced module, root `src/*.cpp`, `.h` header return, missing archive, unsafe sync path, or missing standalone CI coverage. |
| `solver_public_header_dependency_policy.py` | Blocks public headers from including private flat source headers. | `summary: 2 passed, 0 failed`. | Any new public-to-private include or stale grandfather entry. |
| `solver_diagnostics_boundary_policy.py` | Keeps `DiagnosticsWriter` owned by lifecycle/orchestration surfaces. | `summary: 7 passed, 0 failed` currently. | Leaf modules include or emit diagnostics directly. |
| `solver_diagnostics_policy.py` | Smoke-checks optional diagnostics JSONL lifecycle. | Passes or skips build-dependent checks safely when no binary exists. | Missing lifecycle event, wrong cancellation event, diagnostics created by default. |
| `solver_diagnostic_events_schema_policy.py` | Locks diagnostic event builder schema fields. | `summary: 10 passed, 0 failed`. | Missing schema fields, renamed events, bridge-prune event drift. |
| `solver_progress_policy.py` | Locks progress JSON anchors and build-conditional progress smoke cases. | `summary: 20 passed, 0 failed` currently when build exists. | Missing phase/progress fields, non-monotone progress, shape/motion progress regression. |
| `property_solver_routing_policy.py` | Keeps route priority and route execution outside the app entry point. | `summary: 3 passed, 0 failed`. | Route ladder moved into wrong layer or priority not explicit. |
| `main_dispatch_only_policy.py` | Ensures `src/app/main.cpp` stays command-dispatch only. | `summary: 4 passed, 0 failed`. | Main gains lifecycle/progress/solver orchestration. |
| `motion_smooth_facade_lock_policy.py` | Locks motion-smooth façade/sub-module ownership. | `summary: 36 passed, 0 failed` currently. | Deleted bodies return, façade drops exports, ownership duplicates. |
| `temporal_refit_refactor_policy.py` | Locks temporal-refit helper/facade boundaries. | `summary: 18 passed, 0 failed` currently. | Helper grows into orchestration, public header grows, diff leaves lane. |
| `path_multimode_refactor_policy.py` | Locks path/multimode coordinator and helper purity. | `summary: 5 passed, 0 failed`. | Coordinator grows past target or helpers own diagnostics/progress. |
| `sample_json_refactor_policy.py` | Keeps `io_json.cpp` a file facade and JSON parsing in extracted modules. | `summary: 7 passed, 0 failed`. | File facade regains parsing responsibilities or extracted modules gain filesystem/diagnostics ownership. |

## Missing Granularity

This catalog explains the test families and their pass/fail meaning. It does
not yet document every individual assertion inside each C++ test binary.
For exact per-assertion expected values, the current source files are the
authority.

If a future test adds a golden output file or fixture-specific baseline, record
that baseline in this catalog or a linked fixture document so the intended
valid and invalid outputs are visible without reading the test body.
