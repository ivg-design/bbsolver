#!/usr/bin/env python3
"""Patch Blender's raw FBX JSON tree with bbsolver KeyBundle curves.

This is an experimental sparse-FBX roundtrip harness:

    python3 Blender/io_scene_fbx/fbx2json.py source.fbx
    python3 examples/blender/fbx_json_patch_bbky.py \
      source.json source.bbsm.json source.bbky.json patched.json
    /Applications/Blender.app/Contents/Resources/4.5/python/bin/python3.11 \
      Blender/io_scene_fbx/json2fbx.py patched.json

The script preserves the source FBX object graph and replaces matched
AnimationCurve KeyTime/KeyValueFloat arrays with bbsolver output. Blender's FBX
importer currently reads these curves as linear keyframes, so this proves sparse
FBX writeback shape but does not yet encode bbsolver's Bezier tangent metadata.
"""

from __future__ import annotations

import argparse
import json
import math
import statistics
from pathlib import Path
from typing import Any


FBX_TICKS_PER_SECOND = 46_186_158_000
AXIS_BY_CONNECTION = {"d|X": 0, "d|Y": 1, "d|Z": 2}
FBX_PROPERTY_BY_CHANNEL = {
    "location": "Lcl Translation",
    "rotation": "Lcl Rotation",
    "scale": "Lcl Scaling",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Patch Blender fbx2json output with bbsolver KeyBundle values."
    )
    parser.add_argument("input_fbx_json", type=Path)
    parser.add_argument("input_bbsm", type=Path)
    parser.add_argument("input_bbky", type=Path)
    parser.add_argument("output_fbx_json", type=Path)
    parser.add_argument(
        "--prefer-layer",
        default="BaseLayer::AnimLayer",
        help="AnimationLayer name to prefer when duplicate curve nodes exist.",
    )
    parser.add_argument(
        "--collapse-unmatched-dense",
        action="store_true",
        help=(
            "Collapse dense animation curves that are not present in the KeyBundle "
            "to a single first-value key. Use only when missing channels were "
            "intentionally skipped as constants during extraction."
        ),
    )
    return parser.parse_args()


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def save_json(path: Path, data: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="ascii") as handle:
        json.dump(data, handle, separators=(",", ":"))


def child(node: list[Any], name: str) -> list[Any] | None:
    for item in node[3]:
        if item[0] == name:
            return item
    return None


def prop_id_parts(property_id: str) -> tuple[str, str, str, str] | None:
    prefix = "fbx:"
    if not property_id.startswith(prefix):
        return None
    parts = property_id[len(prefix) :].split(":")
    if len(parts) < 4:
        return None
    owner = parts[0]
    kind = parts[1]
    channel = parts[-1]
    target = ":".join(parts[2:-1])
    if kind not in {"pose_bone", "armature_object", "object"}:
        return None
    if channel not in FBX_PROPERTY_BY_CHANNEL:
        return None
    return owner, kind, target, channel


def strict_values(node: list[Any], name: str) -> list[Any]:
    item = child(node, name)
    if item is None or not item[1] or not isinstance(item[1][0], list):
        raise ValueError(f"{node[0]} missing array child {name}")
    return item[1][0]


def set_array(node: list[Any], name: str, values: list[Any]) -> None:
    item = child(node, name)
    if item is None or not item[1] or not isinstance(item[1][0], list):
        raise ValueError(f"{node[0]} missing array child {name}")
    item[1][0] = values


def set_scalar(node: list[Any], name: str, value: Any) -> None:
    item = child(node, name)
    if item is not None and item[1]:
        item[1][0] = value


def wrap_degrees(delta: float) -> float:
    return ((delta + 180.0) % 360.0) - 180.0


def fit_linear(raw: list[float], sample: list[float]) -> tuple[float, float, float]:
    n = min(len(raw), len(sample))
    if n == 0:
        return 1.0, 0.0, math.inf
    x = raw[:n]
    y = sample[:n]
    sx = sum(x)
    sy = sum(y)
    sxx = sum(value * value for value in x)
    sxy = sum(a * b for a, b in zip(x, y))
    den = n * sxx - sx * sx
    if abs(den) < 1e-12:
        scale = 0.0
        offset = sy / n
    else:
        scale = (n * sxy - sx * sy) / den
        offset = (sy - scale * sx) / n
    err = max(abs((scale * a + offset) - b) for a, b in zip(x, y))
    return scale, offset, err


def fit_angular(raw: list[float], sample: list[float]) -> tuple[float, float, float]:
    n = min(len(raw), len(sample))
    best = (1.0, 0.0, math.inf)
    if n == 0:
        return best
    for sign in (1.0, -1.0):
        deltas = [raw[i] - sign * sample[i] for i in range(n)]
        base = statistics.median(deltas)
        candidates = [base + 360.0 * k for k in range(-2, 3)]
        for offset in candidates:
            err = max(abs(wrap_degrees(raw[i] - (sign * sample[i] + offset))) for i in range(n))
            if err < best[2]:
                best = (sign, offset, err)
    return best


def build_indexes(root: list[Any], prefer_layer: str) -> dict[str, Any]:
    objects_node = next(node for node in root if node[0] == "Objects")
    connections_node = next(node for node in root if node[0] == "Connections")
    objects = {node[1][0]: node for node in objects_node[3] if node[1]}
    names = {object_id: node[1][1] for object_id, node in objects.items()}
    connections = connections_node[3]

    layer_by_id = {
        object_id: name
        for object_id, name in names.items()
        if objects[object_id][0] == "AnimationLayer"
    }
    preferred_layer_id = next(
        (object_id for object_id, name in layer_by_id.items() if name == prefer_layer),
        None,
    )
    model_by_name = {
        name.removesuffix("::Model"): object_id
        for object_id, name in names.items()
        if objects[object_id][0] == "Model" and name.endswith("::Model")
    }

    node_layers: dict[int, set[int]] = {}
    node_targets: dict[int, tuple[int, str]] = {}
    node_axes: dict[int, list[int | None]] = {}
    for conn in connections:
        data = conn[1]
        if len(data) < 3:
            continue
        if data[0] == "OO" and objects.get(data[1], [None])[0] == "AnimationCurveNode":
            if data[2] in layer_by_id:
                node_layers.setdefault(data[1], set()).add(data[2])
        if len(data) >= 4 and data[0] == "OP":
            if objects.get(data[1], [None])[0] == "AnimationCurveNode":
                node_targets[data[1]] = (data[2], data[3])
            elif objects.get(data[1], [None])[0] == "AnimationCurve" and data[3] in AXIS_BY_CONNECTION:
                axes = node_axes.setdefault(data[2], [None, None, None])
                axes[AXIS_BY_CONNECTION[data[3]]] = data[1]

    curve_nodes_by_target: dict[tuple[int, str], list[tuple[int, int, list[int | None]]]] = {}
    for node_id, target in node_targets.items():
        axes = node_axes.get(node_id)
        if not axes or any(axis is None for axis in axes):
            continue
        length = max(len(strict_values(objects[curve_id], "KeyTime")) for curve_id in axes if curve_id)
        curve_nodes_by_target.setdefault(target, []).append((node_id, length, axes))

    return {
        "objects": objects,
        "model_by_name": model_by_name,
        "node_layers": node_layers,
        "preferred_layer_id": preferred_layer_id,
        "curve_nodes_by_target": curve_nodes_by_target,
    }


def choose_axes(indexes: dict[str, Any], model_id: int, fbx_prop: str) -> list[int] | None:
    candidates = indexes["curve_nodes_by_target"].get((model_id, fbx_prop), [])
    if not candidates:
        return None
    preferred_layer_id = indexes["preferred_layer_id"]

    def score(item: tuple[int, int, list[int | None]]) -> tuple[int, int]:
        node_id, length, _axes = item
        preferred = int(preferred_layer_id is not None and preferred_layer_id in indexes["node_layers"].get(node_id, set()))
        return preferred, length

    chosen = max(candidates, key=score)
    return [int(axis) for axis in chosen[2] if axis is not None]


def converter_for_axis(
    *,
    channel: str,
    axis_curve: list[Any],
    source_samples: list[dict[str, Any]],
    axis: int,
) -> tuple[float, float]:
    raw_values = [float(value) for value in strict_values(axis_curve, "KeyValueFloat")]
    sample_values = [float(sample["v"][axis]) for sample in source_samples]
    if channel == "rotation":
        sign, offset, err = fit_angular(raw_values, sample_values)
        if err <= 0.01:
            # raw = sign * solved + offset
            return sign, offset
    scale, offset, err = fit_linear(raw_values, sample_values)
    if abs(scale) < 1e-12:
        return 1.0, 0.0
    # sample = scale * raw + offset, so raw = solved / scale - offset / scale.
    return 1.0 / scale, -offset / scale


def patch_curve(
    curve: list[Any],
    keys: list[dict[str, Any]],
    axis: int,
    raw_scale: float,
    raw_offset: float,
) -> None:
    key_times = [int(round(float(key["t_sec"]) * FBX_TICKS_PER_SECOND)) for key in keys]
    key_values = [
        float(raw_scale * float(key["v"][axis]) + raw_offset)
        for key in keys
    ]
    set_array(curve, "KeyTime", key_times)
    set_array(curve, "KeyValueFloat", key_values)
    set_array(curve, "KeyAttrRefCount", [len(key_times)])
    set_scalar(curve, "Default", key_values[0] if key_values else 0.0)


def main() -> int:
    args = parse_args()
    root = load_json(args.input_fbx_json)
    sample_bundle = load_json(args.input_bbsm)
    key_bundle = load_json(args.input_bbky)
    indexes = build_indexes(root, args.prefer_layer)
    objects = indexes["objects"]

    samples_by_property = {
        item["property"]["id"]: item["samples"]
        for item in sample_bundle.get("properties", [])
    }

    patched = 0
    patched_curve_ids: set[int] = set()
    skipped: list[str] = []
    for result in key_bundle.get("property_results", []):
        property_id = str(result.get("property_id", ""))
        parsed = prop_id_parts(property_id)
        keys = result.get("keys", [])
        if parsed is None or not keys:
            skipped.append(property_id)
            continue
        owner, kind, target, channel = parsed
        if kind == "pose_bone":
            model_name = target
        elif kind == "armature_object":
            model_name = owner
        else:
            model_name = target
        model_id = indexes["model_by_name"].get(model_name)
        axes = choose_axes(indexes, model_id, FBX_PROPERTY_BY_CHANNEL[channel]) if model_id else None
        source_samples = samples_by_property.get(property_id)
        if not axes or not source_samples:
            skipped.append(property_id)
            continue
        for axis, curve_id in enumerate(axes):
            curve = objects[curve_id]
            raw_scale, raw_offset = converter_for_axis(
                channel=channel,
                axis_curve=curve,
                source_samples=source_samples,
                axis=axis,
            )
            patch_curve(curve, keys, axis, raw_scale, raw_offset)
            patched_curve_ids.add(curve_id)
        patched += 1

    collapsed = 0
    if args.collapse_unmatched_dense:
        for object_id, curve in objects.items():
            if curve[0] != "AnimationCurve" or object_id in patched_curve_ids:
                continue
            key_times = strict_values(curve, "KeyTime")
            key_values = strict_values(curve, "KeyValueFloat")
            if len(key_times) <= 2 or not key_values:
                continue
            set_array(curve, "KeyTime", [key_times[0]])
            set_array(curve, "KeyValueFloat", [key_values[0]])
            set_array(curve, "KeyAttrRefCount", [1])
            set_scalar(curve, "Default", key_values[0])
            collapsed += 1

    save_json(args.output_fbx_json, root)
    print(
        f"fbx_json_patch_bbky: patched {patched} properties; "
        f"collapsed {collapsed} unmatched dense curves; "
        f"skipped {len(skipped)}; wrote {args.output_fbx_json}"
    )
    if skipped:
        for item in skipped[:24]:
            print(f"  skipped: {item}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
