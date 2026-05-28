"""Level-playing-field comparison: retune each open-source tool's input ε
so that its measured max_err meets a target accuracy, then compare key
counts. This neutralizes the structural advantage bbsolver has by virtue
of validating its output against source samples — instead of comparing
at matched requested ε (where bbsolver enforces ε and the others don't),
we compare at matched achieved accuracy (so all four methods are being
asked the same question: "what's the smallest key count that hits this
fidelity?").

Procedure:
    1. Run each open-source tool on a dense input-ε sweep
       (0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 1.5, 2.0, 3.0).
    2. For each target accuracy T ∈ {0.5, 1.0, 2.0, 3.0}, find the
       largest input ε for that tool whose max_err is ≤ T.
    3. Report keys produced at that retuned ε.
    4. Compare against bbsolver's keys at ε=T (where bbsolver's
       max_err is already ≤ T by construction).

Output:
    supplementary/fbx_mocap_level_playing_field.csv
    figures/fbx_mocap_level_playing_field.png
"""
from __future__ import annotations

import csv
import json
import os
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from _paths import resolve_fbx_mocap_bbsm
from external_runners.joosten_reducer import reduce_channel as joosten_reduce
from external_runners.toolchefs_reducer import reduce_channel as toolchefs_reduce

ARXIV_ROOT = HERE.parent
MOCAP_BBSM = Path(resolve_fbx_mocap_bbsm())
WORK = ARXIV_ROOT / "work" / "fbx_mocap"
OUT_CSV = ARXIV_ROOT / "data" / "supplementary" / "fbx_mocap_level_playing_field.csv"

# Dense input-ε grid to sweep the open-source tools across
SWEEP_EPS = [0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 1.5, 2.0, 3.0, 5.0]

# Target achieved max_err thresholds to compare at
TARGETS = [0.5, 1.0, 2.0, 3.0]


def _load_bundle():
    return json.load(open(MOCAP_BBSM))


def _run_method_at_eps(method_name, bundle, eps):
    """Returns dict with: total_keys, max_err_max."""
    if method_name == "joosten":
        reduce_fn = joosten_reduce
    elif method_name == "toolchefs":
        reduce_fn = toolchefs_reduce
    else:
        raise ValueError(method_name)
    total_keys = 0
    max_err_overall = 0.0
    for prop in bundle["properties"]:
        dims = int(prop["property"].get("dimensions", 1))
        samples = prop["samples"]
        prop_max_err = 0.0
        for axis in range(dims):
            times = [float(s["t_sec"]) for s in samples]
            values = [float(s["v"][axis]) for s in samples]
            n_keys, max_resid, _ = reduce_fn(times, values, eps)
            total_keys += n_keys
            if max_resid > prop_max_err:
                prop_max_err = max_resid
        if prop_max_err > max_err_overall:
            max_err_overall = prop_max_err
    return total_keys, max_err_overall


def _load_blender_cache(eps):
    p = WORK / f"blender_eps_{eps}.json"
    if not p.exists():
        return None
    d = json.load(open(p))
    total_keys = 0
    max_err = 0.0
    for prop in d["properties"]:
        for ch in prop["channels"]:
            total_keys += ch["n_keys"] if "n_keys" in ch else len(ch.get("keys", []))
            if ch["max_abs_residual"] > max_err:
                max_err = ch["max_abs_residual"]
    return total_keys, max_err


def _load_bbsolver_cache(eps):
    p = WORK / f"bbky_eps_{eps}.json"
    if not p.exists():
        return None
    ky = json.load(open(p))
    total_keys = sum(len(r["keys"]) for r in ky["property_results"])
    max_err = max(r["max_err"] for r in ky["property_results"])
    return total_keys, max_err


def sweep_method(method_name, bundle, eps_grid):
    """Run a method across all eps values; return list of dicts.

    For joosten/toolchefs we run live (cheap). For blender we use cached
    runs (only 0.5/1/2/3 are cached, so blender's level-playing-field
    table is sparse — flagged).
    """
    results = []
    for eps in eps_grid:
        if method_name == "blender":
            r = _load_blender_cache(eps)
            if r is None:
                continue
            keys, max_err = r
        elif method_name == "bbsolver":
            r = _load_bbsolver_cache(eps)
            if r is None:
                continue
            keys, max_err = r
        else:
            t0 = time.time()
            keys, max_err = _run_method_at_eps(method_name, bundle, eps)
            print(f"  [{method_name} ε={eps}] {keys} keys, max_err={max_err:.3f} ({time.time()-t0:.1f}s)")
        results.append({"eps": eps, "keys": keys, "max_err": max_err})
    return results


def retune_to_target(sweep_rows, target_max_err):
    """Find the largest input ε in the sweep whose measured max_err is ≤ target.
    Returns dict with eps, keys, max_err. None if no eps satisfies target."""
    matches = [r for r in sweep_rows if r["max_err"] <= target_max_err]
    if not matches:
        return None
    # Largest eps among those that meet the target (= fewest keys at acceptable accuracy)
    return max(matches, key=lambda r: r["eps"])


def main():
    bundle = _load_bundle()

    # bbsolver: cached results at 0.5/1/2/3 cover all targets we care about
    print("=== bbsolver (cached from earlier runs) ===")
    bbsolver_sweep = sweep_method("bbsolver", bundle, SWEEP_EPS)

    # Blender: cached only at 0.5/1/2/3 — limited
    print("=== Blender Decimate (cached only at 0.5/1/2/3) ===")
    blender_sweep = sweep_method("blender", bundle, SWEEP_EPS)

    # Joosten + Toolchefs: dense sweep, run live
    print("=== Joosten sweep (live) ===")
    joosten_sweep = sweep_method("joosten", bundle, SWEEP_EPS)
    print("=== Toolchefs sweep (live) ===")
    toolchefs_sweep = sweep_method("toolchefs", bundle, SWEEP_EPS)

    # Write CSV
    rows = []
    print("\n=== Level-playing-field comparison ===")
    for target in TARGETS:
        for method, sweep in (
            ("bbsolver", bbsolver_sweep),
            ("blender_decimate", blender_sweep),
            ("joosten_reducer", joosten_sweep),
            ("toolchefs_reducer", toolchefs_sweep),
        ):
            best = retune_to_target(sweep, target)
            if best is None:
                rows.append({
                    "target_max_err": target,
                    "method": method,
                    "retuned_eps": "n/a",
                    "achieved_max_err": "n/a",
                    "keys": "n/a",
                    "note": "no input ε in sweep achieves target",
                })
                print(f"  target={target}  {method}: NO INPUT ε IN SWEEP ACHIEVES TARGET")
            else:
                rows.append({
                    "target_max_err": target,
                    "method": method,
                    "retuned_eps": best["eps"],
                    "achieved_max_err": round(best["max_err"], 4),
                    "keys": best["keys"],
                    "note": "",
                })
                print(f"  target={target}  {method}: input ε={best['eps']}, achieved {best['max_err']:.3f}, keys={best['keys']}")

    # Full sweep table also written so reviewers can see the raw retuning data
    sweep_rows_all = []
    for method, sweep in (
        ("bbsolver", bbsolver_sweep),
        ("blender_decimate", blender_sweep),
        ("joosten_reducer", joosten_sweep),
        ("toolchefs_reducer", toolchefs_sweep),
    ):
        for r in sweep:
            sweep_rows_all.append({
                "method": method,
                "input_eps": r["eps"],
                "keys": r["keys"],
                "achieved_max_err": round(r["max_err"], 4),
            })

    OUT_CSV.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT_CSV, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=[
            "target_max_err", "method", "retuned_eps",
            "achieved_max_err", "keys", "note",
        ])
        w.writeheader()
        w.writerows(rows)
    print(f"\nwrote {OUT_CSV} ({len(rows)} rows)")

    sweep_csv = OUT_CSV.with_name("fbx_mocap_full_sweep.csv")
    with open(sweep_csv, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=["method", "input_eps", "keys", "achieved_max_err"])
        w.writeheader()
        w.writerows(sweep_rows_all)
    print(f"wrote {sweep_csv} ({len(sweep_rows_all)} rows)")


if __name__ == "__main__":
    main()
