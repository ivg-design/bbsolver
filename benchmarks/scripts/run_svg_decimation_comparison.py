"""Run bbsolver vertex decimation against a directory of SVG fixtures.

For each SVG:
    1. Parse the first <path> element's polyline (M + L commands).
    2. Build a single-sample, single-property shape_flat bbsm bundle.
    3. Run bbsolver --solve-mode vertex_only at multiple tolerances.
    4. Decode the bbky output back to a polyline.
    5. Measure max perpendicular distance from each ORIGINAL vertex to
       the polyline reconstructed by the kept vertices (true L∞ error
       in pixel space).
    6. Emit decimated SVGs for each tolerance so the user can compare
       against Illustrator's Object > Path > Simplify output.

Outputs:
    work/svg_decimation/<svg_name>/bbsm.json
    work/svg_decimation/<svg_name>/bbky_eps_<eps>.json
    work/svg_decimation/<svg_name>/bbsolver_eps_<eps>.svg
    supplementary/svg_decimation_results.csv
"""
from __future__ import annotations

import csv
import json
import math
import re
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from _paths import resolve_bbsolver_binary

ARXIV_ROOT = HERE.parent
SVG_DIR = ARXIV_ROOT / "fixtures" / "svg_decimation"
WORK_ROOT = ARXIV_ROOT / "work" / "svg_decimation"
WORK_ROOT.mkdir(parents=True, exist_ok=True)
RESULTS_CSV = ARXIV_ROOT / "data" / "supplementary" / "svg_decimation_results.csv"

BBSOLVER = resolve_bbsolver_binary()
EPS_VALUES = [0.5, 1.0, 2.0, 5.0, 10.0]

# ---------------------------------------------------------------------------
# SVG parsing: pull the first <path>'s d= attribute, decompose to vertices.
# Supports simple M + L commands (closed by Z). Other commands fall back to
# treating each control as a vertex.
# ---------------------------------------------------------------------------

_PATH_D_RE = re.compile(r'<path[^>]*\sd="([^"]+)"', re.IGNORECASE | re.DOTALL)
_TOKEN_RE = re.compile(r'[MLZmlz]|-?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?')


def parse_svg_vertices(svg_text):
    m = _PATH_D_RE.search(svg_text)
    if not m:
        raise ValueError("no <path d=> found in SVG")
    tokens = _TOKEN_RE.findall(m.group(1))
    vertices = []
    i = 0
    closed = False
    while i < len(tokens):
        t = tokens[i]
        if t in ("M", "L"):
            x = float(tokens[i + 1])
            y = float(tokens[i + 2])
            vertices.append((x, y))
            i += 3
        elif t in ("m", "l"):
            x = float(tokens[i + 1])
            y = float(tokens[i + 2])
            if vertices:
                x += vertices[-1][0]
                y += vertices[-1][1]
            vertices.append((x, y))
            i += 3
        elif t in ("Z", "z"):
            closed = True
            i += 1
        else:
            i += 1
    return vertices, closed


# ---------------------------------------------------------------------------
# bbsm bundle construction
# ---------------------------------------------------------------------------

def vertices_to_bbsm(vertices, closed, eps):
    """Build a bbsm with one shape_flat (Custom kind) property; static path
    sampled at multiple frames so vertex_only mode runs. Shape encoding
    matches the real AE bbsm format: kind=Custom, units_label=shape_flat,
    v = [closed_flag, n_vertices, (vx,vy,ix,iy,ox,oy) × n].
    """
    n = len(vertices)
    closed_flag = 1.0 if closed else 0.0
    flat = [closed_flag, float(n)]
    for x, y in vertices:
        flat.extend([float(x), float(y), 0.0, 0.0, 0.0, 0.0])

    # Emit 5 identical samples so the temporal pass has something to work on
    # and reduces to a single key; vertex_only mode then reduces vertices.
    n_samples = 5
    samples = [
        {"t_sec": i / 24.0, "v": list(flat)}
        for i in range(n_samples)
    ]

    return {
        "_schema": "samples",
        "schema_version": 1,
        "request_id": "svg_decimation",
        "comp": {
            "fps": 24, "duration_sec": (n_samples - 1) / 24.0,
            "width": 1000, "height": 1000, "pixel_aspect": 1,
            "work_area_start_sec": 0, "work_area_end_sec": (n_samples - 1) / 24.0,
        },
        "config": {
            "tolerance": eps,
            "tolerance_screen_px": 0,
            "weight_pos": 1, "weight_vel": 0.1, "weight_acc": 0.01,
            "weight_curv": 0, "weight_screen": 0,
            "allow_hold": True, "allow_linear": True, "allow_bezier": True,
            "allow_shape_temporal_bezier": True,
            # IMPORTANT: disable both spatial-fit and replacement-fit
            # preprocessing — for a pure vertex-decimation comparison we need
            # bbsolver to operate on the ORIGINAL vertex set, not a
            # spatially-pre-fit version of it. With allow_path_spatial_fit=True,
            # bbsolver collapsed 153→37 vertices on the spiral and 239→29 on
            # the silhouette BEFORE vertex_only ran, which produced an
            # over-smoothed output that lost most source detail.
            "allow_path_spatial_fit": False,
            "allow_path_replacement_fit": False,
            "path_replacement_prefer_vertices": True,
            "path_preserve_sharp_corners": True,
            "path_sharp_corner_angle_deg": 90,
            "path_sharp_corner_tolerance": 1.5,
            "min_influence": 0.1, "max_influence": 100,
            "max_iters_per_segment": 100,
            "min_segment_frames": 2,
            "parallel_jobs": 0,
            "verbose": False,
            "solve_optimization_mode": "vertex_only",
        },
        "properties": [{
            "property": {
                "id": "svg_path",
                "match_name": "ADBE Vector Shape",
                "display_name": "Path",
                "layer_id": 1,
                "layer_index": 1,
                "kind": "Custom",
                "dimensions": 2 + n * 6,
                "is_spatial": False,
                "is_separated": False,
                "source_key_times": [],
                "sample_space": "property",
                "writeback_mode": "normal",
                "units_label": "shape_flat",
                "shape_canonicalized": False,
                "shape_variable_topology": False,
                "shape_canonical_method": "shape_flat_raw_static",
                "shape_canonical_vertex_count": n,
                "shape_max_vertex_count": n,
                "shape_source_topologies": [f"closed={int(closed_flag)},n={n}"],
                "min_value": [],
                "max_value": [],
            },
            "t_start_sec": 0.0,
            "t_end_sec": (n_samples - 1) / 24.0,
            "samples_per_frame": 1,
            "samples": samples,
            "layer_xform_at_start": [[1, 0, 0], [0, 1, 0], [0, 0, 1]],
            "hash_of_expression": "svg_static",
        }],
    }


# ---------------------------------------------------------------------------
# bbky decoding: pull the kept vertices from the first key's shape_flat
# ---------------------------------------------------------------------------

def decode_bbky_vertices(bbky_path):
    """Decode bbky's first key into a list of bezier vertices with full handles.

    Each vertex is a dict:
        {x, y, in_x, in_y, out_x, out_y}
    where in_x/in_y is the offset from the anchor to the incoming control point,
    and out_x/out_y is the offset from the anchor to the outgoing control point.
    """
    d = json.load(open(bbky_path))
    pr = d["property_results"][0]
    keys = pr["keys"]
    if not keys:
        return None, None
    v = keys[0]["v"]
    closed = bool(int(v[0]))
    n = int(v[1])
    verts = []
    for i in range(n):
        o = 2 + i * 6
        verts.append({
            "x": float(v[o]),    "y": float(v[o + 1]),
            "in_x": float(v[o + 2]),  "in_y": float(v[o + 3]),
            "out_x": float(v[o + 4]), "out_y": float(v[o + 5]),
        })
    return verts, closed


# ---------------------------------------------------------------------------
# L∞ error: max perpendicular distance from each original vertex to the
# polyline through the kept vertices.
# ---------------------------------------------------------------------------

def point_segment_distance(px, py, x1, y1, x2, y2):
    dx, dy = x2 - x1, y2 - y1
    seg_len_sq = dx * dx + dy * dy
    if seg_len_sq < 1e-12:
        return math.hypot(px - x1, py - y1)
    t = max(0.0, min(1.0, ((px - x1) * dx + (py - y1) * dy) / seg_len_sq))
    cx, cy = x1 + t * dx, y1 + t * dy
    return math.hypot(px - cx, py - cy)


def _bezier_segment_polyline(v0, v1, n=32):
    """Sample a cubic Bezier segment from vertex v0 to vertex v1 into n+1 points.

    Control points: P0 = v0.anchor; P1 = v0.anchor + v0.out; P2 = v1.anchor + v1.in; P3 = v1.anchor.
    Returns list of (x, y) tuples (n+1 points along the curve, anchor to anchor).
    """
    p0 = (v0["x"], v0["y"])
    p1 = (v0["x"] + v0["out_x"], v0["y"] + v0["out_y"])
    p2 = (v1["x"] + v1["in_x"], v1["y"] + v1["in_y"])
    p3 = (v1["x"], v1["y"])
    pts = []
    for i in range(n + 1):
        t = i / n
        u = 1 - t
        b0 = u * u * u
        b1 = 3 * u * u * t
        b2 = 3 * u * t * t
        b3 = t * t * t
        x = b0 * p0[0] + b1 * p1[0] + b2 * p2[0] + b3 * p3[0]
        y = b0 * p0[1] + b1 * p1[1] + b2 * p2[1] + b3 * p3[1]
        pts.append((x, y))
    return pts


def bezier_path_max_distance(original_vertices, kept_vertices, closed, samples_per_segment=32):
    """For each original vertex, find min distance to the bezier path
    formed by the kept vertices and their handles. Report the max.

    The bezier path is approximated as a dense polyline (samples_per_segment
    points per segment) and point-to-segment distances are computed against
    that approximation.
    """
    if len(kept_vertices) < 2:
        return float("inf")
    # Build dense polyline approximation of the bezier path
    dense_pts = []
    for i in range(len(kept_vertices) - 1):
        seg = _bezier_segment_polyline(kept_vertices[i], kept_vertices[i + 1],
                                       n=samples_per_segment)
        dense_pts.extend(seg[:-1])  # drop last point to avoid duplicating with next segment's first
    if closed:
        seg = _bezier_segment_polyline(kept_vertices[-1], kept_vertices[0],
                                       n=samples_per_segment)
        dense_pts.extend(seg[:-1])
    else:
        dense_pts.append((kept_vertices[-1]["x"], kept_vertices[-1]["y"]))
    # Close the dense polyline if path is closed
    if closed:
        segments = list(zip(dense_pts, dense_pts[1:])) + [(dense_pts[-1], dense_pts[0])]
    else:
        segments = list(zip(dense_pts, dense_pts[1:]))
    max_d = 0.0
    for px, py in original_vertices:
        d_min = min(point_segment_distance(px, py, x1, y1, x2, y2)
                    for (x1, y1), (x2, y2) in segments)
        if d_min > max_d:
            max_d = d_min
    return max_d


# ---------------------------------------------------------------------------
# SVG writer (output simplified path back to disk)
# ---------------------------------------------------------------------------

def write_svg_bezier(out_path, kept_vertices, closed, title, eps, n_orig):
    """Emit the bbsolver output as a real cubic-Bezier SVG path (C commands)
    so the user can visually compare against Illustrator's simplified output.
    """
    if not kept_vertices:
        return
    v0 = kept_vertices[0]
    parts = [f"M {v0['x']:.4f},{v0['y']:.4f}"]
    n = len(kept_vertices)
    for i in range(1, n):
        prev = kept_vertices[i - 1]
        curr = kept_vertices[i]
        cx1 = prev["x"] + prev["out_x"]
        cy1 = prev["y"] + prev["out_y"]
        cx2 = curr["x"] + curr["in_x"]
        cy2 = curr["y"] + curr["in_y"]
        parts.append(f"C {cx1:.4f},{cy1:.4f} {cx2:.4f},{cy2:.4f} {curr['x']:.4f},{curr['y']:.4f}")
    if closed and n >= 2:
        prev = kept_vertices[-1]
        curr = kept_vertices[0]
        cx1 = prev["x"] + prev["out_x"]
        cy1 = prev["y"] + prev["out_y"]
        cx2 = curr["x"] + curr["in_x"]
        cy2 = curr["y"] + curr["in_y"]
        parts.append(f"C {cx1:.4f},{cy1:.4f} {cx2:.4f},{cy2:.4f} {curr['x']:.4f},{curr['y']:.4f}")
        parts.append("Z")
    d = " ".join(parts)
    svg = f'''<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1000 1000" width="1000" height="1000">
  <title>{title} (bbsolver vertex_only, eps={eps})</title>
  <desc>{len(kept_vertices)} bezier anchors of {n_orig} source vertices; bbsolver vertex_only solve at tolerance {eps}.</desc>
  <path d="{d}" fill="none" stroke="#0066cc" stroke-width="1.5" stroke-linejoin="round"/>
</svg>
'''
    out_path.write_text(svg)


# ---------------------------------------------------------------------------
# bbsolver invocation
# ---------------------------------------------------------------------------

def run_bbsolver_vertex(bbsm_path, bbky_path, eps):
    cmd = [BBSOLVER, "solve", str(bbsm_path), str(bbky_path),
           "--tolerance", str(eps), "--solve-mode", "vertex_only",
           "--jobs", "0"]
    t0 = time.time()
    r = subprocess.run(cmd, capture_output=True, text=True)
    elapsed = time.time() - t0
    if r.returncode != 0:
        raise RuntimeError(f"bbsolver failed (eps={eps}):\n{r.stderr[-1000:]}")
    return elapsed


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def main():
    # Only process actual source SVGs — skip Illustrator-output files
    # (named "*-illustrator.svg" or "*simplif*.svg") so we don't waste
    # compute decimating already-decimated paths.
    svgs = [p for p in sorted(SVG_DIR.glob("*.svg"))
            if "illustrator" not in p.name.lower()
            and "simplif" not in p.name.lower()]
    if not svgs:
        print(f"no source SVGs found in {SVG_DIR}", file=sys.stderr)
        sys.exit(1)
    print(f"processing {len(svgs)} source SVGs (skipped Illustrator output files)")

    rows = []
    for svg_path in svgs:
        name = svg_path.stem
        print(f"\n=== {name} ===")
        text = svg_path.read_text()
        orig_vertices, closed = parse_svg_vertices(text)
        n_orig = len(orig_vertices)
        print(f"  source: {n_orig} vertices, closed={closed}")

        work_dir = WORK_ROOT / name
        work_dir.mkdir(parents=True, exist_ok=True)

        for eps in EPS_VALUES:
            bbsm_path = work_dir / f"bbsm_eps_{eps}.json"
            bbky_path = work_dir / f"bbky_eps_{eps}.json"
            out_svg = work_dir / f"bbsolver_eps_{eps}.svg"

            bbsm = vertices_to_bbsm(orig_vertices, closed, eps)
            bbsm_path.write_text(json.dumps(bbsm, indent=2))
            try:
                elapsed = run_bbsolver_vertex(bbsm_path, bbky_path, eps)
            except RuntimeError as e:
                print(f"  [eps={eps}] {e}")
                rows.append({
                    "svg": name, "n_original": n_orig, "eps": eps,
                    "n_kept": "n/a", "max_err": "n/a",
                    "compression_ratio": "n/a", "solve_time_ms": "n/a",
                })
                continue

            kept, kept_closed = decode_bbky_vertices(bbky_path)
            if kept is None:
                print(f"  [eps={eps}] no keys emitted")
                continue

            # Distance measured against bbsolver's emitted bezier curve, not
            # just its anchor polyline. This matches bbsolver's own metric.
            max_err = bezier_path_max_distance(orig_vertices, kept, closed)
            # Also capture bbsolver's self-reported max_err for cross-check
            bbky_meta = json.load(open(bbky_path))["property_results"][0]
            solver_max_err = float(bbky_meta.get("max_err", 0.0))
            write_svg_bezier(out_svg, kept, kept_closed, name, eps, n_orig)
            print(f"  [eps={eps}] {len(kept)} bezier anchors kept (vs {n_orig}), "
                  f"bezier max_err={max_err:.4f} px (solver self-reported {solver_max_err:.4f}), "
                  f"{elapsed*1000:.0f} ms")
            rows.append({
                "svg": name, "n_original": n_orig, "eps": eps,
                "n_kept": len(kept), "bezier_max_err": round(max_err, 6),
                "solver_max_err": round(solver_max_err, 6),
                "compression_ratio": round(n_orig / len(kept), 4) if len(kept) else "n/a",
                "solve_time_ms": round(elapsed * 1000, 1),
            })

    RESULTS_CSV.parent.mkdir(parents=True, exist_ok=True)
    with open(RESULTS_CSV, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=[
            "svg", "n_original", "eps", "n_kept",
            "bezier_max_err", "solver_max_err",
            "compression_ratio", "solve_time_ms",
        ])
        w.writeheader()
        w.writerows(rows)
    print(f"\nwrote {RESULTS_CSV} ({len(rows)} rows)")


if __name__ == "__main__":
    main()
