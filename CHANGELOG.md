# bbsolver Changelog

## v1.1.0

- Rebuilt the standalone After Effects ScriptUI test harness from the proven
  internal v103 panel instead of the reduced prototype UI.
- Renamed the panel to `bbsolver-test-harness`, set the public harness version
  to `1.1.0`, and moved support includes under the required
  `bbsolver-test-harness/` support folder.
- Preserved the v103 selected-property workflow, multi-property solve/apply,
  progress bar, elapsed timer, async macOS solver runner, preview, and writeback
  behavior.
- Removed public controls for layer-wide batch bake, pairwise rig-gap verify,
  landmark sub-path emission, raw Bezier/replacement fitting toggles,
  expression archive/delete, and guide-layer archive.
- Added main-panel log clearing/export controls and a settings-path for the
  exported log folder.

## v1.0.0

The first standalone release of `bbsolver`. The package is a self-contained
C++ spatiotemporal optimization engine for animation data.

Supported integration surface, frozen for the `1.0.x` series:

- **CLI process boundary**: subcommands `solve`, `verify`, `dump`,
  `--version`, `--help`; exit codes `0`, `1`, `2`, `3`, `5`; progress
  JSON via `--progress-fd`, diagnostics JSONL via `--diagnostics`,
  cooperative cancellation via `--cancel-file`. See
  [`docs/SOLVER_CLI.md`](docs/SOLVER_CLI.md).
- **JSON IO contract**: strict-schema SampleBundle (`_schema: "samples"`,
  `schema_version: 1`, non-empty `properties[]`) and KeyBundle
  (`_schema: "keys"`, `schema_version: 1`). Per-sample value vectors
  satisfy `v.length == property.dimensions * samples_per_frame`. See
  [`docs/API_REFERENCE.md`](docs/API_REFERENCE.md).
- **CMake package**: `cmake --install` produces a self-contained tree
  with `bin/bbsolver`, public headers under `include/bbsolver/`,
  generated FlatBuffers headers, source schemas under
  `share/bbsolver/protocol/`, runnable examples, hash-locked
  dependency archives, and `lib/cmake/bbsolver/bbsolverConfig.cmake`.
  Consumers can `find_package(bbsolver CONFIG)` and link
  `bbsolver::core` or invoke `bbsolver::bbsolver`. See
  [`docs/PACKAGING.md`](docs/PACKAGING.md).
- **Solve modes**: `full`, `temporal_only`, `vertex_only`,
  `motion_smooth`, `motion_path_smooth`. Advanced path flags
  (`--decompose-paths`, `--fit-canonical-paths`,
  `--fit-replacement-paths`, `--emit-landmark-subpaths`) are off by
  default; see [`docs/PATH_HANDLING.md`](docs/PATH_HANDLING.md).
- **AE ScriptUI test harness** under
  [`examples/after-effects/`](examples/after-effects/) with a
  support-folder layout. Demonstrates sampling, optional
  unparent/bake of parented 2-D Position, expression bake-and-disable,
  CLI spawn, verify, and key writeback. See
  [`docs/AE_SCRIPTUI_HARNESS.md`](docs/AE_SCRIPTUI_HARNESS.md).
- **Three runnable non-AE JSON examples** under
  [`examples/json/`](examples/json/) — scalar, Position, and
  shape-flat — with a documented smoke loop and per-bundle
  reference outcomes.
- **Vendored dependencies** pinned by version + SHA-256 with
  hash-locked fallback archives under
  [`third_party/archive/`](third_party/archive/). License obligations,
  upstream URLs, and update process are documented in
  [`third_party/THIRD_PARTY_NOTICES.md`](third_party/THIRD_PARTY_NOTICES.md).
- **Release gate** at
  [`scripts/validate_standalone_package.py`](scripts/validate_standalone_package.py)
  exercises the standalone build, install, package smoke, JSON
  examples, negative-bundle checks, install hygiene, and clangd
  sweep when available.

The C++ symbol surface of `bbsolver::core` beyond the three command
entry points (`RunSolve`, `RunVerifyCommand`, `RunDumpCommand`) is
source-visible but is not part of the SDK contract; see
[`docs/DEVELOPER_GUIDE.md`](docs/DEVELOPER_GUIDE.md) §11.

For a five-minute build → solve → verify path, start at
[`docs/QUICKSTART.md`](docs/QUICKSTART.md).

---

## Appendix — Internal development history

The entries below are an engineering log of the development work leading up
to `1.0.0`. They are kept for provenance and for in-tree contributors
investigating the history of a specific module; they are not part of the
public release record. For the supported v1 surface, the package docs
under [`docs/`](docs/) are the source of truth.

### History coverage

The log was backfilled from the development branch refs that produced
this release. The earliest repository commit covered is from
`2026-05-14`; the latest is the commit that introduced this changelog
section.

## 2026-05-24 - Standalone Documentation And Policy Layout

### Standalone documentation and policy layout

- Added standalone-facing repository docs under `solver/`:
  - `README.md`
  - `LICENSE`
  - `CONTRIBUTING.md`
- Moved the solver-facing changelog back to `solver/CHANGELOG.md` so it lives
  at the standalone package root.
- Added `solver/examples/after-effects/bbsolver-test-harness.jsx`
  (`bbsolver-test-harness v1.0.0`) as a minimal ScriptUI example for AE
  property sampling, SampleBundle emission, CLI solving, KeyBundle parsing, and
  writeback.
- Added `AE_SCRIPTUI_HARNESS.md` with install, sampling, bundle-flow,
  motion-path-smooth, and writeback notes for the test harness.
- Extended the test harness with a visible build099-style log, macOS/Windows
  solver-path probing, and an optional parent-flattened 2D Position workflow
  that samples parented motion in comp space and unparents before writeback.
- Added persistent `*.log.txt` harness logs, explicit parent-flatten rejection
  reasons, default expression disabling after writeback, and documentation for
  the preferred standalone Windows install paths.
- Copied the solver reference docs into `solver/docs/` so the solver docs are
  no longer dependent on root `docs/solver/` paths:
  - `SOLVER_CLI.md`
  - `TUNING_GUIDE.md`
  - `DP_ALGORITHM.md`
  - `PATH_HANDLING.md`
  - `COMPRESSION_CURVES.md`
  - `PATH_DECOMPOSE_REVIEW.md`
- Added `solver/tests/policies/` for solver-owned Python source-level policy
  checks and rewired the quick Phase 3 guard to execute those solver-local
  policy paths for solver-owned rules.
- Monorepo-only checks remain under root `tests/`, including AE panel,
  installer, progress-report, release, and CI workflow policies.
- Expanded `DEVELOPER_GUIDE.md` with a practical host-product integration
  guide covering subprocess use, input `SampleBundle` JSON, property/sample
  schema requirements, shape-flat path encoding, motion-path-smooth config,
  and output `KeyBundle` writeback semantics.
- Clarified in `USER_GUIDE.md` that camelCase config aliases exist only for
  compatibility with older host-panel/settings bundles; new integrations should
  emit the canonical snake_case fields.
- Added `TEST_CATALOG.md` to explain the solver C++ test families, solver-owned
  Python policies, when each category should be run, and what valid/invalid
  output means.

### Motion Path Smooth mode

- Added the `motion_path_smooth` solve mode for Position-style
  spatial properties.
- Introduced a separate `MotionPathSmoothSpatialTrajectoryKeys` surface under
  the `motion_smooth` area. It is intentionally distinct from the existing
  Motion Smooth trajectory fitter: it smooths the source motion path, then
  reduces keys against the smoothed trajectory while preserving optional
  keyed-frame and sharp-turn constraints.
- Extended solve-mode policy with:
  - `SolveModeIsMotionPathSmooth`
  - `SolveModeUsesMotionSmoothing`
- Extended solver configuration parsing for motion-path smoothing options,
  including compatibility alias handling for `motionPathSmoothingTolerance`.
- Extended solve-mode diagnostics to report both `is_motion_path_smooth` and
  `uses_motion_smoothing`.
- Added focused C++ coverage for:
  - bounce/impact points staying locked as sharp constraints
  - source keyed frames acting as hard constraints when requested
  - smooth arcs reducing source key schedules while preserving endpoints
- Validation for the mode is covered by the v1 release gate and the focused
  motion-path-smooth unit tests.

## 2026-05-24 - Standalone Packaging And Test Layout

### Dependency archive fallback

- Added a tracked backup archive mirror under `solver/third_party/archive/`
  for the pinned solver dependencies:
  - `nlohmann_json` 3.11.3
  - Eigen 3.4.0
  - Ceres Solver 2.2.0
  - oneTBB 2021.13.0
  - FlatBuffers 24.3.25
- `solver/CMakeLists.txt` still prefers installed packages and pinned upstream
  `FetchContent` downloads. The new archive path is a fallback for remote
  download failure, or a forced offline path when
  `BBSOLVER_FORCE_THIRD_PARTY_ARCHIVES=ON`.
- All fallback archives are hash-locked with the same `URL_HASH SHA256`
  values used for remote downloads.
- Validation gate recorded for the checkpoint:
  - forced-local archive configure/build of `bbsolver`
  - normal configure/build
  - full `ctest`
  - all-solver clangd sweep with zero diagnostics
  - quick Phase 3 guard
  - diff checks

### Solver-local test tree

- Moved compiled C++ solver tests from root `tests/solver_unit/` into
  `solver/tests/solver_unit/`.
- Kept root `tests/` for repository-level Python policy and integration
  harnesses.
- Updated CMake discovery, source comments, policy harnesses, and clangd checks
  to point at the solver-local test tree.
- No solver algorithm, progress, cancellation, diagnostics, PIMPL, or multicore
  behavior changed in this lane.

## 2026-05-23 - Phase 3 Standalone Layout Completion

Phase 3 converted the solver from a flat monolithic source tree into a
standalone `bbsolver` package shape while preserving behavior.

### Public API and source layout

- Renamed the solver C++ identity to `bbsolver` and moved public headers under
  `solver/include/bbsolver/`.
- Current public header map:
  - top-level public header: `bbsolver/domain.hpp`
  - area folders: `app`, `diagnostics`, `dp`, `fit`, `io`, `metrics`,
    `motion_smooth`, `path`, `progress`, `replacement_temporal`, `routing`,
    `runtime`, `samples`, `shape`, `solve`, `temporal`, and `verify`
- Moved implementation files under matching `solver/src/<area>/` roots.
- Moved the executable entry point from `solver/src/main.cpp` to
  `solver/src/app/main.cpp`.
- Added a final root-layout guard: no tracked `.cpp`, `.hpp`, or `.h` files
  may live directly under `solver/src`.
- Demoted `solver/src` from a public include root to a private implementation
  include root. Public consumers use `solver/include` and generated protocol
  headers.
- Removed legacy `.h` solver headers. Migrated the final `domain.h` to
  `domain.hpp` and removed legacy-header exemptions from the guard.
- Public-header private-dependency policy now has an empty grandfather map.

### Path replacement layout

- Moved the path replacement family into:
  - `solver/include/bbsolver/path/replacement/`
  - `solver/src/path/replacement/`
- The family includes acceptance, adaptive expansion, baseline solve,
  candidate validation, decision apply, fast-vertex acceptance, feature-layout
  trial, fraction layout/trial, initial scan, notes, Phase 2 fit, post-temporal
  orchestration, preference, progress, retry loop, seed selection, solver, and
  target-ladder modules.
- Consumers now include replacement surfaces through canonical
  `bbsolver/path/replacement/...` paths.
- Progress emission remains caller-owned through `ProgressWriter`/
  `ProgressBridge`; helper modules do not own `DiagnosticsWriter`.
- C++ diagnostics/lifecycle emission remains owned by solve command and solve
  lifecycle orchestration.

### App, verify, runtime, progress, diagnostics, and solve layout

- Moved app CLI options and the dispatch-only executable entry point into the
  `app` area.
- Moved verify/dump command surfaces and key-bundle verifier into the `verify`
  area.
- Moved runtime environment and TBB runtime-scope helpers into the `runtime`
  area.
- Moved progress and cancellation surfaces into the `progress` area.
- Moved solver diagnostics and diagnostic event builders into the
  `diagnostics` area.
- Moved solve orchestration, command config, lifecycle reporting, path
  preparation, property output/completion/post-processing, temporal prelude,
  reporting, observability, static cleanup, and plain/fallback property solvers
  into the `solve` area.
- Tightened `main.cpp` to dispatch-only behavior and then moved it to
  `solver/src/app/main.cpp`.

### Path-family layout

- Moved path frame-fit headers/sources into `path/frame_fit`.
- Split frame-fit types into a dedicated public type header.
- Moved path fit core into `path/fit`, including canonical path fitting,
  fit geometry, fit pipeline, fraction-layout evaluation, and feature-layout
  source implementation.
- Moved path geometry helpers into `path/geometry`, including feature anchors,
  feature clusters, fraction helpers, visible-outline prepass, sharp-feature
  classification, outline extraction/error, outline-fraction expansion, and
  geometry refinement.
- Moved path dense helpers into `path/dense`.
- Moved path decomposition helpers into `path/decompose`.
- Moved path multimode helpers into `path/multimode`.
- Moved path reduction and bridge-refit helpers into `path/reduction`.
- Moved path bridge-prune helpers into `path/bridge_prune`.
- Moved path temporal helpers into `path/temporal`.
- Moved path config helpers into `path/config`.

### DP, metrics, fit, shape, samples, IO, and temporal layout

- Moved DP placement helpers into `dp`:
  `dp_placer`, `dp_forward_placement`, `dp_key_assembly`,
  `dp_placement_limits`, and `dp_placement_progress`.
- Moved curve/error metric helpers into `metrics`.
- Moved segment fitting into `fit`, including samples/policy, Bezier
  reconstruction, unified spatial timing, shape-temporal fitting, Ceres
  fitting, and side-effect-free segment-fit diagnostic event builders.
- Moved shape policy/topology helpers into `shape`.
- Moved source key preservation, sample timing, sample value helpers, and raw
  frame fallback helpers into `samples`.
- Moved JSON IO into `io`, keeping `io_json.cpp` as a file-open/read/write
  facade and pushing key/sample/config parsing into focused modules.
- Moved temporal refit helpers into `temporal/refit`.
- Moved replacement-temporal helpers into `replacement_temporal`.

### Clangd and include cleanliness

- Added strict clangd config policy coverage and registered it in the quick
  Phase 3 guard.
- Added all-solver clangd LSP diagnostics checking and kept the target at zero
  diagnostics.
- Forced CMake to generate `compile_commands.json` so clangd has a solver-aware
  compile database on plain configure.
- Removed stale hard-coded VS Code compile-database assumptions.
- Fixed the DP fallback solver include-cleanliness gap by gating fallback-only
  includes under `#ifndef BBSOLVER_HAVE_DP_PLACER`.

### Layout validation gates

- `tests/solver_layout_policy.py` locks:
  - recursive implementation source discovery
  - public/private include-root separation
  - no root C++ files under `solver/src`
  - `.hpp`-only solver headers
  - public area-folder map
  - app entry path
  - module-specific area placement
  - solver-local C++ unit test tree
  - third-party archive fallback and hash-locking
- `tests/solver_public_header_dependency_policy.py` blocks public headers from
  including flat private `solver/src/*.hpp` headers.
- The standard validation stack for final layout slices was:
  - focused C++ builds/tests for moved modules
  - full `cmake --build`
  - full `ctest`
  - all-solver clangd sweep
  - diagnostics/progress/path-panel policies
  - quick Phase 3 guard
  - `git diff --check`

## 2026-05-22 - Phase 3 Mainline Compaction

Phase 3 began as behavior-neutral extractions from a large `main.cpp` and then
expanded into module-family compaction. The guiding rule was that refactors
must not change key counts, max error, progress JSON shape, cancellation
semantics, or output bundle contracts.

### Main solve orchestration extraction

- Extracted progress writer and progress JSON bridge.
- Extracted CLI option parsing, usage/version handling, verify/dump commands,
  property routing, reporting/note helpers, static cleanup, runtime environment,
  cancellation helpers, solve-mode policy, path-gap policy, sample timing,
  source-key preservation, sample value helpers, property classification,
  solver observability, temporal-refit gating, raw-frame key fallback, plain
  property solver entry, solve command config, lifecycle reporting, cancelled
  lifecycle reporting, solved-property output, fallback property solver, path
  solve preparation, property temporal prelude, property post-processing,
  temporal result reporting, replacement note/progress helpers, replacement
  baseline/retry/validation helpers, parallel runtime scope, reporting helpers,
  post-temporal replacement orchestration, solved-property completion, and
  final solve command dispatch.
- Result: the command path moved from monolithic ownership toward a small app
  dispatcher and a `solve` area that owns orchestration, progress, cancellation,
  and diagnostics boundaries.

### Replacement path solving

- Extracted replacement fraction layout, Phase 2 target fit, seed selection,
  initial-frame scan, feature-layout trial, fraction replay, adaptive expansion,
  note composition, replacement path-fit orchestration, retry helpers,
  validation summaries, baseline solving, fast-vertex acceptance, decision
  application, preference notes, retry loop, and post-temporal orchestration.
- Preserved target-ladder ordering, target start/reject progress payloads,
  Phase-2/median/seed/adaptive/feature trial order, fallback notes, success
  notes, candidate-accounting counters, cancellation behavior, and output
  contracts.

### Replacement temporal solver

- Split replacement temporal solving into focused modules for:
  - option normalization
  - key assembly
  - forward-longest-span placement
  - segment fitting
  - exact-anchor pruning
  - exact-anchor prune fitting
  - relaxed endpoint fitting
- `replacement_temporal_solver.cpp` was reduced below the Phase 3 soft cap and
  became the coordinator.
- Diagnostics protocol:
  - pure option/key helpers emit no diagnostics or progress
  - forward-span and exact-anchor modules keep existing placement-progress
    callback ownership where that behavior already existed
  - segment/relaxed fit helpers return result fields and reason strings only
  - no `DiagnosticsWriter` ownership moved into replacement-temporal helpers

### Bridge prune

- Split post-temporal bridge pruning into compact helpers for candidate
  evaluation, planning, selection, progress event building, note/result
  assembly, per-round orchestration, batch apply, batch attempts, finalization,
  and diagnostics event builders.
- Preserved deterministic candidate comparison, cancellation checks, serial
  fallback, progress emission outside worker threads, note spelling, and output
  contracts.
- Added bridge-prune diagnostic event/schema coverage for phase/result summaries
  without moving `DiagnosticsWriter` ownership into helper modules.

### Path multimode

- Reduced `path_multimode_solver.cpp` from a multi-thousand-line coordinator
  to a compact coordinator.
- Extracted modules for:
  - geometry primitives
  - note formatting
  - temporal support
  - input validation
  - region candidate assembly
  - reconstruction/refinement
  - landmark options/output/emission/partitioning
  - landmark segment and temporal solving
  - visible-probe orchestration
  - mask-channel diagnostics
  - recombined temporal candidates
  - final solver-note builders
- Added a multimode refactor policy to enforce coordinator size, pure-helper
  diagnostics/progress boundaries, and `.hpp` headers.

### Path frame fit

- Reduced `path_frame_fit.cpp` from a large monolith to a compact coordinator.
- Extracted visible-outline, fraction-helper, outline-fraction expansion,
  replacement-target ladder, dense-polyline, dense-landmark,
  feature-anchor/cluster/layout, sharp-feature, cubic-span, decimation,
  outline-error, geometry-refine, at-fractions, and main-frame-fit helper
  surfaces.
- Preserved frame-fit progress/cancellation behavior, note strings, geometry
  warnings, vertex/key counts, and diagnostics ownership.

### Segment fitter

- Reduced `segment_fitter.cpp` from a large mixed implementation to a compact
  coordinator.
- Extracted segment samples/policy, Bezier reconstruction, unified spatial
  timing, shape-temporal fitting, Ceres fitting, and side-effect-free
  segment-fit diagnostic payload builders.
- Added focused tests for the split segment-fit modules and diagnostic event
  builders.

### Motion Smooth extraction

- Split Motion Smooth shape-flat and spatial behavior into compact modules for
  geometry, sample points, endpoint keys, Bezier ease guards, reduction gates,
  shape-flat topology gates, loop/adaptive behavior, quality metrics, source
  key schedules, rove schedules, tangent locks, trajectory smoothing, key
  emission, notes, and dispatch.
- Preserved Motion Smooth source-key timing behavior, endpoint preservation,
  range handling, roving/spatial tangent behavior, and facade contracts.
- Added facade-lock policy coverage to prevent accidental re-monolithization
  or hidden diagnostics/progress ownership in pure helpers.

### IO JSON extraction

- Kept `io_json.cpp` as the file IO facade.
- Extracted solver-config parsing, key-bundle parse/build, sample-bundle
  parse/build, sample timing JSON, sample value JSON, and sample-property JSON
  into focused modules.
- Preserved public `io_json.hpp` API, field names, legacy aliases,
  key-timing legacy/root behavior, value-kind strings, layer/property/sample
  parsing semantics, null-default behavior, and pure-module side-effect
  boundaries.
- Added focused C++ unit tests and a sample JSON refactor policy to keep parse
  and build responsibilities out of the facade.

### Diagnostics track

- Added opt-in JSONL diagnostics through `DiagnosticsWriter`; diagnostics are
  absent by default.
- Added pure diagnostic event builders for:
  - solve start/done/cancel lifecycle
  - parallel runtime
  - solve-mode capabilities
  - cancellation status
  - bridge-prune result and phase events
  - segment-fit diagnostic events
- Added policy coverage for:
  - diagnostics being off by default
  - lifecycle rows when diagnostics are enabled
  - cancellation diagnostics
  - diagnostic event schema stability
  - boundary ownership rules
- Diagnostics ownership rule:
  - pure math/temporal/path helpers return result structs, notes, counters, or
    event-builder payloads
  - command/lifecycle orchestration owns actual `DiagnosticsWriter` emission

### Windows and guard hardening

- Added portable temp/env test support and Windows-focused policy coverage.
- Updated quick/full Phase 3 guard behavior to run the solver test suite and
  relevant source policies consistently.
- Added worktree hygiene tooling for delegated Phase 3 integration lanes.

## 2026-05-21 to 2026-05-22 - Phase 2 Multicore And Performance Closeout

Phase 2 was measurement-first. Accepted changes had to preserve deterministic
outputs across `--jobs 1` and multicore runs, keep quality gates intact, and
improve a measured hotspot.

### Parallel runtime and jobs control

- Added solver-side jobs/runtime helpers for:
  - environment flag parsing
  - positive integer parsing
  - TBB availability checks
  - detected parallel job counts
  - `--jobs` resolution
  - hard cap handling
  - solve-scoped `tbb::global_control`
- Added unit coverage for env parsing, TBB/no-TBB behavior, hard caps, negative
  jobs rejection, and progress phase wording.
- Added progress-policy coverage for the operator-visible `parallel_config`
  surface.

### Baseline multicore result

- Build098 quick baseline compared `jobs=1` and `jobs=8` on three closeout
  cases:
  - `noodle_t2_full`: `165.156s` -> `55.561s`, `2.97x`
  - `regular_default_full`: `112.401s` -> `46.425s`, `2.42x`
  - `position_temporal_baseline`: `4.739s` -> `0.892s`, `5.31x`
- Key counts, output vertex counts, max error, and screen-space max error
  matched across `jobs=1` and `jobs=8`.
- The baseline also exposed progress-cadence gaps, which became a gate for
  future performance work.

### Accepted multicore and performance patches

- Parallel shape-flat validation:
  - parallelized large shape-flat error validation work when
    `parallel_jobs > 1`
  - workers wrote indexed local error records
  - serial reduction preserved deterministic `max_err`,
    `max_err_screen_px`, `rms_err`, and `worst_sample_idx`
  - regular hot case improved from about `46.67s` to `22.98s` at `jobs=8`
- Parallel canonical path-fit keep evaluation:
  - parallelized per-sample keep-mask error scans in stable-topology path fit
  - preserved deterministic kept-vertex selection through serial reduction
  - regular case improved from `23.01s` to `20.99s`
  - canonical path-fit progress gap dropped from `2.361s` to `0.300s`
- Replacement outline oracle cache and cutoff:
  - prebuilt repeated source/candidate outline polylines for replacement
    Shape Path oracle checks
  - cut off non-winning fitted Bezier influence-pair scans once their running
    max error could not beat the strict best
  - regular replacement hot case improved from `20.95s` to `18.98s` at
    `jobs=8`; jobs=1 improved to about `106.6s`
- Ordinary hold/linear fail-fast validation:
  - added finite cutoffs for rejected hold/linear probes
  - accepted fits still use full validation
  - `noodle_t2` improved from `29.46s` to `19.42s` at `jobs=8` and from
    `161.32s` to `127.62s` at `jobs=1`
- Shape-flat exact-match validation fast path:
  - exact valid `shape_flat` matches now report zero error without running the
    outline metric
  - `noodle_t2` improved from `18.79s` to `16.72s`
- Shape-outline cutoff inside the existing outline metric:
  - ordinary hold/linear fail-fast can stop directed outline distance once a
    finite ceiling is exceeded
  - accepted/below-cutoff fits still compute exact errors
  - `noodle_t2` improved from `16.72s` to `15.51s`

### Phase 2 telemetry and profiler work

- Added a repeatable Phase 2 profiler that records wall time, solver time,
  key counts, vertex counts, max error, progress-event gaps, phase timing, and
  jobs information.
- Added top elapsed phase reporting in `summary.csv` and Markdown reports.
- Added DP placement attribution through progress JSON:
  candidate slots, unreachable/incompatible/final-anchor slots, fit wall time,
  and reduction wall time.
- Added replacement fit-function attribution:
  oracle calls/evaluations, oracle work, outline work, relaxed endpoint
  attempts/work.
- Added ordinary `FitSegment` attribution:
  hold/linear attempts and timing, shape-temporal attempts, outline evals, and
  Ceres timing.
- Added ordinary hold/linear internals telemetry:
  evaluated units, fail-fast exits, and shape-outline wall time.
- Added bridge-prune outcome counters and per-step timing:
  fit failures, validation failures, sharp failures, accepted candidates, and
  timing split by candidate construction, temporal validation, sharp-corner
  validation, accepted/rejected buckets, initial round, and batch apply.

### Rejected or reverted Phase 2 experiments

- DP segment-count group pruning preserved output but slowed hot cases.
- DP dispatch cleanup / candidate-buffer reuse was behavior-safe but not a
  measured win.
- Progress-band source-outline caching preserved output but slowed hot cases.
- Bridge-prune chunk32 scheduling preserved headline output but slowed hot
  cases and worsened progress gaps.
- Replacement temporal outline caching inside the shared outline-error path
  preserved output but slowed hot cases.
- Bridge-prune serial inner validation preserved output but slowed both hot
  cases.
- Bridge-prune source-outline caching preserved output but produced no useful
  win and was reverted.
- Ordinary `FitSegment` source-outline caching did not improve the hot case and
  introduced a one-ulp rejected-segment diagnostic drift, so it was reverted.

### Phase 2 closeout

- Closeout ran `tools/bakerboy_phase2_profile.py --from-closeout --jobs 1,8`
  over accepted build098 state.
- Result: `6/6` runs passed with `0` failures/timeouts.
- Normalized `.bbky` output matched across `jobs=1` and `jobs=8` for
  `noodle_t2_full`, `regular_default_full`, and
  `position_temporal_baseline` after excluding runtime-only `solve_time_ms`.
- Jobs=8 closeout timings stayed aligned with the accepted cutoff baseline:
  - `noodle_t2_full`: `15.51s`
  - `regular_default_full`: `18.51s`
  - `position_temporal_baseline`: `0.918s`
- Decision: pause narrow Phase 2 speed slicing unless a fresh profile exposes
  a stronger hotspot than the rejected cache/DP hypotheses.

## 2026-05-21 - Motion Smooth, Modes, And Source-Key Timing

### Motion Smooth spatial support

- Added solver-side Motion Smooth support for unified spatial Position-style
  properties.
- Spatial Motion Smooth now smooths the actual trajectory rather than strict
  fitting the original jagged path.
- Preserves active-range endpoints, emits explicit spatial tangents, and marks
  interior control keys as roving.
- Keeps marker/workarea writeback range behavior intact, so keys outside the
  solved marker range are preserved.

### Source-key scheduled Motion Smooth

- Added AE-to-solver source-key timing export for Motion Smooth.
- When source keys exist in the active range, Motion Smooth uses those existing
  key times as the control schedule rather than generating sampled-frame
  anchors.
- Preserves first/last source-key values, smooths interior values from the
  trajectory filter, emits explicit spatial tangents, and keeps range
  preservation behavior.
- Added explicit Motion Smooth Bezier timing controls and per-property override
  support.
- Extended solver notes with source-key count, key schedule, active Bezier
  curve, and source-key timing diagnostics.

### Solve modes and cleanup modes

- Added explicit solver modes:
  - `full`
  - `temporal_only`
  - `vertex_only`
  - `motion_smooth`
- Added cleanup modes:
  - `Prompt`
  - `Auto`
  - `Off`
- Added per-property solve overrides for tolerance, screen-pixel tolerance,
  solve mode, and cleanup behavior.
- Added migration behavior so older cleanup prompt settings map safely into the
  new cleanup mode model.

## 2026-05-20 - Cleanup Pipeline And Path Quality Work

### Staged cleanup pipeline

- Split post-solve cleanup into deterministic static cleanup, temporal cleanup,
  and guarded vertex cleanup.
- Static cleanup removes redundant contiguous same-value `shape_flat` keys
  while preserving the first and last key of each static run.
- Cleanup notes now record cleanup phase and static-prune counts.
- Probe result: plain temporal-only cleanup over first-pass key anchors did not
  reduce the regular-limb tolerance-2 case by itself; deeper temporal cleanup
  requires solver-side resampling/refit.

### Source key timing preservation

- Vertex-only cleanup sample bundles now preserve:
  - interpolation
  - temporal ease
  - spatial tangents
  - continuity flags
  - auto-Bezier flags
  - roving state
- Cleanup inputs no longer collapse source timing to generic linear timing.

### Visible-outline and replacement improvements

- Added a guarded visible-outline prepass for self-overlapping `shape_flat`
  paths. It lets regular-limb-like shapes optimize against the visible
  silhouette when topology is stable, and skips safely otherwise.
- Reworked replacement path fitting target selection:
  - median and seed layouts are tried before expensive feature layouts
  - adaptive insertions moved earlier
  - already-matching target frames are accepted during Phase 2 validation
  - unsafe fast accepts remain blocked by source/error/sharp-corner validation

### Static-tail correctness

- Fixed a final static-boundary timing regression where tolerance could allow
  the solver to delay the last final-pose key past the actual source static
  frame.
- Replays confirmed noodle default, noodle tolerance 2, regular default, and
  regular tolerance 3 all anchor the last output key on source static frame
  `133`.

## 2026-05-19 - Bridge-Prune, Replay Corpus, And Cleanup Validation

### Multicore bridge-prune safety branch

- Added a safety branch for multicore post-temporal bridge pruning.
- Parallelized legal bridge-candidate evaluation with an immutable key snapshot.
- Kept worker output in pre-sized slots and left progress/log writes on the
  command thread.
- Preserved deterministic main-thread candidate selection after parallel
  evaluation.
- Added `--jobs` controls for the solver process.

### Batched bridge-prune performance checkpoint

- Validated the first visible-line case with unchanged output at 9 keys and 14
  vertices.
- Reduced the measured runtime from about 122 seconds to about 17.5 seconds on
  the checkpoint run.
- Reduced bridge-prune work from 31 rounds and 822 attempts to 2 rounds and 89
  attempts.
- Kept the optimization scoped to solver-side bridge-prune compute, not AE
  communication or AE internals.

### Visible-outline and replacement fast paths

- Added the general visible-outline prepass for shape-flat self-overlap cases.
- Made the replacement target ladder auto-feasible and moved median/seed
  ordering before feature matching.
- Tightened fast vertex-preference acceptance to keep lower-vertex outputs from
  losing the target-region error envelope.
- Preserved the 9-key / 14-vertex spot-check output while identifying remaining
  corner-gate fallback cost.

### Solver cleanup and correctness fixes

- Added a prompted cleanup pass before writeback using the accepted first-pass
  key set.
- Guarded vertex pruning and path temporal fitting so cleanup only applies when
  verification still passes and key or vertex count improves.
- Added final static-tail key cleanup to collapse identical suffix runs after
  path and vertex pruning.
- Fixed duplicate-terminal cleanup and added topology guards so mixed-topology
  outputs are skipped instead of rewritten unsafely.
- Added tolerance-scaled vertex pruning and removed a hard cap that previously
  limited useful removals.
- Added abort/cache/fixed-topology vertex-pruning fixes, including cancel-file
  plus process termination behavior for active solves.

### Corpus and replay infrastructure

- Renamed the command-line solver identity to `bbsolver` and added the benchmark
  corpus convention used by later standalone packaging work.
- Added a repo-local live-run artifact convention under
  `artifacts/bakerboy/corpus/live_runs/<request>/`.
- Added replay and benchmark tooling so solver changes could be validated
  against archived bundles and logs.
- Moved expensive shape endpoint diagnostics behind Phase 2 debug verification
  to avoid post-apply diagnostic chug in normal runs.

### Segment ranges and long-solve behavior

- Added segment-marker bake ranges so long compositions could be solved and
  applied in bounded ranges.
- Fixed segmented apply verification fallback behavior.
- Allowed long position bakes while preserving ranged keys.
- Removed the hard wall-clock timeout for long solves when solver progress is
  still arriving.

## 2026-05-18 - Solver Strategy Search, Progress, And Acceptance Baselines

### Landmark subpath and multimode probes

- Added the landmark subpath writeback probe and then gated landmark subpath
  emission so it stayed explicit and controlled.
- Sparsified landmark subpath output, refined landmark anchors, added temporal
  DP for landmark subpaths, and partitioned landmark subpaths by key count.
- Bounded relaxed landmark influence search and added diagnostics for dense
  subpath singleton runs, semantic split lower bounds, outlier subpath
  partitioning, and landmark segment merge pressure.
- Proved the masked-channel representation gap and recombined regional path
  timing only where the solver could preserve the required timing semantics.

### Acceptance scoreboard and negative-result tracking

- Added the acceptance scoreboard tooling used to compare solver strategies
  against live and replayed bundles.
- Recorded rejected approaches such as manual seed overlap and semantic refit
  variants so later work would not repeat unproductive paths.
- Established that contiguous visible-channel selection was insufficient for the
  difficult cases under review.
- Redirected next-step solver work toward bounded structured non-contiguous or
  hybrid full-visible searches instead of more tuning.

### Bounded path-specific shape solving

- Added bounded path-specific shape solves so high-dimensional `shape_flat` path
  cases no longer relied on unbounded Ceres searches.
- Added path-specific flags and bounded probe behavior that kept representative
  probes around a few seconds.
- Preserved the conclusion that no user-testable baking improvement had landed
  yet until representation, composition, or temporal segmentation changed.

### Placement strategy diagnostics

- Added path optimization diagnostics and placement-strategy comparison.
- Kept the default dynamic-programming placement path while exposing
  `forward_longest_span` as an opt-in comparison strategy.
- Added accuracy-gate diagnostics and AE progress surfaces for long-running path
  strategy experiments.
- Rejected the strategy sweep as a default change after it exposed worse or
  non-improving behavior.

### Replacement gating and vertex pruning foundations

- Added guarded replacement behavior so replacement candidates could not be
  accepted when key count was worse than the temporal baseline.
- Added solver-inner progress events across replacement target frame fitting,
  fraction-layout testing, replacement temporal DP placement, and fallback DP
  placement.
- Added early vertex-pruning and duplicate-terminal cleanup foundations that
  later became the Phase 2 cleanup and bridge-prune work.

### Cooperative solver runner and cancellation

- Added the cooperative AE-side solver runner flow that launches the solver
  through a runner script, returns panel control, and polls solver progress
  output.
- Added cancel-path foundations for long solves so the panel could interrupt
  active solver work instead of waiting for process completion.
- Preserved solver-side progress reporting as the mechanism that later enabled
  long watchdog limits and Phase 2 performance validation.

## 2026-05-17 - Path Replacement, Temporal Influence, And Multimode Foundations

### Shape-path verification and temporal ease

- Changed shape-path verification to use outline distance instead of only flat
  coordinate comparison.
- Constrained shape-path temporal ease speed so fitted path keys stayed aligned
  with AE-observed path timing.
- Added tests for shape outline error behavior and path temporal fitting
  boundaries.

### Guarded replacement path fitting

- Added guarded path replacement fitting through the early
  `path_fit_pipeline` and `path_frame_fit` surfaces.
- Added stable path fraction fitting, stable path fraction coherence, multiple
  path fraction seeds, and bounded path fraction expansion.
- Accepted the first stage of path vertex reduction only behind verification
  and tolerance gates.
- Added budgeted and low-topology noodle path bake paths for difficult
  `shape_flat` cases.

### Replacement temporal and morph-oracle work

- Added the replacement path morph oracle and the first replacement temporal
  solver surface.
- Added path temporal validation and path progress-band oracle coverage.
- Added shape-path temporal influence fitting and relaxed path endpoint keys.
- Added policy coverage so path-panel source contracts followed the new
  replacement and temporal surfaces.

### Multimode precheck foundation

- Added bounded path multimode precheck support.
- Introduced the early multimode solver path and replacement temporal options
  needed for later landmark, visible-region, and multimode refactor slices.
- Kept the work guarded so ordinary path solves could fall back to the existing
  temporal/replacement path when multimode checks were not applicable.

## 2026-05-16 - Path Shape Bakes And Canonical Path Fitting

### Host path-shape sampling support

- Enabled panel-originated path shape bakes.
- Added AE path shape probe tooling and path-panel policy coverage for the
  sampler/writeback contract.
- Improved AE path bake support by adding shape lookup, path sampling,
  writeback, verification, and path keyframe dump tooling.
- Extended the sample protocol and solver parsing to carry the path metadata
  needed by shape-flat path solves.

### Solver-side canonical path fitting

- Added the first solver-side canonical path fitting pass behind explicit path
  fitting controls.
- Added `path_fit` source, path-fit tests, and documentation for canonical
  path fitting behavior.
- Preserved strict validation: fitted path output could only replace the source
  outline when it stayed within the configured tolerance.

### Parented transform flattening context

- Added parented transform flatten bake support on the host side so solver
  inputs could represent selected properties in comp space when needed.
- Added policy and probe coverage around parent flattening to keep the solver
  input contract explicit.

## 2026-05-15 - Unified Spatial Fidelity And Progress Portability

### Windows-safe progress writes

- Gated solver progress writes for Windows so progress reporting would not
  rely on Unix-only file descriptor assumptions.
- Preserved the JSON progress stream contract while keeping host portability in
  view.

### Unified spatial position fidelity

- Preserved unified spatial position bakes by adjusting DP placement and
  segment fitting behavior.
- Forced frame sampling for unified Position where needed by the host contract.
- Matched AE unified spatial speed timing through a new unified-spatial solver
  surface, verifier changes, and replay support.
- Added focused tests for AE curve and segment fitter behavior around unified
  spatial timing.

### Rig-gap and regression validation

- Added rig-gap verification and replay tooling.
- Expanded CI and smoke validation around solver/AE agreement, regression
  reports, and edge behavior.

## 2026-05-14 - Inception, Protocol, CLI, And Solver Core

### Repository and protocol bootstrap

- Created the repository with protocol lock documents, architecture docs, and
  FlatBuffers schemas for `SampleBundle` and `KeyBundle`.
- Established the solver contract around schema-governed input and output,
  no per-frame AE writeback, explicit tangents/ease, and `L∞` acceptance.

### First solver executable and CLI contract

- Added the first solver skeleton, CMake wiring, JSON IO, and AE curve
  evaluator.
- Added the AE harness export path and the initial `SOLVER_CLI` spawn contract
  for solving, verifying, dumping bundles, and reporting version/help.
- Added fixture inspection and pure-Python DP reference tooling for
  cross-checking solver behavior.

### DP, fitting, metrics, and verifier

- Added the DP placer implementation and then linked it to the C++ solver.
- Added segment fitting, error metrics, verifier support, and Ceres linkage.
- Added screen-space and spatial fitting support, separated dimensions, and
  path-aware value handling.
- Added regression baselines across fixture sets, tolerance sweeps, edge tests,
  and diff tooling.

### Progress, decomposition, and path reassembly

- Added solver progress events and tighter fitting behavior.
- Added the first path decomposition implementation and wired it into the
  solver behind explicit controls.
- Added full-range DP candidates, constant-value short-circuit behavior, and
  smarter diff metrics.
- Added flat-anchor path reassembly and a CMake-backed path decomposition test
  target.
- Added separated-position bake validation and AE-faithful offline replay
  tooling.

## Validation Gates Used Across These Changes

Common solver gates recorded through the changelog period:

- Focused C++ target builds for changed modules.
- Focused unit binaries for changed behavior.
- Full `cmake --build solver/build`.
- Full `ctest --test-dir solver/build --output-on-failure`.
- `bash tests/run_unit_tests.sh` where applicable in older checkpoints.
- `python3 tools/p3_refactor_guard.py --tier quick --no-build`.
- Full Phase 3 guard for major commit windows.
- All-solver clangd LSP sweep with zero diagnostics once introduced.
- Policy harnesses for:
  - solver layout
  - public-header dependencies
  - diagnostics boundary and schema
  - progress JSON contracts
  - path panel/source-contract expectations
  - motion-smooth facade boundaries
  - multimode boundaries
  - temporal-refit boundaries
  - sample JSON boundaries
  - clangd config and compile database behavior
  - Windows portability
- Phase 2 profile gates comparing `jobs=1` and multicore runs, with normalized
  output parity after excluding runtime-only `solve_time_ms`.
