"""Generate the level-playing-field comparison figure.

Reads supplementary/fbx_mocap_level_playing_field.csv (achieved
accuracy vs key count, per method per target) and produces:

    figures/fbx_mocap_level_playing_field.png
        Grouped bar chart. At each target accuracy, key-count needed per
        method:
          - bbsolver as vector keys (one ThreeD edit point)
          - bbsolver as scalar-equivalent (vector × 3) to compare on the same
            metric as the baselines, which emit per-axis scalar F-curve keys
          - Blender F-Curve Decimate
          - Joosten / Paper.js Bezier-fit reducer (DNF on every target)
          - Toolchefs keyReducer (RDP-variant)

        Y-axis log scale so bbsolver's 214 keys and Toolchefs' 17 646 keys
        are both readable. DNF flagged with a single inline label.
"""
from __future__ import annotations

import csv
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402

HERE = Path(__file__).resolve().parent
DATA_DIR = HERE.parent / "data" / "supplementary"
FIGURES = HERE.parent / "figures"

# Each bbsolver ThreeD key controls all three axes at the same instant; the
# baselines emit independent per-axis scalar F-curve keys. To compare on the
# same storage / insertion-count metric, multiply bbsolver's vector key count
# by the number of dimensions per property (3 for the FBX ThreeD fixture).
SCALAR_COMPONENTS_PER_VECTOR_KEY = 3

COLOR_BBS_VECTOR = "#0066cc"     # bbsolver vector (animator edit points)
COLOR_BBS_SCALAR = "#5599dd"     # bbsolver scalar-equivalent (lighter shade)
COLOR_BLENDER   = "#cc6600"
COLOR_JOOSTEN   = "#9966cc"
COLOR_TOOLCHEFS = "#009966"


def main():
    rows = list(csv.DictReader(open(DATA_DIR / "fbx_mocap_level_playing_field.csv")))
    targets = sorted({float(r["target_max_err"]) for r in rows})

    series = [
        ("bbsolver_vec",   "bbsolver — vector keys (edit points)", COLOR_BBS_VECTOR),
        ("bbsolver_scl",   "bbsolver — scalar-equivalent (× 3)",   COLOR_BBS_SCALAR),
        ("blender",        "Blender F-Curve Decimate",             COLOR_BLENDER),
        ("joosten",        "Joosten / Paper.js Bezier-fit",        COLOR_JOOSTEN),
        ("toolchefs",      "Toolchefs keyReducer (RDP-variant)",   COLOR_TOOLCHEFS),
    ]

    # by_target[t][method-key] = int(keys) or None for DNF
    by_target: dict[float, dict[str, int | None]] = {t: {} for t in targets}
    for r in rows:
        t = float(r["target_max_err"])
        m = r["method"]
        v = r["keys"]
        if v == "n/a":
            val = None
        else:
            val = int(v)
        if m == "bbsolver":
            by_target[t]["bbsolver_vec"] = val
            by_target[t]["bbsolver_scl"] = (val * SCALAR_COMPONENTS_PER_VECTOR_KEY) if val else None
        elif m == "blender_decimate":
            by_target[t]["blender"] = val
        elif m == "joosten_reducer":
            by_target[t]["joosten"] = val
        elif m == "toolchefs_reducer":
            by_target[t]["toolchefs"] = val

    # Wider figure with right margin reserved for an external legend so it
    # never overlaps a bar regardless of axis-tick choices.
    fig, ax = plt.subplots(figsize=(15, 7.5))
    x = np.arange(len(targets))
    n_series = len(series)
    width = 0.155  # 5 bars × 0.155 = 0.775 group width with small gap

    # Log scale lets 214 and 17 646 coexist legibly. Set a floor so the DNF
    # placeholder marker is visible and so the y-axis starts above zero.
    floor = 50

    for i, (key, label, color) in enumerate(series):
        offsets = (i - (n_series - 1) / 2) * width
        heights = []
        annotations = []  # (kind, text) where kind in {"value", "dnf"}
        for t in targets:
            v = by_target[t].get(key)
            if v is None:
                # Use the floor for visualization so bar has presence; mark as DNF.
                heights.append(floor)
                annotations.append(("dnf", "DNF"))
            else:
                heights.append(v)
                annotations.append(("value", f"{v:,}"))

        bars = ax.bar(
            x + offsets,
            heights,
            width,
            color=color,
            alpha=0.85 if key != "bbsolver_scl" else 0.65,
            edgecolor="white",
            linewidth=0.4,
            label=label,
        )
        # Add a slim hatch to the scalar-eq bbsolver bars so they're
        # distinguishable from the vector bars even in grayscale prints.
        if key == "bbsolver_scl":
            for bar in bars:
                bar.set_hatch("///")

        for bar, (kind, text), h in zip(bars, annotations, heights):
            ax.text(
                bar.get_x() + bar.get_width() / 2,
                h * 1.12,
                text,
                ha="center", va="bottom",
                fontsize=8.5 if kind == "value" else 9,
                color=color,
                fontweight="bold",
                rotation=0,
            )

    ax.set_xticks(x)
    ax.set_xticklabels([f"Target ≤ {t:g}°" for t in targets], fontsize=11)
    ax.set_xlabel("Achieved max_err on worst property (target accuracy threshold)")
    ax.set_ylabel("Keys required to achieve target accuracy  (log scale)")
    ax.set_yscale("log")
    ax.set_ylim(floor * 0.6, 50000)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.grid(True, axis="y", which="both", linestyle=":", alpha=0.4)
    # Legend OUTSIDE plot to the right so no bar or label can ever overlap it.
    ax.legend(
        loc="center left", bbox_to_anchor=(1.01, 0.5),
        framealpha=0.95, fontsize=9, frameon=True,
    )

    plt.title(
        "Level-playing-field comparison: keys needed to ACHIEVE a given accuracy\n"
        "FBX mocap, 45 ThreeD properties × 299 samples. Each baseline's input ε retuned to meet the target. "
        "bbsolver shown on both metrics: vector edit points (one shared-timing key controls 3 axes) "
        "and scalar-equivalent (×3, for apples-to-apples storage comparison).",
        fontsize=10,
    )
    # Reserve right margin for the external legend so layout doesn't crop it.
    plt.tight_layout(rect=(0, 0, 0.78, 0.96))
    out = FIGURES / "fbx_mocap_level_playing_field.png"
    fig.savefig(out, dpi=140)
    plt.close(fig)
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
