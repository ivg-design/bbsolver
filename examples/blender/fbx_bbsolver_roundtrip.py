#!/usr/bin/env python3
"""Roundtrip FBX animation curves through bbsolver.

Pipeline:

  1. Convert binary FBX to Blender's raw FBX JSON tree.
  2. Extract every animated transform curve node from the selected FBX
     AnimationLayer into a bbsolver SampleBundle.
  3. Run bbsolver.
  4. Patch the raw FBX JSON tree with the solved sparse key arrays.
  5. Convert the patched JSON tree back to binary FBX.

This script operates on FBX animation-curve values directly. It does not import
the file into a Blender scene to sample pose-bone transforms, so it can see
animated channels that Blender pose sampling may treat as effectively constant.

Current limitation: Blender's FBX importer reads imported FBX animation as
linear keyframes. The roundtrip therefore proves sparse/interpolated FBX
writeback, but it does not yet encode bbsolver's Bezier tangent metadata in a
way Blender re-imports as Bezier handles.
"""

from __future__ import annotations

import argparse
import bisect
import json
import math
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path
from typing import Any


FBX_TICKS_PER_SECOND = 46_186_158_000
AXIS_BY_CONNECTION = {"d|X": 0, "d|Y": 1, "d|Z": 2}
CHANNEL_BY_FBX_PROPERTY = {
    "Lcl Translation": "location",
    "Lcl Rotation": "rotation",
    "Lcl Scaling": "scale",
}
FBX_PROPERTY_BY_CHANNEL = {value: key for key, value in CHANNEL_BY_FBX_PROPERTY.items()}
DEFAULT_BLENDER_RESOURCES = Path("/Applications/Blender.app/Contents/Resources/4.5")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert FBX animation to bbsolver keys and write sparse FBX."
    )
    parser.add_argument("input_fbx", type=Path)
    parser.add_argument("output_fbx", type=Path)
    parser.add_argument(
        "--work-dir",
        type=Path,
        default=None,
        help="Directory for intermediate JSON/.bbsm/.bbky files. Default: output stem + _roundtrip.",
    )
    parser.add_argument(
        "--solver",
        type=Path,
        default=None,
        help="bbsolver binary. Default: BBSOLVER_BINARY or ~/.bbsolver/bin/bbsolver.",
    )
    parser.add_argument("--tolerance", type=float, default=3.0)
    parser.add_argument("--screen-px", type=float, default=0.0)
    parser.add_argument("--jobs", type=int, default=0)
    parser.add_argument(
        "--solve-mode",
        default="full",
        choices=("full", "temporal_only", "temporal-only", "motion_smooth", "motion-smooth"),
    )
    parser.add_argument(
        "--prefer-layer",
        default="BaseLayer::AnimLayer",
        help="FBX AnimationLayer to solve when duplicate layers exist.",
    )
    parser.add_argument(
        "--blender-resources",
        type=Path,
        default=DEFAULT_BLENDER_RESOURCES,
        help="Blender resource directory containing scripts/addons_core/io_scene_fbx.",
    )
    parser.add_argument(
        "--keep-work",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Keep intermediate files. Default: true.",
    )
    args = parser.parse_args()
    if args.tolerance <= 0:
        parser.error("--tolerance must be positive")
    if args.screen_px < 0:
        parser.error("--screen-px must be non-negative")
    return args


def child(node: list[Any], name: str) -> list[Any] | None:
    for item in node[3]:
        if item[0] == name:
            return item
    return None


def array_child(node: list[Any], name: str) -> list[Any]:
    item = child(node, name)
    if item is None or not item[1] or not isinstance(item[1][0], list):
        raise ValueError(f"{node[0]} missing array child {name}")
    return item[1][0]


def set_array_child(node: list[Any], name: str, values: list[Any]) -> None:
    array_child(node, name)
    child(node, name)[1][0] = values  # type: ignore[index]


def set_scalar_child(node: list[Any], name: str, value: Any) -> None:
    item = child(node, name)
    if item is not None and item[1]:
        item[1][0] = value


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def save_json(path: Path, data: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="ascii") as handle:
        json.dump(data, handle, separators=(",", ":"))


def run_command(command: list[str], *, cwd: Path | None = None) -> None:
    print("+ " + " ".join(command))
    subprocess.run(command, cwd=cwd, check=True)


def solver_path(arg: Path | None) -> Path:
    if arg is not None:
        return arg.expanduser()
    env = os.environ.get("BBSOLVER_BINARY")
    if env:
        return Path(env).expanduser()
    return Path("~/.bbsolver/bin/bbsolver").expanduser()


def blender_paths(resources: Path) -> tuple[Path, Path, Path]:
    fbx_dir = resources / "scripts/addons_core/io_scene_fbx"
    fbx2json = fbx_dir / "fbx2json.py"
    json2fbx = fbx_dir / "json2fbx.py"
    blender_python = resources / "python/bin/python3.11"
    for path in (fbx2json, json2fbx, blender_python):
        if not path.exists():
            raise FileNotFoundError(path)
    return fbx2json, json2fbx, blender_python


def convert_fbx_to_json(fbx2json: Path, input_fbx: Path, raw_json: Path) -> None:
    raw_json.parent.mkdir(parents=True, exist_ok=True)
    temp_fbx = raw_json.with_suffix(".fbx")
    shutil.copy2(input_fbx, temp_fbx)
    run_command([sys.executable, str(fbx2json), str(temp_fbx)])
    produced = temp_fbx.with_suffix(".json")
    if produced.resolve() != raw_json.resolve():
        shutil.move(str(produced), str(raw_json))
    temp_fbx.unlink(missing_ok=True)


def convert_json_to_fbx(blender_python: Path, json2fbx: Path, patched_json: Path, output_fbx: Path) -> None:
    produced = patched_json.with_suffix(".fbx")
    if produced.exists() and produced.resolve() != output_fbx.resolve():
        produced.unlink()
    run_command([str(blender_python), str(json2fbx), str(patched_json)])
    output_fbx.parent.mkdir(parents=True, exist_ok=True)
    if produced.resolve() != output_fbx.resolve():
        shutil.move(str(produced), str(output_fbx))


def model_display_name(name: str) -> str:
    return name.removesuffix("::Model")


def build_indexes(root: list[Any], prefer_layer: str) -> dict[str, Any]:
    objects_node = next(node for node in root if node[0] == "Objects")
    connections_node = next(node for node in root if node[0] == "Connections")
    objects = {node[1][0]: node for node in objects_node[3] if node[1]}
    names = {object_id: node[1][1] for object_id, node in objects.items()}
    connections = connections_node[3]

    layer_ids = {
        object_id: name
        for object_id, name in names.items()
        if objects[object_id][0] == "AnimationLayer"
    }
    preferred_layer_id = next(
        (object_id for object_id, name in layer_ids.items() if name == prefer_layer),
        None,
    )
    if preferred_layer_id is None:
        preferred_layer_id = next(iter(layer_ids), None)

    node_layers: dict[int, set[int]] = {}
    node_targets: dict[int, tuple[int, str]] = {}
    node_axes: dict[int, list[int | None]] = {}
    for conn in connections:
        data = conn[1]
        if len(data) < 3:
            continue
        if data[0] == "OO" and objects.get(data[1], [None])[0] == "AnimationCurveNode":
            if data[2] in layer_ids:
                node_layers.setdefault(data[1], set()).add(data[2])
        if len(data) >= 4 and data[0] == "OP":
            if objects.get(data[1], [None])[0] == "AnimationCurveNode":
                node_targets[data[1]] = (data[2], data[3])
            elif objects.get(data[1], [None])[0] == "AnimationCurve" and data[3] in AXIS_BY_CONNECTION:
                axes = node_axes.setdefault(data[2], [None, None, None])
                axes[AXIS_BY_CONNECTION[data[3]]] = data[1]

    tracks: list[dict[str, Any]] = []
    for node_id, (model_id, fbx_property) in node_targets.items():
        if fbx_property not in CHANNEL_BY_FBX_PROPERTY:
            continue
        if preferred_layer_id is not None and preferred_layer_id not in node_layers.get(node_id, set()):
            continue
        axes = node_axes.get(node_id)
        if not axes or any(axis is None for axis in axes):
            continue
        curve_ids = [int(axis) for axis in axes if axis is not None]
        if not any(len(array_child(objects[curve_id], "KeyTime")) > 1 for curve_id in curve_ids):
            continue
        tracks.append(
            {
                "node_id": node_id,
                "model_id": model_id,
                "model_name": model_display_name(names.get(model_id, str(model_id))),
                "fbx_property": fbx_property,
                "channel": CHANNEL_BY_FBX_PROPERTY[fbx_property],
                "curve_ids": curve_ids,
            }
        )
    tracks.sort(key=lambda item: (item["model_name"], item["fbx_property"], item["node_id"]))
    return {
        "objects": objects,
        "names": names,
        "preferred_layer_id": preferred_layer_id,
        "tracks": tracks,
    }


def interpolate(times: list[int], values: list[float], target: int) -> float:
    if not times:
        return 0.0
    if target <= times[0]:
        return float(values[0])
    if target >= times[-1]:
        return float(values[-1])
    index = bisect.bisect_left(times, target)
    if times[index] == target:
        return float(values[index])
    left_t = times[index - 1]
    right_t = times[index]
    factor = (target - left_t) / (right_t - left_t)
    return float(values[index - 1]) * (1.0 - factor) + float(values[index]) * factor


def track_times_and_values(objects: dict[int, list[Any]], curve_ids: list[int]) -> tuple[list[int], list[list[float]]]:
    axis_times = [[int(value) for value in array_child(objects[curve_id], "KeyTime")] for curve_id in curve_ids]
    axis_values = [[float(value) for value in array_child(objects[curve_id], "KeyValueFloat")] for curve_id in curve_ids]
    union_times = sorted(set().union(*axis_times))
    values: list[list[float]] = []
    for tick in union_times:
        values.append(
            [
                interpolate(axis_times[axis], axis_values[axis], tick)
                for axis in range(3)
            ]
        )
    return union_times, values


def infer_fps(all_times: list[int]) -> float:
    deltas = sorted({b - a for a, b in zip(all_times, all_times[1:]) if b > a})
    if not deltas:
        return 0.0
    return round(FBX_TICKS_PER_SECOND / deltas[0], 6)


def round_float(value: float) -> float:
    rounded = round(float(value), 9)
    return 0.0 if rounded == -0.0 else rounded


def build_sample_bundle(
    *,
    root: list[Any],
    input_fbx: Path,
    request_id: str,
    prefer_layer: str,
    tolerance: float,
    screen_px: float,
    jobs: int,
    solve_mode: str,
) -> tuple[dict[str, Any], dict[str, Any]]:
    indexes = build_indexes(root, prefer_layer)
    objects = indexes["objects"]
    raw_tracks: list[dict[str, Any]] = []
    all_ticks: list[int] = []

    for track in indexes["tracks"]:
        ticks, values = track_times_and_values(objects, track["curve_ids"])
        if not ticks:
            continue
        all_ticks.extend(ticks)
        raw_tracks.append({**track, "ticks": ticks, "values": values})

    if not raw_tracks:
        raise RuntimeError("No animated FBX transform curve nodes found")

    global_start_tick = min(min(track["ticks"]) for track in raw_tracks)
    global_end_tick = max(max(track["ticks"]) for track in raw_tracks)
    fps = infer_fps(sorted(set(all_ticks)))
    properties = []
    property_index: dict[str, dict[str, Any]] = {}

    for ordinal, track in enumerate(raw_tracks):
        property_id = f"fbxjson:{track['model_id']}:{track['channel']}"
        samples = [
            {
                "t_sec": round_float((tick - global_start_tick) / FBX_TICKS_PER_SECOND),
                "v": [round_float(value) for value in row],
            }
            for tick, row in zip(track["ticks"], track["values"])
        ]
        properties.append(
            {
                "property": {
                    "id": property_id,
                    "match_name": f"FBX JSON {track['fbx_property']}",
                    "display_name": f"{track['model_name']} {track['channel']}",
                    "layer_path": track["model_name"],
                    "kind": "ThreeD",
                    "dimensions": 3,
                    "is_spatial": False,
                    "is_separated": False,
                    "units_label": "degrees" if track["channel"] == "rotation" else "fbx_units",
                    "source_key_times": [],
                },
                "t_start_sec": samples[0]["t_sec"],
                "t_end_sec": samples[-1]["t_sec"],
                "samples_per_frame": 1,
                "samples": samples,
            }
        )
        property_index[property_id] = {**track, "ordinal": ordinal}

    bundle = {
        "_schema": "samples",
        "schema_version": 1,
        "request_id": request_id,
        "comp": {
            "fps": fps,
            "duration_sec": round_float((global_end_tick - global_start_tick) / FBX_TICKS_PER_SECOND),
            "width": 0,
            "height": 0,
            "pixel_aspect": 1.0,
            "work_area_start_sec": 0.0,
            "work_area_end_sec": round_float((global_end_tick - global_start_tick) / FBX_TICKS_PER_SECOND),
        },
        "properties": properties,
        "config": {
            "tolerance": tolerance,
            "tolerance_screen_px": screen_px,
            "parallel_jobs": jobs,
            "solve_optimization_mode": solve_mode.replace("-", "_"),
        },
        "generator": {
            "name": "bbsolver examples/blender/fbx_bbsolver_roundtrip.py",
            "input_fbx": str(input_fbx),
            "fbx_layer": prefer_layer,
            "global_start_tick": global_start_tick,
            "global_end_tick": global_end_tick,
            "ticks_per_second": FBX_TICKS_PER_SECOND,
            "track_count": len(properties),
        },
    }
    return bundle, {"indexes": indexes, "property_index": property_index, "global_start_tick": global_start_tick}


def patch_curve(curve: list[Any], keys: list[dict[str, Any]], axis: int, global_start_tick: int) -> None:
    key_times = [
        int(round(global_start_tick + float(key["t_sec"]) * FBX_TICKS_PER_SECOND))
        for key in keys
    ]
    key_values = [float(key["v"][axis]) for key in keys]
    set_array_child(curve, "KeyTime", key_times)
    set_array_child(curve, "KeyValueFloat", key_values)
    set_array_child(curve, "KeyAttrRefCount", [len(key_times)])
    set_scalar_child(curve, "Default", key_values[0] if key_values else 0.0)


def patch_root_with_key_bundle(root: list[Any], patch_context: dict[str, Any], key_bundle: dict[str, Any]) -> dict[str, int]:
    objects = patch_context["indexes"]["objects"]
    property_index = patch_context["property_index"]
    global_start_tick = patch_context["global_start_tick"]
    patched_properties = 0
    patched_curves = 0
    skipped = 0
    for result in key_bundle.get("property_results", []):
        property_id = str(result.get("property_id", ""))
        track = property_index.get(property_id)
        keys = result.get("keys", [])
        if track is None or not keys:
            skipped += 1
            continue
        for axis, curve_id in enumerate(track["curve_ids"]):
            patch_curve(objects[curve_id], keys, axis, global_start_tick)
            patched_curves += 1
        patched_properties += 1
    return {
        "patched_properties": patched_properties,
        "patched_curves": patched_curves,
        "skipped_results": skipped,
    }


def solve_bundle(
    solver: Path,
    sample_path: Path,
    key_path: Path,
    *,
    tolerance: float,
    screen_px: float,
    jobs: int,
    solve_mode: str,
) -> None:
    if not solver.exists():
        raise FileNotFoundError(f"bbsolver binary not found: {solver}")
    command = [
        str(solver),
        "solve",
        str(sample_path),
        str(key_path),
        "--tolerance",
        str(tolerance),
        "--screen-px",
        str(screen_px),
        "--jobs",
        str(jobs),
        "--solve-mode",
        solve_mode.replace("-", "_"),
    ]
    run_command(command)


def main() -> int:
    args = parse_args()
    input_fbx = args.input_fbx.resolve()
    output_fbx = args.output_fbx.resolve()
    if not input_fbx.is_file():
        raise SystemExit(f"Input FBX not found: {input_fbx}")

    fbx2json, json2fbx, blender_python = blender_paths(args.blender_resources)
    solver = solver_path(args.solver)
    work_dir = args.work_dir
    if work_dir is None:
        work_dir = output_fbx.with_suffix("").parent / f"{output_fbx.stem}_roundtrip"
    work_dir = work_dir.resolve()
    work_dir.mkdir(parents=True, exist_ok=True)

    raw_json_path = work_dir / f"{input_fbx.stem}.raw.json"
    sample_path = work_dir / f"{input_fbx.stem}.bbsm.json"
    key_path = work_dir / f"{input_fbx.stem}.bbky.json"
    patched_json_path = work_dir / f"{input_fbx.stem}.patched.json"

    convert_fbx_to_json(fbx2json, input_fbx, raw_json_path)
    root = load_json(raw_json_path)
    request_id = f"fbx-roundtrip-{input_fbx.stem}-{int(time.time())}"
    sample_bundle, patch_context = build_sample_bundle(
        root=root,
        input_fbx=input_fbx,
        request_id=request_id,
        prefer_layer=args.prefer_layer,
        tolerance=args.tolerance,
        screen_px=args.screen_px,
        jobs=args.jobs,
        solve_mode=args.solve_mode,
    )
    save_json(sample_path, sample_bundle)
    print(
        f"fbx_bbsolver_roundtrip: extracted {len(sample_bundle['properties'])} animated properties "
        f"to {sample_path}"
    )

    solve_bundle(
        solver,
        sample_path,
        key_path,
        tolerance=args.tolerance,
        screen_px=args.screen_px,
        jobs=args.jobs,
        solve_mode=args.solve_mode,
    )
    key_bundle = load_json(key_path)
    patch_stats = patch_root_with_key_bundle(root, patch_context, key_bundle)
    save_json(patched_json_path, root)
    print(f"fbx_bbsolver_roundtrip: patch stats {patch_stats}")
    convert_json_to_fbx(blender_python, json2fbx, patched_json_path, output_fbx)
    print(f"fbx_bbsolver_roundtrip: wrote {output_fbx}")

    if not args.keep_work:
        shutil.rmtree(work_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
