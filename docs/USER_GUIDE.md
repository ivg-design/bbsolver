# bbsolver User Guide

This guide explains the user-visible behavior of `bbsolver`, the standalone
solver for sampled animation data. It is written for artists, technical
directors, and pipeline integrators who need to choose solver options and
understand why a solve produced a particular set of keys.

The solver reduces dense animation samples into sparse AE-style keys. Its core
contract is conservative: when an accuracy-gated solve is accepted, the
reconstructed animation stays within the configured tolerance, and any
advanced path or smoothing feature records what happened in the output notes
and progress stream.

## Quick Start

For the included AE ScriptUI test harness:

1. Select one or more animatable timeline properties to bake.
2. Open `bbsolver-test-harness`.
3. Start with:
   - `Optimization mode`: `Full (keys + vertices)`
   - `Tolerance`: `0.5`
   - `Screen px`: `1.0`
   - `Solver jobs`: `0` for automatic parallelism
4. Run `Solve and Bake` and inspect the result.
5. If the result has too many keys, loosen `Tolerance` or `Screen px` slightly.
6. If a path loses important corners, keep `Preserve sharp path corners`
   enabled and avoid very loose tolerances.

For CLI use:

```sh
bbsolver solve input.bbsm.json output.bbky.json \
  --tolerance 0.5 \
  --screen-px 1.0 \
  --jobs 0 \
  --progress-fd 1
```

Use `--solve-mode motion-path-smooth` for spatial trajectory smoothing, and
`--fit-replacement-paths` only when you want the advanced Shape Path
refit-ladder to attempt vertex-count reduction before temporal key reduction.

## What bbsolver Reads And Writes

`bbsolver solve` reads a `SampleBundle` and writes a `KeyBundle`.

`SampleBundle` contains:

- comp metadata such as FPS, duration, size, work area, and pixel aspect
- one or more sampled properties
- each property's dense sample values over time
- optional source key times and source key timing
- a request-wide `SolverConfig`

`KeyBundle` contains:

- one `PropertyKeys` result per solved property
- produced key values, interpolation, temporal ease, spatial tangents, and
  roving flags
- max error fields
- segment reports
- `converged` status
- `notes`, which are the main human-readable explanation of special solver
  choices

The included AE test harness is the reference producer and consumer. It samples
live AE properties, writes a temporary bundle, launches `bbsolver`, then writes
the returned keys back into AE.

## Accuracy Controls

The three most important accuracy controls are related but not interchangeable.

| Control | Where | Meaning |
|---|---|---|
| `tolerance` / AE `Tolerance` | CLI, bundle config, AE settings | Maximum accepted error in property units. For Position this is usually pixels or comp units; for Rotation it is degrees; for opacity it is 0 to 100 units. |
| `tolerance_screen_px` / AE `Screen px` | CLI, bundle config, AE settings | Optional projected screen-space bound. `0` disables it. When active, it is an additional acceptance gate, not a replacement for `tolerance`. |
| Motion smoothing tolerances | AE settings and bundle config | Controls smoothing strength or reduced-path accuracy for smoothing modes. These do not change the normal acceptance gate unless the mode explicitly evaluates against them. |

For ordinary temporal solves, tighter tolerance means more keys and stricter
fidelity. Looser tolerance means fewer keys and more visible drift risk.

For path replacement and path pruning, the path-specific effective tolerance is
derived from the configured accuracy fields. The solver uses this budget to
decide whether a fitted or pruned path is safe enough to accept.

## Solve Modes

`solve_optimization_mode` is normalized by the solver. Dashes and underscores
are equivalent, so `motion-path-smooth` and `motion_path_smooth` refer to the
same mode.

| Mode | Best For | Temporal key reduction | Vertex/path reduction | Motion smoothing |
|---|---|---:|---:|---:|
| `full` | Normal production bakes | yes | yes, when path options allow it | no |
| `temporal_only` | Reduce key count without changing Shape Path topology | yes | no | no |
| `vertex_only` | Try guarded Shape Path vertex pruning without temporal optimization | no | yes | no |
| `motion_smooth` | Normalize manually keyed motion and shape trajectories | special smoothing route | no first-pass path reducers | yes |
| `motion_path_smooth` | Smooth Position-style motion paths while preserving sharp/keyed anchors | special smoothing route | no first-pass path reducers | yes |

### Full

`full` is the default. It allows:

- ordinary temporal key reduction
- static-key cleanup
- temporal refit
- canonical path fitting when enabled
- fitted replacement path solving when enabled
- post-temporal path vertex pruning when enabled

Use this for most bakes.

### Temporal Only

`temporal_only` keeps the source property's topology and dimensionality. It
reduces the number of keys in time but suppresses vertex/topology changes.

Use this when:

- a Shape Path must keep the same vertex count
- you are debugging a path reducer and want a clean temporal baseline
- the writeback target is sensitive to vertex identity

Ignored or suppressed:

- fitted replacement path topology
- post-solve vertex pruning
- spatial topology changes

### Vertex Only

`vertex_only` skips temporal optimization and produces frame/source keys, then
allows guarded vertex pruning for eligible `shape_flat` paths.

Use this when:

- the exact keyed timing matters more than key count
- you want to test path vertex reduction independently
- you are working with a path where temporal interpolation is risky

Ignored or suppressed:

- temporal placement
- shape temporal Bezier spans
- fitted replacement path temporal solving

The solver notes include
`solve_mode_vertex_only; temporal_optimization_skipped=true` on the frame-key
fallback result.

### Motion Smooth

`motion_smooth` is not a normal compression mode. It constructs a smoothed
motion result from the source sample range.

It has different branches:

- spatial properties use a spatial trajectory filter
- `shape_flat` paths use the shape-flat trajectory smoother
- non-spatial properties fall back to endpoint keys

Use it for manually keyed or noisy motion where the goal is smoother motion,
not exact reproduction of every source sample.

### Motion Path Smooth

`motion_path_smooth` is the newer intended mode for Position-style spatial
properties. It smooths the sampled motion path and then emits a reduced key set
against that smoothed path.

It is designed for cases like:

- a ball moving through a smooth arc
- a cursor or object path that was over-keyed
- a path with intentional sharp reversals that should stay sharp

It currently has a dedicated spatial-property implementation. Non-spatial and
Shape Path behavior routes through the broader motion-smoothing surface.

## AE Panel Settings

The AE panel persists settings in a JSON preferences file and writes the active
values into the SampleBundle before launching `bbsolver`.

### Core Bake Settings

| AE Setting | Default | Effect |
|---|---:|---|
| `Tolerance` | `0.5` | Main property-unit accuracy bound. |
| `Screen px` | `1.0` | Projected pixel bound for UI bakes. |
| `Use work area` | on | Limits sampling to the comp work area. |
| `Use segment markers` | off | Splits work by layer markers containing `bbsolver_segment`, `bbsolve`, or `bbsolver`. |
| `Solver jobs` | `0` | `0` means auto; positive values cap solver worker count. |
| `Show preview` | on | Shows a result summary before writeback. |
| `Verify round trip` | off | Replays the written keys back against samples. |

### Property And Rig Settings

| AE Setting | Default | Effect |
|---|---:|---|
| `Auto separate for bake` | off | Allows AE Separate Dimensions on eligible unkeyed spatial properties. |
| `Flatten parented position` | off | Samples parented Position in comp space and writes a flattened result. |
| `Preserve selected parenting` | off | Keeps parent links between selected layers; unparents only from unselected parents. |
| `Flatten parented tolerance` | `0.05` | Stricter tolerance for parent-flattened rig transforms. |
| `Rig rotation tolerance` | `0.01` degrees | Tightens rotation-group tolerance because small angular errors are visible in limbs/joints. |
| `Disable expression` | on | Disables source expressions after successful writeback. |

### Optimization And Smoothing Settings

| AE Setting | Default | Applies To | Effect |
|---|---:|---|---|
| `Optimization mode` | `full` | all properties | Chooses the solve route. |
| `Ease / rove Motion Smooth keys` | off | `motion_smooth`, `motion_path_smooth` | Applies the configured Bezier timing curve and enables roving where the mode supports it. |
| `Preserve source key poses during Motion Smooth` | off | `motion_smooth` only | Adds source-pose constraints for shape-flat motion smoothing. |
| `Motion smooth tolerance` | `3.0` | `motion_smooth` | Smoothing strength for spatial and shape-flat smoothing. Not the normal accuracy gate. |
| `Motion smooth cleanup tol` | `2.0` | AE second pass after Motion Smooth path results | Tolerance used by optional cleanup over smoothed path keys. |
| `Motion smooth ease x1,y1,x2,y2` | `0.33,0,0.67,1` | smoothing modes | Cubic timing curve used when smoothing ease is enabled. |
| `α Smooth` | `3.0` | `motion_path_smooth` | Dimensionless spatial fairing strength. Valid range is `1.0`-`32.0`; higher values smooth more aggressively and values above `32.0` are capped. |
| `ε Fit` | `1.5` | `motion_path_smooth` | Spatiotemporal fit tolerance from emitted reduced keys to the smoothed motion path, in property units; Position uses comp pixels. |
| `Preserve global path bounds` | off | `motion_path_smooth` | Constrains the smoothed target path to the original global path footprint. |
| `ε Bounds` | `0.0` | `motion_path_smooth` | Global bounds side tolerance in property units; Position uses comp pixels. `0` keeps the original outer bounds, larger values allow more footprint deviation. |
| `Preserve sharp motion-path reversals` | on | `motion_path_smooth` | Keeps cusp-like direction changes locked. |
| `Sharp reversal angle` | `75.0` degrees | `motion_path_smooth` | Turn angle treated as an intentional sharp point. |
| `Respect keyed frames in Motion Path Smooth` | off | `motion_path_smooth` | Keeps source key times and poses as hard anchors. |

### Path Settings

| AE Setting | Default | Effect |
|---|---:|---|
| `Run second-pass vertex prune after key reduction` | off | After a successful solve, tries removing existing path vertices while keeping accepted key times and tolerance. |
| `Cleanup pass` | `prompt` | Optional second solver pass before writeback. `Prompt` asks, `Auto` runs eligible cleanup, `Off` skips. |
| `Preserve sharp path corners under loose tolerances` | on | Locks durable high-angle shape landmarks during replacement and vertex pruning. |

## CLI Options

Current `solve` usage:

```sh
bbsolver solve <in.bbsm.json> <out.bbky.json> \
  [--tolerance 0.5] \
  [--screen-px 0] \
  [--jobs N] \
  [--progress-fd N] \
  [--diagnostics PATH] \
  [--cancel-file PATH] \
  [--decompose-paths] \
  [--fit-canonical-paths] \
  [--fit-replacement-paths] \
  [--emit-landmark-subpaths] \
  [--solve-mode full|temporal-only|vertex-only|motion-smooth|motion-path-smooth] \
  [--verbose]
```

| Option | Effect |
|---|---|
| `--tolerance T` | Overrides bundle `config.tolerance`. |
| `--screen-px P` | Overrides bundle `config.tolerance_screen_px`. `0` disables screen-px checking. |
| `--jobs N` | `0` resolves automatically; positive values cap TBB worker count. Negative values are rejected. |
| `--progress-fd FD` | Writes newline-delimited progress JSON to the file descriptor. |
| `--diagnostics PATH` | Writes JSONL diagnostic lifecycle/event rows. |
| `--cancel-file PATH` | Solver polls the path and exits with code `5` after writing a partial output if it appears. |
| `--decompose-paths` | Enables per-vertex/tangent path decomposition route for eligible shape-flat paths. |
| `--fit-canonical-paths` | Enables canonical path topology fitting for eligible shape-flat paths. |
| `--fit-replacement-paths` | Enables fitted replacement path topology before temporal solving. |
| `--emit-landmark-subpaths` | Appends diagnostic landmark subpath outputs for accepted replacement path results. |
| `--solve-mode MODE` | Overrides bundle `solve_optimization_mode`. |
| `--solve-optimization-mode MODE` | Alias for `--solve-mode`. |
| `--verbose` | Prints per-property summary lines. |

Environment defaults are also supported for path options:

- `BBSOLVER_DECOMPOSE_PATHS`
- `BBSOLVER_FIT_CANONICAL_PATHS`
- `BBSOLVER_FIT_REPLACEMENT_PATHS`
- `BBSOLVER_EMIT_LANDMARK_SUBPATHS`

CLI flags win by forcing the corresponding option on.

Use the `BBSOLVER_*` environment-variable form for all host automation.
The v1 package does not document or depend on pre-standalone compatibility
aliases.

## SolverConfig Fields

The SampleBundle `config` object is the complete solver knob surface. New JSON
producers should use the snake_case names below.

| Field | Default | Notes |
|---|---:|---|
| `tolerance` | `0.5` | Main property-unit accuracy. |
| `tolerance_screen_px` | `0.0` in solver, `1.0` from AE panel | Screen-space acceptance field. |
| `weight_pos` | `1.0` | Ceres residual weighting, not an acceptance gate. |
| `weight_vel` | `0.1` | Penalizes velocity drift during nonlinear fitting. |
| `weight_acc` | `0.01` | Penalizes acceleration/jerk. |
| `weight_curv` | `0.0` | Curvature term, off by default. |
| `weight_screen` | `0.0` | Screen-space residual weight when screen px is active. |
| `allow_hold` | true | Allows Hold segments. |
| `allow_linear` | true | Allows Linear segments. |
| `allow_bezier` | true | Allows Bezier segments and smoothing ease. |
| `allow_shape_temporal_bezier` | false | Enables shape-temporal Bezier fitting for paths. |
| `allow_path_spatial_fit` | false | Enables canonical path fitting when mode permits topology changes. |
| `allow_path_replacement_fit` | false | Enables fitted replacement topology when mode permits topology changes. |
| `path_replacement_prefer_vertices` | false | Allows post-temporal vertex pruning / vertex preference behavior. |
| `solve_optimization_mode` | `full` | See Solve Modes. |
| `motion_smooth_use_ease` | false | Enables smoothing Bezier/rove behavior where applicable. |
| `motion_smooth_source_fidelity` | false | Adds source-pose constraints for Motion Smooth shape paths. |
| `motion_smooth_tolerance` | `3.0` | Motion Smooth smoothing strength. |
| `motion_smooth_bezier_x1/y1/x2/y2` | `0.33,0,0.67,1` | Timing curve for smoothing ease. |
| `motion_path_smoothing_tolerance` | `3.0` | Motion Path Smooth unitless fairing strength, clamped to `1.0`-`32.0`. |
| `motion_path_accuracy_tolerance` | `1.5` | Motion Path Smooth reduced-key accuracy against the smoothed path, in property units; Position uses comp pixels. The AE test panel clamps this to `0.1`-`200`. |
| `motion_path_preserve_bounds` | false | Enables global bounds preservation in Motion Path Smooth. |
| `motion_path_bounds_tolerance` | `0.0` | Per-side global bounds tolerance for Motion Path Smooth, in property units; Position uses comp pixels. The AE test panel clamps this to `0`-`500`. |
| `motion_path_preserve_sharp_points` | true | Locks sharp path reversals. |
| `motion_path_sharp_angle_deg` | `75.0` | Angle threshold for sharp motion path points. |
| `motion_path_respect_keyed_frames` | false | Locks source keyed frames in Motion Path Smooth. |
| `path_preserve_sharp_corners` | true | Locks persistent shape corners. |
| `path_sharp_corner_angle_deg` | `90.0` | Corner angle threshold for Shape Path preservation. |
| `path_sharp_corner_tolerance` | `1.5` | Corner lock tolerance floor/scaled bound. |
| `path_replacement_min_vertices` | `4` | Minimum replacement/pruned path vertex count. |
| `path_replacement_max_vertices` | `0` | `0` means no explicit configured maximum. |
| `path_replacement_max_key_growth_ratio` | `4.0` | Bound for accepting vertex-priority candidates with more keys. |
| `path_replacement_min_vertex_reduction_ratio` | `0.20` | Required vertex reduction for some vertex-priority decisions. |
| `path_specific_max_gap` | `0` | `0` means use default/env max-gap policy. |
| `shape_temporal_bezier_attempt_threshold_ratio` | `-1.0` | Negative disables the threshold gate. |
| `min_influence` | `0.1` | Ease influence clamp. |
| `max_influence` | `100.0` | Ease influence clamp. |
| `max_iters_per_segment` | `100` | Ceres iteration cap per segment. |
| `min_segment_frames` | `2` | Minimum segment length. |
| `max_keys_hint` | `0` | Optional planning hint. |
| `parallel_jobs` | `0` | Runtime-resolved parallelism. |
| `placement_strategy` | `dp` | Dynamic-programming placement strategy. |
| `verbose` | false | Prints per-property solver summaries. |

### Compatibility aliases

The parser still accepts a small set of camelCase names from older host-panel
builds and pre-standalone test fixtures. These aliases are not a second public
schema and new integrations should not emit them. They exist so older saved
settings, sample bundles, and test artifacts do not fail immediately after the
solver's JSON contract moved to snake_case.

Accepted compatibility aliases include `solveOptimizationMode`,
`motionSmoothUseEase`, `motionSmoothSourceFidelity`,
`motionSmoothSourceKeyFidelity`, `motionSmoothTolerance`,
`motionSmoothBezierX1`, `motionSmoothBezierY1`, `motionSmoothBezierX2`,
`motionSmoothBezierY2`, `motionPathSmoothingTolerance`,
`motionPathAccuracyTolerance`, `motionPathPreserveSharpPoints`,
`motionPathSharpAngleDeg`, and `motionPathRespectKeyedFrames`.

There are also a few older snake_case aliases retained for the same reason:
`shape_temporal_bezier`, `path_spatial_fit`, `path_replacement_fit`,
`motion_smooth_source_key_fidelity`, and
`shape_temporal_bezier_gate_ratio`.

## How Properties Route Through The Solver

For every property, the solve command runs the same high-level phases:

1. prepare path inputs and optional path fitting
2. prepare temporal samples and emit a `property_start` progress event
3. choose the property solve route
4. solve the property
5. run post-solve reducers and cleanup gates
6. append the result to the output bundle

The route order is:

1. preserve source keys for already-near-optimal shape paths
2. motion smoothing when the mode uses motion smoothing
3. frame-key fallback when temporal optimization is disabled
4. replacement shape-flat temporal solve when a path fit produced replacement
   samples
5. path decomposition when `--decompose-paths` applies
6. ordinary temporal solve

This matters because modes and path flags can be set at the same time. For
example, `motion_smooth` wins over ordinary temporal solving, while
`temporal_only` suppresses topology reducers even if a path fit flag is present.

## Motion Smooth

Motion Smooth is a solver mode for smoothing a keyed motion range. It is useful
when source keys are artist intent anchors, but sampled in-between motion is too
noisy or over-defined.

### Spatial Properties

For Position-style spatial properties, Motion Smooth:

- reads the sampled point cloud
- smooths it using `motion_smooth_tolerance`
- uses source key times when at least two source key times exist
- otherwise simplifies the smoothed point cloud with an RDP-style schedule
- emits spatial Bezier tangents clamped to adjacent segment length
- optionally applies the configured smoothing Bezier ease

Output notes include tokens such as:

- `solve_mode_motion_smooth`
- `motion_smooth_spatial_trajectory_filter=true`
- `motion_smooth_source_key_times=true|false`
- `smoothing_strength=...`
- `motion_smooth_ease=on|off`
- `source_error_not_constrained=true`

### Shape Paths

For `shape_flat` paths, Motion Smooth is a shape trajectory smoother. It
requires:

- at least two samples
- valid fixed topology
- stable vertex count and dimensions
- at least two source key times

If those gates fail, it falls back to frame keys with notes such as
`solve_mode_motion_smooth_skipped: variable_shape_topology` or
`solve_mode_motion_smooth_skipped: no_source_key_schedule`.

When accepted, it:

- builds a source-key schedule
- smooths the trajectory of the full shape
- optionally constrains source poses with `motion_smooth_source_fidelity`
- locks rotational tangents for shape control points
- can apply adaptive closed-loop resampling
- emits roved shape keys
- records detailed motion-quality and smoothing notes

Source fidelity is strongest when the original keyed poses are important and
the in-between path needs smoothing. Leave it off when the visual goal is a
more globally smoothed path.

### Endpoint Fallback

For non-spatial non-path properties, Motion Smooth emits endpoint keys and does
not evaluate source error. Notes include `source_error_not_evaluated=true`.

## Motion Path Smooth

Motion Path Smooth is a spatial trajectory mode intended for Position-like
properties. It should be used when you want to smooth the path of motion while
preserving specific path features.

It:

- always locks the first and last samples
- smooths non-locked samples with `motion_path_smoothing_tolerance`
- treats `motion_path_smoothing_tolerance` as a dimensionless fairing strength:
  default `3.0`, minimum `1.0`, maximum `32.0`
- keeps emitted keys within `motion_path_accuracy_tolerance` of the smoothed
  path; this accuracy is measured in property units, so Position uses comp
  pixels. The default is `1.5`; the AE test panel exposes `0.1`-`200`.
- optionally constrains global motion-path bounds with
  `motion_path_preserve_bounds`; when enabled,
  `motion_path_bounds_tolerance` is the per-side allowed deviation in property
  units. For Position, `0` comp px keeps the original outer bounds, and larger
  values allow the footprint to shrink or expand by that many pixels per side.
- optionally locks sharp turns such as bounce impacts
- optionally locks source keyed frames
- emits zero handles and non-continuous flags on locked points
- can apply the same smoothing Bezier ease controls as Motion Smooth

### Smooth Arc Example

For a ball traveling in a broad arc with noisy sampled motion:

- use `motion_path_smooth`
- keep `Preserve sharp motion-path reversals` on, but it may find no sharp
  points
- leave `Respect keyed frames` off unless the source keys are exact poses
- start with smoothing tolerance `3.0` and accuracy tolerance `1.5`
- enable smoothing ease if you want steadier time interpolation

The expected notes include
`motion_path_spatial_trajectory_filter=true` and a reduced output key count.

### Bouncing Ball Reversal Example

For a bouncing ball with a sharp floor impact:

- use `motion_path_smooth`
- keep `Preserve sharp motion-path reversals` on
- start with sharp angle `75` degrees
- enable `Respect keyed frames` if the impact is explicitly keyed
- avoid overly high smoothing tolerance if the bounce needs to stay crisp

The impact sample is locked when the turn angle meets the threshold. Notes
report `motion_path_sharp_points=N` and, when source key locking is enabled,
`motion_path_keyed_points=N`.

## Shape Path Handling

AE Shape Path and Mask Path properties are encoded as `Custom` properties with
`units_label == "shape_flat"`.

Each sample value is:

```text
[ closed_flag,
  vertex_count,
  vx0, vy0, in_x0, in_y0, out_x0, out_y0,
  vx1, vy1, in_x1, in_y1, out_x1, out_y1,
  ... ]
```

Total dimension is `2 + 6 * vertex_count`.

### Fixed Topology

When vertex count and closed/open state are stable, the solver can use path
specific routes:

- shape-temporal Bezier spans
- path decomposition
- canonical fitting
- fitted replacement
- post-temporal vertex pruning
- landmark subpath diagnostics

### Variable Topology

The AE panel samples expression-generated paths as raw `shape_flat` frames. When
the vertex count changes over time, it marks the property as variable topology
and expects solver-side replacement/canonical fitting to build a stable fitted
layout before temporal reduction. A path that toggles open/closed is not treated
as the same topology and is skipped by the path-specific route.

Raw SampleBundles with malformed or changing topology fall back to raw frame
keys or safer flat Hold/Linear behavior when no fitted topology is accepted, and
record a topology note.

### Canonical Path Fitting

Canonical path fitting (`allow_path_spatial_fit` or `--fit-canonical-paths`)
tries to fit a stable path topology before temporal key reduction. It is only
used when the solve mode permits spatial topology changes. It is conservative:
if the fitted path cannot stay inside tolerance, the solver keeps the original
path stream.

### Fitted Replacement Path

Fitted replacement path solving (`allow_path_replacement_fit` or
`--fit-replacement-paths`) is the heavier advanced path route. It attempts
to replace the source path with a lower-vertex representation before temporal
solving.

The replacement pipeline:

1. fits each sampled frame independently to estimate needed vertex counts
2. refits every frame to a fixed target vertex count
3. tries fraction-coherent seeds so vertex slots mean the same thing over time
4. adaptively inserts fractions when fixed-count seeds fail
5. validates final temporal output against outline error and sharp-corner gates
6. falls back to the original solve if any acceptance contract fails

Notes include tokens such as:

- `path_replacement_fit`
- `path_replacement_fit skipped: ...`
- `path_replacement_accepted_stage1_vertex_reduction`
- `source_vertices=...`
- `fitted_vertices=...`
- `frame_outline_error=...`
- `fraction_coherence=...`

### Post-Temporal Vertex Pruning

The second path simplification lane runs after key reduction. It tries to prune
vertices from already-accepted shape keys while preserving key timing and
outline tolerance.

It is controlled by:

- `path_replacement_prefer_vertices`
- AE `Run second-pass vertex prune after key reduction`
- `vertex_only` mode
- cleanup-pass settings

It never blindly removes a point. It validates candidate removals, respects
sharp-corner protection when enabled, and rejects the whole attempt if a
matching key cannot be reduced safely.

### Sharp Corner Preservation

Sharp corner preservation protects persistent high-angle path landmarks. It is
enabled by default in AE and by `path_preserve_sharp_corners`.

Important fields:

- `path_sharp_corner_angle_deg`: defaults to `90`
- `path_sharp_corner_tolerance`: defaults to `1.5`

The lock tolerance also scales with the active solve tolerance, so loose solves
can still prune smooth bridge vertices without deleting intentional corners.

### Landmark Subpaths

`--emit-landmark-subpaths` is diagnostic. When enabled for an
accepted replacement path, the solver appends additional `PropertyKeys` entries
with notes beginning `landmark_subpath`. AE can route those to adjacent
`bb_lm_N` paths for inspection. Do not use this for ordinary production bakes.

## Cleanup Passes

The AE panel can run a second solver pass over accepted first-pass keys before
writeback.

| Mode | Behavior |
|---|---|
| `Prompt` | Shows eligible cleanup candidates and asks before running. |
| `Auto` | Runs eligible cleanup automatically. |
| `Off` | Skips cleanup. |

Cleanup uses accepted keys as its input samples. This avoids resampling AE
expressions and lets the solver try lower-key or lower-vertex replacements over
the candidate it already accepted.

Motion Smooth path results can use `Motion smooth cleanup tol` for the temporal
cleanup pass. This is separate from the first-pass Motion Smooth tolerance.

## Progress, Diagnostics, And Cancellation

### Progress JSON

`--progress-fd FD` writes newline-delimited JSON progress events. Common events
include:

- `solve_start`
- `parallel_config`
- `property_prepare`
- `property_start`
- `temporal_solve_progress`
- `replacement_temporal_solve_progress`
- `temporal_solve_done`
- `post_solve_vertex_reduction_start`
- `post_solve_vertex_reduction_done`
- `static_key_run_collapse`
- `final_static_boundary_anchor`
- `temporal_refit_done`
- `property_done`
- `done`

Path routes can add more specialized events such as `vert_done`,
`path_replacement_fit`, `path_fit`, `visible_outline_prepass`,
`path_geometry_refine_done`, and bridge-prune candidate events.

Progress values are normalized to the property loop. They are meant for UI
feedback, not for exact timing.

### Diagnostics JSONL

`--diagnostics PATH` writes side-effect-free diagnostic event JSONL. Current
diagnostic events include lifecycle, parallel runtime, solve mode capabilities,
cancellation status, and bridge-prune result/phase events. Diagnostics do not
replace progress JSON; they are a separate troubleshooting surface.

### Cancellation

`--cancel-file PATH` asks the solver to poll for a sentinel file. If the file
appears, the solver writes a partial KeyBundle, marks already appended property
results non-converged with a `cancelled` note, emits cancellation diagnostics
when diagnostics are active, and exits with code `5`.

The AE panel uses this to support cooperative cancellation during long solves.

## Jobs And Parallelism

`--jobs` controls the solve-scoped parallel runtime limiter.

| Value | Meaning |
|---:|---|
| `0` | Auto-detect worker count and clamp to the solver hard cap. |
| `1` | Deterministic single-worker execution. |
| `N > 1` | Limit parallel work to `N` workers, subject to the hard cap. |

If TBB is unavailable, the solver reports that in progress/diagnostics and
falls back to the available runtime behavior. Negative job counts are rejected.

Use `--jobs 1` when investigating determinism or reproducing a bug. Use `0`
for normal panel bakes.

## Output Notes

`PropertyKeys.notes` is the best place to understand unusual behavior. Notes
are semicolon-separated tokens. Common examples:

| Token | Meaning |
|---|---|
| `solve_mode_vertex_only` | Temporal optimization was intentionally skipped. |
| `solve_mode_motion_smooth` | Motion Smooth route handled the property. |
| `solve_mode_motion_path_smooth` | Motion Path Smooth spatial route handled the property. |
| `source_error_not_constrained=true` | A smoothing mode did not promise exact source-sample fidelity. |
| `source_key_timing_preserved=true` | Source key timing was copied to output keys. |
| `path_replacement_fit` | A fitted replacement path was attempted or accepted. |
| `path_replacement_fit skipped: ...` | Replacement did not pass a gate. |
| `post_solve_vertex_reduction_accepted; mode=post_temporal_bridge_prune` | Post-temporal path pruning accepted a vertex reduction. |
| `motion_smooth_temporal_cleanup_rejected=true` | A cleanup candidate would have degraded shape motion quality. |
| `no_practical_optimization_at_accuracy_gate=true` | The current tolerance is too strict for meaningful key reduction. |
| `cancelled` | The solve was stopped by a cancellation sentinel. |

When a result has more keys than expected, first inspect `notes`, then compare
`max_err`, `max_err_screen_px`, source sample count, and tolerance.

## Recommended Workflows

### General Property Bake

Use:

- `Full`
- tolerance `0.5`
- screen px `1.0`
- jobs `0`
- path replacement off

If the result has too many keys, loosen tolerance. If it drifts visibly, tighten
tolerance or screen px.

### Hero Position With Smooth Arc

Use:

- `motion_path_smooth`
- motion path smoothing tolerance `3.0`
- motion path accuracy tolerance `1.5`
- preserve sharp reversals on
- respect keyed frames off unless source keys are intentional anchors
- ease/rove on if steadier time interpolation is desired

This is appropriate for a smooth ball flight, camera move, cursor trail, or
other motion where the path should become cleaner than the sampled input.

### Bouncing Ball With Sharp Impact

Use:

- `motion_path_smooth`
- preserve sharp reversals on
- sharp reversal angle around `75`
- respect keyed frames on when the impact frame is explicitly keyed

If the impact rounds off, lower the sharp angle threshold or enable keyed-frame
respect. If the arc remains too noisy, increase smoothing tolerance carefully.

### Shape Path With Too Many Keys

Use:

- `Full`
- cleanup pass `Prompt`

If the path still has too many vertices, try:

- `Run second-pass vertex prune after key reduction`
- preserve sharp path corners on

If the path has many expression-generated vertices and needs a true lower-
vertex representation, then try fitted replacement path.

### Preserve Exact Path Topology

Use:

- `temporal_only`
- fitted replacement off
- vertex prune off

This keeps vertex count stable while still reducing key count in time.

### Test Vertex Reduction Alone

Use:

- `vertex_only`
- preserve sharp path corners on
- fitted replacement off unless specifically testing it

This keeps timing dense/source-like and isolates path vertex behavior.

### Debug A Slow Bake

Use:

- `--jobs 1` to reproduce deterministically
- `--progress-fd 1` for live progress
- `--diagnostics diagnostics.jsonl` for lifecycle and runtime events
- `--verbose` for property summaries

For AE, keep the progress log and output `.bbky.json` artifacts. The notes
usually reveal whether the time went into path replacement, temporal placement,
or post-solve pruning.

## Troubleshooting

### Too Many Keys

Likely causes:

- tolerance is too strict
- source motion contains real high-frequency detail
- screen px is active and tighter than property tolerance
- path topology or shape features force more anchors
- `source fidelity` or `respect keyed frames` intentionally locks anchors

Try:

- loosen tolerance or screen px
- use `motion_path_smooth` for noisy spatial arcs
- disable source/key locks if they are not artist-intent anchors
- inspect `no_practical_optimization_at_accuracy_gate` notes

### Shape Path Corners Disappear

Try:

- keep `Preserve sharp path corners` on
- lower path tolerance
- avoid fitted replacement until the ordinary temporal path is acceptable
- inspect notes for sharp-corner rejected/accepted decisions

### Motion Smooth Ignores Some Accuracy Expectations

Motion Smooth and Motion Path Smooth are smoothing modes. They may record
`source_error_not_constrained=true` because their goal is a cleaner trajectory,
not exact reproduction of every input sample. Use `full` or `temporal_only` if
you need strict source fidelity.

### Motion Path Smooth Does Not Affect Shape Paths As Expected

The dedicated Motion Path Smooth implementation is currently for spatial
Position-style properties. Shape Path smoothing is handled by the existing
Motion Smooth shape-flat route. For path geometry cleanup, use the Shape Path
features in `full`, `temporal_only`, or `vertex_only` instead.

### Cancellation Wrote A Partial Output

That is expected. Exit code `5` means cancellation, not corruption. The partial
KeyBundle contains completed property results and `cancelled` notes where
applicable. The AE panel should treat it as an aborted bake rather than a
successful final result.

## Implementation Status Notes

- `motion_path_smooth` is available in the solver mode policy, CLI help,
  config parser, diagnostics capability event, AE settings defaults, and the
  current AE advanced settings surface. Its dedicated implementation is
  spatial-property focused.
- The normal Shape Path user path is still `shape_flat` output in the main
  property result. Landmark subpaths are diagnostic add-ons.
- Fitted replacement path solving is intentionally advanced and guarded by
  acceptance checks. Rejections are normal when a candidate cannot beat the
  original temporal solve within tolerance.
- Diagnostics are event-builder/lifecycle surfaces. Pure solver helpers do not
  own runtime diagnostics emission.
