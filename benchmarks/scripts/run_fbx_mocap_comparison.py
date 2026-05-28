"""Orchestrator: run bbsolver vs Blender F-Curve Decimate on the FBX mocap
fixture across multiple ε levels, then compute head-to-head Pareto data.

Inputs (already present):
    benchmarks/fbx_mocap_retarget_full_size/pose_sampled_blender_action/retarget_full_size.bbsm.json

For each ε in {0.5, 1.0, 2.0, 3.0}:
    1. Run bbsolver solve → record total_keys, max_err per property,
       solver wall-clock.
    2. Verify in-process by reading the bbky.json and bbsm.json directly
       (matches the bbsolver verify subcommand semantics: per-property
       max L∞ between key-interpolated and source-sampled values).
    3. Run Blender F-Curve Decimate on the SAME bbsm (all 45 ThreeD
       properties decomposed into 135 scalar F-curves) at the same
       error tolerance. Record total kept keys per F-curve and per-curve
       max absolute residual.

Output:
    supplementary/fbx_mocap_method_comparison.csv      headline per-ε per-method row
    supplementary/fbx_mocap_per_property.csv           per-property per-method row
    work/fbx_mocap/bbky_eps_{eps}.json                      bbsolver outputs per ε
    work/fbx_mocap/blender_eps_{eps}.json                   blender decimate outputs per ε

Designed to be re-runnable. Skips solves whose outputs already exist.
"""
from __future__ import annotations

import csv
import json
import os
import subprocess
import sys
import time
from pathlib import Path

# Add the scripts directory to sys.path so the local _paths helper and the
# external_runners ports can be imported.
SCRIPTS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPTS_DIR))
sys.path.insert(0, str(SCRIPTS_DIR.parent))  # for the external_runners package one level up
from _paths import (  # noqa: E402
    resolve_bbsolver_binary, resolve_blender_binary, resolve_fbx_mocap_bbsm,
)
from external_runners.joosten_reducer import reduce_channel as joosten_reduce  # noqa: E402
from external_runners.toolchefs_reducer import reduce_channel as toolchefs_reduce  # noqa: E402

ARXIV_ROOT = SCRIPTS_DIR.parent
MOCAP_BBSM = Path(resolve_fbx_mocap_bbsm())
WORK = ARXIV_ROOT / "work" / "fbx_mocap"
WORK.mkdir(parents=True, exist_ok=True)
OUT_CSV_DIR = ARXIV_ROOT / "data" / "supplementary"
OUT_CSV_DIR.mkdir(parents=True, exist_ok=True)

BBSOLVER = resolve_bbsolver_binary()
BLENDER = resolve_blender_binary()
BLENDER_DECIMATE_SCRIPT = SCRIPTS_DIR / "blender_fbx_decimate_inner.py"

EPS_VALUES = [0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 1.5, 2.0, 3.0, 5.0]


# --------------------- bbsolver branch ---------------------

def run_bbsolver(eps):
    out = WORK / f"bbky_eps_{eps}.json"
    if out.exists():
        print(f"  [bbsolver eps={eps}] cached → {out.name}")
        return out
    t0 = time.time()
    # NOTE on QoS throttling: subprocess children inherit the Python parent's
    # macOS QoS class, which can be Utility for processes launched via
    # backgrounded shells or automation harnesses. This caps per-thread
    # scheduling and produces ~5x slower wall-clock vs direct-shell launches.
    # `taskpolicy` does not have a clean inverse-throttle flag in this macOS
    # version; we leave the throttle in place and report relative comparisons
    # (both methods affected equally) rather than absolute solve times.
    cmd = [BBSOLVER, "solve", str(MOCAP_BBSM), str(out),
           "--tolerance", str(eps), "--jobs", "0"]
    print(f"  [bbsolver eps={eps}] solving …")
    subprocess.run(cmd, check=True)
    elapsed = time.time() - t0
    print(f"  [bbsolver eps={eps}] {elapsed:.1f}s")
    return out


def analyze_bbsolver(eps, bbky_path):
    sm = json.load(open(MOCAP_BBSM))
    ky = json.load(open(bbky_path))
    per_prop = []
    total_keys = 0
    for r in ky["property_results"]:
        nk = len(r.get("keys", []))
        total_keys += nk
        per_prop.append({
            "property_id": r["property_id"],
            "keys": nk,
            "max_err": r["max_err"],
            "converged": r["converged"],
            "dimensions": r.get("dimensions", 3),
        })
    return {
        "method": "bbsolver",
        "eps": eps,
        "total_samples": ky["total_samples_input"],
        "total_keys": total_keys,
        "solve_time_ms": ky["solve_time_ms"],
        "n_properties": len(per_prop),
        "per_property": per_prop,
    }


# --------------------- Blender decimate branch ---------------------

def run_blender_decimate(eps):
    out = WORK / f"blender_eps_{eps}.json"
    if out.exists():
        print(f"  [blender eps={eps}] cached → {out.name}")
        return out
    t0 = time.time()
    cmd = [
        BLENDER, "--background", "--factory-startup",
        "--python", str(BLENDER_DECIMATE_SCRIPT),
        "--",
        "--input", str(MOCAP_BBSM),
        "--tolerance", str(eps),
        "--output", str(out),
    ]
    print(f"  [blender eps={eps}] decimating …")
    result = subprocess.run(cmd, capture_output=True, text=True)
    elapsed = time.time() - t0
    if result.returncode != 0:
        print(f"  [blender eps={eps}] FAILED ({elapsed:.1f}s):\n{result.stderr[-800:]}")
        raise RuntimeError(f"Blender decimate failed at eps={eps}")
    print(f"  [blender eps={eps}] {elapsed:.1f}s")
    return out


def analyze_blender(eps, blender_path):
    d = json.load(open(blender_path))
    per_prop = []
    total_keys = 0
    for prop in d["properties"]:
        # property has 3 dims; sum keys across dims
        keys_per_dim = [len(ch["keys"]) for ch in prop["channels"]]
        max_err = max(ch["max_abs_residual"] for ch in prop["channels"])
        total_keys += sum(keys_per_dim)
        per_prop.append({
            "property_id": prop["property_id"],
            "keys": sum(keys_per_dim),
            "keys_per_dim": keys_per_dim,
            "max_err": max_err,
            "dimensions": 3,
        })
    return {
        "method": "blender_decimate",
        "eps": eps,
        "total_samples": d["meta"]["total_samples"],
        "total_keys": total_keys,
        "wall_clock_ms": d["meta"]["wall_clock_ms"],
        "n_properties": len(per_prop),
        "per_property": per_prop,
    }


# --------------------- joosten reducer branch (pure-python, no Maya) ---------------------

def run_joosten(eps):
    """Run the ported joosten/paper.js reducer on every scalar channel of
    the mocap bbsm. Pure-Python; no external process needed."""
    out = WORK / f"joosten_eps_{eps}.json"
    if out.exists():
        print(f"  [joosten eps={eps}] cached → {out.name}")
        return out
    bundle = json.load(open(MOCAP_BBSM))
    fps = float(bundle.get("comp", {}).get("fps", 25.0))
    properties_out = []
    t0 = time.time()
    total_samples = 0
    for prop in bundle["properties"]:
        prop_id = prop["property"].get("id", "?")
        dims = int(prop["property"].get("dimensions", 1))
        samples = prop["samples"]
        channels = []
        for axis in range(dims):
            times = [float(s["t_sec"]) for s in samples]
            values = [float(s["v"][axis]) for s in samples]
            total_samples += len(times)
            n_keys, max_resid, kfs = joosten_reduce(times, values, eps)
            channels.append({
                "axis_index": axis,
                "n_keys": n_keys,
                "max_abs_residual": max_resid,
            })
        properties_out.append({
            "property_id": prop_id,
            "dimensions": dims,
            "channels": channels,
        })
    elapsed_ms = (time.time() - t0) * 1000.0
    out_json = {
        "schema_version": 2,
        "meta": {
            "tolerance": eps,
            "total_samples": total_samples,
            "wall_clock_ms": elapsed_ms,
            "fps": fps,
        },
        "properties": properties_out,
    }
    out.write_text(json.dumps(out_json, indent=2) + "\n")
    print(f"  [joosten eps={eps}] {elapsed_ms/1000:.1f}s")
    return out


def analyze_joosten(eps, joosten_path):
    d = json.load(open(joosten_path))
    per_prop = []
    total_keys = 0
    for prop in d["properties"]:
        keys_per_dim = [ch["n_keys"] for ch in prop["channels"]]
        max_err = max(ch["max_abs_residual"] for ch in prop["channels"])
        total_keys += sum(keys_per_dim)
        per_prop.append({
            "property_id": prop["property_id"],
            "keys": sum(keys_per_dim),
            "keys_per_dim": keys_per_dim,
            "max_err": max_err,
            "dimensions": 3,
        })
    return {
        "method": "joosten_reducer",
        "eps": eps,
        "total_samples": d["meta"]["total_samples"],
        "total_keys": total_keys,
        "wall_clock_ms": d["meta"]["wall_clock_ms"],
        "n_properties": len(per_prop),
        "per_property": per_prop,
    }


# --------------------- toolchefs reducer branch ---------------------

def run_toolchefs(eps):
    out = WORK / f"toolchefs_eps_{eps}.json"
    if out.exists():
        print(f"  [toolchefs eps={eps}] cached → {out.name}")
        return out
    bundle = json.load(open(MOCAP_BBSM))
    fps = float(bundle.get("comp", {}).get("fps", 25.0))
    properties_out = []
    t0 = time.time()
    total_samples = 0
    for prop in bundle["properties"]:
        prop_id = prop["property"].get("id", "?")
        dims = int(prop["property"].get("dimensions", 1))
        samples = prop["samples"]
        channels = []
        for axis in range(dims):
            times = [float(s["t_sec"]) for s in samples]
            values = [float(s["v"][axis]) for s in samples]
            total_samples += len(times)
            n_keys, max_resid, kept = toolchefs_reduce(times, values, eps)
            channels.append({
                "axis_index": axis,
                "n_keys": n_keys,
                "max_abs_residual": max_resid,
            })
        properties_out.append({
            "property_id": prop_id,
            "dimensions": dims,
            "channels": channels,
        })
    elapsed_ms = (time.time() - t0) * 1000.0
    out_json = {
        "schema_version": 2,
        "meta": {
            "tolerance": eps,
            "total_samples": total_samples,
            "wall_clock_ms": elapsed_ms,
            "fps": fps,
        },
        "properties": properties_out,
    }
    out.write_text(json.dumps(out_json, indent=2) + "\n")
    print(f"  [toolchefs eps={eps}] {elapsed_ms/1000:.1f}s")
    return out


def analyze_toolchefs(eps, path):
    d = json.load(open(path))
    per_prop = []
    total_keys = 0
    for prop in d["properties"]:
        keys_per_dim = [ch["n_keys"] for ch in prop["channels"]]
        max_err = max(ch["max_abs_residual"] for ch in prop["channels"])
        total_keys += sum(keys_per_dim)
        per_prop.append({
            "property_id": prop["property_id"],
            "keys": sum(keys_per_dim),
            "keys_per_dim": keys_per_dim,
            "max_err": max_err,
            "dimensions": 3,
        })
    return {
        "method": "toolchefs_reducer",
        "eps": eps,
        "total_samples": d["meta"]["total_samples"],
        "total_keys": total_keys,
        "wall_clock_ms": d["meta"]["wall_clock_ms"],
        "n_properties": len(per_prop),
        "per_property": per_prop,
    }


# --------------------- aggregate + write CSV ---------------------

def write_results(results):
    # Headline: one row per (method, eps)
    headline = OUT_CSV_DIR / "fbx_mocap_method_comparison.csv"
    with open(headline, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow([
            "method", "eps",
            "total_samples", "total_keys",
            "key_compression_ratio",
            "median_per_prop_keys",
            "max_err_min", "max_err_median", "max_err_max",
            "wall_clock_ms",
        ])
        for r in results:
            errs = [p["max_err"] for p in r["per_property"]]
            keys = [p["keys"] for p in r["per_property"]]
            import statistics
            w.writerow([
                r["method"], r["eps"],
                r["total_samples"], r["total_keys"],
                round(r["total_samples"] / r["total_keys"], 4) if r["total_keys"] else 0,
                statistics.median(keys),
                round(min(errs), 6),
                round(statistics.median(errs), 6),
                round(max(errs), 6),
                round(r.get("solve_time_ms", r.get("wall_clock_ms", 0)), 1),
            ])
    print(f"wrote {headline}")

    # Per-property: one row per (method, eps, property)
    per_prop = OUT_CSV_DIR / "fbx_mocap_per_property.csv"
    with open(per_prop, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow([
            "method", "eps", "property_id",
            "keys", "max_err",
        ])
        for r in results:
            for p in r["per_property"]:
                w.writerow([
                    r["method"], r["eps"],
                    p["property_id"],
                    p["keys"],
                    round(p["max_err"], 6),
                ])
    print(f"wrote {per_prop}")


# --------------------- main ---------------------

def main(args=None):
    args = args or sys.argv[1:]
    skip_blender = "--skip-blender" in args

    results = []

    print("=== bbsolver ===")
    for eps in EPS_VALUES:
        bbky = run_bbsolver(eps)
        results.append(analyze_bbsolver(eps, bbky))

    if not skip_blender:
        print("\n=== blender F-Curve Decimate ===")
        for eps in EPS_VALUES:
            try:
                blender_out = run_blender_decimate(eps)
                results.append(analyze_blender(eps, blender_out))
            except Exception as e:
                print(f"  skipping blender eps={eps}: {e}")

    print("\n=== joosten / Paper.js bezier-fit reducer (open-source port) ===")
    for eps in EPS_VALUES:
        try:
            joosten_out = run_joosten(eps)
            results.append(analyze_joosten(eps, joosten_out))
        except Exception as e:
            print(f"  skipping joosten eps={eps}: {e}")

    print("\n=== Toolchefs keyReducer (open-source port, RDP-variant) ===")
    for eps in EPS_VALUES:
        try:
            tc_out = run_toolchefs(eps)
            results.append(analyze_toolchefs(eps, tc_out))
        except Exception as e:
            print(f"  skipping toolchefs eps={eps}: {e}")

    print("\n=== aggregating ===")
    write_results(results)


if __name__ == "__main__":
    main()
