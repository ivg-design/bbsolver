"""Generate figures comparing bbsolver vs Blender F-Curve Decimate on the
FBX mocap fixture. Reads:
    supplementary/fbx_mocap_method_comparison.csv  (headline per (method, eps))
    supplementary/fbx_mocap_per_property.csv       (per-property breakdown)

Produces:
    figures/fbx_mocap_pareto.png         keys-vs-eps Pareto with max_err overlay
    figures/fbx_mocap_accuracy_bars.png  max_err per (method, eps) bar chart
                                          showing budget compliance
"""
from __future__ import annotations
import csv
from pathlib import Path
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

HERE = Path(__file__).resolve().parent
DATA_DIR = HERE.parent / "data" / "supplementary"
FIGURES = HERE.parent / "figures"

COLOR_BBS = "#0066cc"
COLOR_BLENDER = "#cc6600"
COLOR_JOOSTEN = "#9966cc"
COLOR_TOOLCHEFS = "#009966"
COLOR_BUDGET = "#666666"
DPI = 140


def load_headline():
    rows = list(csv.DictReader(open(DATA_DIR / "fbx_mocap_method_comparison.csv")))
    for r in rows:
        r["eps"] = float(r["eps"])
        r["total_keys"] = int(r["total_keys"])
        r["key_compression_ratio"] = float(r["key_compression_ratio"])
        r["max_err_median"] = float(r["max_err_median"])
        r["max_err_max"] = float(r["max_err_max"])
        r["wall_clock_ms"] = float(r["wall_clock_ms"])
    return rows


def load_per_property():
    rows = list(csv.DictReader(open(DATA_DIR / "fbx_mocap_per_property.csv")))
    for r in rows:
        r["eps"] = float(r["eps"])
        r["keys"] = int(r["keys"])
        r["max_err"] = float(r["max_err"])
    return rows


# --------------------- Figure 1: Pareto + max_err overlay ---------------------

def fig_pareto():
    """Two-row stacked panels:
        top:    keys vs ε (4 methods)
        bottom: max_err vs ε (4 methods + budget line)

    Stacking avoids dual-y-axis label collision and gives each curve room.
    Single legend lives outside on the right so it can never overlap data.
    """
    rows = load_headline()
    bbs = [r for r in rows if r["method"] == "bbsolver"]
    bld = [r for r in rows if r["method"] == "blender_decimate"]
    jos = [r for r in rows if r["method"] == "joosten_reducer"]
    tc  = [r for r in rows if r["method"] == "toolchefs_reducer"]

    series = [
        ("bbsolver",          bbs, "o", COLOR_BBS,       2.0, 10),
        ("Blender F-Curve Decimate", bld, "s", COLOR_BLENDER,  2.0, 10),
        ("Joosten Paper.js reducer", jos, "D", COLOR_JOOSTEN,  2.0,  9),
        ("Toolchefs keyReducer",     tc,  "P", COLOR_TOOLCHEFS, 2.0, 10),
    ]

    # Figure proportions tuned so the bottom-panel legend has its own row and
    # never overlaps data. height_ratios privileges the two data panels; the
    # third "panel" is just the legend strip via fig.legend below.
    fig, (ax_keys, ax_err) = plt.subplots(
        2, 1, figsize=(12, 8.5), sharex=True,
        gridspec_kw={"height_ratios": [1, 1], "hspace": 0.14},
    )

    # --- top panel: keys ---
    for name, rows_m, marker, color, lw, ms in series:
        eps_v = [r["eps"] for r in rows_m]
        keys_v = [r["total_keys"] for r in rows_m]
        ax_keys.plot(eps_v, keys_v, "-", marker=marker, color=color,
                     linewidth=lw, markersize=ms, label=name)
    ax_keys.set_xscale("log")
    ax_keys.set_ylabel("Total keys\n(across 45 mocap properties)")
    ax_keys.grid(True, which="both", linestyle=":", alpha=0.4)
    ax_keys.spines["top"].set_visible(False)
    ax_keys.spines["right"].set_visible(False)

    # --- bottom panel: max_err with budget reference line ---
    all_eps = sorted({r["eps"] for r in rows})
    # Shade the within-budget region FIRST so it sits behind the budget line +
    # data curves. Lighter alpha than before but with a hatch pattern so the
    # region reads clearly on both screen and print.
    ax_err.fill_between(all_eps, 1e-3, all_eps, color="#cfe8cf", alpha=0.55,
                        hatch="///", edgecolor="#2a8f2a", linewidth=0.0,
                        zorder=0, label="within-budget region (max_err ≤ ε)")
    # The budget line is the narrative anchor of this figure: bbsolver tracks
    # it, baselines exceed it. Render it as a thick crimson dashed line that
    # contrasts with the four data colors and the green within-budget shading.
    ax_err.plot(all_eps, all_eps, "--", color="#c2185b", linewidth=3.0,
                dashes=(7, 4), alpha=1.0, zorder=2,
                label="ε (requested budget — accept iff max_err ≤ ε)")
    for name, rows_m, marker, color, lw, ms in series:
        eps_v = [r["eps"] for r in rows_m]
        err_v = [r["max_err_max"] for r in rows_m]
        ax_err.plot(eps_v, err_v, "-", marker=marker, color=color,
                    linewidth=lw, markersize=ms, zorder=3,
                    label=f"{name} — max_err")
    # In-figure annotation pointing at the budget line so it's unmissable
    # even at thumbnail size.
    ann_x = all_eps[len(all_eps) // 2]
    ax_err.annotate(
        "ε budget — bbsolver stays ≤, baselines exceed",
        xy=(ann_x, ann_x), xycoords="data",
        xytext=(0.55, 0.92), textcoords="axes fraction",
        fontsize=10, color="#c2185b", ha="left", va="top",
        arrowprops=dict(arrowstyle="->", color="#c2185b", lw=1.4),
    )
    ax_err.set_xscale("log")
    ax_err.set_yscale("log")
    ax_err.set_xlabel("L∞ budget ε (degrees / units)")
    ax_err.set_ylabel("max_err on worst property\n(degrees / units, log)")
    ax_err.grid(True, which="both", linestyle=":", alpha=0.4)
    ax_err.spines["top"].set_visible(False)
    ax_err.spines["right"].set_visible(False)

    # Combined legend BELOW the plot (multi-column) so it never overlaps data.
    h, l = ax_keys.get_legend_handles_labels()
    h_err, l_err = ax_err.get_legend_handles_labels()
    # Carry the err-panel-only entries (budget line + within-budget region +
    # per-method err entries) into the combined legend. Drop the redundant
    # "— max_err" suffix on per-method entries because the top-panel entries
    # already name each method without duplication.
    for hh, ll in zip(h_err, l_err):
        if ll.startswith("_"):
            continue
        if ll in {l_existing for l_existing in l}:
            continue
        if " — max_err" in ll:
            continue
        h.append(hh)
        l.append(ll)
    fig.legend(h, l,
               loc="lower center", bbox_to_anchor=(0.5, -0.02),
               ncol=3, framealpha=0.95, fontsize=10,
               columnspacing=2.0, handlelength=2.5,
               title=None)

    fig.suptitle(
        "FBX mocap 4-way head-to-head: keys (top) and worst-property max_err (bottom) at matched ε\n"
        "45 ThreeD properties × 299 samples × 25 fps. bbsolver curve stays under the ε budget line; "
        "the three baselines exceed it by 4.8-91× on their worst-affected channel.",
        fontsize=11, y=0.98,
    )
    plt.tight_layout(rect=(0, 0.10, 1.0, 0.95))
    out = FIGURES / "fbx_mocap_pareto.png"
    fig.savefig(out, dpi=DPI, bbox_inches="tight")
    plt.close(fig)
    print(f"wrote {out}")


# --------------------- Figure 2: max_err per (method, eps) bar chart ---------------------

def fig_accuracy_bars():
    """Bar chart of max_err per (method, ε) with the requested-ε reference line.

    Layout fixes applied:
      - Log-scale y so bbsolver's 0.49 and Joosten's 51.9 share the same panel
        legibly (no more bar-label collisions in the linear-scale upper band).
      - Compact bar labels: just the numeric max_err. The "Nx over budget"
        framing is in the title, not duplicated above every bar.
      - Legend inside the chart's upper-right empty corner (no external area
        means no title clipping from a constrained tight_layout rect).
      - `bbox_inches="tight"` on save handles final margins cleanly.
    """
    rows = load_headline()
    eps_levels = sorted(set(r["eps"] for r in rows))
    by_method_eps = {(r["method"], r["eps"]): r for r in rows}

    fig, ax = plt.subplots(figsize=(13, 7))
    x = np.arange(len(eps_levels))
    width = 0.20

    bbs_max = [by_method_eps[("bbsolver", e)]["max_err_max"] for e in eps_levels]
    bld_max = [by_method_eps[("blender_decimate", e)]["max_err_max"] for e in eps_levels]
    jos_max = [by_method_eps[("joosten_reducer", e)]["max_err_max"] for e in eps_levels]
    tc_max = [by_method_eps[("toolchefs_reducer", e)]["max_err_max"] for e in eps_levels]

    bars_bbs = ax.bar(x - 1.5 * width, bbs_max, width, color=COLOR_BBS, alpha=0.85,
                      label="bbsolver")
    bars_bld = ax.bar(x - 0.5 * width, bld_max, width, color=COLOR_BLENDER, alpha=0.85,
                     label="Blender F-Curve Decimate")
    bars_jos = ax.bar(x + 0.5 * width, jos_max, width, color=COLOR_JOOSTEN, alpha=0.85,
                     label="Joosten / Paper.js Bezier-fit")
    bars_tc = ax.bar(x + 1.5 * width, tc_max, width, color=COLOR_TOOLCHEFS, alpha=0.85,
                    label="Toolchefs keyReducer (RDP)")

    # Requested-ε reference line at each bin, single legend entry.
    for i, e in enumerate(eps_levels):
        ax.plot([i - 2 * width, i + 2 * width], [e, e], color="k", linewidth=2.0,
                alpha=0.7,
                label="ε (requested budget)" if i == 0 else None)

    def annotate(bars, vals, color, fontsize=9):
        # Compact single-line label, padded above the bar by a fixed-pixel
        # offset so labels don't overflow into neighbours horizontally on
        # log scale.
        for bar, val in zip(bars, vals):
            ax.annotate(
                f"{val:.2f}" if val < 10 else f"{val:.1f}",
                xy=(bar.get_x() + bar.get_width() / 2, bar.get_height()),
                xytext=(0, 4), textcoords="offset points",
                ha="center", va="bottom",
                fontsize=fontsize, color=color, fontweight="bold",
            )

    annotate(bars_bbs, bbs_max, COLOR_BBS)
    annotate(bars_bld, bld_max, COLOR_BLENDER)
    annotate(bars_jos, jos_max, COLOR_JOOSTEN)
    annotate(bars_tc, tc_max, COLOR_TOOLCHEFS)

    ax.set_xticks(x)
    ax.set_xticklabels([f"ε = {e}" for e in eps_levels], fontsize=11)
    ax.set_ylabel("Max verify error on worst property (degrees / units, log scale)")
    ax.set_yscale("log")
    ax.set_ylim(0.3, max(jos_max) * 1.8)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    # Legend INSIDE plot at upper-right (free area above all bars on log scale).
    ax.legend(loc="upper right", framealpha=0.95, fontsize=9, frameon=True)
    ax.grid(True, axis="y", which="both", linestyle=":", alpha=0.4)

    ax.set_title(
        "FBX mocap 4-way accuracy comparison: bbsolver vs three open-source per-curve simplifiers\n"
        "bbsolver enforces the L∞ budget via explicit validation; "
        "the three open-source baselines exceed ε by 4.8-91× on their worst-affected channel.",
        fontsize=11,
    )
    out = FIGURES / "fbx_mocap_accuracy_bars.png"
    fig.savefig(out, dpi=DPI, bbox_inches="tight")
    plt.close(fig)
    print(f"wrote {out}")


def main():
    fig_pareto()
    fig_accuracy_bars()


if __name__ == "__main__":
    main()
