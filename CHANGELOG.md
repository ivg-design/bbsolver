# bbsolver Changelog

## v1.0.1 (Verifier bug fix — variable-topology shape_flat)

Fixes a verifier-side strict-dim check that rejected variable-topology
`shape_flat` bundles (noodle and blob fixtures in the paper corpus) with
`key_value_dimension_mismatch`. This is a verifier bug fix only — the
solver itself is unchanged. Running `bbsolver 1.0.1 solve` against the
same `bbsm.json` produces a bit-identical `bbky.json` (same `max_err`
to six decimal places, same key counts, same per-key vertex counts).

**The fix.** The canonical `bbsolver verify <bbky> <bbsm>` CLI used to
enforce `expected_dimensions == key_dimensions` strictly. That check is
correct for fixed-topology properties but wrong for canonicalized
variable-topology `shape_flat`: the bbsm-declared `dimensions` is
`2 + 6 × shape_max_vertex_count` while each emitted key carries a v[]
array of length `2 + 6 × vertex_count` where `vertex_count` is encoded
per-key at `v[1]`. Blob v1 in particular has per-key v[] lengths
ranging from 50 to 194 inside a single bundle.

v1.0.1 keeps the strict gate everywhere outside the verify CLI (`solve`,
`apply`, `dump`, and the main `ReadKeyBundleJson` parser are unchanged
— byte-for-byte identical behaviour for non-verify call sites). The
verify CLI now uses a separate `ReadKeyBundleJsonForVerify` path that
skips parse-time dim equality, then applies a sample-aware rule gated
on `units_label == "shape_flat" && shape_variable_topology == true`:

  * each VT key's `v[]` must be length ≥ 2,
  * `v[1]` must be a finite non-negative integer-ish vertex count,
  * `v.size() == 2 + 6 * round(v[1])`,
  * `vertex_count <= shape_max_vertex_count` when the max is known.

Malformed VT keys are reported under the new
`invalid_shape_flat_key_dimensions` reason code with a `malformed_keys`
array carrying `property_id`, `key_index`, `key_t_sec`,
`actual_length`, `expected_length`, `vertex_count`,
`max_vertex_count`, and a per-key detail string. The bbsm side is
cross-checked separately: `shape_flat` properties must satisfy
`dimensions == 2 + 6 * shape_max_vertex_count`, or, when the max is
absent, `(dimensions - 2) % 6 == 0`. Violations get
`invalid_shape_flat_sample_metadata`.

Non-shape and fixed-topology paths keep the existing
`key_value_dimension_mismatch` reason code unchanged.

### Code
- `include/bbsolver/domain.hpp` — `PropertyInfo` extended with
  `shape_variable_topology`, `shape_canonical_method`,
  `shape_canonical_vertex_count`, `shape_max_vertex_count`.
- `src/io/sample_property_io.cpp` — read VT fields from bbsm JSON
  (silently dropped pre-v1.0.1).
- `include/bbsolver/io/{io_json,key_bundle_io}.hpp` +
  `src/io/{io_json,key_bundle_io}.cpp` — new
  `RequireKeyBundleJsonRootForVerify` and `ReadKeyBundleJsonForVerify`
  entry points; main strict path unchanged.
- `src/verify/verify_dump_commands.cpp` — VT-aware dim gate,
  per-key shape rule, metadata cross-check, new reason codes.
- `CMakeLists.txt` + `include/bbsolver/app/cli_options.hpp` +
  `src/io/schema_contract.cpp` — version bumped to `1.0.1`.

### Tests
- `tests/solver_unit/test_verify_dump_commands.cpp` — seven new
  unit cases: VT acceptance with uniform short v[], VT with per-key
  varying counts (blob-v1-like), malformed v[] length, over-source-max
  vertex count, bbsm metadata inconsistency, fixed-topology shape_flat
  staying strict, non-shape (scalar) mismatch staying strict.
- `tests/solver_unit/test_verify_paper_corpus_integration.cpp` —
  new integration test walks `corpus/req-*/` and asserts
  all 16 shipped `bbky.json` bundles reach the sample-aware verifier
  without schema rejection.
- ctest: 117/117 passing on macOS arm64, macOS x86_64.

### Paper corpus
- All 11 paper-cited rows are now uniformly produced by the canonical
  `bbsolver verify (canonical CLI)` (recorded in
  `regenerator` field of each `<req>.verify.json`). Pre-v1.0.1 the
  6 variable-topology rows had AE-harness-produced verify cards
  because the canonical CLI rejected them.
- `THRESHOLD_NOTE.md` reduced from a Kind A / Kind B provenance
  split to a single-paragraph footnote documenting v1.0.0's
  verifier-side limitation.

### Independent validation
- An independent release-gating black-box validation matrix was run
  against this commit. Verdict: GREEN, no blockers under any of the 4
  decision gates (no schema rejection on valid VT shape_flat; no
  relaxation of fixed-topology or non-shape mismatch detection; no
  relaxation of the solve/apply main parser; non-shape gate still
  strict). Adversarial probes covered NaN/±Inf at v[1] (early JSON
  reject), missing v[1] (early structural reject), negative
  vertex_count (rejected as `invalid_shape_flat_key_dimensions`),
  non-integer vertex_count e.g. 4.5 (rejected likewise), `shape_flat`
  without `shape_variable_topology` carrying per-key varying lengths
  (rejected as `key_value_dimension_mismatch` — confirms the gate
  requires BOTH conditions), fixed-topology dim disagreement (rejected
  as `key_value_dimension_mismatch`), and bbsm metadata inconsistency.

### AE harness companion fix
- `examples/after-effects/bbsolver-test-harness.jsx`: verify call
  no longer tightens the per-property OK/FAIL tolerance below
  `settings.tolerance` by default. The hardcoded 0.05 px
  (`flattenParentedTolerance`) and 0.01° (`rigRotationTolerance`)
  defaults were UI-quality gates that diverged from the solver ε,
  producing the verify-cards-say-FAIL-but-paper-says-OK gap. Defaults
  now mirror `settings.tolerance`. Opt-in legacy behavior via
  `settings.coupleVerifyToSamplerPrecision = true`, mirroring the
  existing `coupleSolveToSamplerPrecision` solve-side flag.

### Release artifacts
- `bbsolver-v1.0.1-macos-arm64.tar.gz` —
  `c28ce4c9530f298b094a7ec35305ca39dc35d25f9adf2014c7f30cb69deb26ad`
- `bbsolver-v1.0.1-macos-x86_64.tar.gz` —
  `6e221df339dd89366ad38ca3a29dbf4501550d3be73d9bb863cd2eb6f3167d5e`
- `bbsolver-v1.0.1-windows-arm64.zip` —
  `776f0f8278aa76fd0fdcb3f45bc0ce1d62d68ba8eb31b833f8ced1964c067f08`
- `bbsolver-v1.0.1-windows-x64.zip` —
  `daa6448a888b18820b18971d4465c4c1668414bc2f75ad6f220a458bc7d5e24c`

The v1.0.0 release stays available unchanged for byte-level
reproduction of the original solver outputs.

## Paper artifact package — Benchmark whitepaper open-source release (no solver version bump)

This section documents the **non-solver** repository additions that
shipped together with the paper-corpus drop on the same commit as
v1.0.1. None of the items below modify solver behaviour. The current
solver release is **v1.0.1**; the changes recorded here only add
benchmark report, corpus, supplementary CSVs, AE project, baseline
runners, and reference adapters under `benchmarks/` and `examples/`.

- Added the **arxiv technical report** at
  [`benchmarks/`](benchmarks/):
  `bbsolver_arxiv_full_technical_report_v3.md` and the
  `figures/` directory.
- Added the **paper-figure reproducibility corpus** at
  [`benchmarks/corpus/`](benchmarks/corpus/):
  raw `bbsm` / `bbky` / `verify` / `progress.log` bundles for all 11 cited
  solves (~62 MB total), indexed by `corpus_manifest.csv` and a per-bundle
  README. Solver build for every bundle: `bbsolver 1.0.0` (tag `v1.0.0`).
- Added the **supplementary CSVs** at
  [`benchmarks/supplementary/`](benchmarks/supplementary/):
  per-table / per-figure CSVs, the level-playing-field comparison, the FBX
  mocap method comparison, and the SVG-decimation combined results.
- Added the **After Effects benchmark project** at
  [`benchmarks/after_effects_benchmark_project/bbSolver_benchmarking.aep`](benchmarks/after_effects_benchmark_project/bbSolver_benchmarking.aep)
  containing every fixture cited in the paper: DUIK humanoid rig, ant
  hexapod rig, FK noodle setup, v1-v6 blob expression lineage, and the
  CS1/CS2 path fixtures.
- Added the **standalone-Python ports of two open-source baselines** at
  [`benchmarks/external_runners/`](benchmarks/external_runners/):
  `joosten_reducer/` (Paper.js Schneider-style cubic-Bezier reducer port of
  `robertjoosten/maya-keyframe-reduction`, Apache 2.0) and
  `toolchefs_reducer/` (LGPL iterative RDP variant from `Toolchefs/keyReducer`).
  Both preserve upstream provenance files (`fit_original.py`,
  `keyReducerCmd_original.cpp`, `THIRD_PARTY_LICENSE`) for diffability.
- Added a **direct FBX-to-FBX roundtrip** reference adapter at
  [`examples/blender/fbx_bbsolver_roundtrip.py`](examples/blender/fbx_bbsolver_roundtrip.py)
  (plus `fbx_to_bbsm.py`, `bbky_apply_to_fbx.py`, and
  `FBX_BEZIER_TANGENTS.md`). Documents the Blender 4.5 FBX-importer
  limitation that linearizes all imported keyframe tangents (`import_fbx.py:715-716, 878`).
- AE harness (`examples/after-effects/bbsolver-test-harness.jsx`): clarified
  per-kind default tolerances (`DEFAULT_TOLPX = 1.0` for position
  screen-px, `rigRotationTolerance = 0.01` for rotation degrees) and the
  `verifyRoundTrip` setting that gates the post-solve AE re-evaluation.
- `docs/PATH_HANDLING.md`: aligned terminology with the paper's
  "in-loop tolerance-bounded candidate rejection" framing for the
  inner acceptance gate.

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



## Architectural history

The notes below summarise the structural and architectural progress that led
to `1.0.0`. They focus on the engineering decisions that shape the public
surface today rather than on the day-to-day work log.

### Repository and build system

- The standalone C++ package is laid out as `include/bbsolver/<area>/…`
  public headers, `src/<area>/…` private implementations, and
  `tests/solver_unit/…` unit tests. This split is enforced by source-level
  policy guards under `tests/policies/`.
- CMake configures the core via `GLOB_RECURSE` on the source tree so new
  per-area subdirectories are picked up automatically. `bbsolver_core` is
  the embeddable library target and `bbsolver` is the CLI.
- Dependencies (Ceres, Eigen, oneTBB, FlatBuffers, nlohmann/json) are
  pulled via `FetchContent` with hash-locked tarball archives shipped under
  `third_party/archive/`. Offline builds are supported through the
  `BBSOLVER_FORCE_THIRD_PARTY_ARCHIVES` switch.
- Public headers were migrated from `.h` to `.hpp` and re-exported through
  façade headers so downstream consumers continue to see a single include
  per area.

### Solver core

- The solver is organised as a candidate-fit-validate pipeline:
  classification → mandatory key seeding → candidate generation →
  interpolation fitting → validation → pruning/refinement → KeyBundle
  emission.
- Temporal key placement uses dynamic programming with feasibility
  delegated to the segment fitter. When no feasible segmentation exists at
  the configured tolerance and gap limit, the implementation falls back to
  per-sample anchors.
- Segment fitting tries hold, linear, shape-temporal Bezier, Hermite
  Bezier, and Ceres Bezier candidates in turn, using `ComputeError` and
  the acceptance policy before returning feasibility.

### Path handling

- `shape_flat` properties serialise a closed flag, a vertex count, vertex
  positions, and in/out tangent data as a flat numeric vector. The error
  metric densifies the segments into polylines and computes outline
  distance.
- Constant-topology paths share the temporal-key pipeline with scalar/
  vector properties; vertex layout stays uniform across the keyed range.
- Variable-topology paths run through a separate path-replacement
  compressor that proposes a topology-aware sparse representation before
  the temporal pipeline. A bridge-prune pass removes redundant keys that
  no longer contribute after replacement is accepted.
- A multimode precheck explores per-region replacement when the budget
  allows it; rejected proposals fall back to the host-compatible
  uniform-topology writeback.

### Motion smoothing

- The motion-smooth solver is split into independent façades over
  shape-loop processing, schedule construction, and key emission, so
  downstream consumers see one include per phase.
- Spatial trajectory smoothing produces endpoint keys with Bezier ease
  control derived from the sampled trajectory. Source-pose constraints
  let the host pin individual frames against the smoothed result.
- A separate shape-flat closed-loop branch handles topology-stable closed
  paths, with a topology gate that screens incoming frames for stable
  vertex counts before entering the loop.

### Validation

- The solver runs an in-loop L∞ validation gate on every accepted
  candidate: re-sample the candidate at the property's source times and
  compare against the input. Any property over budget rejects the
  candidate.
- Post-solve `bbsolver verify` re-runs the same comparison against the
  shipped `bbky.json` + `bbsm.json` pair from the canonical CLI. The
  v1.0.1 verifier additionally accepts canonicalised variable-topology
  `shape_flat` bundles via a sample-aware per-key shape check.
- Host round-trip evaluation (the AE harness writes keys back into After
  Effects, then re-samples playback) is the third independent verification
  layer used for AE-side fixtures.
- A 60-invocation determinism audit (three fixtures × {serial, multi-
  threaded} × 10 repetitions) records bit-identical normalised output
  hashes for every condition; raw rows ship at
  `benchmarks/supplementary/determinism_audit.csv`.

### Performance

- The parallel runtime distributes per-property solves across worker
  threads with bit-identical output across thread counts. `--jobs 0`
  uses the hardware-derived default; `--jobs 1` forces serial execution
  for reproducibility comparisons.
- Cooperative cancellation through `--cancel-file` lets host adapters
  stop a long solve without losing already-validated property output.
- A bounded path-specific shape solver caps the explored vertex space
  on heavy bundles so worst-case wall-clock stays predictable.

### Host integration

- The host-agnostic contract is JSON: `SampleBundle` in,
  `KeyBundle` out. Both schemas are documented under `schemas/` and
  carry FlatBuffers references under `protocol/` for future binary
  payloads.
- The After Effects reference adapter under `examples/after-effects/`
  samples AE properties, runs `bbsolver` as a subprocess, writes the
  resulting keys back, and optionally re-samples playback for AE round-
  trip verification.
- The Blender FBX reference adapter under `examples/blender/` runs the
  same contract host-less: an FBX action becomes a `SampleBundle`,
  bbsolver emits a `KeyBundle`, and a writeback script lays the keys
  back into the FBX. The adapter documents the Blender 4.5 FBX importer's
  linearisation of imported keyframe tangents as a known limitation.
- Parented transform flattening lets adapters serialise the
  sampled-comp state in a host-agnostic way, including pixel-aspect,
  layer transforms, and source-key timing alignment.

### CLI and public API

- The CLI surface is stable: `--tolerance`, `--screen-px`, `--jobs`,
  `--solve-mode`, `--diagnostics`, `--cancel-file`, and the path/shape
  experimental flag set (`--decompose-paths`, `--fit-canonical-paths`,
  `--fit-replacement-paths`, `--emit-landmark-subpaths`). JSON is the
  only public IO format.
- The verify subcommand emits stable reason codes:
  `key_value_dimension_mismatch` (strict path), and as of v1.0.1
  `invalid_shape_flat_key_dimensions` and
  `invalid_shape_flat_sample_metadata` for variable-topology shape_flat
  rows. Malformed VT keys ship per-key diagnostics in a
  `malformed_keys` array.
- The CMake package exports `bbsolver::bbsolver` (the CLI) and
  `bbsolver::core` (the static library) for in-tree embedding and tests.

### Reproducibility infrastructure

- `benchmarks/corpus/` ships raw `bbsm` / `bbky` / `verify` / `progress.log`
  bundles for every benchmark-cited solve, indexed by
  `corpus_manifest.csv`.
- `benchmarks/supplementary/` ships one CSV per quantitative table or
  figure, plus a `determinism_audit.csv` and an aggregate per-run +
  summary view of the production-corpus statistics.
- `benchmarks/scripts/` is a deterministic Python pipeline: a smoke
  reproduction script, the figure / table generators, a canonical-CLI
  re-verify runner, and a portability helper that resolves the
  `bbsolver` binary, the Blender executable, and the corpus root through
  CLI / env / OS-default lookup.
