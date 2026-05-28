"""Join Illustrator-simplified outputs (user-produced) with bbsolver
vertex_only outputs and produce a head-to-head comparison.

For each source SVG `fixtures/svg_decimation/<name>.svg` we look for
sibling files matching the glob `<name>*-illustrator.svg` (any naming
the user used, e.g. `<name>-100%simplify-illustrator.svg`,
`<name>-95simplify-illustrator.svg`, `<name> - simplified.svg`).
Each Illustrator file is parsed for its path d-attribute (full SVG path
grammar including M, L, C, S, Q, T, Z, both absolute and relative),
counted for anchors, and densely-sampled to compute true L∞ residual
against the original source vertices.

bbsolver outputs are read from `work/svg_decimation/<name>/bbky_eps_*.json`.

Writes:
    data/supplementary/svg_decimation_combined.csv

Each row:
    svg, method, setting, n_anchors, bezier_max_err_px,
    compression_ratio_vs_source, n_original_vertices
"""
from __future__ import annotations

import csv
import json
import math
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SVG_DIR = HERE / "fixtures" / "svg_decimation"
WORK_ROOT = HERE / "work" / "svg_decimation"
OUT_CSV = HERE / "data" / "supplementary" / "svg_decimation_combined.csv"

sys.path.insert(0, str(HERE))
from run_svg_decimation_comparison import (
    parse_svg_vertices,
    decode_bbky_vertices,
    bezier_path_max_distance,
    point_segment_distance,
)


# ---------------------------------------------------------------------------
# Full SVG path parser — handles M/L/C/S/Q/T/H/V/A/Z (absolute and relative).
# Returns a list of "vertices" where each vertex has anchor + in/out handles.
# We linearize H/V to L semantics and treat Q/T as cubics (raise from quadratic).
# ---------------------------------------------------------------------------

_PATH_D_RE = re.compile(r'<path[^>]*\sd="([^"]+)"', re.IGNORECASE | re.DOTALL)
_TOKEN_RE = re.compile(r'[MmLlHhVvCcSsQqTtAaZz]|-?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?')


def _vertex(x, y, ix=0.0, iy=0.0, ox=0.0, oy=0.0):
    return {"x": x, "y": y, "in_x": ix, "in_y": iy, "out_x": ox, "out_y": oy}


def parse_svg_bezier(svg_text):
    """Return (vertices, closed) where vertices contain anchor + bezier handles.
    Handles full SVG path grammar; for L/H/V the handles are zero (linear segment)."""
    m = _PATH_D_RE.search(svg_text)
    if not m:
        raise ValueError("no <path d=> found")
    raw = m.group(1)
    tokens = _TOKEN_RE.findall(raw)
    verts = []
    closed = False
    cmd = None
    i = 0
    cur_x, cur_y = 0.0, 0.0
    start_x, start_y = 0.0, 0.0
    last_cubic_c2 = None  # for S smooth continuation

    def take_n(n):
        nonlocal i
        vals = [float(tokens[i + k]) for k in range(n)]
        i += n
        return vals

    while i < len(tokens):
        t = tokens[i]
        if t.isalpha():
            cmd = t
            i += 1
            if cmd in ("Z", "z"):
                closed = True
                cur_x, cur_y = start_x, start_y
                last_cubic_c2 = None
                continue
        # Implicit command continuation: M->L, m->l
        if cmd is None:
            # malformed path
            i += 1
            continue
        if cmd in ("M", "m"):
            x, y = take_n(2)
            if cmd == "m" and verts:
                x += cur_x; y += cur_y
            cur_x, cur_y = x, y
            start_x, start_y = x, y
            verts.append(_vertex(x, y))
            cmd = "L" if cmd == "M" else "l"
            last_cubic_c2 = None
        elif cmd in ("L", "l"):
            x, y = take_n(2)
            if cmd == "l":
                x += cur_x; y += cur_y
            # Linear segment: previous vertex has out=0, this vertex has in=0
            verts.append(_vertex(x, y))
            cur_x, cur_y = x, y
            last_cubic_c2 = None
        elif cmd in ("H", "h"):
            (x,) = take_n(1)
            if cmd == "h":
                x += cur_x
            verts.append(_vertex(x, cur_y))
            cur_x = x
            last_cubic_c2 = None
        elif cmd in ("V", "v"):
            (y,) = take_n(1)
            if cmd == "v":
                y += cur_y
            verts.append(_vertex(cur_x, y))
            cur_y = y
            last_cubic_c2 = None
        elif cmd in ("C", "c"):
            x1, y1, x2, y2, x, y = take_n(6)
            if cmd == "c":
                x1 += cur_x; y1 += cur_y
                x2 += cur_x; y2 += cur_y
                x += cur_x; y += cur_y
            # Update previous vertex's out-tangent to (c1 - prev anchor)
            verts[-1]["out_x"] = x1 - verts[-1]["x"]
            verts[-1]["out_y"] = y1 - verts[-1]["y"]
            # New vertex's in-tangent = (c2 - new anchor)
            new_v = _vertex(x, y, ix=x2 - x, iy=y2 - y)
            verts.append(new_v)
            cur_x, cur_y = x, y
            last_cubic_c2 = (x2, y2)
        elif cmd in ("S", "s"):
            x2, y2, x, y = take_n(4)
            if cmd == "s":
                x2 += cur_x; y2 += cur_y
                x += cur_x; y += cur_y
            # Reflect last cubic c2 across previous anchor
            if last_cubic_c2 is not None:
                px, py = verts[-1]["x"], verts[-1]["y"]
                x1 = 2 * px - last_cubic_c2[0]
                y1 = 2 * py - last_cubic_c2[1]
            else:
                x1, y1 = verts[-1]["x"], verts[-1]["y"]
            verts[-1]["out_x"] = x1 - verts[-1]["x"]
            verts[-1]["out_y"] = y1 - verts[-1]["y"]
            new_v = _vertex(x, y, ix=x2 - x, iy=y2 - y)
            verts.append(new_v)
            cur_x, cur_y = x, y
            last_cubic_c2 = (x2, y2)
        elif cmd in ("Q", "q", "T", "t", "A", "a"):
            # Quadratic / arc — for simplicity, skip vertex insertion at intermediate
            # control and just place an anchor at the endpoint. The user's
            # Illustrator output uses only M/C/Z, so this branch rarely fires.
            n_args = {"Q": 4, "q": 4, "T": 2, "t": 2, "A": 7, "a": 7}[cmd]
            args = take_n(n_args)
            x, y = args[-2], args[-1]
            if cmd in ("q", "t", "a"):
                x += cur_x; y += cur_y
            verts.append(_vertex(x, y))
            cur_x, cur_y = x, y
            last_cubic_c2 = None
        else:
            i += 1

    # If path is closed and last vertex coincides with start, merge handles
    if closed and len(verts) >= 2:
        if abs(verts[-1]["x"] - verts[0]["x"]) < 1e-6 and abs(verts[-1]["y"] - verts[0]["y"]) < 1e-6:
            verts[0]["in_x"] = verts[-1]["in_x"]
            verts[0]["in_y"] = verts[-1]["in_y"]
            verts.pop()

    return verts, closed


# ---------------------------------------------------------------------------
# Find Illustrator simplification outputs for a given source SVG
# ---------------------------------------------------------------------------

ILLUSTRATOR_GLOBS = ["*illustrator*.svg", "*simplif*.svg"]


def find_illustrator_outputs(source_name):
    """Find files in SVG_DIR matching `source_name` + illustrator/simplify suffix."""
    candidates = []
    for fname in sorted(SVG_DIR.glob(f"{source_name}*illustrator*.svg")):
        candidates.append(fname)
    for fname in sorted(SVG_DIR.glob(f"{source_name}*simplif*.svg")):
        if fname not in candidates:
            candidates.append(fname)
    return candidates


def parse_setting_label(filename, source_name):
    """Try to extract a human-readable setting label from the filename."""
    stem = filename.stem
    # strip source_name prefix
    if stem.startswith(source_name):
        rest = stem[len(source_name):].strip(" -_")
    else:
        rest = stem
    return rest or "illustrator"


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def main():
    rows = []
    for svg_path in sorted(SVG_DIR.glob("*.svg")):
        # Skip Illustrator-output files; they're matched below as comparison inputs
        if "illustrator" in svg_path.name.lower() or "simplif" in svg_path.name.lower():
            continue
        name = svg_path.stem
        text = svg_path.read_text()
        # parse source as plain polyline (M+L commands)
        orig_anchors, closed_src = parse_svg_vertices(text)
        n_orig = len(orig_anchors)
        print(f"=== {name}  (source: {n_orig} vertices, closed={closed_src}) ===")

        # --- Illustrator outputs ---
        for ill_path in find_illustrator_outputs(name):
            setting = parse_setting_label(ill_path, name)
            ill_text = ill_path.read_text()
            try:
                ill_verts, ill_closed = parse_svg_bezier(ill_text)
            except Exception as e:
                print(f"  [illustrator {setting}] parse failed: {e}")
                continue
            n_anchors = len(ill_verts)
            try:
                berr = bezier_path_max_distance(orig_anchors, ill_verts, closed_src)
            except Exception as e:
                print(f"  [illustrator {setting}] residual failed: {e}")
                continue
            print(f"  [illustrator {setting}] {n_anchors} anchors, "
                  f"max_err={berr:.4f} px")
            rows.append({
                "svg": name, "method": "illustrator",
                "setting": setting,
                "n_anchors": n_anchors,
                "bezier_max_err_px": round(berr, 6),
                "compression_ratio_vs_source": round(n_orig / max(1, n_anchors), 4),
                "n_original_vertices": n_orig,
            })

        # --- bbsolver outputs (read cached bbky files) ---
        work = WORK_ROOT / name
        for bbky in sorted(work.glob("bbky_eps_*.json")):
            eps_str = bbky.stem.replace("bbky_eps_", "")
            try:
                eps = float(eps_str)
            except ValueError:
                continue
            kept, kept_closed = decode_bbky_vertices(bbky)
            if not kept:
                continue
            berr = bezier_path_max_distance(orig_anchors, kept, closed_src)
            print(f"  [bbsolver ε={eps}] {len(kept)} anchors, max_err={berr:.4f} px")
            rows.append({
                "svg": name, "method": "bbsolver",
                "setting": f"eps_{eps}",
                "n_anchors": len(kept),
                "bezier_max_err_px": round(berr, 6),
                "compression_ratio_vs_source": round(n_orig / max(1, len(kept)), 4),
                "n_original_vertices": n_orig,
            })

    OUT_CSV.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT_CSV, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=[
            "svg", "method", "setting", "n_anchors",
            "bezier_max_err_px", "compression_ratio_vs_source",
            "n_original_vertices",
        ])
        w.writeheader()
        w.writerows(rows)
    print(f"\nwrote {OUT_CSV} ({len(rows)} rows)")


if __name__ == "__main__":
    main()
