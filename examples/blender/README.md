# Blender FBX harness

This example harness converts Blender-importable FBX animation into a
bbsolver SampleBundle, runs the solver, and applies the solved KeyBundle
back onto the imported rig for inspection in Blender.

The harness is meant for rigged mocap-style animation where the useful solve
surface is local rig channels rather than baked world-space joint paths.
`fbx_to_bbsm.py` samples armature pose-bone `matrix_basis` channels and emits
ordinary `ThreeD` bbsolver properties for local location, Euler rotation, and
scale. `bbky_apply_to_fbx.py` maps solved property ids back to pose-bone,
armature-object, or object transform F-curves.

For sparse FBX writeback, use `fbx_bbsolver_roundtrip.py`. It converts the
binary FBX to Blender's raw FBX JSON tree, extracts every animated transform
curve from the selected FBX animation layer, runs bbsolver, patches the solved
sparse keys back into the raw FBX JSON, and converts that JSON back to binary
FBX. This path sees animation curves directly, including channels that may not
survive a Blender pose-bone sampling filter.

## Full FBX-to-FBX roundtrip

```sh
python3 examples/blender/fbx_bbsolver_roundtrip.py \
  input.fbx output_sparse.fbx \
  --tolerance 3 \
  --screen-px 0 \
  --jobs 0
```

The script writes intermediate files under `output_sparse_roundtrip/` by
default:

- `*.raw.json`: Blender `fbx2json.py` output from the source binary FBX.
- `*.bbsm.json`: bbsolver SampleBundle extracted from all animated FBX curve
  nodes in the selected animation layer.
- `*.bbky.json`: solved sparse KeyBundle.
- `*.patched.json`: raw FBX JSON with solved `AnimationCurve` time/value arrays.

By default the script uses:

- `/Applications/Blender.app/Contents/Resources/4.5` for Blender's
  `io_scene_fbx` helper scripts.
- `BBSOLVER_BINARY`, if set, otherwise `~/.bbsolver/bin/bbsolver`.
- `BaseLayer::AnimLayer` as the FBX animation layer to solve.

Current limitation: Blender's FBX importer reads imported FBX animation curves
as linear keyframes. The sparse FBX is therefore suitable for proving reduced
interpolated FBX writeback, but Bezier tangent metadata is not yet roundtripped
as Blender-visible Bezier handles.

## Convert FBX to SampleBundle

```sh
/Applications/Blender.app/Contents/MacOS/Blender --background \
  --python examples/blender/fbx_to_bbsm.py -- \
  input.fbx output.bbsm.json \
  --skip-constant \
  --tolerance 3 \
  --jobs 0
```

Useful options:

- `--channels location,rotation,scale` controls exported transform channels.
- `--bone-regex REGEX` limits exported pose bones.
- `--max-bones N` creates small smoke fixtures.
- `--rotation-order XYZ` and `--rotation-units degrees` control Euler export.
- `--source-key-times samples` records every sampled source time.

Validate the generated bundle:

```sh
python3 scripts/validate_json_bundle.py output.bbsm.json
```

## Solve

```sh
./build/bbsolver solve output.bbsm.json output.bbky.json --tolerance 3 --jobs 0
./build/bbsolver verify output.bbky.json output.bbsm.json
```

## Apply solved keys back to Blender

```sh
/Applications/Blender.app/Contents/MacOS/Blender --background \
  --python examples/blender/bbky_apply_to_fbx.py -- \
  input.fbx output.bbky.json output_optimized.blend \
  --export-fbx output_optimized.fbx
```

Inspect the `.blend` first. Blender preserves the optimized layered Action
data directly. The optional FBX export is a baked preview export because the
Blender FBX exporter does not preserve Blender 4.5 layered Action slots as
reduced editable curves.
