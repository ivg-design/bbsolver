"""Generate comparison figures for the SVG vertex-decimation head-to-head
(bbsolver `vertex_only` vs Adobe Illustrator `Object > Path > Simplify` at
100% setting).

Reads supplementary/svg_decimation_combined.csv and produces:

    figures/svg_decimation_pareto_grid.png
        2×4 grid: one panel per fixture. Each panel plots
        bbsolver's keys-vs-error Pareto curve and overlays Illustrator's
        single (anchors, max_err) point. Shows clearly that bbsolver's
        curve lies below Illustrator's point on most fixtures.

    figures/svg_decimation_summary.png
        Single panel: all 8 fixtures' (anchor-count, max_err) plotted as
        connected lines for bbsolver and single markers for Illustrator.
        Annotated with fixture names.
"""
from __future__ import annotations
import csv
from collections import defaultdict
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

HERE = Path(__file__).resolve().parent
DATA_CSV = HERE.parent / "data" / "supplementary" / "svg_decimation_combined.csv"
FIGURES = HERE.parent / "figures"

COLOR_BBS = "#0066cc"
COLOR_ILLUSTRATOR = "#cc6600"
DPI = 140


def load_rows():
    rows = list(csv.DictReader(open(DATA_CSV)))
    for r in rows:
        r["n_anchors"] = int(r["n_anchors"])
        r["bezier_max_err_px"] = float(r["bezier_max_err_px"])
        r["n_original_vertices"] = int(r["n_original_vertices"])
    return rows


def group_by_fixture(rows):
    by = defaultdict(lambda: {"bbsolver": [], "illustrator": []})
    for r in rows:
        method = r["method"]
        by[r["svg"]][method].append(r)
    for fixture_data in by.values():
        # Sort bbsolver by ε (parse from setting)
        fixture_data["bbsolver"].sort(key=lambda r: float(r["setting"].replace("eps_", "")))
    return by


def fig_pareto_grid():
    rows = load_rows()
    by_fixture = group_by_fixture(rows)
    fixtures = sorted(by_fixture.keys())

    fig, axes = plt.subplots(2, 4, figsize=(18, 9))
    axes_flat = axes.flatten()

    for idx, name in enumerate(fixtures):
        ax = axes_flat[idx]
        fixture_data = by_fixture[name]
        n_orig = (fixture_data["bbsolver"][0]["n_original_vertices"]
                  if fixture_data["bbsolver"] else 0)

        # bbsolver pareto curve
        bbs = fixture_data["bbsolver"]
        if bbs:
            xs = [r["n_anchors"] for r in bbs]
            ys = [r["bezier_max_err_px"] for r in bbs]
            eps_labels = [r["setting"].replace("eps_", "ε=") for r in bbs]
            ax.plot(xs, ys, "-o", color=COLOR_BBS, linewidth=2,
                    markersize=8, label="bbsolver vertex_only")
            for x, y, lbl in zip(xs, ys, eps_labels):
                ax.annotate(lbl, xy=(x, y), xytext=(5, 5),
                            textcoords="offset points", fontsize=7,
                            color=COLOR_BBS)

        # Illustrator point(s)
        for r in fixture_data["illustrator"]:
            ax.scatter(r["n_anchors"], r["bezier_max_err_px"],
                       marker="s", color=COLOR_ILLUSTRATOR, s=120,
                       edgecolor="black", linewidth=0.8, zorder=10,
                       label=f"Illustrator (100% Simplify)")
            ax.annotate(f"{r['n_anchors']} pts\n{r['bezier_max_err_px']:.2f} px",
                        xy=(r["n_anchors"], r["bezier_max_err_px"]),
                        xytext=(8, -2), textcoords="offset points",
                        fontsize=8, color=COLOR_ILLUSTRATOR, fontweight="bold")

        ax.set_title(f"{name}  ({n_orig} source verts)", fontsize=10)
        ax.set_xlabel("Anchors kept", fontsize=9)
        ax.set_ylabel("max_err vs source (px)", fontsize=9)
        ax.set_yscale("log")
        ax.grid(True, which="both", linestyle=":", alpha=0.4)
        if idx == 0:
            ax.legend(loc="upper right", fontsize=8, framealpha=0.92)

    fig.suptitle(
        "SVG vertex-decimation head-to-head: bbsolver vertex_only "
        "(L∞-validated) vs Illustrator Simplify at 100 %\n"
        "Lower-and-leftward is better. Each panel: bbsolver's Pareto curve across "
        "ε ∈ {0.5, 1, 2, 5, 10} vs Illustrator's single 100%-setting point.",
        fontsize=12, y=0.995)
    plt.tight_layout(rect=(0, 0, 1, 0.96))
    out = FIGURES / "svg_decimation_pareto_grid.png"
    fig.savefig(out, dpi=DPI)
    plt.close(fig)
    print(f"wrote {out}")


def fig_summary_scatter():
    """Single-panel scatter: all 8 fixtures, both methods. The legend (with
    fixture names) lives outside the plot to the right, so curves stay
    annotation-free and overlapping in-figure labels are eliminated."""
    rows = load_rows()
    by_fixture = group_by_fixture(rows)
    fixtures = sorted(by_fixture.keys())

    # Wider figure to make room for the right-side legend.
    fig, ax = plt.subplots(figsize=(14, 7.5))

    for name in fixtures:
        fd = by_fixture[name]
        bbs = fd["bbsolver"]
        if not bbs:
            continue
        xs = [r["n_anchors"] for r in bbs]
        ys = [r["bezier_max_err_px"] for r in bbs]
        # Pretty label for the legend
        pretty = name.replace("_", " ")
        line, = ax.plot(xs, ys, "-o", linewidth=1.6, markersize=6,
                        alpha=0.85, label=pretty)
        # Illustrator point in the matching color (open square).
        for r in fd["illustrator"]:
            ax.scatter(r["n_anchors"], r["bezier_max_err_px"],
                       marker="s", facecolor="white",
                       edgecolor=line.get_color(), s=130, linewidth=2.0,
                       zorder=10)

    ax.set_xlabel("Anchors kept after decimation")
    ax.set_ylabel("max_err vs source polyline (px, log scale)")
    ax.set_yscale("log")
    ax.grid(True, which="both", linestyle=":", alpha=0.4)

    # Marker-convention legend at the top (no overlap with data, just
    # explains the convention). The fixture-list legend is on the right.
    from matplotlib.lines import Line2D
    convention_handles = [
        Line2D([0], [0], marker="o", color="#444", linewidth=1.6,
               markersize=7, label="bbsolver vertex_only Pareto (ε sweep)"),
        Line2D([0], [0], marker="s", markerfacecolor="white",
               markeredgecolor="#444", linewidth=0, markersize=10,
               markeredgewidth=2, label="Illustrator Simplify @ 100 %"),
    ]
    convention_legend = ax.legend(
        handles=convention_handles,
        loc="upper left", bbox_to_anchor=(0.0, 1.0),
        fontsize=9, framealpha=0.95, title="markers",
    )
    ax.add_artist(convention_legend)

    # Fixture-color legend OUTSIDE plot to the right (no overlap with data).
    h, l = ax.get_legend_handles_labels()
    fig.legend(
        h, l,
        loc="center left", bbox_to_anchor=(0.78, 0.5),
        fontsize=9, framealpha=0.95, title="fixture",
    )

    plt.title(
        "SVG vertex-decimation, 8 fixtures: bbsolver (lines, ε ∈ {0.5, 1, 2, 5, 10}) "
        "vs Illustrator 100% Simplify (open squares)\n"
        "Lower-and-leftward is better; bbsolver's curve lies below or left of its Illustrator point on 6/8 fixtures.",
        fontsize=10)
    # Reserve right margin for the external legend.
    plt.tight_layout(rect=(0, 0, 0.77, 0.96))
    out = FIGURES / "svg_decimation_summary.png"
    fig.savefig(out, dpi=DPI)
    plt.close(fig)
    print(f"wrote {out}")


def main():
    fig_pareto_grid()
    fig_summary_scatter()


if __name__ == "__main__":
    main()
