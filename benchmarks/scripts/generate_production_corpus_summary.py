#!/usr/bin/env python3
"""Generate the production-corpus summary CSV that backs §5.1 of the paper.

Walks every request_id directory under the two on-disk live_runs corpora
(combined development corpus), parses each bbky.json for solve_time_ms / total_keys /
total_samples_input / property_results, infers whether the run touched a path
property by inspecting property IDs (AE path properties contain
'Vector Shape' or 'Path'), and emits two CSVs:

    supplementary/production_corpus_per_run.csv
        one row per request_id with raw measurements

    supplementary/production_corpus_summary.csv
        single-row aggregate statistics: counts, medians, percentiles

The script is path-agnostic via two CLI arguments (defaults reflect the
author's local checkout). Set --no-default-paths to require explicit paths.
"""
from __future__ import annotations

import argparse
import csv
import json
import os
import re
import statistics
import sys
from pathlib import Path
from typing import Iterable


# ---------------------------------------------------------------------------
# Path-touching property detection
# ---------------------------------------------------------------------------

PATH_PROPERTY_PATTERNS = (
    re.compile(r"Vector Shape", re.IGNORECASE),
    re.compile(r"ADBE Vector Shape", re.IGNORECASE),
    re.compile(r"Mask Path", re.IGNORECASE),
    re.compile(r"ADBE Mask Shape", re.IGNORECASE),
    re.compile(r"shape_flat", re.IGNORECASE),
)

TOPOLOGY_NOTE_PATTERNS = (
    re.compile(r"replacement", re.IGNORECASE),
    re.compile(r"variable.topology", re.IGNORECASE),
    re.compile(r"topology.event", re.IGNORECASE),
    re.compile(r"vertex.count.change", re.IGNORECASE),
    re.compile(r"canonical.path", re.IGNORECASE),
    re.compile(r"per.region", re.IGNORECASE),
    re.compile(r"landmark", re.IGNORECASE),
)


def is_path_property(prop: dict) -> bool:
    pid = prop.get("property_id") or ""
    units = prop.get("units_label") or ""
    kind = prop.get("kind") or ""
    haystack = f"{pid} {units} {kind}"
    return any(p.search(haystack) for p in PATH_PROPERTY_PATTERNS)


def has_topology_note(prop: dict) -> bool:
    notes = prop.get("notes")
    if not notes:
        return False
    if isinstance(notes, list):
        joined = " ".join(str(n) for n in notes)
    else:
        joined = str(notes)
    return any(p.search(joined) for p in TOPOLOGY_NOTE_PATTERNS)


# ---------------------------------------------------------------------------
# Per-request parsing
# ---------------------------------------------------------------------------

def parse_request_dir(req_dir: Path) -> dict | None:
    """Parse every bbky.json in a request directory; return aggregated stats.

    Returns None if the directory has no bbky.json files (e.g. only verify
    artifacts present, or directory is otherwise incomplete).
    """
    req_id = req_dir.name
    bbky_files = sorted(req_dir.glob(f"{req_id}_g*.bbky.json"))
    if not bbky_files:
        return None

    total_keys = 0
    total_samples = 0
    solve_time_ms = 0.0
    n_properties = 0
    n_path_properties = 0
    n_topology_notes = 0
    solver_versions: set[str] = set()
    has_timing = False
    max_err_overall = 0.0
    max_err_screen_px_overall = 0.0
    converged_count = 0
    converged_total = 0

    for bbky in bbky_files:
        try:
            data = json.loads(bbky.read_text())
        except (OSError, json.JSONDecodeError):
            continue
        total_keys += int(data.get("total_keys") or 0)
        total_samples += int(data.get("total_samples_input") or 0)
        stm = data.get("solve_time_ms")
        if isinstance(stm, (int, float)) and stm > 0:
            solve_time_ms += float(stm)
            has_timing = True
        sv = data.get("solver_version")
        if sv:
            solver_versions.add(sv)

        for prop in data.get("property_results") or ():
            n_properties += 1
            converged_total += 1
            if prop.get("converged"):
                converged_count += 1
            if is_path_property(prop):
                n_path_properties += 1
            if has_topology_note(prop):
                n_topology_notes += 1
            me = prop.get("max_err")
            if isinstance(me, (int, float)):
                max_err_overall = max(max_err_overall, float(me))
            mep = prop.get("max_err_screen_px")
            if isinstance(mep, (int, float)):
                max_err_screen_px_overall = max(max_err_screen_px_overall, float(mep))

    if total_samples == 0 and total_keys == 0 and not has_timing:
        return None

    reduction = (total_samples / total_keys) if total_keys else None
    converge_frac = (converged_count / converged_total) if converged_total else None

    return {
        "request_id": req_id,
        "n_groups": len(bbky_files),
        "n_properties": n_properties,
        "n_path_properties": n_path_properties,
        "n_topology_notes": n_topology_notes,
        "total_samples_input": total_samples,
        "total_keys_output": total_keys,
        "reduction_ratio": round(reduction, 4) if reduction is not None else None,
        "has_timing": has_timing,
        "solve_time_ms": round(solve_time_ms, 2) if has_timing else None,
        "solver_versions": ";".join(sorted(solver_versions)) if solver_versions else "",
        "max_err": round(max_err_overall, 6),
        "max_err_screen_px": round(max_err_screen_px_overall, 6),
        "converge_fraction": round(converge_frac, 4) if converge_frac is not None else None,
    }


def walk_corpus(roots: Iterable[Path]) -> list[dict]:
    rows: dict[str, dict] = {}
    for root in roots:
        if not root.is_dir():
            print(f"[warn] skipping missing corpus root: {root}", file=sys.stderr)
            continue
        for req_dir in sorted(root.iterdir()):
            if not req_dir.is_dir() or not req_dir.name.startswith("req-"):
                continue
            parsed = parse_request_dir(req_dir)
            if parsed is None:
                continue
            # Prefer the row with more groups (more complete) if a request_id
            # exists in both corpora.
            prev = rows.get(parsed["request_id"])
            if prev is None or parsed["n_groups"] > prev["n_groups"]:
                rows[parsed["request_id"]] = parsed
    return sorted(rows.values(), key=lambda r: r["request_id"])


# ---------------------------------------------------------------------------
# Aggregation
# ---------------------------------------------------------------------------

def percentile(values: list[float], p: float) -> float | None:
    if not values:
        return None
    s = sorted(values)
    k = max(0, min(len(s) - 1, int(round((p / 100) * (len(s) - 1)))))
    return s[k]


def summarize(rows: list[dict]) -> dict:
    total = len(rows)
    timed = [r for r in rows if r.get("has_timing")]
    samples = [r["total_samples_input"] for r in rows if r["total_samples_input"] > 0]
    keys = [r["total_keys_output"] for r in rows if r["total_keys_output"] > 0]
    reductions = [r["reduction_ratio"] for r in rows if r.get("reduction_ratio")]
    solve_times = [r["solve_time_ms"] for r in timed]
    path_runs = [r for r in rows if r["n_path_properties"] > 0]
    topology_runs = [r for r in rows if r["n_topology_notes"] > 0]

    return {
        "total_runs": total,
        "runs_with_timing": len(timed),
        "fraction_with_timing": round(len(timed) / total, 4) if total else None,
        "runs_with_path_properties": len(path_runs),
        "fraction_path_runs": round(len(path_runs) / total, 4) if total else None,
        "runs_with_topology_notes": len(topology_runs),
        "fraction_topology_runs": round(len(topology_runs) / total, 4) if total else None,
        "median_samples_input": int(statistics.median(samples)) if samples else None,
        "median_keys_output": int(statistics.median(keys)) if keys else None,
        "median_reduction_ratio": round(statistics.median(reductions), 4) if reductions else None,
        "mean_reduction_ratio": round(statistics.mean(reductions), 4) if reductions else None,
        "p10_reduction_ratio": percentile(reductions, 10),
        "p90_reduction_ratio": percentile(reductions, 90),
        "median_solve_time_ms": round(statistics.median(solve_times), 1) if solve_times else None,
        "mean_solve_time_ms": round(statistics.mean(solve_times), 1) if solve_times else None,
        "p10_solve_time_ms": round(percentile(solve_times, 10), 1) if solve_times else None,
        "p90_solve_time_ms": round(percentile(solve_times, 90), 1) if solve_times else None,
        "max_solve_time_ms": round(max(solve_times), 1) if solve_times else None,
        "sum_samples_input": sum(samples),
        "sum_keys_output": sum(keys),
        "overall_reduction_ratio": round(sum(samples) / sum(keys), 4) if sum(keys) else None,
    }


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

DEFAULT_ROOTS = [
    Path.home() / "github/bbsolver/artifacts/bbsolver/corpus/live_runs.alt",
    Path.home() / "github/bbsolver/artifacts/bbsolver/corpus/live_runs",
]

DEFAULT_OUT_DIR = Path(__file__).resolve().parent.parent / "data" / "supplementary"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--corpus-root",
        action="append",
        type=Path,
        default=None,
        help=(
            "live_runs/ directory to scan. Can be repeated. Defaults to "
            ""
            "~/github/bbsolver/artifacts/bbsolver/corpus/live_runs."
        ),
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=DEFAULT_OUT_DIR,
        help="Directory to write the two output CSVs (default: %(default)s).",
    )
    parser.add_argument(
        "--no-default-paths",
        action="store_true",
        help="Require explicit --corpus-root; do not fall back to defaults.",
    )
    args = parser.parse_args()

    roots = args.corpus_root
    if not roots:
        if args.no_default_paths:
            parser.error("at least one --corpus-root is required")
        roots = DEFAULT_ROOTS

    print(f"scanning {len(roots)} corpus root(s):")
    for r in roots:
        print(f"  {r}  (exists={r.is_dir()})")

    rows = walk_corpus(roots)
    if not rows:
        print("[error] no rows produced", file=sys.stderr)
        return 1

    args.out_dir.mkdir(parents=True, exist_ok=True)

    per_run_csv = args.out_dir / "production_corpus_per_run.csv"
    summary_csv = args.out_dir / "production_corpus_summary.csv"

    fieldnames = [
        "request_id", "n_groups", "n_properties", "n_path_properties",
        "n_topology_notes", "total_samples_input", "total_keys_output",
        "reduction_ratio", "has_timing", "solve_time_ms", "solver_versions",
        "max_err", "max_err_screen_px", "converge_fraction",
    ]
    with per_run_csv.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)
    print(f"wrote {per_run_csv}  ({len(rows)} rows)")

    summary = summarize(rows)
    with summary_csv.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(summary.keys()))
        writer.writeheader()
        writer.writerow(summary)
    print(f"wrote {summary_csv}")
    print()
    print("=== aggregate stats ===")
    for k, v in summary.items():
        print(f"  {k}: {v}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
