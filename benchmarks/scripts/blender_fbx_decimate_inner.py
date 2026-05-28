"""Blender-side runner: build one Action with all FBX mocap F-curves from a
bbsm.json, run Blender's F-Curve Decimate (ERROR mode) on all of them at
the supplied tolerance, then write per-curve decimated keys + max
absolute residual versus the source samples back to disk as JSON.

Invocation:
    blender --background --factory-startup \
        --python blender_fbx_decimate_inner.py -- \
        --input <bbsm.json> --tolerance <eps> --output <out.json>

Operates on every property regardless of kind (Scalar, TwoD, TwoD_Spatial,
ThreeD, ThreeD_Spatial, Color). Each property's per-dimension samples are
turned into separate F-curves and decimated independently. This is exactly
what an artist using Blender's Graph Editor → Decimate ('Allowed Error')
workflow would do on the same FBX import.

Output JSON shape:
{
    "schema_version": 2,
    "meta": {"tolerance": eps, "total_samples": <int>, "wall_clock_ms": <float>},
    "properties": [
        {
            "property_id": "fbx:Armature:pose_bone:mixamorig:Head:rotation",
            "dimensions": 3,
            "channels": [
                {
                    "axis_index": 0,
                    "keys": [{"t_sec": 0.0, "value": ..., "left": [...], "right": [...]}],
                    "max_abs_residual": 0.123
                },
                ...
            ]
        }
    ]
}
"""
from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

import bpy


def _argv():
    if "--" not in sys.argv:
        return []
    return sys.argv[sys.argv.index("--") + 1 :]


def _build_action(bundle):
    """Build a single Blender Action containing one F-curve per (property, axis)."""
    fps = float(bundle.get("comp", {}).get("fps", 60.0))
    action = bpy.data.actions.new("mocap_decimate")

    fcurve_index = 0
    props_meta = []  # parallel list of (property_id, dimensions, fcurve_indices)

    for prop in bundle["properties"]:
        prop_id = prop["property"].get("id", f"prop_{len(props_meta)}")
        dims = int(prop["property"].get("dimensions", 1))
        samples = prop["samples"]
        if not samples:
            continue
        fcurve_idxs = []
        for axis_index in range(dims):
            # Each F-curve uses data_path="location" with index = axis_index;
            # the data_path itself doesn't matter (we never assign to an object),
            # but Blender requires F-curves to have one. We use distinct indices
            # by varying the action_group prefix so curves don't collide.
            fcurve = action.fcurves.new(
                data_path=f"pose.bones[\"bbsolver_{fcurve_index}\"].location",
                index=axis_index,
            )
            # Pre-allocate keyframe points
            kp = fcurve.keyframe_points
            kp.add(len(samples) - 1)  # 1 implicit key, then add the rest
            for kp_obj, sample in zip(kp, samples):
                v = sample["v"]
                value = float(v[axis_index]) if axis_index < len(v) else 0.0
                kp_obj.co = (float(sample["t_sec"]) * fps, value)
                kp_obj.interpolation = "BEZIER"
                kp_obj.select_control_point = True
            fcurve.select = True
            fcurve.update()
            fcurve_idxs.append(fcurve_index)
            fcurve_index += 1
        props_meta.append((prop_id, dims, fcurve_idxs))

    # Bind action to an object so Graph Editor has something to operate on
    obj = bpy.data.objects.new("bbsolver_benchmark_object", None)
    bpy.context.scene.collection.objects.link(obj)
    obj.animation_data_create()
    obj.animation_data.action = action
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)

    return action, props_meta, fps


def _graph_editor_override():
    window = bpy.context.window_manager.windows[0] if bpy.context.window_manager.windows else None
    screen = window.screen if window is not None else bpy.context.screen
    if screen is None:
        raise RuntimeError("no Blender screen available")
    area = next((a for a in screen.areas if a.type == "GRAPH_EDITOR"), None)
    if area is None:
        area = screen.areas[0]
        area.type = "GRAPH_EDITOR"
    region = next((r for r in area.regions if r.type == "WINDOW"), None)
    if region is None:
        raise RuntimeError("Graph Editor has no WINDOW region")
    return {"window": window, "screen": screen, "area": area, "region": region}


def _decimate_all(action, tolerance):
    for fcurve in action.fcurves:
        fcurve.select = True
        for k in fcurve.keyframe_points:
            k.select_control_point = True
    with bpy.context.temp_override(**_graph_editor_override()):
        bpy.ops.graph.decimate(mode="ERROR", remove_error_margin=tolerance)


def _residual_against_samples(fcurve, samples, axis_index, fps):
    """Compute max absolute residual between fcurve evaluation and source samples
    at each sample time."""
    max_abs = 0.0
    for s in samples:
        t = float(s["t_sec"])
        v_src = float(s["v"][axis_index]) if axis_index < len(s["v"]) else 0.0
        v_eval = fcurve.evaluate(t * fps)
        diff = abs(v_eval - v_src)
        if diff > max_abs:
            max_abs = diff
    return max_abs


def _extract(action, props_meta, samples_by_prop, fps):
    properties = []
    fcurves_by_idx = {}
    for fc in action.fcurves:
        # F-curves are returned in creation order; map by their internal data_path
        # which encodes the original fcurve_index.
        dp = fc.data_path
        # "pose.bones[\"bbsolver_<N>\"].location" → N
        try:
            n = int(dp.split("bbsolver_")[1].split("\"]")[0])
        except (IndexError, ValueError):
            continue
        fcurves_by_idx[(n, fc.array_index)] = fc

    for prop_id, dims, fcurve_idxs in props_meta:
        samples = samples_by_prop[prop_id]
        channels = []
        for axis_index, fidx in enumerate(fcurve_idxs):
            fc = fcurves_by_idx.get((fidx, axis_index))
            if fc is None:
                continue
            keys = []
            for kp in sorted(fc.keyframe_points, key=lambda k: k.co.x):
                keys.append({
                    "t_sec": float(kp.co.x) / fps,
                    "value": float(kp.co.y),
                    "left": [float(kp.handle_left.x) / fps, float(kp.handle_left.y)],
                    "right": [float(kp.handle_right.x) / fps, float(kp.handle_right.y)],
                })
            max_abs = _residual_against_samples(fc, samples, axis_index, fps)
            channels.append({
                "axis_index": axis_index,
                "keys": keys,
                "max_abs_residual": max_abs,
            })
        properties.append({
            "property_id": prop_id,
            "dimensions": dims,
            "channels": channels,
        })
    return properties


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--tolerance", required=True, type=float)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args(_argv())

    bundle = json.loads(args.input.read_text())
    total_samples = sum(len(p["samples"]) * int(p["property"].get("dimensions", 1))
                        for p in bundle["properties"])
    samples_by_prop = {p["property"].get("id", f"prop_{i}"): p["samples"]
                       for i, p in enumerate(bundle["properties"])}

    t0 = time.time()
    action, props_meta, fps = _build_action(bundle)
    _decimate_all(action, args.tolerance)
    properties = _extract(action, props_meta, samples_by_prop, fps)
    elapsed_ms = (time.time() - t0) * 1000.0

    out = {
        "schema_version": 2,
        "meta": {
            "tolerance": args.tolerance,
            "total_samples": total_samples,
            "wall_clock_ms": elapsed_ms,
            "fps": fps,
        },
        "properties": properties,
    }
    args.output.write_text(json.dumps(out, indent=2) + "\n")
    bpy.ops.wm.quit_blender()


if __name__ == "__main__":
    main()
