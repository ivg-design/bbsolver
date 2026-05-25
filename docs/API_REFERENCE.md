# bbsolver API Reference

Concise reference for building, invoking, and integrating the standalone
`bbsolver` command-line solver.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/bbsolver --version
```

With tests:

```sh
cmake -S . -B build -DBBSOLVER_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure -j 8
```

## Binary

Preferred standalone install paths:

| OS | Path |
|---|---|
| macOS user | `~/.bbsolver/bin/bbsolver` |
| macOS system | `/usr/local/bin/bbsolver`, `/opt/homebrew/bin/bbsolver` |
| Windows user | `%APPDATA%\bbsolver\bin\bbsolver.exe` |
| Windows system | `%ProgramFiles%\bbsolver\bin\bbsolver.exe` |

Set `BBSOLVER_BIN` when an integration needs an explicit binary path.

## Commands

```sh
bbsolver solve <input.bbsm.json> <output.bbky.json> [options]
bbsolver verify <output.bbky.json> <input.bbsm.json>
bbsolver dump <bundle.bbsm.json|bundle.bbky.json>
bbsolver --version
bbsolver --help
```

The CLI accepts JSON bundle files only. FlatBuffers schemas are present
in [`../protocol/`](../protocol/) as schema references and for generated
C++ types, but binary `.bbsm` / `.bbky` CLI IO is not part of the
process contract.

Host examples that need a minimal JSON file shim can use
[`../examples/after-effects/bbsolver-json-shim.jsx`](../examples/after-effects/bbsolver-json-shim.jsx).
It provides validated `writeSampleBundleJson()`, `readSampleBundleJson()`, and
`readKeyBundleJson()` helpers for ExtendScript-style integrations.

## `solve` Options

| Option | Type | Default | Meaning |
|---|---:|---:|---|
| `--tolerance T` | double | `0.5` | Property-unit `L_inf` error budget. |
| `--screen-px P` | double | `0` | Screen-space error budget; `0` disables. |
| `--jobs N` | int | `0` | Parallel jobs; `0` means auto. |
| `--progress-fd FD` | int | unset | Writes newline JSON progress events to this FD. |
| `--diagnostics PATH` | path | unset | Writes optional diagnostics JSONL. |
| `--cancel-file PATH` | path | unset | Solver exits with code `5` when this file appears. |
| `--decompose-paths` | flag | off | Enables path-channel decomposition. |
| `--fit-canonical-paths` | flag | off | Enables canonical Shape Path fitting. |
| `--fit-replacement-paths` | flag | off | Enables fitted Shape Path replacement. |
| `--emit-landmark-subpaths` | flag | off | Emits diagnostic landmark subpaths. |
| `--solve-mode MODE` | string | `full` | Selects solve route policy. |
| `--verbose` | flag | off | Enables additional console output. |

`MODE` values:

| Mode | Use |
|---|---|
| `full` | Normal temporal solve plus eligible path optimization. |
| `temporal_only` | Key reduction without path topology changes. |
| `vertex_only` | Path vertex/reduction passes without temporal fitting. |
| `motion_smooth` | Motion Smooth on source-key schedule. |
| `motion_path_smooth` | Position-style motion-path smoothing. |

## Exit Codes

| Code | Meaning |
|---:|---|
| `0` | Command completed. For `solve`, inspect per-property `converged` flags before applying results. |
| `1` | Runtime, IO, format, or unsupported schema error. |
| `2` | Usage error such as missing arguments. |
| `3` | `verify` completed and at least one property failed validation. |
| `5` | Cancelled solve via `--cancel-file`. |

## SampleBundle Input

Top-level JSON:

```json
{
  "_schema": "samples",
  "schema_version": 1,
  "request_id": "unique-request-id",
  "comp": { "fps": 24.0, "duration_sec": 1.0 },
  "properties": [
    {
      "property": {
        "id": "Layer/Transform/Opacity",
        "kind": "Scalar",
        "dimensions": 1
      },
      "samples": [
        { "t_sec": 0.0, "v": [0.0] },
        { "t_sec": 1.0, "v": [100.0] }
      ]
    }
  ],
  "config": {}
}
```

`_schema` must be `"samples"`, and `schema_version` must equal the supported
version declared in `include/bbsolver/io/schema_contract.hpp`.
`solve` rejects missing, swapped, unsupported, empty, or structurally malformed
SampleBundle markers before writing output.

Every property must declare a positive integer `property.dimensions`.
For each sample, `v.length` must equal `property.dimensions *
samples_per_frame`; with ordinary frame-center sampling,
`samples_per_frame` is `1`, so `v.length == property.dimensions`.
The exception is raw variable-topology `shape_flat` input:
when `units_label` is `"shape_flat"` and `shape_variable_topology` is `true`,
each `v` must be a valid flat path vector whose length does not exceed
`property.dimensions`, which represents the maximum sampled flat-vector length.

`comp` fields:

| Field | Required | Notes |
|---|:-:|---|
| `fps` | yes | Sample cadence. |
| `duration_sec` | yes | Source comp or sampled range duration. |
| `width`, `height` | recommended | Required for meaningful screen-px tolerances. |
| `pixel_aspect` | optional | Defaults to `1.0`. |
| `work_area_start_sec`, `work_area_end_sec` | recommended | Host-sampled range. |

Property entry:

```json
{
  "property": {
    "id": "Layer/Transform/Position",
    "kind": "TwoD_Spatial",
    "dimensions": 2,
    "is_spatial": true,
    "units_label": "px"
  },
  "t_start_sec": 0.0,
  "t_end_sec": 1.0,
  "samples_per_frame": 1,
  "samples": [
    { "t_sec": 0.0, "v": [100.0, 200.0] },
    { "t_sec": 0.041666667, "v": [104.0, 202.0] }
  ]
}
```

`property.kind` values:

| Kind | Sample vector |
|---|---|
| `Scalar` | 1 number |
| `TwoD` | 2 numbers |
| `ThreeD` | 3 numbers |
| `TwoD_Spatial` | 2 numbers plus spatial key metadata support |
| `ThreeD_Spatial` | 3 numbers plus spatial key metadata support |
| `Color` | 4 numbers |
| `Custom` + `units_label="shape_flat"` | Shape Path flat vector |

Optional `sample.key_timing`:

```json
{
  "interp_in": "Bezier",
  "interp_out": "Bezier",
  "temporal_ease_in": [{ "speed": 0.0, "influence": 33.3 }],
  "temporal_ease_out": [{ "speed": 0.0, "influence": 33.3 }],
  "spatial_in": [0.0, 0.0],
  "spatial_out": [0.0, 0.0],
  "roving": false
}
```

## SolverConfig

`config` fields commonly set by integrations:

| Field | Type | Default | Notes |
|---|---:|---:|---|
| `tolerance` | double | `0.5` | Property-unit error budget. |
| `tolerance_screen_px` | double | `0` | Screen-px error budget. |
| `parallel_jobs` | int | `0` | Overridden by `--jobs`. |
| `solve_optimization_mode` | string | `full` | Same values as `--solve-mode`. |
| `allow_hold` | bool | `true` | Allow hold spans. |
| `allow_linear` | bool | `true` | Allow linear spans. |
| `allow_bezier` | bool | `true` | Allow Bezier spans. |
| `allow_path_replacement_fit` | bool | `false` | Enables fitted Shape Path replacement. |
| `allow_path_spatial_fit` | bool | `false` | Enables canonical Shape Path fitting. |
| `motion_path_smoothing_tolerance` | double | `3.0` | Unitless fairing strength, clamped to `1.0`-`32.0`. |
| `motion_path_accuracy_tolerance` | double | `1.5` | Property units; Position uses comp px. |
| `motion_path_preserve_bounds` | bool | `false` | Constrains Motion Path Smooth to the original global path footprint. |
| `motion_path_bounds_tolerance` | double | `0.0` | Per-side global bounds tolerance in property units; Position uses comp px. |
| `motion_path_preserve_sharp_points` | bool | `true` | Locks sharp motion-path reversals. |
| `motion_path_respect_keyed_frames` | bool | `false` | Locks original source key times. |

CLI flags override the equivalent config fields for tolerance, screen-px, jobs,
solve mode, and path-fit enablement.

## Shape Path Encoding

Use `kind="Custom"` and `units_label="shape_flat"`.

```text
v = [closed_flag, n_vertices,
     vx0, vy0, in_x0, in_y0, out_x0, out_y0,
     vx1, vy1, in_x1, in_y1, out_x1, out_y1,
     ...]
dimensions = 2 + 6 * n_vertices
```

Stable topology: every sample has the same `n_vertices` and closed flag.

Variable topology: samples may have different `n_vertices`, but the closed flag
must stay stable. These metadata fields may be emitted by hosts for traceability:

```json
{
  "property": {
    "kind": "Custom",
    "units_label": "shape_flat",
    "dimensions": 314,
    "shape_variable_topology": true,
    "shape_canonical_method": "shape_flat_raw_variable",
    "shape_max_vertex_count": max_vertices
  },
  "config": {
    "allow_path_replacement_fit": true
  }
}
```

`shape_variable_topology`, `shape_canonical_method`, and
`shape_max_vertex_count` are advisory metadata: the JSON parser tolerates
them but does not store them in `PropertyInfo` or use them to route the
solve. The solver derives path topology from the encoded `v` arrays and
uses `allow_path_replacement_fit` plus `--fit-replacement-paths` to
enable replacement fitting. If no fitted topology is accepted, the
solver falls back to raw frame keys or safe flat behavior and records a
topology note.

## KeyBundle Output

Top-level JSON:

```json
{
  "_schema": "keys",
  "schema_version": 1,
  "request_id": "unique-request-id",
  "property_results": [
    {
      "property_id": "Layer/Transform/Opacity",
      "dimensions": 1,
      "keys": [
        { "t_sec": 0.0, "v": [0.0] },
        { "t_sec": 1.0, "v": [100.0] }
      ],
      "max_err": 0.0,
      "max_err_screen_px": 0.0,
      "segments": [],
      "converged": true,
      "notes": ""
    }
  ],
  "solver_version": "bbsolver <version>",
  "solve_time_ms": 0.0,
  "total_keys": 2,
  "total_samples_input": 2
}
```

`_schema` must be `"keys"`, and `schema_version` matches the supported version
declared in `include/bbsolver/io/schema_contract.hpp`. `verify` rejects
missing, swapped, or unsupported KeyBundle/SampleBundle markers before
replaying keys.

Each `property_results[]` entry must include a non-empty `property_id`, a
positive integer `dimensions`, and a `keys` array. Every `keys[].v` length must
equal `property_results[].dimensions`. The `keys` array is non-empty for
converged results. It may be empty only when `converged` is `false`, such as a
cancelled partial result.

Property result:

```json
{
  "property_id": "Layer/Transform/Position",
  "dimensions": 2,
  "keys": [
    {
      "t_sec": 0.0,
      "v": [100.0, 200.0],
      "interp_in": "Bezier",
      "interp_out": "Bezier",
      "temporal_ease_in": [{ "speed": 0.0, "influence": 33.3 }],
      "temporal_ease_out": [{ "speed": 0.0, "influence": 33.3 }],
      "spatial_in": [0.0, 0.0],
      "spatial_out": [0.0, 0.0],
      "roving": false
    }
  ],
  "max_err": 0.0,
  "max_err_screen_px": 0.0,
  "segments": [],
  "converged": true,
  "notes": ""
}
```

Integrations should match `property_results[].property_id` to the source
property id, then write `keys[].t_sec`, `keys[].v`, interpolation, ease, spatial
tangents, continuity flags, and roving flags into the host animation model.

Do not apply a result with `converged=false` as a successful solve.

## Progress Events

With `--progress-fd FD`, the solver writes newline-delimited JSON to the
file descriptor. Hosts typically pass the write end of a pipe and parse
events as they arrive. Each line is a complete JSON object.

### Common fields

Every progress event includes:

| Field | Meaning |
|---|---|
| `event` | Machine-readable event id (see catalogue below). |
| `phase` | Human-readable phase label suitable for UI. |
| `progress` | Normalized solve progress in `[0, 1]`. Monotonically non-decreasing across a single solve. |

Property-scoped events additionally include:

| Field | Meaning |
|---|---|
| `id` | Property id, matches `properties[].property.id` in the SampleBundle. |
| `i`, `n` | Property index (0-based) and total property count. |
| `display_name`, `layer_path`, `units_label` | Echo of `PropertyInfo` for log lines. |
| `samples` | Sample count on the property. |
| `K` | Key count on completion events. |
| `max_err` | Error value when a phase reports one. |

Per-phase events may add phase-specific fields (e.g. `placement_step`,
`placement_total`, `segments_tried` for temporal placement). Treat
unknown fields as additive.

### Event catalogue

`event` ids follow `<scope>_<verb>` snake_case. Stable consumers should
dispatch on `event` and pass through unknown ids without erroring.

**Lifecycle (whole-solve, single occurrence each):**

| `event` | Emitted | Notes |
|---|---|---|
| `solve_start` | Once at solve entry | Includes `request_id`, `properties`, `solve_optimization_mode`. |
| `parallel_config` | After TBB scope opens | Reports resolved jobs and TBB availability. |
| `done` | Once after every property finishes | `progress` settles at 1.0. |

**Per-property lifecycle (one set per property):**

| `event` | Emitted | Notes |
|---|---|---|
| `property_prepare` | Before solve starts | Includes `samples`, `units_label`, classification. |
| `property_start` | Solve begins for this property | |
| `property_done` | Solve finishes for this property | Includes `K`, `max_err`, `converged`. |

**Per-property phase events** (subset; the full set is emitted by
`src/solve/`, `src/path/`, and `src/routing/` —
treat as additive):

| `event` | Phase |
|---|---|
| `visible_outline_prepass`, `visible_outline_prepass_skipped` | Path geometry pre-pass for spatial-topology routes. |
| `path_fit`, `path_fit_start` | Path-fit pipeline. |
| `path_replacement_fit`, `path_replacement_baseline_start`, `path_replacement_baseline_done`, `path_replacement_baseline_progress`, `replacement_retry_start`, `replacement_retry_done`, `path_replacement_target_start`, `path_replacement_target_rejected`, `path_replacement_target_layout_done`, `path_replacement_target_layout_progress`, `path_replacement_target_phase2_start`, `path_replacement_target_phase2_progress`, `path_replacement_target_phase2_done`, `path_replacement_target_done` | Fitted-replacement-path ladder when `--fit-replacement-paths` is set. |
| `path_validation_start`, `path_validation_done` | Replacement-path validation outcome. |
| `path_geometry_refine_start`, `path_geometry_refine_done` | Path geometry refinement. |
| `temporal_solve_progress`, `temporal_solve_done` | Temporal DP placement (high-traffic; emits `placement_stage`, `placement_step`, `placement_total`). |
| `post_solve_vertex_reduction_start`, `post_solve_vertex_reduction_done` | Post-temporal vertex pruning. |
| `post_solve_vertex_bridge_prune_candidate`, `post_solve_vertex_bridge_prune_progress` | Bridge-prune candidate evaluation. |
| `static_key_run_collapse`, `final_static_boundary_anchor` | Static-key cleanup. |
| `temporal_refit_done` | Temporal refit pass result. |
| `vert_done`, `child_done` | Decomposed path channels finishing. |
| `landmark_subpaths`, `landmark_subpaths_start` | Output of optional landmark sub-paths when `--emit-landmark-subpaths` is set. |
| `optimization_diagnostic` | Per-property diagnostic note. |
| `candidate_progress` | DP candidate-evaluation progress under a long placement. |

The above list is concrete at the current build; the
`grep -rnE '"event",\s*"[a-z_]+"' src` family is the source of
truth for future enumeration. New events may be added in minor versions
without breaking the schema — see "Treat unknown fields as additive"
above; consumers should also treat unknown `event` ids as additive.

### Ordering and monotonicity

Within a single solve, events on the progress FD arrive in lifecycle
order: `solve_start` → `parallel_config` → for each property `property_prepare` →
`property_start` → one or more phase events → `property_done` →
eventually `done`. The `progress` field is non-decreasing across the
stream. The progress FD is written from the calling thread; with
`--jobs > 1`, per-property events for distinct properties may
interleave but events for a single property remain in order.

A trivial host consumer can ignore everything except `solve_start`,
`property_done`, and `done` and still drive a usable progress UI.

## Diagnostics

`--diagnostics PATH` writes optional JSONL diagnostics to a file. Each
line is a complete JSON object. Diagnostics target debugging, CI logs,
and post-mortem analysis. Host products should drive user-facing solve
results from the `KeyBundle` and reserve diagnostics for engineering
use.

### Diagnostics-channel event catalogue

The diagnostics channel emits a small, stable set of events documented
inside `src/diagnostics/solver_diagnostic_events.cpp`. As of
this release the channel emits:

| `event` | Emitted | Notes |
|---|---|---|
| `solve_start` | Once at solve entry | Echoes `request_id`, `schema_version`, `tolerance`, `screen_px`, advanced-flag state, input/output paths. Companion to the progress-channel `solve_start`. |
| `parallel_runtime` | Once after TBB scope opens | Reports `requested_jobs`, `resolved_jobs`, `detected_jobs`, `hard_cap`, `tbb_available`. |
| `solve_mode_capabilities` | Once after mode normalization | Reports `mode`, `allows_temporal`, `allows_vertex`, `allows_spatial_topology`, `is_motion_smooth`, `is_motion_path_smooth`, `uses_motion_smoothing`. |
| `cancellation_status` | Once before per-property work | Reports `cancel_file_set`, `cancel_file_path`, `cancel_file_exists`, `partial_write_exit_code`. |
| `post_temporal_bridge_prune_phase` | Per phase of bridge-prune | Phase-scoped progress for the post-temporal bridge-prune pass when active. |
| `post_temporal_bridge_prune_result` | Per bridge-prune outcome | Reports vertex-reduction acceptance/rejection details when active. |
| `solve_cancelled` | At cancel | Final diagnostics event when `--cancel-file` triggered cancellation; `solve_done` is not emitted in that case. |
| `solve_done` | Once at successful solve end | Reports `properties`, `solve_time_ms`, `total_keys`, `total_samples_input`. |

Every event carries `request_id` and `schema_version`. Consumers should
filter on `event` and treat unknown fields and unknown event ids as
additive.

## Verification

```sh
bbsolver verify output.bbky.json input.bbsm.json
```

Use this in integration tests after writing a new sampler or host adapter. Exit
code `0` means the KeyBundle reconstructs within tolerance for all checked
properties; exit code `3` means at least one property exceeded tolerance.
`verify` also checks bundle identity before replay: the first argument must
carry `_schema: "keys"` and the second must carry `_schema: "samples"`.
Malformed or swapped bundle arguments are format errors and exit `1`.

## C++ Surface

The supported integration surface is this document's process-boundary
and bundle-schema contract. The CMake package additionally exports
`bbsolver::bbsolver` (the CLI) and `bbsolver::core` (a static library
for in-tree embedding and tests) — see [`PACKAGING.md`](PACKAGING.md).

If you embed, prefer the three command entry points; they map
one-to-one to the CLI subcommands and use the same JSON contract
described above:

```cpp
int bbsolver::RunSolve(int argc, char** argv);
int bbsolver::RunVerifyCommand(int argc, char** argv);
int bbsolver::RunDumpCommand(int argc, char** argv);
```

The C++ symbol surface of `bbsolver::core` beyond these three entries
is source-visible but is not part of the SDK contract — package
internals (path/dp/fit/motion-smooth helpers, diagnostics writers, IO
helpers, etc.) may move between releases. Some headers inside
`include/bbsolver/` are explicitly marked internal in their leading
comment (for example `include/bbsolver/io/solver_config_io.hpp`); treat
anything so marked as private even though it lives in the include
tree. See [`DEVELOPER_GUIDE.md`](DEVELOPER_GUIDE.md) §11 for the full
boundary discussion and the rules for extending the solver in-tree.
