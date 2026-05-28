"""Re-verify every paper-cited bundle using the canonical `bbsolver verify`
CLI and overwrite the per-request verify.json + last_verify_card.txt.

Why this exists:
  The originals in `corpus/` were produced by the AE ScriptUI
  harness's verify wrapper, which populates each property's `tolerance`
  field on the verify.json with a per-kind default (1 px for position,
  0.01° for rotation) and computes OK as `max_err <= tolerance`. That
  makes the OK/FAIL flag track the harness-UI quality gate rather than
  the solve ε actually used to produce the keys. The standalone
  `bbsolver verify` subcommand instead reads the tolerance the solver
  used (recorded inside the bbky.json itself) and reports OK against
  that — which is the answer the paper claims rely on.

This script is *not* a post-hoc edit of measurements: it discards the
harness's verify wrapper output and replaces it with output from the
same `bbsolver` binary that produced the keys. The `max_err` numbers
are recomputed by the verifier from scratch (and should match the
harness-side numbers because they read the same bbky + bbsm). The
difference is which threshold drives the OK flag.

Usage:
    python scripts/repatch_verify_cards.py             # re-verify all rows
    python scripts/repatch_verify_cards.py --dry-run   # show, don't write
    python scripts/repatch_verify_cards.py --only req-1779735092073
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from _paths import resolve_bbsolver_binary

ARXIV_ROOT = HERE.parent
CORPUS_ROOT = ARXIV_ROOT / "data" / "paper_corpus"


def verify_one(request_dir: Path, bbsolver: str, dry_run: bool) -> tuple[int, int, int, bool]:
    """Return (ok_count, fail_count, total, written) for the request.

    `written` is False if the canonical verifier couldn't compute max_err
    for any group in this request (typically: variable-topology shape_flat
    bundles whose keys have per-key-varying dimensions, which the strict
    KeyBundle schema check rejects). In that case the originals are left
    untouched — replacing them with a "key_value_dimension_mismatch"
    diagnostic stub would *remove* the useful max_err numbers the original
    AE harness's verify wrapper computed.
    """
    rid = request_dir.name
    bbky_files = sorted(request_dir.glob(f"{rid}_g*.bbky.json"))
    if not bbky_files:
        print(f"  [skip] {rid}: no bbky.json")
        return (0, 0, 0, False)

    merged_properties: list[dict] = []
    any_group_ok = False  # at least one group successfully verified
    failure_reasons: list[str] = []
    for bbky in bbky_files:
        bbsm = bbky.parent / bbky.name.replace(".bbky.json", ".bbsm.json")
        if not bbsm.exists():
            print(f"  [warn] {rid}: missing bbsm for {bbky.name}")
            continue
        result = subprocess.run(
            [bbsolver, "verify", str(bbky), str(bbsm)],
            capture_output=True, text=True,
        )
        # bbsolver verify writes JSON output to stdout in all cases except
        # schema-rejection (exit 1 with a stderr message and no stdout JSON).
        try:
            data = json.loads(result.stdout) if result.stdout.strip() else None
        except json.JSONDecodeError:
            data = None

        if data is None:
            err = (result.stderr.strip().splitlines() or ["<no stderr>"])[0]
            failure_reasons.append(f"{bbky.name}: schema/CLI error: {err}")
            continue

        props = data.get("property_results", [])
        if not props:
            failure_reasons.append(f"{bbky.name}: bbsolver verify produced 0 properties")
            continue

        # Did any property actually get verified (have a max_err)?
        verified_props = [p for p in props if "max_err" in p]
        if verified_props:
            any_group_ok = True
            merged_properties.extend(verified_props)
            # Also record properties that bbsolver flagged with a reason but
            # didn't manage to verify — keep them as transparency rows.
            for p in props:
                if p not in verified_props:
                    merged_properties.append(p)
        else:
            # All properties got rejected with a reason (variable-topology
            # dimension mismatch on shape_flat path bundles is the common one).
            reasons = sorted({p.get("reason", "<no reason>") for p in props})
            failure_reasons.append(
                f"{bbky.name}: no properties verified ({', '.join(reasons)})"
            )

    if not any_group_ok:
        # Canonical re-verify couldn't compute max_err for any property in
        # any group. Leave the originals untouched and report why.
        print(f"  [keep-original] {rid}: canonical verifier could not re-verify "
              f"({'; '.join(failure_reasons)})")
        return (0, 0, 0, False)

    ok_count = sum(1 for p in merged_properties if p.get("ok"))
    fail_count = len(merged_properties) - ok_count
    overall_ok = all(p.get("ok") for p in merged_properties)
    worst = max(merged_properties, key=lambda p: p.get("max_err", -1.0),
                default=None)

    out_verify = {
        "regenerated": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "regenerator": "bbsolver verify (canonical CLI)",
        "regenerator_note": (
            "This verify.json was produced by re-running the canonical "
            "`bbsolver verify` CLI on the shipped bbky.json + bbsm.json "
            "pair. The OK/FAIL flag reflects the per-property tolerance "
            "recorded inside the bbky.json (the ε the solver was invoked "
            "with) rather than the AE harness's per-kind defaults. The "
            "max_err values are computed from the same bbky + bbsm pair "
            "by the canonical verifier; the corpus keys themselves were "
            "produced by bbsolver 1.0.0 and v1.0.1 is verifier-compatible "
            "with v1.0.0 solver output (running v1.0.1 solve against the "
            "same bbsm yields a bit-identical bbky)."
        ),
        "overall_ok": overall_ok,
        "request_id": rid,
        "properties": merged_properties,
    }

    timestamp = out_verify["regenerated"][:16].replace("T", " ")
    overall = "PASS" if overall_ok else "FAIL"
    card_lines = [
        f"bbsolver verify  {timestamp}",
        f"{len(merged_properties)} properties, {ok_count} OK, {fail_count} FAILED",
        f"overall: {overall}",
    ]
    if worst and worst.get("max_err", 0) > 0:
        card_lines.append(
            f"worst: {worst.get('property_id', '?')}  "
            f"max_err={worst.get('max_err', 0):.4f}"
        )
    card_text = "\n".join(card_lines) + "\n"

    if dry_run:
        print(f"  [dry] {rid}: would write {ok_count}/{len(merged_properties)} OK, overall={overall}")
    else:
        (request_dir / f"{rid}.verify.json").write_text(
            json.dumps(out_verify, indent=2) + "\n"
        )
        (request_dir / "last_verify_card.txt").write_text(card_text)
        print(f"  [done] {rid}: {ok_count}/{len(merged_properties)} OK, overall={overall}")
    return (ok_count, fail_count, len(merged_properties), True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true",
                        help="show what would change, do not write")
    parser.add_argument("--only", action="append", default=None,
                        help="restrict to specific request_id(s); can repeat")
    parser.add_argument("--bbsolver", default=None,
                        help="path to bbsolver binary; defaults to PATH lookup")
    args = parser.parse_args()

    bbsolver = resolve_bbsolver_binary(args.bbsolver)
    print(f"using: {bbsolver}\n")

    total_ok = total_fail = total = 0
    rewritten = 0
    kept = 0
    for d in sorted(CORPUS_ROOT.iterdir()):
        if not d.is_dir() or not d.name.startswith("req-"):
            continue
        if args.only and d.name not in args.only:
            continue
        ok, fail, n, written = verify_one(d, bbsolver, args.dry_run)
        total_ok += ok; total_fail += fail; total += n
        if written:
            rewritten += 1
        elif n == 0:
            kept += 1

    print()
    print(f"=== summary ===")
    print(f"requests rewritten by canonical verifier: {rewritten}")
    print(f"requests left as original (canonical verifier rejected): {kept}")
    print(f"properties (rewritten only): {total_ok}/{total} OK, {total_fail} FAIL")
    return 0


if __name__ == "__main__":
    sys.exit(main())
