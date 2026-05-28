#!/usr/bin/env python3
"""Convert animated FBX transform channels to a bbsolver SampleBundle.

Run this script with Blender, not system Python:

    blender --background --python examples/blender/fbx_to_bbsm.py -- \
      input.fbx output.bbsm.json --skip-constant

The converter imports the FBX, samples evaluated armature pose channels, and
writes ordinary bbsolver scalar/vector properties. Bone transforms are sampled
from pose-bone matrix_basis by default so the output represents local rig
channels instead of baked world-space joint positions.
"""

from __future__ import annotations

import argparse
import json
import math
import re
import sys
import time
from pathlib import Path
from typing import Any


CHANNELS = ("location", "rotation", "scale")
ROTATION_ORDERS = ("XYZ", "XZY", "YXZ", "YZX", "ZXY", "ZYX")


def blender_args(argv: list[str]) -> list[str]:
    if "--" in argv:
        return argv[argv.index("--") + 1 :]
    return argv[1:]


def parse_channels(value: str) -> tuple[str, ...]:
    channels = tuple(item.strip().lower() for item in value.split(",") if item.strip())
    invalid = [item for item in channels if item not in CHANNELS]
    if invalid:
        raise argparse.ArgumentTypeError(
            f"unsupported channel(s): {', '.join(invalid)}; expected location, rotation, scale"
        )
    if not channels:
        raise argparse.ArgumentTypeError("at least one channel is required")
    return channels


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert FBX animation to bbsolver SampleBundle JSON."
    )
    add = parser.add_argument
    add("input_fbx", type=Path, help="Input .fbx file")
    add("output_bbsm", type=Path, help="Output .bbsm.json file")
    add("--channels", type=parse_channels, default=parse_channels("location,rotation,scale"),
        help="Comma-separated channels to export. Default: location,rotation,scale")
    add("--frame-start", type=int, default=None,
        help="First integer frame to sample. Default: detected action start.")
    add("--frame-end", type=int, default=None,
        help="Last integer frame to sample, inclusive. Default: detected action end.")
    add("--sample-step", type=int, default=1, help="Sample every Nth frame. Default: 1.")
    add("--fps", type=float, default=None, help="Override output FPS. Default: imported scene FPS.")
    add("--rotation-order", choices=ROTATION_ORDERS, default="XYZ",
        help="Euler order used for sampled rotation vectors. Default: XYZ.")
    add("--rotation-units", choices=("degrees", "radians"), default="degrees",
        help="Rotation sample units. Default: degrees.")
    add("--skip-constant", action="store_true",
        help="Do not emit channels whose sampled values are constant.")
    add("--constant-epsilon", type=float, default=1e-8,
        help="Absolute epsilon for --skip-constant comparisons. Default: 1e-8.")
    add("--bone-regex", default=None,
        help="Only export pose bones whose names match this Python regex.")
    add("--max-bones", type=int, default=0, help="Limit exported bones. 0 means no limit.")
    add("--include-armature-object", action=argparse.BooleanOptionalAction, default=True,
        help="Also export animated armature object transform channels. Default: true.")
    add("--include-non-armature-objects", action=argparse.BooleanOptionalAction, default=False,
        help="Export animated transform channels from non-armature scene objects.")
    add("--source-key-times", choices=("none", "samples"), default="none",
        help="Populate source_key_times. Default: none; samples stores every sampled time.")
    add("--request-id", default=None, help="Override SampleBundle request_id.")
    add("--tolerance", type=float, default=0.5,
        help="Bundle config tolerance in property units. Default: 0.5.")
    add("--screen-px", type=float, default=0.0,
        help="Bundle config screen tolerance. Default: 0.0.")
    add("--jobs", type=int, default=0, help="Bundle config parallel_jobs. Default: 0.")
    add("--solve-mode", default="full",
        choices=("full", "temporal_only", "temporal-only", "motion_smooth", "motion-smooth"),
        help="Bundle config solve_optimization_mode. Default: full.")
    args = parser.parse_args(argv)
    if args.sample_step < 1:
        parser.error("--sample-step must be >= 1")
    if args.max_bones < 0:
        parser.error("--max-bones must be >= 0")
    if args.tolerance <= 0:
        parser.error("--tolerance must be positive")
    if args.screen_px < 0:
        parser.error("--screen-px must be >= 0")
    if args.fps is not None and args.fps <= 0:
        parser.error("--fps must be positive")
    if args.frame_start is not None and args.frame_end is not None:
        if args.frame_end < args.frame_start:
            parser.error("--frame-end must be >= --frame-start")
    return args


def require_blender() -> Any:
    try:
        import bpy  # type: ignore
    except ModuleNotFoundError as exc:
        raise SystemExit(
            "fbx_to_bbsm.py must be run by Blender, for example:\n"
            "  blender --background --python examples/blender/fbx_to_bbsm.py -- input.fbx output.bbsm.json"
        ) from exc
    return bpy


def sanitize_id(value: str) -> str:
    clean = re.sub(r"[^A-Za-z0-9_.:/-]+", "_", value.strip())
    clean = re.sub(r"_+", "_", clean).strip("_")
    return clean or "unnamed"


def round_float(value: float) -> float:
    if not math.isfinite(value):
        raise ValueError(f"non-finite sample value: {value!r}")
    rounded = round(float(value), 9)
    return 0.0 if rounded == -0.0 else rounded


def scene_fps(scene: Any) -> float:
    fps_base = float(getattr(scene.render, "fps_base", 1.0) or 1.0)
    return float(scene.render.fps) / fps_base


def clear_scene(bpy: Any) -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()


def import_fbx(bpy: Any, path: Path) -> None:
    result = bpy.ops.import_scene.fbx(filepath=str(path))
    if "FINISHED" not in set(result):
        raise RuntimeError(f"Blender FBX import did not finish: {result}")


def action_frame_bounds(objects: list[Any]) -> tuple[int, int] | None:
    lo = math.inf
    hi = -math.inf
    for obj in objects:
        animation_data = getattr(obj, "animation_data", None)
        action = getattr(animation_data, "action", None) if animation_data else None
        if action is None:
            continue
        for fcurve in action.fcurves:
            for point in fcurve.keyframe_points:
                frame = float(point.co.x)
                lo = min(lo, frame)
                hi = max(hi, frame)
    if not math.isfinite(lo) or not math.isfinite(hi):
        return None
    return (math.floor(lo), math.ceil(hi))


def sampled_frames(start: int, end: int, step: int) -> list[int]:
    frames = list(range(start, end + 1, step))
    if frames and frames[-1] != end:
        frames.append(end)
    return frames


def unwrap_radians(values: list[list[float]]) -> list[list[float]]:
    if not values:
        return values
    unwrapped = [values[0][:]]
    offsets = [0.0, 0.0, 0.0]
    prev = values[0][:]
    for current in values[1:]:
        row: list[float] = []
        for axis in range(3):
            delta = current[axis] - prev[axis]
            if delta > math.pi:
                offsets[axis] -= 2.0 * math.pi
            elif delta < -math.pi:
                offsets[axis] += 2.0 * math.pi
            row.append(current[axis] + offsets[axis])
        unwrapped.append(row)
        prev = current[:]
    return unwrapped


def is_constant(values: list[list[float]], epsilon: float) -> bool:
    if len(values) < 2:
        return True
    first = values[0]
    return all(
        abs(value - first[index]) <= epsilon
        for row in values[1:]
        for index, value in enumerate(row)
    )


def object_matrix_for_space(obj: Any) -> Any:
    return obj.matrix_basis.copy()


def transform_values(matrix: Any, rotation_order: str) -> dict[str, list[float]]:
    loc, quat, scale = matrix.decompose()
    euler = quat.to_euler(rotation_order)
    return {
        "location": [float(loc.x), float(loc.y), float(loc.z)],
        "rotation": [float(euler.x), float(euler.y), float(euler.z)],
        "scale": [float(scale.x), float(scale.y), float(scale.z)],
    }


def prepare_channel_values(
    channel: str,
    values: list[list[float]],
    rotation_units: str,
) -> tuple[list[list[float]], str]:
    if channel != "rotation":
        units = "blender_units" if channel == "location" else "scale"
        return values, units
    unwrapped = unwrap_radians(values)
    if rotation_units == "degrees":
        return [[math.degrees(item) for item in row] for row in unwrapped], "degrees"
    return unwrapped, "radians"


def property_kind(channel: str) -> str:
    return "ThreeD"


def build_property_samples(
    *,
    prop_id: str,
    display_name: str,
    layer_path: str,
    match_name: str,
    channel: str,
    values: list[list[float]],
    times: list[float],
    units_label: str,
    source_key_times: list[float],
) -> dict[str, Any]:
    samples = [
        {"t_sec": round_float(t), "v": [round_float(component) for component in row]}
        for t, row in zip(times, values)
    ]
    return {
        "property": {
            "id": prop_id,
            "match_name": match_name,
            "display_name": display_name,
            "layer_path": layer_path,
            "kind": property_kind(channel),
            "dimensions": 3,
            "is_spatial": False,
            "is_separated": False,
            "units_label": units_label,
            "source_key_times": [round_float(t) for t in source_key_times],
        },
        "t_start_sec": round_float(times[0]),
        "t_end_sec": round_float(times[-1]),
        "samples_per_frame": 1,
        "samples": samples,
    }


def selected_pose_bones(armature: Any, pattern: re.Pattern[str] | None, max_bones: int) -> list[Any]:
    bones = list(armature.pose.bones)
    if pattern is not None:
        bones = [bone for bone in bones if pattern.search(bone.name)]
    bones.sort(key=lambda bone: bone.name)
    if max_bones > 0:
        bones = bones[:max_bones]
    return bones


def animated_non_armature_objects(bpy: Any) -> list[Any]:
    excluded = {"ARMATURE", "CAMERA", "LIGHT"}
    objects = []
    for obj in bpy.context.scene.objects:
        if obj.type in excluded:
            continue
        if getattr(obj, "animation_data", None) and obj.animation_data.action:
            objects.append(obj)
    objects.sort(key=lambda obj: obj.name)
    return objects


def collect_tracks(bpy: Any, args: argparse.Namespace) -> tuple[list[dict[str, Any]], dict[str, Any]]:
    scene = bpy.context.scene
    all_objects = list(scene.objects)
    detected_bounds = action_frame_bounds(all_objects)
    scene_start = int(scene.frame_start)
    scene_end = int(scene.frame_end)
    default_start, default_end = detected_bounds or (scene_start, scene_end)
    frame_start = args.frame_start if args.frame_start is not None else default_start
    frame_end = args.frame_end if args.frame_end is not None else default_end
    frames = sampled_frames(frame_start, frame_end, args.sample_step)
    fps = args.fps if args.fps is not None else scene_fps(scene)
    times = [(frame - frame_start) / fps for frame in frames]
    source_key_times = times if args.source_key_times == "samples" else []
    bone_pattern = re.compile(args.bone_regex) if args.bone_regex else None

    armatures = [obj for obj in all_objects if obj.type == "ARMATURE"]
    armatures.sort(key=lambda obj: obj.name)
    if not armatures:
        print("fbx_to_bbsm: warning: no armature objects found", file=sys.stderr)

    raw_tracks: list[dict[str, Any]] = []

    def add_track(
        *,
        owner_type: str,
        armature_name: str,
        target_name: str,
        layer_path: str,
        matrix_getter: Any,
    ) -> None:
        channel_values = {channel: [] for channel in args.channels}
        for frame in frames:
            scene.frame_set(frame)
            evaluated = transform_values(matrix_getter(), args.rotation_order)
            for channel in args.channels:
                channel_values[channel].append(evaluated[channel])

        for channel in args.channels:
            values, units_label = prepare_channel_values(
                channel, channel_values[channel], args.rotation_units
            )
            if args.skip_constant and is_constant(values, args.constant_epsilon):
                continue
            prop_id = sanitize_id(f"fbx:{armature_name}:{owner_type}:{target_name}:{channel}")
            raw_tracks.append(
                build_property_samples(
                    prop_id=prop_id,
                    display_name=f"{target_name} {channel}",
                    layer_path=layer_path,
                    match_name=f"FBX {owner_type} {channel}",
                    channel=channel,
                    values=values,
                    times=times,
                    units_label=units_label,
                    source_key_times=source_key_times,
                )
            )

    for armature in armatures:
        if args.include_armature_object:
            add_track(
                owner_type="armature_object",
                armature_name=armature.name,
                target_name=armature.name,
                layer_path=armature.name,
                matrix_getter=lambda armature=armature: object_matrix_for_space(armature),
            )
        for bone in selected_pose_bones(armature, bone_pattern, args.max_bones):
            add_track(
                owner_type="pose_bone",
                armature_name=armature.name,
                target_name=bone.name,
                layer_path=f"{armature.name}/pose.bones/{bone.name}",
                matrix_getter=lambda bone=bone: bone.matrix_basis.copy(),
            )

    if args.include_non_armature_objects or not armatures:
        for obj in animated_non_armature_objects(bpy):
            add_track(
                owner_type="object",
                armature_name=obj.name,
                target_name=obj.name,
                layer_path=obj.name,
                matrix_getter=lambda obj=obj: object_matrix_for_space(obj),
            )

    metadata = {
        "frame_start": frame_start,
        "frame_end": frame_end,
        "sample_step": args.sample_step,
        "sample_count": len(frames),
        "fps": fps,
        "duration_sec": times[-1] if times else 0.0,
        "armature_count": len(armatures),
        "track_count": len(raw_tracks),
    }
    return raw_tracks, metadata


def build_bundle(args: argparse.Namespace, tracks: list[dict[str, Any]], metadata: dict[str, Any]) -> dict[str, Any]:
    request_id = args.request_id
    if request_id is None:
        request_id = f"fbx-{args.input_fbx.stem}-{int(time.time())}"
    return {
        "_schema": "samples",
        "schema_version": 1,
        "request_id": request_id,
        "comp": {
            "fps": round_float(metadata["fps"]),
            "duration_sec": round_float(metadata["duration_sec"]),
            "width": 0,
            "height": 0,
            "pixel_aspect": 1.0,
            "work_area_start_sec": 0.0,
            "work_area_end_sec": round_float(metadata["duration_sec"]),
        },
        "properties": tracks,
        "config": {
            "tolerance": args.tolerance,
            "tolerance_screen_px": args.screen_px,
            "parallel_jobs": args.jobs,
            "solve_optimization_mode": args.solve_mode.replace("-", "_"),
        },
        "generator": {
            "name": "bbsolver examples/blender/fbx_to_bbsm.py",
            "input_fbx": str(args.input_fbx),
            "frame_start": metadata["frame_start"],
            "frame_end": metadata["frame_end"],
            "sample_step": metadata["sample_step"],
            "rotation_order": args.rotation_order,
            "rotation_units": args.rotation_units,
            "channels": list(args.channels),
            "bone_space": "pose_bone.matrix_basis",
        },
    }


def main(argv: list[str]) -> int:
    args = parse_args(blender_args(argv))
    if not args.input_fbx.is_file():
        raise SystemExit(f"fbx_to_bbsm: input file not found: {args.input_fbx}")

    bpy = require_blender()
    clear_scene(bpy)
    import_fbx(bpy, args.input_fbx.resolve())
    tracks, metadata = collect_tracks(bpy, args)
    if not tracks:
        raise SystemExit(
            "fbx_to_bbsm: no animated properties were exported; "
            "try removing --skip-constant or relaxing --bone-regex"
        )

    bundle = build_bundle(args, tracks, metadata)
    args.output_bbsm.parent.mkdir(parents=True, exist_ok=True)
    with args.output_bbsm.open("w", encoding="utf-8") as handle:
        json.dump(bundle, handle, indent=2)
        handle.write("\n")

    print(
        "fbx_to_bbsm: wrote "
        f"{len(tracks)} properties, {metadata['sample_count']} samples/property, "
        f"frames {metadata['frame_start']}..{metadata['frame_end']} "
        f"to {args.output_bbsm}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
