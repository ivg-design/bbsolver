#!/usr/bin/env python3
"""Apply a bbsolver KeyBundle back onto an imported FBX rig.

Run with Blender:

    blender --background --python examples/blender/bbky_apply_to_fbx.py -- \
      source.fbx solved.bbky.json output.blend --export-fbx output.fbx

The companion fbx_to_bbsm.py script encodes property ids as:

    fbx:<armature-or-object>:<owner-type>:<target>:<channel>

This applier maps those ids back to armature object, pose-bone, or object
channels and creates reduced Blender F-curves from the solved keys.
"""

from __future__ import annotations

import argparse
import json
import math
import re
import sys
from pathlib import Path
from typing import Any


CHANNEL_TO_PATH = {
    "location": "location",
    "rotation": "rotation_euler",
    "scale": "scale",
}


def blender_args(argv: list[str]) -> list[str]:
    if "--" in argv:
        return argv[argv.index("--") + 1 :]
    return argv[1:]


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Apply bbsolver keys to an FBX rig.")
    parser.add_argument("input_fbx", type=Path, help="Source FBX file")
    parser.add_argument("input_bbky", type=Path, help="Solved .bbky.json file")
    parser.add_argument("output_blend", type=Path, help="Output .blend file")
    parser.add_argument("--export-fbx", type=Path, default=None, help="Optional output .fbx")
    parser.add_argument(
        "--rotation-order",
        default=None,
        choices=("XYZ", "XZY", "YXZ", "YZX", "ZXY", "ZYX"),
        help="Euler order. Default reads generator.rotation_order from sibling .bbsm when available, else XYZ.",
    )
    parser.add_argument(
        "--rotation-units",
        default=None,
        choices=("degrees", "radians"),
        help="Rotation units in KeyBundle. Default reads sibling .bbsm generator, else degrees.",
    )
    parser.add_argument(
        "--frame-start",
        type=float,
        default=None,
        help="Frame for t_sec=0. Default reads sibling .bbsm generator.frame_start, else 1.",
    )
    parser.add_argument(
        "--fps",
        type=float,
        default=None,
        help="FPS for t_sec to frame conversion. Default reads sibling .bbsm comp.fps, else scene FPS.",
    )
    return parser.parse_args(argv)


def require_blender() -> Any:
    try:
        import bpy  # type: ignore
    except ModuleNotFoundError as exc:
        raise SystemExit(
            "bbky_apply_to_fbx.py must be run by Blender, for example:\n"
            "  blender --background --python examples/blender/bbky_apply_to_fbx.py -- source.fbx keys.bbky.json out.blend"
        ) from exc
    return bpy


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if not isinstance(data, dict):
        raise ValueError(f"{path}: expected top-level JSON object")
    return data


def load_sample_metadata(bbky_path: Path) -> dict[str, Any]:
    candidate = bbky_path.with_suffix("").with_suffix(".bbsm.json")
    if not candidate.is_file():
        return {}
    try:
        return load_json(candidate)
    except (OSError, json.JSONDecodeError, ValueError):
        return {}


def scene_fps(scene: Any) -> float:
    fps_base = float(getattr(scene.render, "fps_base", 1.0) or 1.0)
    return float(scene.render.fps) / fps_base


def import_fbx(bpy: Any, path: Path) -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()
    result = bpy.ops.import_scene.fbx(filepath=str(path.resolve()))
    if "FINISHED" not in set(result):
        raise RuntimeError(f"Blender FBX import did not finish: {result}")


def parse_property_id(property_id: str) -> tuple[str, str, str, str] | None:
    match = re.match(
        r"^fbx:(?P<owner>.*?):(?P<kind>pose_bone|armature_object|object):"
        r"(?P<target>.*):(?P<channel>location|rotation|scale)$",
        property_id,
    )
    if not match:
        return None
    return (
        match.group("owner"),
        match.group("kind"),
        match.group("target"),
        match.group("channel"),
    )


def data_path_for(kind: str, target: str, channel: str) -> str:
    path = CHANNEL_TO_PATH[channel]
    if kind != "pose_bone":
        return path
    escaped = target.replace("\\", "\\\\").replace('"', '\\"')
    return f'pose.bones["{escaped}"].{path}'


def make_action(obj: Any) -> Any:
    obj.animation_data_create()
    import bpy  # type: ignore

    action = bpy.data.actions.new(name=f"{obj.name}_bbsolver_optimized")
    action.id_root = "OBJECT"
    obj.animation_data.action = action
    return action


def interp_for_segment(left: dict[str, Any], right: dict[str, Any]) -> str:
    out_interp = left.get("interp_out", "Bezier")
    in_interp = right.get("interp_in", "Bezier")
    if out_interp == "Hold" or in_interp == "Hold":
        return "CONSTANT"
    if out_interp == "Linear" or in_interp == "Linear":
        return "LINEAR"
    return "BEZIER"


def ease_at(eases: list[dict[str, Any]], dim: int) -> dict[str, float]:
    if not eases:
        return {"speed": 0.0, "influence": 33.3}
    item = eases[min(dim, len(eases) - 1)]
    return {
        "speed": float(item.get("speed", 0.0)),
        "influence": max(0.0, min(100.0, float(item.get("influence", 33.3)))),
    }


def key_value(channel: str, raw_value: float, rotation_units: str) -> float:
    if channel == "rotation" and rotation_units == "degrees":
        return math.radians(raw_value)
    return raw_value


def create_fcurves(
    action: Any,
    datablock: Any,
    data_path: str,
    channel: str,
    keys: list[dict[str, Any]],
    *,
    fps: float,
    frame_start: float,
    rotation_units: str,
) -> int:
    if not keys:
        return 0
    written = 0
    for dim in range(3):
        fcurve = action.fcurve_ensure_for_datablock(datablock, data_path, index=dim)
        fcurve.keyframe_points.clear()
        fcurve.keyframe_points.add(len(keys))
        for index, key in enumerate(keys):
            frame = frame_start + float(key["t_sec"]) * fps
            value = key_value(channel, float(key["v"][dim]), rotation_units)
            point = fcurve.keyframe_points[index]
            point.co = (frame, value)
            point.interpolation = (
                interp_for_segment(key, keys[index + 1])
                if index + 1 < len(keys)
                else "BEZIER"
            )
            point.handle_left_type = "AUTO"
            point.handle_right_type = "AUTO"

        for index in range(len(keys) - 1):
            left = keys[index]
            right = keys[index + 1]
            if interp_for_segment(left, right) != "BEZIER":
                continue
            left_point = fcurve.keyframe_points[index]
            right_point = fcurve.keyframe_points[index + 1]
            left_point.handle_right_type = "FREE"
            right_point.handle_left_type = "FREE"
            dt_sec = max(float(right["t_sec"]) - float(left["t_sec"]), 1e-9)
            dt_frame = dt_sec * fps
            out_ease = ease_at(left.get("temporal_ease_out", []), dim)
            in_ease = ease_at(right.get("temporal_ease_in", []), dim)
            left_dx = dt_frame * out_ease["influence"] / 100.0
            right_dx = dt_frame * in_ease["influence"] / 100.0
            left_speed = out_ease["speed"]
            right_speed = in_ease["speed"]
            if channel == "rotation" and rotation_units == "degrees":
                left_speed = math.radians(left_speed)
                right_speed = math.radians(right_speed)
            left_point.handle_right = (
                left_point.co.x + left_dx,
                left_point.co.y + left_speed * (left_dx / fps),
            )
            right_point.handle_left = (
                right_point.co.x - right_dx,
                right_point.co.y - right_speed * (right_dx / fps),
            )
        fcurve.update()
        written += len(keys)
    return written


def target_object(bpy: Any, owner: str, kind: str, target: str) -> Any | None:
    if kind == "object":
        return bpy.data.objects.get(target)
    return bpy.data.objects.get(owner)


def prepare_rotation_modes(obj: Any, kind: str, target: str, channel: str, order: str) -> None:
    if channel != "rotation":
        return
    if kind == "pose_bone":
        bone = obj.pose.bones.get(target) if obj and obj.type == "ARMATURE" else None
        if bone is not None:
            bone.rotation_mode = order
    else:
        obj.rotation_mode = order


def main(argv: list[str]) -> int:
    args = parse_args(blender_args(argv))
    if not args.input_fbx.is_file():
        raise SystemExit(f"bbky_apply_to_fbx: input FBX not found: {args.input_fbx}")
    if not args.input_bbky.is_file():
        raise SystemExit(f"bbky_apply_to_fbx: input KeyBundle not found: {args.input_bbky}")

    bpy = require_blender()
    key_bundle = load_json(args.input_bbky)
    sample_bundle = load_sample_metadata(args.input_bbky)
    generator = sample_bundle.get("generator", {}) if sample_bundle else {}
    comp = sample_bundle.get("comp", {}) if sample_bundle else {}

    import_fbx(bpy, args.input_fbx)
    scene = bpy.context.scene
    fps = args.fps or float(comp.get("fps") or scene_fps(scene))
    frame_start = args.frame_start
    if frame_start is None:
        frame_start = float(generator.get("frame_start", 1.0))
    rotation_order = args.rotation_order or generator.get("rotation_order", "XYZ")
    rotation_units = args.rotation_units or generator.get("rotation_units", "degrees")
    scene.render.fps = int(round(fps))
    scene.frame_start = int(math.floor(frame_start))

    actions_by_object: dict[str, Any] = {}
    applied_properties = 0
    written_component_keys = 0
    max_frame = frame_start
    skipped: list[str] = []

    for result in key_bundle.get("property_results", []):
        parsed = parse_property_id(str(result.get("property_id", "")))
        if parsed is None:
            skipped.append(str(result.get("property_id", "")))
            continue
        owner, kind, target, channel = parsed
        obj = target_object(bpy, owner, kind, target)
        if obj is None:
            skipped.append(str(result.get("property_id", "")))
            continue
        if kind == "pose_bone" and obj.pose.bones.get(target) is None:
            skipped.append(str(result.get("property_id", "")))
            continue
        prepare_rotation_modes(obj, kind, target, channel, rotation_order)
        action = actions_by_object.get(obj.name)
        if action is None:
            action = make_action(obj)
            actions_by_object[obj.name] = action
        count = create_fcurves(
            action,
            obj,
            data_path_for(kind, target, channel),
            channel,
            result.get("keys", []),
            fps=fps,
            frame_start=frame_start,
            rotation_units=rotation_units,
        )
        if count:
            applied_properties += 1
            written_component_keys += count
            for key in result.get("keys", []):
                max_frame = max(max_frame, frame_start + float(key["t_sec"]) * fps)

    if skipped:
        print(f"bbky_apply_to_fbx: warning: skipped {len(skipped)} unmapped properties", file=sys.stderr)
        for item in skipped[:12]:
            print(f"  skipped: {item}", file=sys.stderr)

    args.output_blend.parent.mkdir(parents=True, exist_ok=True)
    scene.frame_start = int(math.floor(frame_start))
    scene.frame_end = int(math.ceil(max_frame))
    bpy.ops.wm.save_as_mainfile(filepath=str(args.output_blend.resolve()))
    if args.export_fbx is not None:
        args.export_fbx.parent.mkdir(parents=True, exist_ok=True)
        bpy.ops.export_scene.fbx(
            filepath=str(args.export_fbx.resolve()),
            bake_anim=True,
            bake_anim_use_all_actions=False,
            bake_anim_use_nla_strips=False,
            bake_anim_step=1.0,
            bake_anim_simplify_factor=0.0,
            add_leaf_bones=False,
        )

    print(
        "bbky_apply_to_fbx: applied "
        f"{applied_properties} properties, {written_component_keys} component keys, "
        f"fps={fps}, frame_start={frame_start}, blend={args.output_blend}"
    )
    if args.export_fbx is not None:
        print(f"bbky_apply_to_fbx: exported {args.export_fbx}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
