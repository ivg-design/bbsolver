#!/usr/bin/env python3
"""Validate bbsolver JSON bundles against packaged schemas.

The validator intentionally uses only the Python standard library so it works
from a clean checkout. It implements the JSON Schema subset used by the
packaged schemas, then adds bbsolver-specific semantic checks that JSON Schema
cannot express cleanly without extensions.
"""

from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
SCHEMA_DIR = ROOT / "schemas"
SCHEMA_BY_MARKER = {
    "samples": SCHEMA_DIR / "sample_bundle.schema.json",
    "keys": SCHEMA_DIR / "key_bundle.schema.json",
}


class ValidationError(RuntimeError):
    pass


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Validate a bbsolver SampleBundle or KeyBundle JSON file."
    )
    parser.add_argument("bundle", type=Path, help="Path to *.bbsm.json or *.bbky.json")
    parser.add_argument(
        "--schema",
        choices=("auto", "samples", "keys"),
        default="auto",
        help="Schema to use. Default detects from the bundle _schema marker.",
    )
    return parser.parse_args()


def load_json(path: Path) -> Any:
    try:
        with path.open("r", encoding="utf-8") as handle:
            return json.load(handle)
    except OSError as exc:
        raise ValidationError(f"{path}: unable to read file: {exc}") from exc
    except json.JSONDecodeError as exc:
        raise ValidationError(f"{path}: invalid JSON at line {exc.lineno}: {exc.msg}") from exc


def json_type_name(value: Any) -> str:
    if value is None:
        return "null"
    if isinstance(value, bool):
        return "boolean"
    if isinstance(value, int) and not isinstance(value, bool):
        return "integer"
    if isinstance(value, float):
        return "number"
    if isinstance(value, str):
        return "string"
    if isinstance(value, list):
        return "array"
    if isinstance(value, dict):
        return "object"
    return type(value).__name__


def matches_type(value: Any, expected: str) -> bool:
    if expected == "object":
        return isinstance(value, dict)
    if expected == "array":
        return isinstance(value, list)
    if expected == "string":
        return isinstance(value, str)
    if expected == "boolean":
        return isinstance(value, bool)
    if expected == "integer":
        return isinstance(value, int) and not isinstance(value, bool)
    if expected == "number":
        return (
            isinstance(value, (int, float))
            and not isinstance(value, bool)
            and math.isfinite(float(value))
        )
    return True


def validate_schema(value: Any, schema: dict[str, Any], path: str) -> None:
    if "const" in schema and value != schema["const"]:
        raise ValidationError(f"{path}: expected constant {schema['const']!r}")
    if "enum" in schema and value not in schema["enum"]:
        allowed = ", ".join(repr(item) for item in schema["enum"])
        raise ValidationError(f"{path}: expected one of {allowed}")

    expected_type = schema.get("type")
    if isinstance(expected_type, list):
        if not any(matches_type(value, item) for item in expected_type):
            raise ValidationError(
                f"{path}: expected type {' or '.join(expected_type)}, got {json_type_name(value)}"
            )
    elif isinstance(expected_type, str) and not matches_type(value, expected_type):
        raise ValidationError(
            f"{path}: expected type {expected_type}, got {json_type_name(value)}"
        )

    if "minimum" in schema and isinstance(value, (int, float)) and value < schema["minimum"]:
        raise ValidationError(f"{path}: expected value >= {schema['minimum']}")
    if "minLength" in schema and isinstance(value, str) and len(value) < schema["minLength"]:
        raise ValidationError(f"{path}: expected string length >= {schema['minLength']}")
    if "minItems" in schema and isinstance(value, list) and len(value) < schema["minItems"]:
        raise ValidationError(f"{path}: expected at least {schema['minItems']} item(s)")

    if isinstance(value, dict):
        for field in schema.get("required", []):
            if field not in value:
                raise ValidationError(f"{path}: missing required field {field!r}")
        properties = schema.get("properties", {})
        if isinstance(properties, dict):
            for key, child_schema in properties.items():
                if key in value:
                    validate_schema(value[key], child_schema, f"{path}.{key}")

    if isinstance(value, list) and isinstance(schema.get("items"), dict):
        item_schema = schema["items"]
        for index, item in enumerate(value):
            validate_schema(item, item_schema, f"{path}[{index}]")


def semantic_check_samples(bundle: dict[str, Any]) -> None:
    for prop_index, prop_samples in enumerate(bundle.get("properties", [])):
        prop = prop_samples.get("property", {})
        dimensions = prop.get("dimensions")
        samples_per_frame = prop_samples.get("samples_per_frame", 1)
        units_label = prop.get("units_label", "")
        variable_topology = bool(prop.get("shape_variable_topology", False))
        expected_len = dimensions * samples_per_frame
        for sample_index, sample in enumerate(prop_samples.get("samples", [])):
            values = sample.get("v", [])
            if units_label == "shape_flat" and variable_topology:
                if len(values) > dimensions:
                    raise ValidationError(
                        f"properties[{prop_index}].samples[{sample_index}].v: "
                        f"length {len(values)} exceeds shape_flat dimensions {dimensions}"
                    )
                continue
            if len(values) != expected_len:
                raise ValidationError(
                    f"properties[{prop_index}].samples[{sample_index}].v: "
                    f"length {len(values)} must equal dimensions*samples_per_frame "
                    f"({dimensions}*{samples_per_frame}={expected_len})"
                )


def semantic_check_keys(bundle: dict[str, Any]) -> None:
    for result_index, result in enumerate(bundle.get("property_results", [])):
        dimensions = result.get("dimensions")
        keys = result.get("keys", [])
        if result.get("converged") is True and len(keys) == 0:
            raise ValidationError(
                f"property_results[{result_index}].keys: converged results must contain keys"
            )
        for key_index, key in enumerate(keys):
            values = key.get("v", [])
            if len(values) != dimensions:
                raise ValidationError(
                    f"property_results[{result_index}].keys[{key_index}].v: "
                    f"length {len(values)} must equal dimensions ({dimensions})"
                )


def main() -> int:
    args = parse_args()
    bundle = load_json(args.bundle)
    if not isinstance(bundle, dict):
        raise ValidationError("$: expected top-level object")

    marker = bundle.get("_schema")
    schema_kind = marker if args.schema == "auto" else args.schema
    if schema_kind not in SCHEMA_BY_MARKER:
        raise ValidationError(
            "$._schema: expected 'samples' or 'keys' for auto detection"
        )

    schema = load_json(SCHEMA_BY_MARKER[schema_kind])
    validate_schema(bundle, schema, "$")
    if schema_kind == "samples":
        semantic_check_samples(bundle)
    else:
        semantic_check_keys(bundle)

    print(f"ok: {args.bundle} matches bbsolver {schema_kind} schema")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ValidationError as exc:
        print(f"bbsolver-schema: {exc}", file=sys.stderr)
        raise SystemExit(1)
