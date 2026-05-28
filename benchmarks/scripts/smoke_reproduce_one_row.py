"""Reproducibility smoke test — re-solves one paper-cited bundle and asserts
the published per-property max_err is reached.

Picks the cheapest fixture in `corpus/` (the noodle ε=1.0 bake,
req-1779737483003 — single property, 242 samples, ~5 s solve) and:

  1. Reads the shipped SampleBundle.
  2. Invokes `bbsolver solve` at the recorded ε and screen-px tolerance.
  3. Reads the produced KeyBundle and compares per-property `max_err` to the
     value recorded in the shipped KeyBundle.
  4. Exits 0 on PASS, non-zero on FAIL with a clear diff.

Designed to run from a clean clone in ~30 seconds. Suitable for the existing
GitHub Actions `bbsolver CI` workflow's manual-dispatch trigger; the workflow
file does not need to be modified to add a new auto-trigger.

Usage:
    python scripts/smoke_reproduce_one_row.py
    python scripts/smoke_reproduce_one_row.py --request req-1779759462540  # ant rig
    python scripts/smoke_reproduce_one_row.py --tolerance-frac 0.02        # require within 2% of published
"""
from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from _paths import resolve_bbsolver_binary, resolve_request_dir

# Default smoke target: noodle ε=1.0. Single property, ~5 s solve, deterministic.
DEFAULT_REQUEST = "req-1779737483003"
DEFAULT_GROUP = "g1"
# bbsolver records max_err deterministically; the smoke tolerates only tiny
# floating-point drift from compiler/libm differences across platforms.
DEFAULT_TOLERANCE_FRAC = 0.01  # 1 % relative


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--request", default=DEFAULT_REQUEST,
                        help=f"request_id under corpus/ (default: {DEFAULT_REQUEST})")
    parser.add_argument("--group", default=DEFAULT_GROUP,
                        help=f"property group suffix _g1/_g2/_g3 (default: {DEFAULT_GROUP})")
    parser.add_argument("--bbsolver", default=None,
                        help="path to bbsolver binary (defaults to PATH lookup; "
                             "see scripts/_paths.py for the full search order)")
    parser.add_argument("--tolerance-frac", type=float, default=DEFAULT_TOLERANCE_FRAC,
                        help="relative tolerance for per-property max_err comparison "
                             "(default: %(default)s = 1%%)")
    args = parser.parse_args()

    bbsolver = resolve_bbsolver_binary(args.bbsolver)
    request_dir = Path(resolve_request_dir(args.request))
    bbsm = request_dir / f"{args.request}_{args.group}.bbsm.json"
    expected_bbky = request_dir / f"{args.request}_{args.group}.bbky.json"

    if not bbsm.is_file():
        print(f"FAIL: missing SampleBundle {bbsm}", file=sys.stderr)
        return 2
    if not expected_bbky.is_file():
        print(f"FAIL: missing expected KeyBundle {expected_bbky}", file=sys.stderr)
        return 2

    expected = json.loads(expected_bbky.read_text())
    eps = None
    screen_px = None
    for prop in expected.get("property_results", []):
        # Pull tolerance from the first property's recorded config; bbsolver
        # records this per-property.
        rec_eps = prop.get("eps") or prop.get("tolerance")
        rec_screen = prop.get("screen_px") or prop.get("tolerance_screen_px")
        if rec_eps is not None and eps is None:
            eps = rec_eps
        if rec_screen is not None and screen_px is None:
            screen_px = rec_screen
    if eps is None:
        # Fallback: derive from request_id mapping to corpus_manifest.csv
        # (or just default to ε=1 which is the default smoke target).
        eps = 1.0
    if screen_px is None:
        screen_px = 0  # disable separate screen-px gate

    with tempfile.TemporaryDirectory() as tmpdir:
        produced_bbky = Path(tmpdir) / "produced.bbky.json"
        cmd = [
            bbsolver, "solve", str(bbsm), str(produced_bbky),
            "--tolerance", str(eps),
            "--screen-px", str(screen_px),
            "--jobs", "0",
        ]
        print(f"running: {' '.join(cmd)}")
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"FAIL: bbsolver exited {result.returncode}")
            print("--- stdout ---"); print(result.stdout)
            print("--- stderr ---"); print(result.stderr)
            return result.returncode

        produced = json.loads(produced_bbky.read_text())

    # Compare per-property max_err.
    def err_map(bundle):
        return {p["property_id"]: p.get("max_err", 0.0) for p in bundle.get("property_results", [])}

    exp_err = err_map(expected)
    got_err = err_map(produced)

    if set(exp_err) != set(got_err):
        print(f"FAIL: property_id sets differ. "
              f"expected only: {set(exp_err) - set(got_err)}. "
              f"produced only: {set(got_err) - set(exp_err)}.")
        return 3

    all_pass = True
    max_diff_seen = 0.0
    for pid, exp_v in exp_err.items():
        got_v = got_err[pid]
        denom = max(abs(exp_v), 1e-9)
        rel = abs(got_v - exp_v) / denom
        max_diff_seen = max(max_diff_seen, rel)
        status = "ok" if rel <= args.tolerance_frac else "FAIL"
        if status != "ok":
            all_pass = False
            print(f"  [{status}] {pid}  expected_max_err={exp_v:.6f}  got={got_v:.6f}  "
                  f"rel_diff={rel:.4%}")

    print()
    print(f"properties checked: {len(exp_err)}")
    print(f"max relative max_err drift: {max_diff_seen:.4%}  "
          f"(tolerance: {args.tolerance_frac:.2%})")
    print(f"published total_keys: {expected.get('total_keys')}  "
          f"reproduced total_keys: {produced.get('total_keys')}")
    if all_pass and expected.get("total_keys") == produced.get("total_keys"):
        print("PASS: reproduction matches published bundle within tolerance.")
        return 0
    if expected.get("total_keys") != produced.get("total_keys"):
        print(f"FAIL: total_keys differs (expected {expected.get('total_keys')} "
              f"vs got {produced.get('total_keys')})")
    return 1


if __name__ == "__main__":
    sys.exit(main())
