# FBX Bezier Tangent Roundtrip Notes

## Current State

`fbx_bbsolver_roundtrip.py` writes sparse FBX animation curves by patching the
raw `AnimationCurve` records in Blender's `fbx2json.py` tree. The current patch
updates:

- `KeyTime`
- `KeyValueFloat`
- `KeyAttrRefCount`
- `Default`

This removes frame-by-frame keys and produces a valid sparse FBX. It does not
yet encode bbsolver's Bezier handles into FBX tangent metadata.

## Why Blender Re-imports Sparse Curves As Linear

Blender 4.5's stock FBX importer reads only `KeyTime` and `KeyValueFloat` from
each FBX animation curve. It does not read `KeyAttrFlags`, `KeyAttrDataFloat`,
or `KeyAttrRefCount` when creating Blender keyframes.

The importer then explicitly fills every generated Blender keyframe with
`LINEAR` interpolation in `blen_store_keyframes_multi()`. This is why even a
valid FBX cubic curve will not reappear as Blender Bezier handles through the
stock importer.

Local evidence:

- `/Applications/Blender.app/Contents/Resources/4.5/scripts/addons_core/io_scene_fbx/import_fbx.py`
  - `blen_read_animation_curve()` reads `KeyTime` and `KeyValueFloat`.
  - `blen_store_keyframes_multi()` says linear interpolation is assumed and
    writes `LINEAR` for all imported keyframes.
- `/Applications/Blender.app/Contents/Resources/4.5/scripts/addons_core/io_scene_fbx/export_fbx_bin.py`
  - Blender's exporter writes `KeyAttrFlags` and `KeyAttrDataFloat`, but uses
    linear-style flags for its baked export path.

## What FBX Needs For Cubic Curves

FBX animation curves can represent cubic/Bezier-style interpolation. Autodesk's
FBX SDK exposes this through `FbxAnimCurve` and `FbxAnimCurveKey`:

- interpolation modes include constant, linear, and cubic;
- tangent modes include auto, TCB, user, break, clamp, and time-independent
  variants;
- tangent data includes right slope, next-left slope, weights, and velocities.

In raw FBX JSON terms, this means solving more than `KeyTime` and
`KeyValueFloat`; we need to write coherent values for:

- `KeyAttrFlags`
- `KeyAttrDataFloat`
- `KeyAttrRefCount`

Autodesk references:

- `FbxAnimCurve` describes each curve as sorted keys over time and exposes
  `KeySet()` with interpolation, tangent mode, slope, weight, and velocity
  parameters:
  <https://help.autodesk.com/cloudhelp/2019/ENU/FBX-Developer-Help/cpp_ref/class_fbx_anim_curve.html>
- `FbxAnimCurveDef` defines constant/linear/cubic interpolation, user/break
  tangent modes, weighted modes, and the tangent data indices for right slope,
  next-left slope, right weight, and next-left weight:
  <https://help.autodesk.com/cloudhelp/2019/ENU/FBX-Developer-Help/cpp_ref/fbxanimcurve_8h_source.html>

The likely direct mapping for user/break Bezier tangents is:

- `KeyAttrFlags`: cubic interpolation plus user or break tangent flags, plus
  weighted flags if handle lengths need to be preserved.
- `KeyAttrDataFloat[0]`: right slope.
- `KeyAttrDataFloat[1]`: next-left slope.
- `KeyAttrDataFloat[2]`: right weight when weighted mode is enabled.
- `KeyAttrDataFloat[3]`: next-left weight when weighted mode is enabled.

Exact compatibility should be proven against Autodesk FBX SDK, Maya, MotionBuilder,
or another importer that actually consumes FBX tangent metadata.

## Practical Options

1. **Best current Blender artifact:** keep generating `.blend` from bbsolver
   keys. Blender Python can create real Bezier F-curves and set handles directly.
   This is already the most faithful way to inspect/edit solved curves in
   Blender.

2. **Sparse FBX for interchange:** keep `fbx_bbsolver_roundtrip.py` as the
   sparse FBX transport path. It removes dense keys and preserves the rig/mesh
   graph, but Blender's stock importer displays the keys as linear.

3. **FBX cubic metadata writer:** extend `fbx_bbsolver_roundtrip.py` to encode
   cubic `KeyAttrFlags` and tangent data. This may improve behavior in Autodesk
   tools and other importers. It will not by itself fix stock Blender re-import.

4. **Blender importer patch or add-on:** patch Blender's FBX importer, or create
   a post-import add-on, to read FBX tangent metadata and set Blender
   `Keyframe.interpolation`, `handle_left`, `handle_right`, and handle types.
   This is the only path that makes a raw FBX re-import show Blender Bezier
   handles without a sidecar `.bbky` apply step.

## Recommended Next Step

Build a small one-channel FBX tangent fixture:

1. Create a known two- or three-key Bezier curve in Blender.
2. Export FBX.
3. Convert to raw JSON and inspect `KeyAttrFlags` / `KeyAttrDataFloat`.
4. Recreate equivalent metadata from bbsolver output.
5. Test in Blender, Maya/MotionBuilder if available, and the raw JSON roundtrip.

This will let us separate two questions:

- whether we can write correct FBX cubic tangent data;
- whether the target application actually imports that tangent data.
