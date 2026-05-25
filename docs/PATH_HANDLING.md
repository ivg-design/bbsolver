# Shape Path Handling

`bbsolver` represents AE Shape Path samples as `shape_flat` values: one closed
or open path flattened into a numeric vector with vertices and tangent handles.
The default path route preserves topology and solves the sampled path values as
animated keyframes. Optional path modes can reduce vertices, replace topology, or
emit helper landmark paths for inspection.

## Input Requirements

For topology-preserving solves:

- every sample for the property must have the same vertex count
- every sample must agree on open/closed state
- the flattened dimensions must match the path schema
- vertex, in-tangent, and out-tangent channels must be finite

For variable-topology replacement solves, the host sampler must canonicalize the
path into a solver-ready shape payload and set the shape-variable-topology flag
in the property metadata. The AE test harness includes a variable-topology
checkbox that adds `--fit-replacement-paths` when the selected property was
sampled as variable topology.

## Solve Modes

| Mode | Effect |
|---|---|
| `full` | Runs the normal temporal solve. Eligible paths may also use configured path optimization. |
| `temporal_only` | Reduces keys without changing path topology. |
| `vertex_only` | Runs eligible vertex reduction without temporal key reduction. |
| `motion_smooth` | Smooths sampled path motion where the motion-smooth path route applies. |

## Path Flags

| Flag | Purpose |
|---|---|
| `--decompose-paths` | Attempts per-channel path decomposition for eligible stable-topology paths. |
| `--fit-canonical-paths` | Enables canonical topology fitting when the path can be represented by a stable canonical layout. |
| `--fit-replacement-paths` | Enables variable-topology replacement fitting before temporal solving. |
| `--emit-landmark-subpaths` | Emits diagnostic landmark subpath outputs for accepted replacement results. |

The equivalent environment toggles are:

- `BBSOLVER_DECOMPOSE_PATHS=1`
- `BBSOLVER_FIT_CANONICAL_PATHS=1`
- `BBSOLVER_FIT_REPLACEMENT_PATHS=1`
- `BBSOLVER_EMIT_LANDMARK_SUBPATHS=1`

CLI flags force the matching option on for that process.

## Sharp Corners

The path solver can preserve persistent sharp corners so loose tolerances do not
round intentional cusps. Relevant config fields:

- `path_preserve_sharp_corners`
- `path_sharp_corner_angle_deg`
- `path_sharp_corner_tolerance`

When preservation is enabled, the solver detects stable high-angle corners and
locks them against candidate simplifications that would exceed the corner error
bound.

## Replacement Topology

Replacement fitting is intended for paths whose sampled outlines can be
represented by a cleaner topology than the source. The replacement route:

1. fits candidate outlines for sampled frames
2. searches stable vertex/fraction layouts
3. validates outline error against the configured tolerance
4. builds a temporally solved replacement key stream
5. rejects the replacement if it loses the error contract

Accepted replacements write normal `PropertyKeys` for the source path property.
Rejected replacements fall back to the topology-preserving route.

Useful notes emitted by accepted or rejected replacement attempts include:

- `replacement_path_fit=accepted` or rejection reason
- `target_vertices=N`
- `outline_tolerance=X`
- `fraction_coherence=...`
- `adaptive_fraction_insertions=N`
- `replacement_max_err=X`

## Landmark Subpaths

`--emit-landmark-subpaths` is a diagnostic output mode. When enabled for an
accepted replacement result, the solver appends additional `PropertyKeys` entries
with notes such as:

```text
landmark_subpath; subpath_index=0
```

Host integrations can use these entries to visualize internal landmark tracks.
They are not required for normal solve/apply behavior.

## Validation

Path changes should be validated with:

```sh
cmake --build build --target test_path_fit_pipeline -j 8
./build/test_path_fit_pipeline
ctest --test-dir build -R "^test_path_" --output-on-failure
```

Also run the path-related policies under `tests/policies/` when changing path
layout, public headers, or ownership boundaries.
