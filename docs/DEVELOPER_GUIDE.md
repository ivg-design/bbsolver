# bbsolver Developer Guide

A developer-facing reference for `bbsolver`: a standalone C++ spatiotemporal
optimization engine for animation data. It fits sparse keyframes from dense
samples, optimizes animated paths through temporal fitting and vertex
decimation, and smooths keyframed layer motion paths while preserving explicit
accuracy and constraint rules.

The repository also includes a small ExtendScript/ScriptUI After Effects test
harness. The harness is not a product shell; it is a practical integration
example that samples AE properties into `SampleBundle` input, invokes the
solver, receives `KeyBundle` output, and writes solved keys back into AE.

This guide is the orientation for integrators, third-party consumers, and
contributors. It documents the public API, data contracts, folder layout,
architectural rules, extension points, and validation surfaces.

If you came here looking for the concise integration contract, see
[`API_REFERENCE.md`](API_REFERENCE.md). This document is the architectural
overview for developers changing the solver implementation.

---

## 1. What bbsolver does

`bbsolver` is an offline command-line application that ingests a
`SampleBundle` (dense time/value samples of one or more animation properties)
and produces a `KeyBundle` (sparse keyframes that interpolate within a target
error tolerance). It is designed as a self-contained C++17 solver core with no
runtime dependency on After Effects. AE integration is demonstrated by the
included ScriptUI test harness, but the solver contract is host-agnostic.

Two output guarantees:

- **Behavior-neutral reduction**: every key produced reconstructs to within
  the configured `L_inf` tolerance against the source samples for every
  channel (modulo the noted advanced modes).
- **Stable contracts**: `SampleBundle` and `KeyBundle` evolve under
  explicit schema versions and the CLI accepts JSON bundles only; the
  property output bundle is the stable host-facing surface.

## 2. Quick orientation

```
bbsolver/
├── CHANGELOG.md            # Standalone solver changelog
├── include/bbsolver/        # Public headers (the API surface)
│   ├── domain.hpp           # Core value/key/sample contracts
│   ├── app/                 # CLI options, entry-point glue
│   ├── solve/               # solve command, lifecycle, completion, output
│   ├── routing/             # Property classification + solve route picker
│   ├── io/                  # JSON SampleBundle/KeyBundle/SolverConfig
│   ├── progress/            # ProgressWriter + cancellation
│   ├── diagnostics/         # DiagnosticsWriter + event builders
│   ├── runtime/             # TBB scope + env-var helpers
│   ├── samples/             # Sample timing/value helpers, source preservation
│   ├── path/                # Path-property machinery
│   │   ├── temporal/        # Path temporal validation/progress/influence
│   │   ├── frame_fit/       # Per-frame Shape Path fitter
│   │   ├── multimode/       # Multi-region path solver
│   │   ├── dense/           # Dense polyline + landmarks
│   │   ├── bridge_prune/    # Post-temporal bridge-prune passes
│   │   ├── decompose/       # Path decomposition / per-channel solve
│   │   ├── config/          # Path-specific solver config helpers
│   │   ├── reduction/       # Vertex/bridge reduction post-solve
│   │   ├── replacement/     # Fitted-path replacement orchestrator
│   │   ├── fit/             # path_fit pipeline glue
│   │   └── geometry/        # Path geometry refinement, outline error
│   ├── motion_smooth/       # Motion Smooth and Motion Path Smooth solvers
│   ├── temporal/refit/      # Post-solve temporal refit
│   ├── replacement_temporal/# Replacement temporal solver family
│   ├── dp/                  # Dynamic-programming keyframe placement
│   ├── fit/                 # Segment fitters (Ceres + analytic)
│   ├── metrics/             # AE curve eval, error metrics, unified spatial
│   ├── shape/               # Sharp-corner policy
│   └── verify/              # KeyBundle ⇄ SampleBundle verifier
├── src/                     # Implementation (mirrors include/bbsolver/ layout)
├── tests/solver_unit/       # C++ unit tests
├── tests/policies/          # Solver-owned source-level policy checks
├── examples/after-effects/  # Minimal AE ScriptUI integration harness
└── docs/                    # This guide and related docs
protocol/                    # FlatBuffers schemas (.fbs) for bundles
tools/                       # Optional developer scripts and validation helpers
```

Every solver translation unit and stable header lives under
`include/bbsolver/<area>/` (public) or `src/<area>/`
(implementation). The flat `src/` root is intentionally empty of
tracked C++ sources.

## 3. Public API surface

### 3.1 Process boundary

The most stable consumer surface is the CLI binary itself, documented in
[`SOLVER_CLI.md`](SOLVER_CLI.md). Subcommands:

- `bbsolver solve <input.bbsm.json> <output.bbky.json> [opts]` — reduce samples
  to keys.
- `bbsolver verify <bundle.bbky.json> <samples.bbsm.json>` — replay keys against
  samples and emit a per-property error report.
- `bbsolver dump <bundle.bbsm.json|bundle.bbky.json>` — pass a JSON bundle to
  stdout.
- `bbsolver --version`, `bbsolver --help`.

Exit codes are stable: `0` completed command, `1` runtime/IO/format/schema
error, `2` usage error, `3` verification mismatch, `5` cancelled.

### 3.2 C++ entry point

`src/app/main.cpp` is dispatch-only: it forwards to one of three
command entry points in the `bbsolver` namespace:

```cpp
int bbsolver::RunSolve(int argc, char** argv);
int bbsolver::RunVerifyCommand(int argc, char** argv);
int bbsolver::RunDumpCommand(int argc, char** argv);
```

These are the only intended C++ embedding entry points. Direct embedding
(linking against `bbsolver_core`) is source-visible for trusted integrations,
but the supported SDK contract is the CLI process boundary plus the
SampleBundle/KeyBundle schemas.

### 3.3 Core data types (`bbsolver/domain.hpp`)

| Type | Role |
|---|---|
| `enum class ValueKind` | Scalar / 2D / 3D / 2D-Spatial / 3D-Spatial / Color / Custom |
| `enum class InterpType` | Hold / Linear / Bezier |
| `struct TemporalEase` | `{ speed, influence }` for per-channel Bezier easing |
| `struct KeyTiming` | Per-sample AE key metadata (interp, ease, spatial tangents) |
| `struct CompInfo` | Comp FPS, duration, dimensions, transform context |
| `struct LayerXform` | Snapshot of layer transform at sample t |
| `struct PropertyInfo` | Identity of a property (id, kind, type, dims) |
| `struct Sample` | `{ t_sec, v[], optional<KeyTiming> }` |
| `struct PropertySamples` | One property's full sample stream + config refs |
| `struct SolverConfig` | Tolerance, screen px, solve mode, AE-specific knobs |
| `struct SampleBundle` | The full solve input: `{ schema_version, request_id, comp, properties[], config }` |
| `struct Key` | A produced keyframe `{ t_sec, v[], interp_in/out, ease, spatial }` |
| `struct SegmentReport` | Per-segment error/convergence diagnostics on a property |
| `struct PropertyKeys` | One property's solver output: `{ property_id, dimensions, keys[], max_err, converged, notes }` |
| `struct KeyBundle` | Full solve output: `{ request_id, property_results[], solve_time_ms, total_keys, ... }` |

These are POD-like value structs; treat their public fields as the contract.
The plural fields (`v`, `keys`, `ease`, `spatial_in`, `spatial_out`) are
`std::vector` — their dimensionality is interpreted per-property by the
type system encoded in `ValueKind`/`InterpType`/`PropertyInfo`.

### 3.4 Solve options (`bbsolver/app/cli_options.hpp`)

```cpp
struct SolveOptions {
  double tolerance         = 0.5;     // L_inf in property units
  double screen_px         = 0.0;     // L_inf in projected px; 0 disables
  int    jobs              = 0;       // 0 = auto via TBB
  int    progress_fd       = -1;      // file descriptor for progress JSON
  std::optional<std::filesystem::path> diagnostics_file;
  std::optional<std::filesystem::path> cancel_file;
  bool   decompose_paths            = false;
  bool   fit_canonical_paths        = false;
  bool   fit_replacement_paths      = false;
  bool   emit_landmark_subpaths     = false;
  std::optional<std::string> solve_optimization_mode;
  bool   verbose                    = false;
};
```

`solve_optimization_mode` accepts (case-insensitive, dashes-or-underscores):

| Mode | Allows temporal | Allows vertex reduction | Allows spatial topology | Uses motion smoothing |
|---|:-:|:-:|:-:|:-:|
| `full` (default; also `default`/`auto`) | yes | yes | yes | no |
| `temporal_only` | yes | no | no | no |
| `vertex_only` | no | yes | no | no |
| `motion_smooth` | (motion-smooth schedule) | n/a | n/a | yes |
| `motion_path_smooth` | (motion-smooth schedule) | n/a | n/a | yes |

See `routing/solve_mode_policy.hpp` for the live predicate functions.

## 4. Integration model

### 4.1 Third-party process embedding

The supported integration is to spawn `bbsolver` as a subprocess.
Producers emit JSON SampleBundles (`.bbsm.json`) and read back JSON
KeyBundles (`.bbky.json`). Binary FlatBuffers bundle IO is not part of
the CLI contract. The AE ScriptUI reference implementation is documented
in [`AE_SCRIPTUI_HARNESS.md`](AE_SCRIPTUI_HARNESS.md); the full CLI
process contract is in [`SOLVER_CLI.md`](SOLVER_CLI.md).

Producers should:

1. Serialize a `SampleBundle` with `samples_per_frame` set per the comp
   (typically `1`).
2. Set `SolverConfig.tolerance` (and optionally `screen_px`, `comp` dims).
3. Pick `solve_optimization_mode` if non-default behavior is needed.
4. Run `bbsolver solve input output --jobs N --progress-fd FD --cancel-file P`.
5. Watch the progress FD for JSON events; check `cancel-file` periodically;
   read the resulting `KeyBundle`.

### 4.2 In-process embedding (advanced)

`bbsolver_core` is a static library; consumers can link it and call
`RunSolve()` directly with an `argv`-style array. The library exposes the
domain types and a number of building blocks (`PropertyRouteSolveRequest`,
`SolveProperty*` helpers, `Verifier`, `DiagnosticsWriter`, `ProgressWriter`),
but those symbols are source-visible only and are not part of the SDK contract.
The stable contract for external products is the process boundary plus the
bundle schemas.

### 4.3 Wire formats

- **JSON**: `bbsolver/io/io_json.hpp`, `sample_bundle_io.hpp`,
  `key_bundle_io.hpp`, `sample_property_io.hpp`, `solver_config_io.hpp`,
  `sample_json_{timing,value}_io.hpp`. The functions are pure
  `ParseXJson(nlohmann::json) -> X` / `BuildXJson(X) -> nlohmann::json`.
- **FlatBuffers**: schemas live in `protocol/samples.fbs` and
  `protocol/keys.fbs`, identifiers `BBSM` and `BBKY` respectively.
  CMake generates headers via `flatc` at configure time. These schemas
  are design references; the CLI does not currently read or write binary
  bundles.

Schema evolution rule: bump `schema_version` and keep parsers backward-
compatible. The currently supported `schema_version` is declared in
`include/bbsolver/io/schema_contract.hpp` and is enforced for
both SampleBundle and KeyBundle JSON at load time.

### 4.4 Practical host integration

For a product, script, or extension, treat `bbsolver` as a local worker
process:

1. Build or install the `bbsolver` binary next to your product, or store its
   absolute path in user settings.
2. On startup or before the first solve, run `bbsolver --version` and confirm
   the version/protocol your product expects.
3. Sample the source animation in the host application.
4. Write a `SampleBundle` JSON file (`.bbsm.json`) with `schema_version: 1`.
5. Launch `bbsolver solve input.bbsm.json output.bbky.json` with explicit
   tolerances and mode flags.
6. Read newline-delimited JSON progress events from `--progress-fd` if your
   host can pass a pipe/file descriptor. Otherwise poll for process completion
   and show coarse progress from your own UI.
7. To cancel, create the path passed via `--cancel-file`; the solver exits
   with code `5` after writing a partial bundle where completed results are
   marked cancelled.
8. Parse the `KeyBundle` and write the returned keys into the host's animation
   model.
9. Optionally run `bbsolver verify output.bbky.json input.bbsm.json` as a
   development or QA check.

The recommended stable contract is the process boundary plus the bundle
schemas. Directly linking against `bbsolver_core` is useful for tests and
experiments, but the C++ symbol surface is source-visible only and is not part
of the SDK contract.

For a runnable AE integration example, see
[`AE_SCRIPTUI_HARNESS.md`](AE_SCRIPTUI_HARNESS.md) and
`examples/after-effects/bbsolver-test-harness.jsx`. The harness samples
selected AE properties, writes a JSON `SampleBundle`, invokes
`bbsolver solve`, reads a JSON `KeyBundle`, and writes solved keys back to AE.

### 4.5 SampleBundle JSON shape

New JSON producers should use snake_case field names. Some older camelCase
config aliases are accepted for compatibility with early host-panel builds, but
they are not the recommended schema for new integrations.

Top-level input:

```json
{
  "_schema": "samples",
  "schema_version": 1,
  "request_id": "host-generated-unique-id",
  "comp": { "...": "CompInfo" },
  "properties": [{ "...": "PropertySamples" }],
  "config": { "...": "SolverConfig" }
}
```

`_schema` and `schema_version` are required at the process boundary.
`bbsolver solve` requires `_schema: "samples"` and `bbsolver verify`
requires `_schema: "keys"` for the KeyBundle argument and `_schema:
"samples"` for the SampleBundle argument. A useful solve must also provide
at least one populated property.

`CompInfo`:

| Field | Required | Notes |
|---|:-:|---|
| `fps` | yes | Frames per second used to interpret sample cadence. |
| `duration_sec` | yes | Full comp or sampled-range duration. |
| `width`, `height` | recommended | Needed for screen-space tolerances. |
| `pixel_aspect` | optional | Defaults to `1.0`. |
| `shutter_angle_deg`, `shutter_phase_deg`, `motion_blur_enabled` | optional | Preserved metadata for hosts that care about motion blur. |
| `work_area_start_sec`, `work_area_end_sec` | recommended | Host work-area or sampled range. |

`PropertySamples`:

```json
{
  "property": { "...": "PropertyInfo" },
  "t_start_sec": 0.0,
  "t_end_sec": 1.0,
  "samples_per_frame": 1,
  "samples": [
    { "t_sec": 0.0, "v": [0.0, 0.0] },
    { "t_sec": 0.041666667, "v": [10.0, 4.0] }
  ],
  "layer_xform_at_start": null,
  "hash_of_expression": "optional-cache-key"
}
```

`PropertyInfo`:

| Field | Required | Notes |
|---|:-:|---|
| `id` | yes | Stable unique id for this request; returned as `property_id`. |
| `match_name`, `display_name`, `layer_path` | recommended | Human/debug metadata. |
| `kind` | yes | One of `Scalar`, `TwoD`, `ThreeD`, `TwoD_Spatial`, `ThreeD_Spatial`, `Color`, `Custom`. |
| `dimensions` | yes | Number of numeric channels in each sample value. |
| `is_spatial` | yes for Position-style data | Enables spatial tangent output. |
| `is_separated` | recommended | `true` for separated Position dimensions. |
| `units_label` | recommended | Examples: `px`, `deg`, `%`, `shape_flat`. |
| `source_key_times` | optional | Source key anchors used by preservation and smoothing modes. |
| `min_value`, `max_value` | optional | Host clamp metadata. |

For ordinary scalar/vector properties, each sample value `v` should have
`dimensions * samples_per_frame` numbers. For frame-center sampling this is
just `dimensions`. For Position-style properties, use `TwoD_Spatial` or
`ThreeD_Spatial`, set `is_spatial=true`, and provide 2 or 3 numbers per sample.
Raw variable-topology `shape_flat` input is the only exception: each sample must
be a valid flat path vector, and `dimensions` is the maximum sampled flat-vector
length.

For Shape Path data, use `kind="Custom"` and `units_label="shape_flat"`. Each
sample value uses the flat path encoding documented in
[`PATH_HANDLING.md`](PATH_HANDLING.md):

```text
v = [closed_flag, n_vertices,
     vx0, vy0, ix0, iy0, ox0, oy0,
     vx1, vy1, ix1, iy1, ox1, oy1,
     ...]
dimensions = 2 + 6 * n_vertices
```

Stable-topology streams keep one vertex count for all samples. For
variable-topology streams, hosts can export raw per-frame `shape_flat` values,
set `shape_variable_topology=true`, set `dimensions` to the maximum sampled
flat-vector length, and enable `allow_path_replacement_fit` /
`--fit-replacement-paths`. The closed flag must remain stable.

`shape_variable_topology`, `shape_canonical_method`, and
`shape_max_vertex_count` are advisory host metadata. The JSON parser
tolerates these fields but does not store them in `PropertyInfo` or use
them for route selection; topology is derived from the encoded `v`
arrays and replacement fitting is enabled by config/CLI flags.

Sample timing metadata is optional. A host can either nest it:

```json
{
  "t_sec": 0.0,
  "v": [100.0, 200.0],
  "key_timing": {
    "interp_in": "Bezier",
    "interp_out": "Bezier",
    "temporal_ease_in": [{ "speed": 0.0, "influence": 33.3 }],
    "temporal_ease_out": [{ "speed": 0.0, "influence": 33.3 }],
    "spatial_in": [0.0, 0.0],
    "spatial_out": [0.0, 0.0],
    "roving": false
  }
}
```

or place those timing fields directly on the sample object. New producers
should prefer the nested `key_timing` form.

### 4.6 Minimal Position integration example

```json
{
  "_schema": "samples",
  "schema_version": 1,
  "request_id": "demo-position-001",
  "comp": {
    "fps": 24.0,
    "duration_sec": 1.0,
    "width": 1920,
    "height": 1080,
    "pixel_aspect": 1.0,
    "work_area_start_sec": 0.0,
    "work_area_end_sec": 1.0
  },
  "properties": [
    {
      "property": {
        "id": "Layer 1/Transform/Position",
        "match_name": "ADBE Position",
        "display_name": "Position",
        "layer_path": "Comp/Layer 1/Transform/Position",
        "kind": "TwoD_Spatial",
        "dimensions": 2,
        "is_spatial": true,
        "is_separated": false,
        "units_label": "px"
      },
      "t_start_sec": 0.0,
      "t_end_sec": 1.0,
      "samples_per_frame": 1,
      "samples": [
        { "t_sec": 0.0, "v": [100.0, 200.0] },
        { "t_sec": 0.5, "v": [260.0, 120.0] },
        { "t_sec": 1.0, "v": [420.0, 200.0] }
      ]
    }
  ],
  "config": {
    "tolerance": 0.5,
    "tolerance_screen_px": 1.0,
    "solve_optimization_mode": "full",
    "parallel_jobs": 0
  }
}
```

Typical subprocess call:

```sh
bbsolver solve demo.bbsm.json demo.bbky.json \
  --tolerance 0.5 \
  --screen-px 1.0 \
  --jobs 0 \
  --progress-fd 1
```

For Position motion-path smoothing:

```json
{
  "solve_optimization_mode": "motion_path_smooth",
  "motion_path_smoothing_tolerance": 3.0,
  "motion_path_accuracy_tolerance": 1.5,
  "motion_path_preserve_bounds": false,
  "motion_path_bounds_tolerance": 0.0,
  "motion_path_preserve_sharp_points": true,
  "motion_path_sharp_angle_deg": 75.0,
  "motion_path_respect_keyed_frames": false
}
```

Those fields go inside the top-level `config` object.
`motion_path_smoothing_tolerance` is a dimensionless smoothing strength, not a
property-unit accuracy tolerance. The solver clamps it to `1.0`-`32.0`;
`3.0` is the default and `32.0` is aggressive fairing.

`motion_path_accuracy_tolerance` is the reduced-key error budget against the
smoothed path. It is measured in property units, so AE Position values use comp
pixels. The default is `1.5`; values must be positive; the solver does not impose
a hard maximum.

`motion_path_preserve_bounds` constrains the smoothed target to the original
sampled motion-path footprint. When enabled, `motion_path_bounds_tolerance` is
the allowed per-side bounds deviation in property units; AE Position values use
comp pixels.

### 4.7 Reading KeyBundle output

The output JSON has this shape:

```json
{
  "_schema": "keys",
  "schema_version": 1,
  "request_id": "demo-position-001",
  "property_results": [
    {
      "property_id": "Layer 1/Transform/Position",
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
          "temporal_continuous": false,
          "spatial_continuous": false,
          "temporal_auto_bezier": false,
          "spatial_auto_bezier": false,
          "roving": false
        }
      ],
      "max_err": 0.0,
      "max_err_screen_px": 0.0,
      "segments": [],
      "converged": true,
      "notes": ""
    }
  ],
  "solver_version": "bbsolver 1.0.0",
  "solver_build": "",
  "solve_time_ms": 0.0,
  "total_keys": 1,
  "total_samples_input": 3
}
```

Host writeback should match each `property_results[].property_id` to the
source property id, remove or replace old keys according to the host's policy,
then write:

- `keys[].t_sec` as key time
- `keys[].v` as the key value; its length equals `property_results[].dimensions`
- `interp_in` / `interp_out` as temporal interpolation
- `temporal_ease_in` / `temporal_ease_out` as host temporal ease data
- `spatial_in` / `spatial_out`, `spatial_continuous`,
  `spatial_auto_bezier`, and `roving` for spatial properties

If `converged=false`, do not blindly apply the result as a successful solve.
Either fall back to source keys, retry with safer settings, or present the
failure to the user. The `notes` string is diagnostic metadata; treat it as
tokens to search, not as a positional format.

## 5. Architecture and folder layout

The solver layout has three practical goals:

- keep the public C++ surface under `include/bbsolver/`
- keep implementation files grouped by feature area under `src/`
- keep tests, documentation, and dependency fallback assets inside the solver
  tree so the solver can later be split into its own repository

This guide is self-contained for normal solver development. Repo-level project
management notes are intentionally excluded from the standalone solver docs.

### 5.1 Areas at a glance

| Area | Purpose |
|---|---|
| `app/` | CLI options, version string, entry-point dispatch |
| `solve/` | `RunSolve` orchestrator + per-property lifecycle helpers |
| `routing/` | Property classification (shape-flat? unseparated spatial?) and the route picker |
| `io/` | JSON readers/writers for bundles + config |
| `progress/` | Progress JSON channel + cancellation polling |
| `diagnostics/` | Optional diagnostic JSON-event channel (default off) |
| `runtime/` | TBB scope guard + env-var parsers + jobs resolver |
| `samples/` | Source-key preservation, key timing helpers, value helpers |
| `path/temporal/` | Temporal validation/progress/influence for path properties |
| `path/frame_fit/` | Per-frame Shape Path fitter (cubic span / decimate) |
| `path/multimode/` | Multi-region path solver and landmark machinery |
| `path/dense/` | Dense polyline + landmark sampling at fractions |
| `path/bridge_prune/` | Post-temporal bridge-prune passes (batched + scheduled) |
| `path/decompose/` | Per-channel solver via `path_decomposed_solver` |
| `path/config/` | Path-specific config tightening (gaps, child policies) |
| `path/reduction/` | Vertex-count and bridge reduction after the temporal pass |
| `path/replacement/` | Replacement-path orchestrator (acceptance, retry, notes) |
| `path/fit/` | path_fit pipeline glue: regime detection + linear pre-check |
| `path/geometry/` | Outline error, geometry refinement |
| `motion_smooth/` | Motion Smooth and Motion Path Smooth solver family |
| `temporal/refit/` | Post-solve temporal refit (key-count reduction loop) |
| `replacement_temporal/` | Replacement temporal solver (segment fit + relaxed fit) |
| `dp/` | DP keyframe placement (`dp_placer`, forward placement, key assembly) |
| `fit/` | Segment fitters: Ceres, analytic Bezier, shape-temporal, unified spatial |
| `metrics/` | `ae_curve` eval, `ErrorReport`, `unified_spatial` |
| `shape/` | Sharp corner policy |
| `verify/` | `Verifier` + the verify/dump subcommand helpers |

### 5.2 Include conventions

- Public headers under `include/bbsolver/<area>/` are included via
  `#include "bbsolver/<area>/<stem>.hpp"`. This is the *canonical form*.
- Implementation files under `src/<area>/<stem>.cpp` first-include
  their own public header in canonical form, then standard library and
  external deps.
- Public-header to private-header cross-area dependencies are NOT allowed
  (locked by `tests/policies/solver_public_header_dependency_policy.py`). The
  grandfather map is intentionally empty as of the current state.
- Direct includes are required: no relying on a transitive include from a
  re-export shim.

### 5.3 Layout rules are not behavior changes

This section is only about file placement and module ownership. It does not
change the solver's mathematical contract or public integration contract.

Do not infer new behavior from a module move. These contracts remain governed
by their owning code and tests:

- DP placement, segment fitting, spatial unification, and error metrics define
  the mathematical acceptance behavior.
- Progress and diagnostics modules define emitted event schemas and lifecycle
  ownership.
- IO and protocol modules define `SampleBundle` and `KeyBundle` compatibility.
- App and solve modules define the CLI surface, version string, and exit codes.

In other words: moving a helper into a clearer folder is not permission to
change key counts, max error, emitted notes, progress cadence, cancellation
semantics, or wire-format fields.

## 6. Solve route logic

`RunSolve` is the top-level orchestrator. For each property in the input
bundle, it follows roughly this lifecycle:

1. **Cancellation check** — if `--cancel-file` exists, write a partial
   KeyBundle and exit 5.
2. **Path preparation** (`solve/solve_path_preparation.hpp`) — for path
   properties, possibly apply canonical path conversion or fitted-path
   replacement, and detect a near-optimal fast path.
3. **Temporal prelude** (`solve/solve_property_temporal_prelude.hpp`) —
   set up temporal-config tightening, source-key preservation gates, and
   the replacement temporal-max-gap.
4. **Route selection** (`routing/property_solver_routing.hpp`) — `ChoosePropertySolveRoute()` picks
   one of:
   - `PreserveSourceKeys` — source keys carry directly through.
   - `MotionSmooth` — motion-smooth pipeline (also `motion_path_smooth`).
   - `FrameKeyFallback` — raw per-frame keys.
   - `ReplacementShapeFlatTemporal` — replacement temporal solver path.
   - `PathDecomposed` — per-channel path solve.
   - `PlainTemporal` — the default DP + segment-fit pipeline.
5. **Solve** (`routing/property_route_solver.hpp::SolvePropertyRoute`) — the
   route-specific solver returns a `PropertyKeys`.
6. **Temporal result reporting** (`solve/solve_property_temporal_result.hpp`)
   — capture progress events, validation, cancel-phase reporting.
7. **Post-processing** (`solve/solve_property_post_processing.hpp`) — temporal
   refit (`temporal/refit/`), vertex/bridge reduction (`path/reduction/`,
   `path/bridge_prune/`), motion-smooth reduction gate.
8. **Output assembly** (`solve/solve_property_output.hpp`) — assemble the
   final `PropertyKeys` and append to the running `KeyBundle`.

The whole loop is gated by cancellation polling and progress emission at
every stage boundary.

### 6.1 Property classification

`routing/property_classification.hpp`:

```cpp
bool IsShapeFlatPath(const PropertySamples& property_samples);
bool IsUnseparatedSpatial(const PropertySamples& property_samples);
```

These two predicates drive most routing decisions. `IsShapeFlatPath` returns
true for Shape Path properties whose dimensions encode a fixed number of
vertices (closed flag stable across the stream). `IsUnseparatedSpatial`
returns true for `TwoD_Spatial` / `ThreeD_Spatial` value kinds.

## 7. Module responsibilities (deep dive)

### 7.1 `solve/` — orchestration

The solve area is the top of the call graph. Notable headers:

- `solve_command.hpp` — `RunSolve(argc, argv)`.
- `solve_command_config.hpp` — argv → `SolveCommandConfig` (parsed options +
  loaded sample bundle + resolved parallel jobs).
- `solve_lifecycle_reporting.hpp` — emits the `solve_start`, `solve_done`,
  `solve_cancelled` events to both the progress channel and the optional
  diagnostics channel.
- `solve_property_completion.hpp` — per-property finalisation.
- `solve_property_output.hpp` — final PropertyKeys assembly.
- `solve_property_post_processing.hpp` — temporal refit + reductions +
  motion-smooth gate.
- `solver_observability.hpp` — instrumentation helpers used by the lifecycle
  reporting layer.
- `solver_reporting.hpp` — note builders and per-property reporting helpers.
- `static_key_cleanup.hpp` — removes redundant constant keys.

`plain_property_solver.hpp` and `fallback_property_solver.hpp` are the
direct property solvers wrapped by the route picker.

### 7.2 `routing/` — picker

`solve_mode_policy.hpp` exposes the predicate suite shown in §3.4.
`property_solver_routing.hpp` is the pure picker: it receives a
`PropertySolveRouteInput` struct of booleans and returns the
`PropertySolveRoute` enum. The picker is intentionally side-effect free —
all the gating logic that decides "is preserve_source_keys enabled?",
"is motion_smooth enabled?" etc. lives in the calling lifecycle code, not
in the picker.

### 7.3 `path/` — path-property machinery

Path properties (Shape Path animations) are the most algorithm-rich area.
Their stack roughly composes as:

```
SolvePropertyRoute → PathDecomposed
                       └─ path/decompose/path_decomposed_solver
                            ├─ path/dense/      (densify + landmarks)
                            ├─ path/frame_fit/  (cubic spans + decimation)
                            ├─ path/multimode/  (multi-region landmark fit)
                            ├─ path/replacement/ (try fitted replacement)
                            │     └─ replacement_temporal/  (replacement temporal solver)
                            └─ path/bridge_prune/  (post-temporal bridge-prune)
```

Inside `path/replacement/`, the `path_replacement_solver` orchestrator
delegates to acceptance (`path_replacement_acceptance`), retry-loop
control (`path_replacement_retry_loop`), candidate validation, decision
application, and notes assembly. The family is diagnostics-free — every
progress event is emitted by the caller (`solve_command.cpp`) and not
inside the replacement modules.

### 7.4 `motion_smooth/` — smoothing solver

`motion_smooth_solver.hpp` is the front-door façade for the motion-smooth
pipeline. The body is split into many cohesive sub-modules (sample points,
endpoint keys, bezier-ease, spatial trajectory, shape flat closed loop,
adaptive resample, etc.). The façade preserves IWYU re-export semantics
so existing consumers (`main.cpp`, dispatch, reduction gate, downstream
focused tests) keep compiling unchanged.

There are 23 motion_smooth headers and 21 source files (plus a façade
header re-exporting its sub-headers). Sub-module symbol ownership is
locked by `tests/policies/motion_smooth_facade_lock_policy.py` (36 distinct
invariants).

**`motion_path_smooth_spatial_trajectory.hpp`** owns the spatial trajectory
implementation for the `motion_path_smooth` mode (see §10).

### 7.5 `replacement_temporal/` — replacement temporal solver

The `replacement_temporal_solver` is invoked from
`path/replacement/path_replacement_solver` when fitted-path replacement is
in play. It composes:

- `replacement_temporal_anchor_prune` and `_anchor_prune_fit` — anchor
  selection for the relaxed fit.
- `replacement_temporal_forward_span` — forward-span seed.
- `replacement_temporal_keys` — per-segment key emission.
- `replacement_temporal_options` / `replacement_temporal_solve_options` —
  per-solver configuration.
- `replacement_temporal_relaxed_fit` (re-exported by
  `replacement_temporal_segment_fit`) — the relaxed endpoint fitter.

### 7.6 `temporal/refit/` — post-solve temporal refit

`temporal_refit.hpp` is the public façade. The helper family
(`_budget`, `_candidate`, `_dimensions`, `_gate`, `_resample`, `_shape`,
`_structural`, `_support`, `_validation`) implements key-count reduction
through DP-driven candidate generation. The refit is diagnostics-free;
the gate (`temporal_refit_gate.hpp`) decides eligibility per property.

### 7.7 `dp/` — DP placement

The DP family is the algorithmic core of the temporal solver:

- `dp_placer.hpp` — main placement DP; exposes
  `struct DPPlacement { sample_indices[], segments[] }` and the
  `SegmentFitResult` envelope.
- `dp_forward_placement.hpp` — forward-pass DP recursion.
- `dp_key_assembly.hpp` — assemble `Key` objects from a `DPPlacement` and
  per-segment fit results.
- `dp_placement_limits.hpp` — budget arithmetic (caps on segments tried).
- `dp_placement_progress.hpp` — `PlacementProgress` event emission.

The DP is pluggable in the sense that the segment fitter passed to it is
chosen by the calling layer (Ceres vs. analytic Bezier vs. shape-temporal).

### 7.8 `fit/` — segment fitters

`segment_fitter.hpp` is the umbrella entry point. The fitters:

- `segment_fit_ceres.hpp` — Ceres-Solver-based non-linear least squares;
  the default for high-dimensional and spatial properties.
- `segment_fit_bezier.hpp` — analytic Bezier fitter for scalar / 2D / 3D
  non-spatial inputs.
- `segment_fit_shape_temporal.hpp` — shape-flat temporal specialisation.
- `segment_fit_unified_spatial.hpp` — unified spatial fitter.
- `segment_fit_policy.hpp` — picks the fitter implementation by `ValueKind`
  and property characteristics.
- `segment_fit_samples.hpp` — sample window construction.
- `segment_fit_diagnostic_events.hpp` — diagnostic event builders for
  fitter outcomes (consumed by the solve lifecycle).

### 7.9 `metrics/` — math kernels

- `ae_curve.hpp` — evaluates AE-style Bezier curves and keys.
- `error_metrics.hpp` — `ErrorReport` builder, L_inf reduction, screen-px
  projection.
- `unified_spatial.hpp` — unified spatial math used by the spatial fitter
  and the verifier.

These are intentionally treated as hot kernels — see §11 on hot-kernel
constraints.

### 7.10 `verify/` — independent validation

`verifier.hpp` exposes `EvalKeysAt` and `ValidateKeys`, the same code path
used by the `bbsolver verify` subcommand and by the unit tests. The
`KeyBundle` produced by a solve is always validated by an independent
re-evaluation here; cross-channel error envelopes are summarised in
`ErrorReport`.

## 8. Diagnostics, progress, and cancellation

The solver has three completely separate observability channels.

### 8.1 Progress (`bbsolver/progress/progress.hpp`)

`class ProgressWriter` wraps a file descriptor and emits one
newline-delimited JSON event per `Emit()` call. The descriptor comes from
`--progress-fd FD`; producers feed it the write end of a pipe and read
events as they appear.

Event helpers:

- `PropertyProgressEvent(event, phase, property_idx, count, stage_fraction, ps)`
- `PlacementProgressEvent(event, phase_prefix, property_idx, count, stage_start, stage_end, ps, placement)`

The progress channel is the operator-facing channel; it is the only
channel the AE panel reads.

### 8.2 Diagnostics (`bbsolver/diagnostics/solver_diagnostics.hpp`)

`class DiagnosticsWriter` is the **opt-in, file-backed** diagnostic
channel. Off by default; enabled via `--diagnostics PATH`. Event builders
live in `solver_diagnostic_events.hpp`:

- `BuildSolveStartEvent`, `BuildSolveDoneEvent`, `BuildSolveCancelledEvent`
- `BuildParallelRuntimeEvent`, `BuildSolveModeCapabilitiesEvent`
- `BuildCancellationStatusEvent`
- `BuildBridgePruneResultEvent`, `BuildBridgePrunePhaseEvent`

The strict rule: **only orchestration boundaries** can construct or emit through
`DiagnosticsWriter`. Leaf modules (math kernels, algorithm building blocks,
pure formatters) must never include
`bbsolver/diagnostics/solver_diagnostics.hpp`. This is enforced by
`tests/policies/solver_diagnostics_boundary_policy.py`.

### 8.3 Cancellation (`bbsolver/progress/solve_cancellation.hpp`)

```cpp
bool CancelFileExists(const std::optional<std::filesystem::path>& cancel_file);
void MarkCancelledPartial(KeyBundle& keys);
int  WriteCancelledPartial(const std::filesystem::path& output_path,
                           KeyBundle& keys,
                           std::chrono::steady_clock::time_point start);
```

Cancellation is cooperative and polled. Every property-loop iteration
checks `CancelFileExists` and, if true, writes whatever keys have been
produced so far (marked with a cancellation note) and exits with code 5.

## 9. Determinism and parallel jobs

### 9.1 TBB scope (`bbsolver/runtime/`)

`SolveParallelRuntimeScope` is an RAII guard that pins the TBB
`global_control` to a resolved job count for the lifetime of a solve.

The resolution algorithm (in `runtime_env.hpp`):

1. If `--jobs N` is specified, clamp to `[1, kParallelJobsHardCap]` (64).
2. Otherwise auto-detect via TBB.
3. If TBB is unavailable in this build, force `1`.

The resolved value is reported verbatim in the `parallel_config` progress
event so producers can see exactly what the solver chose.

### 9.2 Determinism guarantees

- **Per-property**: solves are deterministic given identical input and
  identical `SolverConfig` + `SolveOptions` (including the resolved jobs
  count).
- **Across job counts**: results are stable provided no parallel reductions
  depend on iteration order. The current solver design avoids order-
  dependent parallel reductions in algorithm-critical paths.
- **Time stamps**: `solve_time_ms` in `KeyBundle` and timing inside
  progress events are wall-clock derived and intentionally non-
  deterministic.

If you observe non-determinism in a non-timing field across runs at the
same job count, that is a bug — please report with the request_id and a
diff of the two key bundles.

## 10. Extension points

### 10.1 Adding a new solve mode

To add a new `solve_optimization_mode`:

1. Extend `NormalizeSolveOptimizationMode` in
   `routing/solve_mode_policy.cpp` to accept the new identifier.
2. Add predicates (`SolveModeAllowsX`, `SolveModeIsX`) as needed.
3. Extend `ChoosePropertySolveRoute` in
   `routing/property_solver_routing.cpp` to route to a new branch when
   appropriate.
4. Add the new branch implementation under the most appropriate area —
   if it touches motion smoothing, under `motion_smooth/`; if it
   introduces a new path strategy, under `path/<new_area>/`.
5. Add a focused unit test under `tests/solver_unit/`.
6. Add a layout-policy area test in
   `tests/policies/solver_layout_policy.py` if you
   created a new `bbsolver/<area>/` namespace.
7. Update `solver_diagnostics_boundary_policy.py` allowlist if (and only if)
   the new mode owns lifecycle diagnostics; pure-leaf modes must not.

### 10.2 Adding a new fitter

Segment fitters are pluggable through `fit/segment_fit_policy.hpp`. To add
a new fitter:

1. Implement the new fitter under `fit/<your_fitter>.hpp/.cpp`.
2. Extend `SegmentFitPolicy` to pick it for the value-kind / property
   characteristic combinations it owns.
3. Make sure it returns `SegmentFitResult { feasible, interp, ease_*, ... }`
   compatible with the DP placer (`dp/dp_placer.hpp`).
4. Add a focused unit test under
   `tests/solver_unit/test_segment_fit_*.cpp`.

### 10.3 Adding a new path strategy

The path-property areas (`path/*`) are designed to compose. To add a new
strategy:

1. Create `include/bbsolver/path/<new_area>/` and
   `src/path/<new_area>/` directories.
2. Add a public façade header with the entry points; sub-headers for
   helpers; sub-cpp files for the implementations.
3. Wire it into the path-property orchestrator
   (`path/decompose/path_decomposed_solver` or
   `path/replacement/path_replacement_solver`).
4. Add an area test to `tests/policies/solver_layout_policy.py`
   (pattern: copy
   one of the existing `test_path_<area>_modules_use_target_<area>_layout`
   tests).
5. If the new area emits its own progress events, route them through the
   existing `ProgressWriter` from the orchestrator — do NOT instantiate a
   new one or take a `DiagnosticsWriter`.

### 10.4 `motion_path_smooth` — extension point

The `motion_path_smooth` mode is a sibling of `motion_smooth`. The public
entry point is exposed at:

```cpp
// include/bbsolver/motion_smooth/motion_path_smooth_spatial_trajectory.hpp
PropertyKeys MotionPathSmoothSpatialTrajectoryKeys(
    const PropertySamples& property_samples,
    const SolverConfig& config);
```

Distinct from `MotionSmoothSpatialTrajectoryKeys`, it reduces keys against
a smoothed trajectory while preserving optional keyed-frame and sharp-turn
constraints. The mode is selected by setting
`solve_optimization_mode = "motion_path_smooth"`; the routing predicates
already handle the dispatch via `SolveModeIsMotionPathSmooth` and
`SolveModeUsesMotionSmoothing`.

`motion_path_smooth` is selected through `solve_optimization_mode` (CLI
`--solve-mode motion-path-smooth`) and is tuned via the
`motion_path_*_tolerance`, `motion_path_preserve_bounds`,
`motion_path_preserve_sharp_points`, and `motion_path_respect_keyed_frames`
config fields. See [`USER_GUIDE.md`](USER_GUIDE.md) for end-user tuning
guidance and the AE ScriptUI harness controls.

## 11. Development constraints currently in force

These constraints protect the public solver contract while internals continue
to move toward the standalone package layout.

### 11.1 Public/private API surface

The supported integration surface has three layers:

1. **Process boundary (stable).** The CLI subcommands, exit codes, JSON
   SampleBundle/KeyBundle schemas, progress JSON events, and
   diagnostics JSON events documented in
   [`SOLVER_CLI.md`](SOLVER_CLI.md) and
   [`API_REFERENCE.md`](API_REFERENCE.md). These change only with a
   documented version bump and an updated `schema_version`.
2. **CMake package (stable).** The CMake install/export described in
   [`PACKAGING.md`](PACKAGING.md) provides
   `find_package(bbsolver CONFIG)`, the `bbsolver::bbsolver` imported
   executable, and the `bbsolver::core` imported library. These names
   and the package config file layout are part of the public contract.
3. **C++ symbols inside `bbsolver::core` (source-visible, not SDK).**
   The three `Run*` entry points (`RunSolve`, `RunVerifyCommand`,
   `RunDumpCommand`) plus the schema-pinned bundle structs in
   `domain.hpp` are the supported C++ embedding surface. Everything
   else — internal headers under `include/bbsolver/`,
   `bbsolver::core` symbols beyond those entries, the path/dp/fit/
   motion-smooth modules — is source-visible but may move between
   releases. Some headers inside `include/bbsolver/` are explicitly
   marked internal in their leading comment (for example
   `include/bbsolver/io/solver_config_io.hpp`); treat anything so
   marked as private even though it lives under the public include
   root.

FlatBuffers schemas under `protocol/` are design references for
a future binary IO surface. Today's CLI exchanges JSON only.

Advanced solver flags (`--decompose-paths`, `--fit-canonical-paths`,
`--fit-replacement-paths`, `--emit-landmark-subpaths`) are off by
default and gated behind explicit opt-in for hosts that need them. They
are not "not ready" — they are deliberately advanced controls; see
[`PATH_HANDLING.md`](PATH_HANDLING.md) for usage and acceptance rules.

### 11.2 Behaviour-neutral by default

Refactors must not change key counts, max error, progress JSON shape,
cancellation semantics, or `KeyBundle` contracts unless explicitly scoped
as a separate behaviour-change patch.

### 11.3 No PImpl unless asked

Most public structs are direct value types. PImpl is not used to hide
internal state in `domain.hpp`, the IO types, or the value-kind/key
algorithms. New PImpl-style encapsulation is deferred until a future
refactor; do not introduce it during a layout migration.

### 11.4 Hot kernels and value structs

- The DP placer (`dp/dp_placer.cpp`), segment fitters (`fit/segment_fit_*`),
  and metric kernels (`metrics/`) are hot. Refactors should avoid adding
  layers of indirection in the inner loops.
- `domain.hpp` types (`Sample`, `Key`, `PropertyKeys`, `KeyBundle`, etc.)
  are value structs by design. They are passed by reference into the hot
  paths and should remain so.

### 11.5 Diagnostics ownership stays at command/lifecycle boundaries

Only `solve/solve_command.cpp`, `solve/solve_lifecycle_reporting.{hpp,cpp}`,
and the small set of files listed in
`tests/policies/solver_diagnostics_boundary_policy.py::DIAGNOSTICS_BOUNDARY_ALLOWLIST`
are allowed to construct `DiagnosticsWriter`. New code that wants to emit
a diagnostic must return a builder result and let the orchestrator route
it through the writer.

### 11.6 Public-header private dependencies are forbidden

`tests/policies/solver_public_header_dependency_policy.py` enforces that no
`include/bbsolver/<area>/*.hpp` includes a flat `src/*.hpp`
file. The grandfather map is intentionally empty as of the current
state; new entries are only added with a documented migration plan.

### 11.7 Layout

Tracked C++ implementation files live under `src/<area>/`; tracked C++
public headers live under `include/bbsolver/<area>/`. Further
reorganisation should stay intra-area unless a new public area is added
deliberately.

## 12. Validation strategy

The solver has four layered validation surfaces, all of which a contributor
or integrator can run locally.

### 12.1 C++ unit tests (ctest)

`tests/solver_unit/` contains focused tests for every public surface.
Build with `-DBBSOLVER_BUILD_TESTS=ON` and run with:

```bash
cmake -S . -B build -DBBSOLVER_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure -j8
```

The suite covers IO round-trips, fitter
properties, DP placer invariants, motion-smooth surfaces, path-family
sub-modules, and the verifier. See [`TEST_CATALOG.md`](TEST_CATALOG.md) for
the per-family run guidance and valid/invalid output expectations.

### 12.2 Python source-level policies

Solver-owned source-level policies belong under `tests/policies/`.
They are pure source-text checks and are part of the standalone solver QA
surface, not runtime code.

Run any solver-owned policy directly:

```bash
python3 tests/policies/<policy>.py
```

Key solver-owned policies:

- `solver_layout_policy.py` — every area has its `bbsolver/<area>/` layout
  test.
- `solver_public_header_dependency_policy.py` — public headers do not include
  private source headers.
- `solver_diagnostics_boundary_policy.py` — diagnostics ownership stays at
  lifecycle/orchestration boundaries.
- `solver_diagnostics_policy.py` and
  `solver_diagnostic_events_schema_policy.py` — diagnostic-event surfaces and
  schema coverage.
- `solver_progress_policy.py` — progress event anchors.
- `property_solver_routing_policy.py` — route picker stays side-effect free.
- `main_dispatch_only_policy.py` — app entry point stays dispatch-only.
- `motion_smooth_facade_lock_policy.py` — motion-smooth façade and sub-module
  ownership.
- `temporal_refit_refactor_policy.py` — temporal-refit family invariants.
- `path_multimode_refactor_policy.py` and `sample_json_refactor_policy.py` —
  path/multimode and JSON IO refactor boundaries.

Host-application checks should stay outside the solver test tree unless they are
rewritten as solver-owned integration tests.

### 12.3 clangd LSP sweep

Run `clangd` against every source file in `src` and every public
header in `include/bbsolver`, using the CMake-generated
`build/compile_commands.json`, and report any include-cleaner or
compile diagnostics. The expected result is zero diagnostics across all
files.

In a standalone checkout, run `clangd --check=<file>` across the same file set
or use an editor/LSP task that reads `build/compile_commands.json`.

### 12.4 Aggregate source guard

```bash
for policy in tests/policies/*_policy.py; do
  python3 "$policy" || exit $?
done
```

Standalone CI can run the solver-owned policies directly. A hosting repository
may wrap these policies with its own broader release checks.

## 13. Adding a new test or policy

- **Focused unit tests** belong under `tests/solver_unit/test_<area>_<surface>.cpp`.
  They link against `bbsolver_core` and call into the public headers.
- **Solver-owned policy checks** belong at
  `tests/policies/<area>_<rule>_policy.py`. Pure source-text where
  possible. Register the new policy in the guard runner used by the checkout
  so the quick guard picks it up.

A new test that targets a hot kernel should preserve behavioural locks
(input → expected output table) rather than micro-benchmarks. Performance
work has a dedicated lane outside this guide.

## 14. Caveats and advanced controls

- **`motion_path_smooth`** is the dedicated motion-path smoothing mode
  for Position-style spatial properties; see §10.4 for the parameters
  and the matching SampleBundle config fields. Use `motion_smooth` for
  the general motion-smooth pipeline that applies to non-spatial
  properties as well.
- **`--emit-landmark-subpaths`** is an advanced output and is off by
  default. The multipath representation requires a host that knows how
  to consume the appended `bb_lm_N` `PropertyKeys`. Hosts that have not
  adopted the multi-path applier should leave this flag off.
- **TBB is required for multi-job**. Builds without TBB fall back to
  `jobs = 1` regardless of the request.
- **FlatBuffers schemas**. The CMake build generates protocol headers via
  `flatc`. Without it, configure fails; this is intentional. The CLI
  does not currently read or write binary bundles — the schemas are
  design references.
- **Ceres is required**. The default segment fitter uses Ceres-Solver;
  builds without it fall back to the analytic fitters where applicable
  but lose accuracy on high-dimensional and spatial properties.
- **`PropertyKeys::notes`** is a `std::string` of comma-or-`; `-separated
  tokens. The full token vocabulary is documented in
  [`PATH_HANDLING.md`](PATH_HANDLING.md) and the
  relevant policy token tables. Do NOT parse it positionally;
  search for known token substrings.

## 15. Pointers to solver documentation

- [`API_REFERENCE.md`](API_REFERENCE.md) — concise CLI, IO schema, progress,
  diagnostics, and integration reference.
- [`SOLVER_CLI.md`](SOLVER_CLI.md) — process-spawn contract.
- [`TUNING_GUIDE.md`](TUNING_GUIDE.md) — tolerance and
  mode tuning.
- [`DP_ALGORITHM.md`](DP_ALGORITHM.md) — DP placement
  algorithm details.
- [`PATH_HANDLING.md`](PATH_HANDLING.md) — path-property
  handling and notes vocabulary.
- [`COMPRESSION_CURVES.md`](COMPRESSION_CURVES.md) — how
  the tolerance maps to reduction curves.
- [`AE_SCRIPTUI_HARNESS.md`](AE_SCRIPTUI_HARNESS.md) — AE sampling and writeback
  example.
- [`TEST_CATALOG.md`](TEST_CATALOG.md) — C++ and policy test catalog.

## 16. Asking questions

If something here is unclear or stale, open an issue with the section heading
you read and the question. This document is meant to track the actual code; if
you spot drift, treat it as a documentation bug.
