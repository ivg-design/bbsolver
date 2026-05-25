# bbsolver JSON examples

Three self-contained JSON SampleBundles that exercise the `bbsolver` CLI end
to end without After Effects or any host application. Each example is small
enough to inspect by eye and verify deterministically against the CLI.

These bundles match the JSON contract documented in
[`../../docs/SOLVER_CLI.md`](../../docs/SOLVER_CLI.md) and
[`../../docs/API_REFERENCE.md`](../../docs/API_REFERENCE.md). JSON Schema
files live in [`../../schemas/`](../../schemas/), and the packaged validator is
[`../../scripts/validate_json_bundle.py`](../../scripts/validate_json_bundle.py).

For an AE-side integration that wraps the same JSON contract in a ScriptUI
panel, see [`../after-effects/`](../after-effects/) and the recipe in
[`../../docs/AE_SCRIPTUI_HARNESS.md`](../../docs/AE_SCRIPTUI_HARNESS.md).

## Examples

| File | Property kind | Samples | Expected reduction | Notes |
|---|---|---|---|---|
| `minimal_scalar.bbsm.json` | `Scalar` (1-D opacity 0 → 100) | 25 | 2 keys (linear ramp) | Smallest possible solve. |
| `minimal_position.bbsm.json` | `TwoD_Spatial` (parabolic arc) | 49 | 2 keys with spatial Bezier tangents | Exercises the unified spatial fitter. |
| `minimal_shape_flat.bbsm.json` | `Custom` `units_label="shape_flat"` (4-vertex square) | 25 | 2 keys at endpoints | Exercises the path-temporal routing. |

## Run the full smoke path

From a fresh checkout, build the CLI and run all three examples through
solve + verify:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# Solve and verify each example. Each verify command should exit 0 (ok=true).
for name in minimal_scalar minimal_position minimal_shape_flat; do
  python3 scripts/validate_json_bundle.py \
    examples/json/${name}.bbsm.json
  ./build/bbsolver solve \
    examples/json/${name}.bbsm.json \
    /tmp/${name}.bbky.json \
    --jobs 1
  python3 scripts/validate_json_bundle.py \
    /tmp/${name}.bbky.json
  ./build/bbsolver verify \
    /tmp/${name}.bbky.json \
    examples/json/${name}.bbsm.json
done
```

Verify prints a JSON report and exits `0` on success or `3` on
non-convergence. The expected outcome for all three bundles is `ok: true`
with `max_err` well under the bundle's `tolerance`.

## Sample report

After running the smoke path on a recent build, you should see per-example
solve summaries like:

```text
minimal_scalar       : 25 samples -> 2 keys, max_err ~3e-3 vs tolerance 0.5
minimal_position     : 49 samples -> 2 keys, max_err ~7e-3 vs tolerance 1.0
minimal_shape_flat   : 25 samples -> 2 keys, max_err ~8e-5 vs tolerance 0.5
```

Exact `max_err` values may shift slightly across builds; the contract that
matters is `verify` exit code `0` and `ok: true` for every property.

## What each bundle proves

- **`minimal_scalar.bbsm.json`** — exercises the dense → sparse temporal
  reduction path on a scalar property. Confirms that a linear ramp collapses
  to two endpoint keys within tolerance, and demonstrates the smallest
  legal `SampleBundle`.
- **`minimal_position.bbsm.json`** — exercises the unified AE spatial fitter
  on a Position-style property (`TwoD_Spatial`, `is_spatial: true`).
  Confirms that a smooth parabolic arc reconstructs from spatial-Bezier
  tangents on two keys within tolerance.
- **`minimal_shape_flat.bbsm.json`** — exercises the path-temporal routing.
  The `Custom` kind with `units_label: "shape_flat"` triggers the shape-path
  pipeline. The 26-double flat layout is documented below.

## SampleBundle shape

Top-level required fields:

- `_schema` — must be `"samples"`. The CLI rejects missing or swapped
  bundle markers before parsing SampleBundle contents.
- `schema_version` — must equal the supported version declared in
  `include/bbsolver/io/schema_contract.hpp`. The CLI rejects
  other values with an "Unsupported SampleBundle schema_version" error.
- `comp` — `CompInfo` (fps, duration_sec, width, height, pixel_aspect).
- `properties[]` — one or more `PropertySamples` blocks.

Each `PropertySamples` block has:

- `property` (`PropertyInfo`): `id`, `kind`, `dimensions`, `is_spatial`,
  `is_separated`, optional `match_name`, `display_name`, `layer_path`,
  `units_label`, `min_value`, `max_value`, `source_key_times`.
- `t_start_sec`, `t_end_sec`, `samples_per_frame`.
- `samples[]` — each sample is `{ t_sec, v: number[] }` with `v.length ==
  property.dimensions * samples_per_frame`; for ordinary frame-center sampling,
  `samples_per_frame` is `1`, so `v.length == property.dimensions`. Raw
  variable-topology `shape_flat` samples may have per-frame flat-vector lengths
  below `property.dimensions`, which is the maximum sampled flat-vector length.
- Optional `config` block at the bundle root sets solver defaults; CLI flags
  override per-run.

See [`../../include/bbsolver/domain.hpp`](../../include/bbsolver/domain.hpp)
for the full type definitions and
[`../../src/io/sample_bundle_io.cpp`](../../src/io/sample_bundle_io.cpp) for
the JSON parser.

### `shape_flat` value encoding

For path properties (`kind: "Custom"`, `units_label: "shape_flat"`), each
sample's `v` array encodes a fixed-topology AE Shape Path as:

```text
v[0]   = closed flag (0.0 = open, 1.0 = closed)
v[1]   = vertex count N
v[2..] = N consecutive 6-tuples [x, y, in_tangent_x, in_tangent_y,
                                  out_tangent_x, out_tangent_y]
```

so `v.length == property.dimensions == 2 + 6 * N`. Tangents are relative
to the vertex (zero for straight-edge corners).
See [`../../include/bbsolver/shape/shape_flat_topology.hpp`](../../include/bbsolver/shape/shape_flat_topology.hpp)
for helpers (`ShapeFlatVertexCount`, `ShapeFlatVertices`,
`ShapeFlatFromVertices`).

## KeyBundle output

`bbsolver solve` writes a KeyBundle JSON with one `property_results[]`
entry per input property. Each entry contains `dimensions`, `keys[]` (sparse
keyframes), `max_err`, `converged`, and `notes`. Every `keys[].v` length
matches the entry's `dimensions`. The KeyBundle preserves the input `request_id`
and `schema_version` so a host can correlate solve responses with its outgoing
requests.

## Adding your own example

1. Copy the closest existing example.
2. Set `_schema: "samples"` and `schema_version: 1`, fill in `comp`, and
   create one property block per channel.
3. Make sure each sample's `v.length == property.dimensions *
   samples_per_frame`, except for raw variable-topology `shape_flat` samples
   whose valid flat-vector length may vary by frame.
4. Pick a `tolerance` appropriate for the units in `v`.
5. Run the smoke path above; if `verify` returns non-zero, inspect
   `max_err` and `notes` in the KeyBundle.

For package-wide validation loops that include these examples, see
[`../../docs/VALIDATION_WORKFLOWS.md`](../../docs/VALIDATION_WORKFLOWS.md).
