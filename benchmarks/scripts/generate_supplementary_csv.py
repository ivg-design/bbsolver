"""Generate supplementary CSV data files supporting the paper's
quantitative tables and figures. Output goes to
``benchmarks/supplementary/``.

Each CSV corresponds to a specific table or figure cited in the paper.
A ``manifest.csv`` indexes every file with section reference, source
artifact request_ids, and a short description so the data archive is
self-describing.

This script is deterministic and re-runnable. The path CSVs distinguish
three independent error measurements when they differ:

  * ``solver_max_err_px`` — in-loop validation recorded inside the
    KeyBundle by the solver against its sampled bbsm input.
  * ``cli_verify_max_err_px`` — recomputed by ``bbsolver verify``
    against the shipped ``<req>.bbky.json`` + ``<req>.bbsm.json`` pair.
    Reads from ``corpus/<rid>/<rid>.verify.json`` which is
    canonical CLI output (regenerator: ``bbsolver verify (canonical CLI)``).
  * ``ae_roundtrip_max_err_px`` — measured by the After Effects host
    after writing the keys back and re-sampling AE playback. This
    artifact only exists in the original development ``live_runs/``
    corpus (not redistributed because some original production
    histories are private). Set the ``BBSOLVER_AE_ROUNDTRIP_ROOT``
    environment variable to a ``live_runs/`` root to regenerate these
    values; otherwise the shipped CSV values are preserved.

The script reads raw bake artifacts at the same paths as
``generate_round_trip_figures.py``. Override paths via the helpers in
``_paths.py``.
"""
from __future__ import annotations

import csv
import json
import os
import statistics
from pathlib import Path

import sys

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from _paths import resolve_request_dir

ARXIV_ROOT = HERE.parent
OUT_DIR = ARXIV_ROOT / "data" / "supplementary"
OUT_DIR.mkdir(parents=True, exist_ok=True)

# Optional override for the After Effects round-trip verify.json source.
# When set, ``ae_roundtrip_max_err_px`` columns are repopulated from
# ``<AE_ROUNDTRIP_ROOT>/<rid>/<rid>.verify.json``. When unset (the
# common case for external reproducers), the script leaves the shipped
# ``ae_roundtrip_max_err_px`` value unchanged — see the AE round-trip
# rows for the cited fixtures in the existing CSVs.
AE_ROUNDTRIP_ROOT = os.environ.get("BBSOLVER_AE_ROUNDTRIP_ROOT")


def _read_ae_roundtrip_max_err(rid: str):
    """Return the worst-property max_err recorded by the original
    AE-harness verify.json for ``rid`` under AE_ROUNDTRIP_ROOT, or None
    when the environment variable is unset or the artifact is absent.
    """
    if not AE_ROUNDTRIP_ROOT:
        return None
    cand = Path(AE_ROUNDTRIP_ROOT) / rid / f"{rid}.verify.json"
    if not cand.is_file():
        return None
    d = json.load(open(cand))
    props = d.get("properties", d.get("property_results", []))
    finite = [p.get("max_err", 0.0) for p in props if "max_err" in p]
    return max(finite) if finite else None

WALK_CYCLE_RUNS = {
    0.05: resolve_request_dir("req-1779727765498"),
    1.0:  resolve_request_dir("req-1779735092073"),
    3.0:  resolve_request_dir("req-1779733597712"),
}
ANT_RIG_RUN = resolve_request_dir("req-1779759462540")
NOODLE_DEFAULT_RUNS = {
    0.5: resolve_request_dir("req-1779740361822"),
    1.0: resolve_request_dir("req-1779737483003"),
    3.0: resolve_request_dir("req-1779740577991"),
}
NOODLE_EXPERIMENTAL_RUN = resolve_request_dir("req-1779741201109")
BLOB_V1_RUN = resolve_request_dir("req-1779762426464")
BLOB_V6_RUN = resolve_request_dir("req-1779786372293")


def _load_run(run_dir):
    run_dir = Path(run_dir).resolve()
    rid = run_dir.name
    sm_list, ky_list = [], []
    for sm_path in sorted(run_dir.glob(f"{rid}_g*.bbsm.json")):
        if any(t in sm_path.name for t in ("cleanup_", "_verify_source")):
            continue
        ky_path = sm_path.with_name(sm_path.name.replace(".bbsm.json", ".bbky.json"))
        sm_list.append(json.load(open(sm_path)))
        ky_list.append(json.load(open(ky_path)))
    vf = json.load(open(run_dir / f"{rid}.verify.json"))
    return sm_list, ky_list, vf, rid


def _write_csv(path, header, rows):
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(header)
        for r in rows:
            w.writerow(r)
    print(f"wrote {path}  ({len(rows)} rows)")


# --------------------- §5.2 walk-cycle headline & per-property ---------------------

def write_walk_cycle_pareto():
    """§5.2 — three-point Pareto on humanoid walk cycle."""
    rows = []
    per_prop_rows = []
    for eps, run_dir in sorted(WALK_CYCLE_RUNS.items()):
        sm_list, ky_list, vf, rid = _load_run(run_dir)
        sm_by_id = {}
        for sm in sm_list:
            for p in sm["properties"]:
                sm_by_id[p["property"]["id"]] = len(p.get("samples", []))
        pos_in = pos_out = rot_in = rot_out = 0
        for ky in ky_list:
            for r in ky["property_results"]:
                pid = r["property_id"]
                n_in = sm_by_id.get(pid, 0)
                n_out = len(r.get("keys", []))
                if "Rotate" in pid:
                    rot_in += n_in; rot_out += n_out
                else:
                    pos_in += n_in; pos_out += n_out
        pos_err = [p["max_err"] for p in vf["properties"] if "Rotate" not in p["id"]]
        rot_err = [p["max_err"] for p in vf["properties"] if "Rotate" in p["id"]]
        rows.append([
            eps, rid,
            pos_in, pos_out, round(pos_in / pos_out, 4),
            rot_in, rot_out, round(rot_in / rot_out, 4),
            (pos_in + rot_in), (pos_out + rot_out),
            round((pos_in + rot_in) / (pos_out + rot_out), 4),
            round(min(pos_err), 6), round(statistics.median(pos_err), 6), round(max(pos_err), 6),
            round(min(rot_err), 6), round(statistics.median(rot_err), 6), round(max(rot_err), 6),
        ])
        # per-property rows
        for vp in vf["properties"]:
            per_prop_rows.append([
                eps, rid, vp["id"], vp["tolerance"],
                round(vp["max_err"], 6), vp["ok"],
                round(vp["worst_t_sec"], 4), vp["worst_sample_index"],
            ])

    _write_csv(
        OUT_DIR / "table_6_6_1_walk_cycle_pareto.csv",
        ["eps", "request_id",
         "pos_samples_in", "pos_keys_out", "pos_compression_ratio",
         "rot_samples_in", "rot_keys_out", "rot_compression_ratio",
         "total_samples_in", "total_keys_out", "combined_compression_ratio",
         "pos_verify_max_err_min", "pos_verify_max_err_median", "pos_verify_max_err_max",
         "rot_verify_max_err_min", "rot_verify_max_err_median", "rot_verify_max_err_max"],
        rows
    )
    _write_csv(
        OUT_DIR / "per_property_6_6_walk_cycle.csv",
        ["eps", "request_id", "property_id", "verify_tolerance",
         "verify_max_err", "ok", "worst_t_sec", "worst_sample_index"],
        per_prop_rows
    )


# --------------------- §5.2 ant rig headline & per-property ---------------------

def write_ant_rig():
    sm_list, ky_list, vf, rid = _load_run(ANT_RIG_RUN)
    sm_by_id = {p["property"]["id"]: len(p.get("samples", []))
                for sm in sm_list for p in sm["properties"]}
    pos_in = pos_out = rot_in = rot_out = 0
    for ky in ky_list:
        for r in ky["property_results"]:
            pid = r["property_id"]
            n_in = sm_by_id.get(pid, 0)
            n_out = len(r.get("keys", []))
            if "Rotate" in pid:
                rot_in += n_in; rot_out += n_out
            else:
                pos_in += n_in; pos_out += n_out
    pos_err = [p["max_err"] for p in vf["properties"] if "Rotate" not in p["id"]]
    rot_err = [p["max_err"] for p in vf["properties"] if "Rotate" in p["id"]]
    _write_csv(
        OUT_DIR / "table_6_6_3_ant_rig_headline.csv",
        ["eps", "request_id",
         "pos_samples_in", "pos_keys_out", "pos_compression_ratio",
         "rot_samples_in", "rot_keys_out", "rot_compression_ratio",
         "total_samples_in", "total_keys_out", "combined_compression_ratio",
         "pos_verify_max_err_min", "pos_verify_max_err_median", "pos_verify_max_err_max",
         "rot_verify_max_err_min", "rot_verify_max_err_median", "rot_verify_max_err_max"],
        [[
            1.0, rid,
            pos_in, pos_out, round(pos_in / pos_out, 4),
            rot_in, rot_out, round(rot_in / rot_out, 4),
            (pos_in + rot_in), (pos_out + rot_out),
            round((pos_in + rot_in) / (pos_out + rot_out), 4),
            round(min(pos_err), 6), round(statistics.median(pos_err), 6), round(max(pos_err), 6),
            round(min(rot_err), 6), round(statistics.median(rot_err), 6), round(max(rot_err), 6),
        ]]
    )

    rows = []
    for vp in vf["properties"]:
        rows.append([
            1.0, rid, vp["id"], vp["tolerance"],
            round(vp["max_err"], 6), vp["ok"],
            round(vp["worst_t_sec"], 4), vp["worst_sample_index"],
        ])
    _write_csv(
        OUT_DIR / "per_property_6_6_3_ant_rig.csv",
        ["eps", "request_id", "property_id", "verify_tolerance",
         "verify_max_err", "ok", "worst_t_sec", "worst_sample_index"],
        rows
    )


# --------------------- §5.7 noodle three-point Pareto ---------------------

def _preserve_ae_roundtrip(csv_path, rid, eps_key="eps"):
    """Read the existing shipped CSV (if present) and return the
    ae_roundtrip_max_err_px column value previously recorded for (eps, rid).
    Falls back to None when the CSV doesn't exist yet."""
    if not csv_path.exists():
        return None
    with open(csv_path) as f:
        reader = csv.DictReader(f)
        for r in reader:
            if r.get("request_id") == rid:
                v = r.get("ae_roundtrip_max_err_px", "")
                try:
                    return float(v) if v else None
                except ValueError:
                    return None
    return None


def write_noodle_pareto():
    out_path = OUT_DIR / "table_6_7_1_noodle_pareto.csv"
    rows = []
    for eps, run_dir in sorted(NOODLE_DEFAULT_RUNS.items()):
        sm_list, ky_list, vf, rid = _load_run(run_dir)
        sm, ky = sm_list[0], ky_list[0]
        samples = sm["properties"][0]["samples"]
        keys = ky["property_results"][0]["keys"]
        src_floats = sum(len(s["v"]) for s in samples)
        out_floats = sum(len(k["v"]) for k in keys)
        ae = _read_ae_roundtrip_max_err(rid)
        if ae is None:
            ae = _preserve_ae_roundtrip(out_path, rid)
        rows.append([
            eps, rid,
            len(samples), len(keys),
            round(len(samples) / len(keys), 4),
            src_floats, out_floats,
            round(src_floats / out_floats, 4),
            int(keys[0]["v"][1]),
            max(int(s["v"][1]) for s in samples),
            round(ky["property_results"][0]["max_err"], 6),
            round(ae, 6) if ae is not None else "",
            round(vf["properties"][0]["max_err"], 6),
            round(ky["solve_time_ms"] / 1000.0, 2),
        ])
    _write_csv(
        out_path,
        ["eps", "request_id",
         "samples", "keys",
         "temporal_compression_ratio",
         "src_floats", "out_floats", "volume_compression_ratio",
         "output_v1_uniform", "source_v1_max",
         "solver_max_err_px", "ae_roundtrip_max_err_px", "cli_verify_max_err_px",
         "solve_time_s"],
        rows
    )


# --------------------- §5.7 noodle default vs experimental ---------------------

def write_noodle_default_vs_experimental():
    out_path = OUT_DIR / "table_6_7_2_noodle_default_vs_experimental.csv"
    rows = []
    # Default ε=1 row
    sm_list, ky_list, vf, rid_d = _load_run(NOODLE_DEFAULT_RUNS[1.0])
    sm_d, ky_d = sm_list[0], ky_list[0]
    samples_d = sm_d["properties"][0]["samples"]
    keys_d = ky_d["property_results"][0]["keys"]
    ae_d = _read_ae_roundtrip_max_err(rid_d) or _preserve_ae_roundtrip(out_path, rid_d)
    rows.append([
        "default_two_pass", 1.0, rid_d,
        len(samples_d), len(keys_d),
        round(len(samples_d) / len(keys_d), 4),
        round(sum(len(s["v"]) for s in samples_d) / sum(len(k["v"]) for k in keys_d), 4),
        int(keys_d[0]["v"][1]),
        round(ky_d["property_results"][0]["max_err"], 6),
        round(ae_d, 6) if ae_d is not None else "",
        round(vf["properties"][0]["max_err"], 6),
        round(ky_d["solve_time_ms"] / 1000.0, 2),
        "none",
    ])
    # Experimental ε=1 row.
    # NOTE: keys count reported here is for the main production output path
    # only. The experimental flag set also emits a diagnostic landmark
    # subpath property; the full bundle therefore contains 2 properties for
    # a total of 276 keys (139 main + 137 landmark). See corpus_manifest.csv
    # for the bundle-level count and §5.7 paper text for the convention.
    sm_list, ky_list, vf_e, rid_e = _load_run(NOODLE_EXPERIMENTAL_RUN)
    sm_e, ky_e = sm_list[0], ky_list[0]
    samples_e = sm_e["properties"][0]["samples"]
    keys_e = ky_e["property_results"][0]["keys"]
    # Pick the main property (first one), not the landmark subpath. The
    # main property has the larger key count and matches the source samples.
    main_prop_idx = max(
        range(len(ky_e["property_results"])),
        key=lambda i: len(ky_e["property_results"][i].get("keys", [])),
    )
    main = ky_e["property_results"][main_prop_idx]
    keys_e = main["keys"]
    cli_main = max(
        (p.get("max_err", 0.0) for p in vf_e.get("properties", []) if "max_err" in p),
        default=0.0,
    )
    ae_e = _read_ae_roundtrip_max_err(rid_e) or _preserve_ae_roundtrip(out_path, rid_e)
    rows.append([
        "experimental_flagset", 1.0, rid_e,
        len(samples_e), len(keys_e),
        round(len(samples_e) / len(keys_e), 4),
        round(sum(len(s["v"]) for s in samples_e) / sum(len(k["v"]) for k in keys_e), 4),
        int(keys_e[0]["v"][1]),
        round(main["max_err"], 6),
        round(ae_e, 6) if ae_e is not None else "",
        round(cli_main, 6),
        round(ky_e["solve_time_ms"] / 1000.0, 2),
        "--decompose-paths --fit-canonical-paths --fit-replacement-paths --emit-landmark-subpaths",
    ])
    _write_csv(
        out_path,
        ["mode", "eps", "request_id",
         "samples", "keys",
         "temporal_compression_ratio", "volume_compression_ratio",
         "output_v1_uniform",
         "solver_max_err_px", "ae_roundtrip_max_err_px", "cli_verify_max_err_px",
         "solve_time_s", "cli_flags"],
        rows
    )


# --------------------- §5.7 noodle source v[1] timeline ---------------------

def write_noodle_source_timeline():
    sm_list, ky_list, vf, rid = _load_run(NOODLE_DEFAULT_RUNS[1.0])
    samples = sm_list[0]["properties"][0]["samples"]
    rows = []
    for i, s in enumerate(samples):
        rows.append([i, round(s["t_sec"], 4), int(s["v"][1])])
    _write_csv(
        OUT_DIR / "figure_6_7_noodle_source_v1_timeline.csv",
        ["sample_index", "t_sec", "v1_vertex_count"],
        rows
    )


# --------------------- §5.7 v6 long-form blob ---------------------

def write_blob_v6_long_form():
    out_path = OUT_DIR / "table_6_7_3_blob_v6_long_form.csv"
    sm_list, ky_list, vf, rid = _load_run(BLOB_V6_RUN)
    verify_sorted = sorted(vf["properties"], key=lambda p: p["max_err"])
    rows_sorted = []
    for sm, ky in zip(sm_list, ky_list):
        cfg = sm.get("config", {})
        samples = sm["properties"][0]["samples"]
        keys = ky["property_results"][0]["keys"]
        src_v = [int(s["v"][1]) for s in samples]
        out_v = [int(k["v"][1]) for k in keys]
        transitions = sum(1 for i in range(1, len(src_v)) if src_v[i] != src_v[i - 1])
        src_floats = sum(len(s["v"]) for s in samples)
        out_floats = sum(len(k["v"]) for k in keys)
        rows_sorted.append({
            "eps": cfg.get("tolerance"),
            "samples": len(samples),
            "duration_s": round(samples[-1]["t_sec"] - samples[0]["t_sec"], 4),
            "keys": len(keys),
            "src_floats": src_floats,
            "out_floats": out_floats,
            "src_unique_v": sorted(set(src_v)),
            "out_unique_v": sorted(set(out_v)),
            "src_transitions": transitions,
            "solver_max_err": ky["property_results"][0]["max_err"],
            "solve_time_s": ky["solve_time_ms"] / 1000.0,
        })
    rows_sorted.sort(key=lambda r: r["eps"])
    # match verify rows by sorted max_err (ascending) — tightest eps maps to smallest err
    for r, vp in zip(rows_sorted, verify_sorted):
        r["cli_verify_max_err"] = vp["max_err"]
    ae = _read_ae_roundtrip_max_err(rid) or _preserve_ae_roundtrip(out_path, rid)
    csv_rows = []
    for r in rows_sorted:
        ae_val = ae if ae is not None else ""
        gap_ref = ae if ae is not None else r["cli_verify_max_err"]
        csv_rows.append([
            r["eps"], rid,
            r["samples"], r["duration_s"], r["keys"],
            round(r["samples"] / r["keys"], 4),
            r["src_floats"], r["out_floats"],
            round(r["src_floats"] / r["out_floats"], 4),
            ";".join(map(str, r["src_unique_v"])),
            ";".join(map(str, r["out_unique_v"])),
            r["src_transitions"],
            round(r["solver_max_err"], 6),
            round(ae_val, 6) if isinstance(ae_val, float) else ae_val,
            round(r["cli_verify_max_err"], 6),
            round(gap_ref / r["solver_max_err"], 4) if r["solver_max_err"] > 0 else "",
            round(r["solve_time_s"], 2),
        ])
    _write_csv(
        out_path,
        ["eps", "request_id",
         "samples", "duration_s", "keys", "temporal_compression_ratio",
         "src_floats", "out_floats", "volume_compression_ratio",
         "source_v1_unique_set", "output_v1_unique_set",
         "source_topology_transitions",
         "solver_max_err_px",
         "ae_roundtrip_max_err_px", "cli_verify_max_err_px",
         "ae_to_solver_gap_ratio",
         "solve_time_s"],
        csv_rows
    )


# --------------------- §5.7 v1 vs v6 comparison ---------------------

def write_blob_v1_vs_v6():
    out_path = OUT_DIR / "table_6_7_3_blob_v1_vs_v6.csv"
    def collect(run_dir, label):
        sm_list, ky_list, vf, rid = _load_run(run_dir)
        verify_sorted = sorted(vf["properties"], key=lambda p: p["max_err"])
        rows = []
        for sm, ky in zip(sm_list, ky_list):
            cfg = sm.get("config", {})
            samples = sm["properties"][0]["samples"]
            keys = ky["property_results"][0]["keys"]
            src_v = [int(s["v"][1]) for s in samples]
            transitions = sum(1 for i in range(1, len(src_v)) if src_v[i] != src_v[i - 1])
            src_floats = sum(len(s["v"]) for s in samples)
            out_floats = sum(len(k["v"]) for k in keys)
            rows.append({
                "label": label, "eps": cfg.get("tolerance"),
                "request_id": rid,
                "samples": len(samples), "keys": len(keys),
                "vol_ratio": src_floats / out_floats,
                "transitions": transitions,
                "solver_max_err": ky["property_results"][0]["max_err"],
                "solve_time_s": ky["solve_time_ms"] / 1000.0,
            })
        rows.sort(key=lambda r: r["eps"])
        for r, vp in zip(rows, verify_sorted):
            r["cli_verify_max_err"] = vp["max_err"]
        ae = _read_ae_roundtrip_max_err(rid) or _preserve_ae_roundtrip(out_path, rid)
        for r in rows:
            r["ae_roundtrip_max_err"] = ae
        return rows

    v1 = collect(BLOB_V1_RUN, "v1_continuous_off_curve")
    v6 = collect(BLOB_V6_RUN, "v6_subdivision_on_curve")
    csv_rows = []
    for r in v1 + v6:
        ae_val = r["ae_roundtrip_max_err"]
        gap_ref = ae_val if ae_val is not None else r["cli_verify_max_err"]
        csv_rows.append([
            r["label"], r["eps"], r["request_id"],
            r["samples"], r["keys"], round(r["vol_ratio"], 4),
            r["transitions"],
            round(r["solver_max_err"], 6),
            round(ae_val, 6) if ae_val is not None else "",
            round(r["cli_verify_max_err"], 6),
            round(gap_ref / r["solver_max_err"], 4) if r["solver_max_err"] > 0 else "",
            round(r["solve_time_s"], 2),
        ])
    _write_csv(
        out_path,
        ["expression_version", "eps", "request_id",
         "samples", "keys", "volume_compression_ratio",
         "source_topology_transitions",
         "solver_max_err_px",
         "ae_roundtrip_max_err_px", "cli_verify_max_err_px",
         "ae_to_solver_gap_ratio",
         "solve_time_s"],
        csv_rows
    )


# --------------------- §5.6 CS2 variable-topology key trajectory ---------------------

def write_cs2_trajectory():
    """Per-key v[1] trajectory and source v[1] trajectory for CS2.

    CS2 fixtures are split across two directories by timestamp; the ε=1 bake
    lives in L2_test_Path_191818/, while ε=2 and ε=3 live in
    L2_test_Path_20260514-191818/ (same source path, different solve dates).
    """
    cs2_root = WHITEPAPER.parent / "keyframe_reduction_comparison" / "results" / "case_studies" / "cs2"
    tol_files = [
        (1.0, cs2_root / "L2_test_Path_191818" / "tol_1p0.json"),
        (2.0, cs2_root / "L2_test_Path_20260514-191818" / "tol_2p0.json"),
        (3.0, cs2_root / "L2_test_Path_20260514-191818" / "tol_3p0.json"),
    ]
    headline_rows = []
    per_key_rows = []
    for eps, path in tol_files:
        if not path.exists():
            print(f"  skipping CS2 eps={eps}: {path} not found")
            continue
        d = json.load(open(path))
        bbs = d["bbsolver"]
        pkvc = bbs["per_key_vertex_counts"]
        inp = d["input"]
        vc_per_frame = inp["vertex_count_per_frame"]
        unique = sorted(set(pkvc))
        headline_rows.append([
            eps, d["input_id"],
            inp["sample_count"], bbs["key_count"],
            round(inp["data_floats"] / bbs["data_floats"], 4),
            ";".join(map(str, sorted(set(vc_per_frame)))),
            ";".join(map(str, unique)),
            inp["topology_continuity"]["n_change_points"],
            round(bbs["wall_clock_ms"] / 1000.0, 2),
            ";".join(bbs["flags"]),
        ])
        for i, v in enumerate(pkvc):
            per_key_rows.append([eps, d["input_id"], i, int(v)])

    _write_csv(
        OUT_DIR / "table_6_2_cs2_headline.csv",
        ["eps", "input_id",
         "samples", "keys", "volume_compression_ratio",
         "source_v1_unique_set", "output_v1_unique_set",
         "source_change_points",
         "solve_time_s", "cli_flags"],
        headline_rows
    )
    _write_csv(
        OUT_DIR / "per_key_6_2_cs2_v1.csv",
        ["eps", "input_id", "key_index", "v1_vertex_count"],
        per_key_rows
    )


# --------------------- Manifest ---------------------

def write_manifest():
    entries = [
        ("fbx_mocap_method_comparison.csv",
         "Tables / Figures §Cross-host validation",
         "4-way head-to-head comparison: bbsolver, Blender F-Curve Decimate, "
         "robertjoosten/maya-keyframe-reduction (Paper.js Schneider reducer), and "
         "toolchefs/keyReducer (LGPL iterative RDP). Maya-plugin baselines ported to "
         "standalone Python; see external_runners/{joosten,toolchefs}_reducer/. Fixture: "
         "FBX mocap retarget (45 ThreeD properties × 299 samples × 25 fps). Per-method "
         "per-ε headline statistics: total keys, max_err, wall_clock. WARNING: bbsolver "
         "wall_clock_ms is ~5x throttled because the orchestrator launches it via Python "
         "subprocess which inherits macOS Utility QoS class. Direct-shell bbsolver "
         "invocations saturate ~30 cores; the reported timings reflect subprocess-throttled runs.",
         "fbx_mocap_retarget_full_size/pose_sampled_blender_action/"),
        ("fbx_mocap_per_property.csv",
         "§Cross-host validation, per-property breakdown",
         "Per-property keys and max_err for all four methods (bbsolver, Blender Decimate, "
         "Joosten reducer, Toolchefs reducer) across ε ∈ {0.5, 1.0, 2.0, 3.0}. Each method × "
         "ε × property = one row (720 rows total). Used to identify which channels each "
         "baseline fails on (RightForeArm:rotation is the worst case for all three "
         "open-source baselines).",
         "fbx_mocap_retarget_full_size/pose_sampled_blender_action/"),
        ("fbx_mocap_level_playing_field.csv",
         "§Cross-host validation, level-playing-field table",
         "At each target achieved max_err threshold ∈ {0.5, 1, 2, 3}, the input ε to which "
         "each open-source baseline must be RETUNED to actually meet the target, and the "
         "key count produced. Apples-to-apples accuracy comparison: keys needed to ACHIEVE a "
         "fidelity, not keys produced at a REQUESTED ε. bbsolver wins by 13.6-82.5× at all "
         "achievable targets. DNF entries indicate the baseline has an intrinsic accuracy "
         "floor and cannot meet the target at any tested input ε.",
         "fbx_mocap_retarget_full_size/pose_sampled_blender_action/"),
        ("fbx_mocap_full_sweep.csv",
         "§Cross-host validation, full input-ε sweep",
         "Raw per-method per-input-ε measurements (keys produced, achieved max_err) for the "
         "level-playing-field analysis. Each method swept across 12 input ε values "
         "(0.005 → 5.0). Used to derive the retuned-ε table and identify accuracy floors.",
         "fbx_mocap_retarget_full_size/pose_sampled_blender_action/"),
        ("table_6_6_1_walk_cycle_pareto.csv",
         "Table §5.2 (walk-cycle Pareto)",
         "DUIK humanoid walk-cycle three-point Pareto. One row per ε in {0.05, 1.0, 3.0}.",
         "req-1779727765498, req-1779735092073, req-1779733597712"),
        ("per_property_6_6_walk_cycle.csv",
         "§5.2 per-property breakdown",
         "Per-property verify max_err for each ε on the walk-cycle fixture. 42 properties × 3 ε = 126 rows.",
         "req-1779727765498, req-1779735092073, req-1779733597712"),
        ("table_6_6_3_ant_rig_headline.csv",
         "Table §5.2 (ant rig headline)",
         "Ant hexapod rig at ε=1. Single row. Position 49 + Rotation 49 = 98 properties.",
         "req-1779759462540"),
        ("per_property_6_6_3_ant_rig.csv",
         "§5.2 (ant rig) per-property breakdown",
         "Per-property verify max_err for ant rig. 98 rows.",
         "req-1779759462540"),
        ("table_6_7_1_noodle_pareto.csv",
         "Table §5.7 (noodle Pareto)",
         "FK noodle three-point Pareto (default two-pass production mode). One row per ε.",
         "req-1779740361822, req-1779737483003, req-1779740577991"),
        ("table_6_7_2_noodle_default_vs_experimental.csv",
         "Table §5.7 (noodle default vs experimental)",
         "Same-fixture noodle comparison: default two-pass vs experimental flag set, both at ε=1.",
         "req-1779737483003 (default), req-1779741201109 (experimental)"),
        ("figure_6_7_noodle_source_v1_timeline.csv",
         "Figure noodle_source_v1_timeline.png",
         "Per-sample source v[1] trajectory for the noodle ε=1 bake. 242 rows.",
         "req-1779737483003"),
        ("table_6_7_3_blob_v6_long_form.csv",
         "Table §5.7 (blob lineage) — v6 long-form Pareto",
         "10-second blob bake on subdivision-based on-curve source. Three ε rows: 0.5 / 1.5 / 3.0.",
         "req-1779786372293"),
        ("table_6_7_3_blob_v1_vs_v6.csv",
         "Table §5.7 (blob lineage) — v1 vs v6 comparison",
         "Comparison of v1 (continuous off-curve activation) and v6 (subdivision on-curve activation) "
         "blob expressions at matched ε levels. 6 rows total.",
         "req-1779762426464 (v1), req-1779786372293 (v6)"),
        ("table_6_2_cs2_headline.csv",
         "Table §5.6",
         "CS2 variable-topology production path at three ε levels under the experimental flag set.",
         "L2_test_Path_191818 corpus capture"),
        ("per_key_6_2_cs2_v1.csv",
         "§5.6 per-key v[1] trajectory",
         "Per-key output v[1] for CS2 at each ε. Used to show output topology mirrors source trajectory.",
         "L2_test_Path_191818 corpus capture"),
    ]
    _write_csv(
        OUT_DIR / "manifest.csv",
        ["filename", "paper_reference", "description", "source_artifacts"],
        entries
    )


def main():
    write_walk_cycle_pareto()
    write_ant_rig()
    write_noodle_pareto()
    write_noodle_default_vs_experimental()
    write_noodle_source_timeline()
    write_blob_v6_long_form()
    write_blob_v1_vs_v6()
    write_cs2_trajectory()
    write_manifest()
    print(f"\nAll CSVs written to: {OUT_DIR}")


if __name__ == "__main__":
    main()
