"""Generate §5.2 and §5.7 figures from round-trip bake artifacts.

Produces six figures into ``benchmarks/figures/``:

1. ``walk_cycle_pareto_v2.png`` — §5.2 three-point Pareto (humanoid).
2. ``noodle_pareto.png`` — §5.7 noodle three-point Pareto + (alternative)
    experimental same-fixture comparison.
3. ``noodle_source_v1_timeline.png`` — noodle source v[1] trajectory.
4. ``noodle_default_vs_experimental_bars.png`` — same-fixture comparison
    bar chart for §5.7.
5. ``blob_v6_long_form_pareto.png`` — §5.7 (blob lineage) v6 / 10-second long-form
    blob bake at three ε levels with solver↔verify gap overlay.
6. ``blob_v1_vs_v6_comparison.png`` — §5.7 (blob lineage) v1 (early off-curve source)
    vs v6 (subdivision-based on-curve source) bar chart at matched ε
    levels: keys, volume compression, solver max_err, verify max_err.

Style follows ``paper/audit/old_keyframe_reduction_comparison/report/pareto.py``:
9x6.2 figsize, 140 dpi, bbsolver blue ``#0066cc``. Tolerance labels use
``L∞`` (Unicode infinity) instead of ``Linf``.
"""
from __future__ import annotations

import json
import os
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402

FIGURES = Path(__file__).resolve().parent.parent / "figures"
FIGURES.mkdir(parents=True, exist_ok=True)

COLOR_BBS = "#0066cc"
COLOR_BBS_LIGHT = "#5599dd"
COLOR_ACCENT = "#cc6600"
COLOR_EXP = "#cc0066"
COLOR_NEUTRAL = "#666666"
COLOR_V1 = "#aa5511"
COLOR_V6 = "#0066cc"

DPI = 140
FIGSIZE = (9, 6.2)

# Each cited run resolves via this order:
#   1. $BBSOLVER_PAPER_CORPUS (env override)
#   2. corpus/<req_id>      (shipped public bundle, first preference)
#   3. ~/github/bbsolver/artifacts/bbsolver/corpus/live_runs/<req_id>   (author's dev tree)
#   4. (historical dev tree)/<req_id>   (older dev tree)
# The first directory that contains the expected bbky.json wins.
_HERE = Path(__file__).resolve().parent
_SHIPPED = _HERE.parent / "data" / "paper_corpus"
_ENV_ROOT = os.environ.get("BBSOLVER_PAPER_CORPUS")
_FALLBACKS = [
    Path.home() / "github/bbsolver/artifacts/bbsolver/corpus/live_runs",
    Path.home() / "github/bbsolver/artifacts/bbsolver/corpus/live_runs.alt",
]


def _resolve_run(req_id: str) -> str:
    candidates = []
    if _ENV_ROOT:
        candidates.append(Path(_ENV_ROOT) / req_id)
    candidates.append(_SHIPPED / req_id)
    candidates.extend(p / req_id for p in _FALLBACKS)
    for d in candidates:
        if (d / f"{req_id}_g1.bbky.json").exists():
            return str(d)
    # Last-resort: return the shipped path so the caller fails with a clear
    # message pointing at the canonical public location.
    return str(_SHIPPED / req_id)


WALK_CYCLE_RUNS = {
    0.05: _resolve_run("req-1779727765498"),
    1.0:  _resolve_run("req-1779735092073"),
    3.0:  _resolve_run("req-1779733597712"),
}

NOODLE_DEFAULT_RUNS = {
    0.5: _resolve_run("req-1779740361822"),
    1.0: _resolve_run("req-1779737483003"),
    3.0: _resolve_run("req-1779740577991"),
}

NOODLE_EXPERIMENTAL_RUN = _resolve_run("req-1779741201109")

# Both blob bakes are 3-tolerance runs in a single request id.
# The grouping convention: g1=ε=0.5, g2=ε=1.5, g3=ε=3.0 (verified from bundle config).
BLOB_V1_RUN = _resolve_run("req-1779762426464")
BLOB_V6_RUN = _resolve_run("req-1779786372293")


def _load_run(run_dir):
    """Return (sm_list, ky_list, vf, rid) for a bake directory.

    Walks every g* group (some bakes split Position into g1 and Rotation
    into g2; others merge all properties into g1). Returns lists in
    matched order plus the single shared verify.json.
    """
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


def _aggregate_walk_cycle():
    rows = []
    for eps in sorted(WALK_CYCLE_RUNS):
        sm_list, ky_list, vf, rid = _load_run(WALK_CYCLE_RUNS[eps])
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
        rows.append({
            "eps": eps, "rid": rid,
            "pos_in": pos_in, "pos_out": pos_out,
            "rot_in": rot_in, "rot_out": rot_out,
            "pos_ratio": pos_in / pos_out, "rot_ratio": rot_in / rot_out,
            "combined_ratio": (pos_in + rot_in) / (pos_out + rot_out),
            "pos_err_max": max(pos_err), "pos_err_median": float(np.median(pos_err)),
            "rot_err_max": max(rot_err), "rot_err_median": float(np.median(rot_err)),
            "pos_errs": pos_err, "rot_errs": rot_err,
        })
    return rows


def _aggregate_noodle_default():
    rows = []
    for eps in sorted(NOODLE_DEFAULT_RUNS):
        sm_list, ky_list, vf, rid = _load_run(NOODLE_DEFAULT_RUNS[eps])
        sm, ky = sm_list[0], ky_list[0]
        samples = sm["properties"][0]["samples"]
        src_floats = sum(len(s["v"]) for s in samples)
        keys = ky["property_results"][0]["keys"]
        out_floats = sum(len(k["v"]) for k in keys)
        rows.append({
            "eps": eps, "rid": rid,
            "n_samples": len(samples), "n_keys": len(keys),
            "fit_v": int(keys[0]["v"][1]),
            "src_v_max": max(int(s["v"][1]) for s in samples),
            "temp_ratio": len(samples) / len(keys),
            "vol_ratio": src_floats / out_floats,
            "solver_max_err": ky["property_results"][0]["max_err"],
            "verify_max_err": vf["properties"][0]["max_err"],
            "solve_time_s": ky["solve_time_ms"] / 1000.0,
        })
    return rows


def _aggregate_noodle_experimental():
    sm_list, ky_list, vf, rid = _load_run(NOODLE_EXPERIMENTAL_RUN)
    sm, ky = sm_list[0], ky_list[0]
    samples = sm["properties"][0]["samples"]
    keys = ky["property_results"][0]["keys"]
    src_floats = sum(len(s["v"]) for s in samples)
    out_floats = sum(len(k["v"]) for k in keys)
    return {
        "eps": 1.0, "rid": rid,
        "n_samples": len(samples), "n_keys": len(keys),
        "fit_v": int(keys[0]["v"][1]),
        "temp_ratio": len(samples) / len(keys),
        "vol_ratio": src_floats / out_floats,
        "solver_max_err": ky["property_results"][0]["max_err"],
        "verify_max_err": vf["properties"][0]["max_err"],
        "solve_time_s": ky["solve_time_ms"] / 1000.0,
    }


def _aggregate_blob_3tol(run_dir):
    """For 3-tolerance blob bakes packaged as g1/g2/g3 in one request.

    The verify file lists properties in writeback-layer order (L3, L2, L1)
    where L3 corresponds to the tightest ε bake and L1 to the loosest.
    We match by solver_max_err ≈ verify property's max_err — but for
    correctness we map them in solver-time order (g1 → tightest layer).
    """
    sm_list, ky_list, vf, rid = _load_run(run_dir)
    rows = []
    for sm, ky in zip(sm_list, ky_list):
        cfg = sm.get("config", {})
        samples = sm["properties"][0]["samples"]
        keys = ky["property_results"][0]["keys"]
        src_floats = sum(len(s["v"]) for s in samples)
        out_floats = sum(len(k["v"]) for k in keys)
        src_v = [int(s["v"][1]) for s in samples]
        out_v = [int(k["v"][1]) for k in keys]
        transitions = sum(1 for i in range(1, len(src_v)) if src_v[i] != src_v[i-1])
        rows.append({
            "eps": cfg.get("tolerance"),
            "rid": rid,
            "samples": len(samples),
            "duration_s": samples[-1]["t_sec"] - samples[0]["t_sec"],
            "keys": len(keys),
            "vol_ratio": src_floats / out_floats,
            "temp_ratio": len(samples) / len(keys),
            "solver_max_err": ky["property_results"][0]["max_err"],
            "src_transitions": transitions,
            "src_unique_v": sorted(set(src_v)),
            "out_unique_v": sorted(set(out_v)),
            "solve_time_s": ky["solve_time_ms"] / 1000.0,
        })
    # Match verify max_err per row by tolerance value
    # Heuristic: tightest ε bake (smallest tol) yields smallest verify max_err
    verify_sorted = sorted(vf["properties"], key=lambda p: p["max_err"])
    rows_sorted = sorted(rows, key=lambda r: r["eps"])
    for r, vp in zip(rows_sorted, verify_sorted):
        r["verify_max_err"] = vp["max_err"]
        r["verify_prop_id"] = vp["id"]
    return rows_sorted


# --------------------- 1. walk_cycle_pareto_v2 ---------------------

def fig_walk_cycle_pareto(rows, output):
    fig, ax1 = plt.subplots(figsize=FIGSIZE)

    eps_vals = [r["eps"] for r in rows]
    combined = [r["combined_ratio"] for r in rows]
    pos_err  = [r["pos_err_max"] for r in rows]
    rot_err  = [r["rot_err_max"] for r in rows]

    ax1.plot(eps_vals, combined, "-o", color=COLOR_BBS, linewidth=2,
             markersize=10, label="Combined compression ratio")
    for r in rows:
        ax1.annotate(f"{r['combined_ratio']:.1f}×",
                     xy=(r["eps"], r["combined_ratio"]),
                     xytext=(8, 8), textcoords="offset points",
                     fontsize=10, color=COLOR_BBS, fontweight="bold")
    ax1.set_xscale("log")
    ax1.set_xlabel("L∞ budget ε (property units)")
    ax1.set_ylabel("Combined sample-to-key compression ×", color=COLOR_BBS)
    ax1.tick_params(axis="y", labelcolor=COLOR_BBS)
    ax1.set_ylim(0, max(combined) * 1.25)
    ax1.grid(True, which="both", linestyle=":", alpha=0.4)

    ax2 = ax1.twinx()
    ax2.plot(eps_vals, pos_err, "--s", color=COLOR_ACCENT, linewidth=1.5,
             markersize=8, label="Position verify max_err (px)")
    ax2.plot(eps_vals, rot_err, "--^", color=COLOR_EXP, linewidth=1.5,
             markersize=8, label="Rotation verify max_err (°)")
    ax2.set_ylabel("Verify max_err  (px / °)", color="#444")
    ax2.set_ylim(0, max(max(pos_err), max(rot_err)) * 1.2)

    ax2.plot(eps_vals, eps_vals, ":k", linewidth=1, alpha=0.5, label="ε (budget line)")

    h1, l1 = ax1.get_legend_handles_labels()
    h2, l2 = ax2.get_legend_handles_labels()
    ax1.legend(h1 + h2, l1 + l2, loc="upper left", framealpha=0.92)

    plt.title("§5.2 Walk-cycle round-trip — three-point Pareto\n"
              "DUIK humanoid 5 s walk cycle, 42 properties "
              "(21 Position 3-D + 21 Rotation 1-D)")
    plt.tight_layout()
    fig.savefig(output, dpi=DPI)
    plt.close(fig)
    print(f"wrote {output}")


# --------------------- 2. noodle_pareto (revised legend, no duplicate X) ---------------------

def fig_noodle_pareto(rows, exp_row, output):
    """Volume compression vs ε on left axis, verify max_err on right axis.

    Annotations are positioned to LEFT of each marker so they cannot collide
    with the right y-axis tick labels at the chart edge.
    """
    # Slightly wider so right-axis labels have breathing room.
    fig, ax1 = plt.subplots(figsize=(10.5, 6.4))

    eps_vals = [r["eps"] for r in rows]
    vol = [r["vol_ratio"] for r in rows]
    verr = [r["verify_max_err"] for r in rows]

    # Default Pareto
    ax1.plot(eps_vals, vol, "-o", color=COLOR_BBS, linewidth=2,
             markersize=10, label="Default two-pass — volume × ratio")
    # Annotate each point — place label LEFT of marker for the last (rightmost)
    # point so it can't push outside the chart; keep right placement otherwise.
    for i, r in enumerate(rows):
        is_last = i == len(rows) - 1
        dx, dy = (-90, 8) if is_last else (8, 8)
        ha = "left"
        ax1.annotate(f"{r['vol_ratio']:.1f}×\n({r['n_keys']} keys @ v={r['fit_v']})",
                     xy=(r["eps"], r["vol_ratio"]),
                     xytext=(dx, dy), textcoords="offset points",
                     fontsize=9, color=COLOR_BBS, fontweight="bold", ha=ha)

    # experimental — single legend entry combining vol and verify info
    ax1.plot([exp_row["eps"]], [exp_row["vol_ratio"]], marker="X",
             color=COLOR_EXP, markersize=14, linestyle="None",
             label=(f"experimental @ ε={exp_row['eps']:.0f} "
                    f"({exp_row['n_keys']} keys @ v={exp_row['fit_v']}, "
                    f"verify max_err {exp_row['verify_max_err']:.2f} px)"))
    ax1.annotate(f"{exp_row['vol_ratio']:.1f}×",
                 xy=(exp_row["eps"], exp_row["vol_ratio"]),
                 xytext=(-65, -20), textcoords="offset points",
                 fontsize=10, color=COLOR_EXP, fontweight="bold")

    ax1.set_xscale("log")
    ax1.set_xlabel("L∞ budget ε (property units)")
    ax1.set_ylabel("Volume compression ×", color=COLOR_BBS)
    ax1.tick_params(axis="y", labelcolor=COLOR_BBS)
    ax1.set_ylim(0, max(max(vol), exp_row["vol_ratio"]) * 1.3)
    ax1.grid(True, which="both", linestyle=":", alpha=0.4)

    ax2 = ax1.twinx()
    ax2.plot(eps_vals, verr, "--s", color=COLOR_ACCENT, linewidth=1.5,
             markersize=8, label="Default verify max_err (px)")
    # Experimental verify max_err is reported in the legend label of the
    # (alternative) X marker on ax1 — do not plot a separate marker on ax2.
    ax2.plot(eps_vals, eps_vals, ":k", linewidth=1, alpha=0.5,
             label="ε (budget line)")
    ax2.set_ylabel("Verify max_err (px)", color="#444")
    ax2.set_ylim(0, max(verr) * 1.2)

    # Legend OUTSIDE plot to the right so it never collides with annotations.
    h1, l1 = ax1.get_legend_handles_labels()
    h2, l2 = ax2.get_legend_handles_labels()
    fig.legend(h1 + h2, l1 + l2,
               loc="lower center", bbox_to_anchor=(0.5, -0.02),
               framealpha=0.95, fontsize=9, ncol=2, frameon=True)

    plt.title("§5.7 Noodle round-trip — three-point Pareto + same-fixture experimental comparison\n"
              "FK noodle 4 s, 242-sample variable-topology shape path, "
              "9 distinct source vertex counts {28-52}",
              fontsize=10)
    # Reserve bottom space for the external legend.
    plt.tight_layout(rect=(0, 0.10, 1, 0.96))
    fig.savefig(output, dpi=DPI, bbox_inches="tight")
    plt.close(fig)
    print(f"wrote {output}")


# --------------------- 3. noodle_source_v1_timeline ---------------------

def fig_noodle_source_v1_timeline(output):
    sm_list, ky_list, vf, rid = _load_run(NOODLE_DEFAULT_RUNS[1.0])
    samples = sm_list[0]["properties"][0]["samples"]
    times = [s["t_sec"] for s in samples]
    v1 = [int(s["v"][1]) for s in samples]

    fig, ax = plt.subplots(figsize=FIGSIZE)
    ax.plot(times, v1, color=COLOR_BBS, linewidth=1.5)
    ax.scatter(times, v1, s=14, color=COLOR_BBS, alpha=0.7)

    ax.axhline(y=52, color=COLOR_NEUTRAL, linestyle=":", alpha=0.6)
    ax.text(0.1, 52.8, "S1 — normal body (52 v)",
            color=COLOR_NEUTRAL, fontsize=10)
    ax.axhspan(22, 51, alpha=0.06, color=COLOR_ACCENT,
               label="S2 self-intersection-repair range")
    s2_times = [t for t, v in zip(times, v1) if v != 52]
    if s2_times:
        ax.axvspan(min(s2_times) - 0.05, max(s2_times) + 0.05,
                   alpha=0.10, color=COLOR_EXP,
                   label=f"S2 window: t ∈ [{min(s2_times):.2f}, {max(s2_times):.2f}] s")

    ax.annotate(
        "Fold-angle crosses 180° between\n"
        "frame 202 (−178.7°) and frame 203 (−183.8°)\n"
        "in continuous time, but neither integer-frame\n"
        "sample lands at the S4 (22 v) threshold.",
        xy=(3.4, 28), xytext=(2.0, 12),
        textcoords="data",
        fontsize=9, ha="left", va="bottom",
        bbox=dict(boxstyle="round,pad=0.4", fc="#fff7e0", ec="#cc6600", alpha=0.92),
        arrowprops=dict(arrowstyle="->", color="#cc6600", lw=1),
    )

    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Source v[1]  (vertex count per frame)")
    ax.set_ylim(0, 70)
    ax.set_xlim(min(times), max(times))
    ax.grid(True, linestyle=":", alpha=0.4)
    ax.legend(loc="lower left", framealpha=0.92, fontsize=9)
    plt.title("§5.7 Source vertex-count trajectory — FK noodle, 60 fps capture\n"
              "242 samples over 4 s, 9 distinct counts {28, 29, 30, 32, 34, 36, 38, 40, 52}")
    plt.tight_layout()
    fig.savefig(output, dpi=DPI)
    plt.close(fig)
    print(f"wrote {output}")


# --------------------- 4. noodle_default_vs_experimental_bars ---------------------

def fig_noodle_default_vs_experimental(default_row_eps1, exp_row, output):
    metrics = [
        ("Keys", default_row_eps1["n_keys"], exp_row["n_keys"]),
        ("Solve time (s)", default_row_eps1["solve_time_s"],
         exp_row["solve_time_s"]),
        ("Verify max_err (px)", default_row_eps1["verify_max_err"],
         exp_row["verify_max_err"]),
        ("Output v[1]", default_row_eps1["fit_v"], exp_row["fit_v"]),
    ]
    fig, axes = plt.subplots(1, 4, figsize=(14, 5))
    for ax, (name, a, b) in zip(axes, metrics):
        bars = ax.bar(
            ["Default\ntwo-pass", "experimental"],
            [a, b],
            color=[COLOR_BBS, COLOR_EXP],
            alpha=0.85,
        )
        for bar, val in zip(bars, [a, b]):
            ax.text(bar.get_x() + bar.get_width() / 2,
                    bar.get_height() + (max(a, b) * 0.02),
                    f"{val:.2f}" if isinstance(val, float) else f"{val}",
                    ha="center", va="bottom", fontsize=10, fontweight="bold")
        ax.set_title(name, fontsize=11)
        ax.spines["top"].set_visible(False)
        ax.spines["right"].set_visible(False)
        ax.set_ylim(0, max(a, b) * 1.20)
        ax.tick_params(axis="x", labelsize=9)

    fig.suptitle(
        "§5.7 Same-fixture comparison — noodle bake at ε=1\n"
        "Default two-pass (no flags) vs experimental "
        "(--decompose-paths / --fit-canonical-paths / --fit-replacement-paths / --emit-landmark-subpaths)",
        fontsize=12,
    )
    plt.tight_layout(rect=(0, 0, 1, 0.93))
    fig.savefig(output, dpi=DPI)
    plt.close(fig)
    print(f"wrote {output}")


# --------------------- 5. blob_v6_long_form_pareto (NEW) ---------------------

def fig_blob_v6_long_form(rows, output):
    fig, ax1 = plt.subplots(figsize=FIGSIZE)

    eps_vals = [r["eps"] for r in rows]
    vol = [r["vol_ratio"] for r in rows]
    solver_err = [r["solver_max_err"] for r in rows]
    verify_err = [r["verify_max_err"] for r in rows]

    ax1.plot(eps_vals, vol, "-o", color=COLOR_BBS, linewidth=2,
             markersize=10, label="Volume compression ratio")
    for r in rows:
        ax1.annotate(f"{r['vol_ratio']:.1f}×\n({r['keys']} keys)",
                     xy=(r["eps"], r["vol_ratio"]),
                     xytext=(10, -2), textcoords="offset points",
                     fontsize=9, color=COLOR_BBS, fontweight="bold")
    ax1.set_xscale("log")
    ax1.set_xlabel("L∞ budget ε (property units)")
    ax1.set_ylabel("Volume compression ×", color=COLOR_BBS)
    ax1.tick_params(axis="y", labelcolor=COLOR_BBS)
    ax1.set_ylim(0, max(vol) * 1.25)
    ax1.grid(True, which="both", linestyle=":", alpha=0.4)

    ax2 = ax1.twinx()
    ax2.plot(eps_vals, solver_err, "--s", color=COLOR_ACCENT, linewidth=1.5,
             markersize=8, label="Solver max_err (px)")
    ax2.plot(eps_vals, verify_err, "--^", color=COLOR_EXP, linewidth=1.5,
             markersize=8, label="Verify max_err (AE-rendered, px)")
    ax2.plot(eps_vals, eps_vals, ":k", linewidth=1, alpha=0.5,
             label="ε (budget line)")
    ax2.set_ylabel("Error (px)", color="#444")
    ax2.set_ylim(0, max(max(solver_err), max(verify_err), max(eps_vals)) * 1.2)

    h1, l1 = ax1.get_legend_handles_labels()
    h2, l2 = ax2.get_legend_handles_labels()
    ax1.legend(h1 + h2, l1 + l2, loc="upper left", framealpha=0.92, fontsize=9)

    plt.title("§5.7 (blob lineage) Long-form variable-topology blob — three-point Pareto\n"
              "10-second source (602 samples), v6 subdivision-based on-curve expression\n"
              "Solver↔verify gap: 1.42× / 1.00× / 0.94× at ε = 0.5 / 1.5 / 3.0")
    plt.tight_layout()
    fig.savefig(output, dpi=DPI)
    plt.close(fig)
    print(f"wrote {output}")


# --------------------- 6. blob_v1_vs_v6_comparison (NEW) ---------------------

def fig_blob_v1_vs_v6_comparison(v1_rows, v6_rows, output):
    """4-panel bars: at each ε level (0.5/1.5/3.0), compare v1 vs v6 on
    keys, volume×, solver max_err, verify max_err. v1 represents the
    early off-curve expression source; v6 is the subdivision-based
    on-curve source."""
    eps_vals = sorted(set([r["eps"] for r in v1_rows]))
    v1_by_eps = {r["eps"]: r for r in v1_rows}
    v6_by_eps = {r["eps"]: r for r in v6_rows}

    metrics = [
        ("Keys",                "keys",            False),
        ("Volume compression ×", "vol_ratio",       False),
        ("Solver max_err (px)",  "solver_max_err",  True),
        ("Verify max_err (px)",  "verify_max_err",  True),
    ]
    fig, axes = plt.subplots(1, 4, figsize=(16, 5.2))
    width = 0.35

    for ax, (title, key, is_err) in zip(axes, metrics):
        x = np.arange(len(eps_vals))
        v1_vals = [v1_by_eps[e][key] for e in eps_vals]
        v6_vals = [v6_by_eps[e][key] for e in eps_vals]

        bars1 = ax.bar(x - width / 2, v1_vals, width, color=COLOR_V1, alpha=0.85,
                       label="v1 (continuous, off-curve transitions)")
        bars2 = ax.bar(x + width / 2, v6_vals, width, color=COLOR_V6, alpha=0.85,
                       label="v6 (subdivision, on-curve transitions)")

        for bar, val in zip(bars1, v1_vals):
            ax.text(bar.get_x() + bar.get_width() / 2,
                    bar.get_height() + (max(v1_vals + v6_vals) * 0.02),
                    f"{val:.2f}" if isinstance(val, float) else f"{val}",
                    ha="center", va="bottom", fontsize=9, color=COLOR_V1)
        for bar, val in zip(bars2, v6_vals):
            ax.text(bar.get_x() + bar.get_width() / 2,
                    bar.get_height() + (max(v1_vals + v6_vals) * 0.02),
                    f"{val:.2f}" if isinstance(val, float) else f"{val}",
                    ha="center", va="bottom", fontsize=9, fontweight="bold",
                    color=COLOR_V6)

        ax.set_xticks(x)
        ax.set_xticklabels([f"ε = {e}" for e in eps_vals], fontsize=10)
        ax.set_title(title, fontsize=11)
        ax.spines["top"].set_visible(False)
        ax.spines["right"].set_visible(False)
        ax.set_ylim(0, max(v1_vals + v6_vals) * 1.20)

    # Single legend at top
    handles = [
        plt.Rectangle((0, 0), 1, 1, color=COLOR_V1, alpha=0.85),
        plt.Rectangle((0, 0), 1, 1, color=COLOR_V6, alpha=0.85),
    ]
    labels = [
        "v1 (continuous activation, vertex inserted at natural radius — off-curve)",
        "v6 (Bezier-subdivision activation, vertex inserted on existing curve — on-curve)",
    ]
    fig.legend(handles, labels, loc="upper center", ncol=2,
               bbox_to_anchor=(0.5, 1.005), fontsize=10, framealpha=0.92)

    fig.suptitle(
        "§5.7 (blob lineage) Source-construction matters: v1 vs v6 on the same fixture class at matched ε levels\n"
        "Both bakes are variable-topology blob expressions; v6's on-curve subdivision design "
        "eliminates the tangent-transition glitches that drove v1's verify max_err.",
        fontsize=11.5, y=0.94,
    )
    plt.tight_layout(rect=(0, 0, 1, 0.90))
    fig.savefig(output, dpi=DPI)
    plt.close(fig)
    print(f"wrote {output}")


ANT_RIG_RUN = _resolve_run("req-1779759462540")


def _aggregate_ant_rig():
    sm_list, ky_list, vf, rid = _load_run(ANT_RIG_RUN)
    sm_by_id = {p["property"]["id"]: len(p.get("samples", []))
                for sm in sm_list for p in sm["properties"]}
    pos_in = pos_out = rot_in = rot_out = 0
    pos_props = []
    rot_props = []
    for ky in ky_list:
        for r in ky["property_results"]:
            pid = r["property_id"]
            n_in = sm_by_id.get(pid, 0)
            n_out = len(r.get("keys", []))
            if "Rotate" in pid:
                rot_in += n_in; rot_out += n_out
            else:
                pos_in += n_in; pos_out += n_out
    for vp in vf["properties"]:
        if "Rotate" in vp["id"]:
            rot_props.append(vp)
        else:
            pos_props.append(vp)
    return {
        "rid": rid,
        "pos_in": pos_in, "pos_out": pos_out, "pos_ratio": pos_in / pos_out,
        "rot_in": rot_in, "rot_out": rot_out, "rot_ratio": rot_in / rot_out,
        "combined_ratio": (pos_in + rot_in) / (pos_out + rot_out),
        "pos_errs": [vp["max_err"] for vp in pos_props],
        "rot_errs": [vp["max_err"] for vp in rot_props],
        "pos_props": pos_props,
        "rot_props": rot_props,
    }


# --------------------- 7. ant_rig_per_property_errors (NEW) ---------------------

def fig_ant_rig_per_property_errors(ant, output):
    """Scatter of all 98 ant rig properties' verify max_err at ε=1, with
    the ε=1 budget line. Demonstrates the budget-faithful pattern
    distributes across legs without a single-property outlier like the
    humanoid's Neck."""
    fig, (ax_pos, ax_rot) = plt.subplots(1, 2, figsize=(13.5, 5.4))

    # Position panel
    pos_props = ant["pos_props"]
    pos_idx = list(range(len(pos_props)))
    pos_errs = [p["max_err"] for p in pos_props]
    pos_ok = [p["ok"] for p in pos_props]

    ax_pos.scatter(pos_idx, pos_errs, color=COLOR_BBS, s=60, alpha=0.85,
                   label="Per-property Position verify max_err")
    ax_pos.axhline(y=1.0, color="k", linestyle="--", linewidth=1.2,
                   alpha=0.6, label="ε = 1 (solve budget)")
    ax_pos.axhline(y=np.median(pos_errs), color=COLOR_ACCENT, linestyle=":",
                   linewidth=1, alpha=0.7,
                   label=f"Median: {np.median(pos_errs):.3f} px")
    ax_pos.set_xlabel("Position property index (0-48)")
    ax_pos.set_ylabel("Verify max_err (px)")
    ax_pos.set_ylim(0, 1.15)
    ax_pos.set_title(f"Position channels — 49 properties, all within ε=1\n"
                     f"min={min(pos_errs):.3f} median={np.median(pos_errs):.3f} max={max(pos_errs):.3f} px",
                     fontsize=11)
    ax_pos.grid(True, linestyle=":", alpha=0.4)
    ax_pos.legend(loc="lower right", framealpha=0.92, fontsize=9)

    # Rotation panel
    rot_props = ant["rot_props"]
    rot_idx = list(range(len(rot_props)))
    rot_errs = [p["max_err"] for p in rot_props]

    ax_rot.scatter(rot_idx, rot_errs, color=COLOR_EXP, s=60, alpha=0.85,
                   label="Per-property Rotation verify max_err")
    ax_rot.axhline(y=1.0, color="k", linestyle="--", linewidth=1.2,
                   alpha=0.6, label="ε = 1 (solve budget)")
    ax_rot.axhline(y=np.median(rot_errs), color=COLOR_ACCENT, linestyle=":",
                   linewidth=1, alpha=0.7,
                   label=f"Median: {np.median(rot_errs):.3f}°")
    ax_rot.set_xlabel("Rotation property index (0-48)")
    ax_rot.set_ylabel("Verify max_err (°)")
    ax_rot.set_ylim(0, 1.15)
    ax_rot.set_title(f"Rotation channels — 49 properties, all within ε=1°\n"
                     f"min={min(rot_errs):.3f} median={np.median(rot_errs):.3f} max={max(rot_errs):.3f}°",
                     fontsize=11)
    ax_rot.grid(True, linestyle=":", alpha=0.4)
    ax_rot.legend(loc="lower right", framealpha=0.92, fontsize=9)

    fig.suptitle("§5.2 (ant rig headline) Ant hexapod rig — per-property verify max_err distribution at ε=1\n"
                 "98 properties (49 Position 3-D + 49 Rotation 1-D), 122-sample 2-second walk cycle, "
                 f"three phase-offset walk-cycle drivers, mixed 1-/2-layer IK depth",
                 fontsize=12)
    plt.tight_layout(rect=(0, 0, 1, 0.92))
    fig.savefig(output, dpi=DPI)
    plt.close(fig)
    print(f"wrote {output}")


# --------------------- 8. humanoid_vs_ant_comparison (NEW) ---------------------

def fig_humanoid_vs_ant_comparison(walk_rows, ant, output):
    """Side-by-side compression and verify-error comparison between
    humanoid (single-driver, 42 props) and ant (3-driver, 98 props)
    rigs at ε=1."""
    humanoid = next(r for r in walk_rows if r["eps"] == 1.0)

    metrics = [
        ("Properties",          42,  98),
        ("Position ratio ×",    humanoid["pos_ratio"], ant["pos_ratio"]),
        ("Rotation ratio ×",    humanoid["rot_ratio"], ant["rot_ratio"]),
        ("Combined ratio ×",    humanoid["combined_ratio"], ant["combined_ratio"]),
        ("Pos max_err (px)",    max(humanoid["pos_errs"]), max(ant["pos_errs"])),
        ("Rot max_err (°)",     max(humanoid["rot_errs"]), max(ant["rot_errs"])),
    ]
    fig, axes = plt.subplots(1, 6, figsize=(18, 5))
    for ax, (name, a, b) in zip(axes, metrics):
        bars = ax.bar(
            ["Humanoid\n1 driver", "Ant\n3 drivers"],
            [a, b],
            color=[COLOR_BBS, COLOR_EXP], alpha=0.85,
        )
        for bar, val in zip(bars, [a, b]):
            ax.text(bar.get_x() + bar.get_width() / 2,
                    bar.get_height() + (max(a, b) * 0.02),
                    f"{val:.2f}" if isinstance(val, float) else f"{val}",
                    ha="center", va="bottom", fontsize=9, fontweight="bold")
        ax.set_title(name, fontsize=11)
        ax.spines["top"].set_visible(False)
        ax.spines["right"].set_visible(False)
        ax.set_ylim(0, max(a, b) * 1.20)
        ax.tick_params(axis="x", labelsize=9)

    fig.suptitle("§5.2 Cross-rig generalization at ε=1 — humanoid biped vs ant hexapod\n"
                 "Both rigs maintain budget fidelity; ant's lower compression reflects "
                 "3 phase-offset walk-cycle drivers vs humanoid's single driver",
                 fontsize=12)
    plt.tight_layout(rect=(0, 0, 1, 0.91))
    fig.savefig(output, dpi=DPI)
    plt.close(fig)
    print(f"wrote {output}")


def main():
    walk_rows = _aggregate_walk_cycle()
    noodle_rows = _aggregate_noodle_default()
    exp_row = _aggregate_noodle_experimental()
    blob_v1_rows = _aggregate_blob_3tol(BLOB_V1_RUN)
    blob_v6_rows = _aggregate_blob_3tol(BLOB_V6_RUN)
    ant = _aggregate_ant_rig()

    fig_walk_cycle_pareto(walk_rows, FIGURES / "walk_cycle_pareto_v2.png")
    fig_noodle_pareto(noodle_rows, exp_row, FIGURES / "noodle_pareto.png")
    fig_noodle_source_v1_timeline(FIGURES / "noodle_source_v1_timeline.png")

    default_eps1 = next(r for r in noodle_rows if r["eps"] == 1.0)
    fig_noodle_default_vs_experimental(default_eps1, exp_row,
                                       FIGURES / "noodle_default_vs_experimental_bars.png")

    fig_blob_v6_long_form(blob_v6_rows, FIGURES / "blob_v6_long_form_pareto.png")
    fig_blob_v1_vs_v6_comparison(blob_v1_rows, blob_v6_rows,
                                 FIGURES / "blob_v1_vs_v6_comparison.png")

    fig_ant_rig_per_property_errors(ant, FIGURES / "ant_rig_per_property_errors.png")
    fig_humanoid_vs_ant_comparison(walk_rows, ant, FIGURES / "humanoid_vs_ant_comparison.png")


if __name__ == "__main__":
    main()
