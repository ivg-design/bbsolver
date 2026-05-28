"""Generate a diverse set of high-vertex-count SVGs for vertex-decimation
benchmarking. All output paths are closed polylines (M + sequence of L
commands + Z) so that Illustrator's Object > Path > Simplify and any
RDP-style decimator can operate on them comparably.

Designed to span the realistic landscape of vector-graphics simplification
problems an artist might face:

  noisy_circle_120.svg      — auto-traced smooth circle (120 vertices, low-amplitude noise)
  signature_curve_180.svg   — handwritten signature analogue (180 vertices, long winding s-curve)
  organic_blob_150.svg      — irregular blob (150 vertices, mix of sharp and smooth regions)
  dense_silhouette_240.svg  — auto-traced character silhouette analogue (240 vertices, varying curvature)
  angular_path_100.svg      — alternating sharp/smooth (100 vertices, tests sharp-feature preservation)
  star_curved_160.svg       — five-point star with bezier-like curved sides (160 vertices)
  spiral_200.svg            — equiangular spiral (200 vertices, decreasing radius of curvature)
  heart_140.svg             — heart outline (140 vertices, sharp top cleft + smooth bottom)

Each SVG is closed (terminates in Z), single-path, in a 1000×1000 viewBox.
Coordinates are floats with 4-decimal precision.
"""
from __future__ import annotations

import math
import random
from pathlib import Path


OUT_DIR = Path(__file__).resolve().parent


def _svg_wrap(d, title, n_vertices, viewbox="0 0 1000 1000"):
    return f'''<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="{viewbox}" width="1000" height="1000">
  <title>{title}</title>
  <desc>{n_vertices} vertices; closed polyline path for vertex-decimation benchmarking.</desc>
  <path d="{d}" fill="none" stroke="#000" stroke-width="1.5" stroke-linejoin="round"/>
</svg>
'''


def _path_d_from_points(points, closed=True):
    parts = [f"M {points[0][0]:.4f},{points[0][1]:.4f}"]
    for x, y in points[1:]:
        parts.append(f"L {x:.4f},{y:.4f}")
    if closed:
        parts.append("Z")
    return " ".join(parts)


# --------------------- 1. noisy_circle ---------------------

def make_noisy_circle(n=120, base_r=380, noise_amp=8, freq_hi=11, seed=7):
    rng = random.Random(seed)
    cx, cy = 500, 500
    pts = []
    # add a deterministic phase-shifted sine for "auto-trace" feel
    for i in range(n):
        theta = (i / n) * 2 * math.pi
        r = base_r \
            + noise_amp * math.sin(theta * freq_hi) \
            + (noise_amp * 0.6) * math.cos(theta * 17 + 0.7) \
            + (noise_amp * 0.4) * (rng.random() - 0.5)
        pts.append((cx + r * math.cos(theta), cy + r * math.sin(theta)))
    return pts


# --------------------- 2. signature_curve ---------------------

def make_signature_curve(n=180, seed=11):
    """A long meandering s-curve sampled densely. Open path becomes closed
    by adding a return segment along the bottom."""
    rng = random.Random(seed)
    pts = []
    # forward stroke: 8 s-curve loops left-to-right
    for i in range(n // 2):
        u = i / (n // 2)
        x = 80 + u * 840
        # base oscillation
        y = 500 + 180 * math.sin(u * 8 * math.pi) \
             + 60 * math.sin(u * 24 * math.pi + 1.2) \
             + 12 * (rng.random() - 0.5)
        pts.append((x, y))
    # return stroke along bottom edge with slight noise
    for i in range(n // 2):
        u = i / (n // 2)
        x = 920 - u * 840
        y = 900 + 6 * math.sin(u * 12 * math.pi) + 4 * (rng.random() - 0.5)
        pts.append((x, y))
    return pts


# --------------------- 3. organic_blob ---------------------

def make_organic_blob(n=150, seed=23):
    """Irregular closed shape with mix of sharp and smooth regions."""
    rng = random.Random(seed)
    cx, cy = 500, 500
    base_r = 320
    pts = []
    # generate radial profile with several "anchors" of varying curvature
    n_anchors = 7
    anchor_thetas = sorted([rng.uniform(0, 2 * math.pi) for _ in range(n_anchors)])
    anchor_rs = [base_r + rng.uniform(-100, 120) for _ in range(n_anchors)]
    anchor_sharpness = [rng.uniform(0.3, 4.0) for _ in range(n_anchors)]
    for i in range(n):
        theta = (i / n) * 2 * math.pi
        # find nearest anchor (cyclic)
        r = 0.0
        wsum = 0.0
        for at, ar, sh in zip(anchor_thetas, anchor_rs, anchor_sharpness):
            d = min(abs(theta - at), 2 * math.pi - abs(theta - at))
            w = math.exp(-(d * sh) ** 2)
            r += w * ar
            wsum += w
        r /= wsum if wsum else 1.0
        # add some local variation
        r += 3 * math.sin(theta * 23 + 0.4)
        pts.append((cx + r * math.cos(theta), cy + r * math.sin(theta)))
    return pts


# --------------------- 4. dense_silhouette ---------------------

def make_dense_silhouette(n=240, seed=42):
    """Closed silhouette resembling an auto-traced figure outline.
    Created by a parametric profile with high vertex density."""
    rng = random.Random(seed)
    pts = []
    # head outline (top, smooth)
    for i in range(60):
        u = i / 60
        theta = math.pi + u * math.pi  # half circle, top
        x = 500 + 130 * math.cos(theta)
        y = 200 + 130 * math.sin(theta)
        pts.append((x + 2 * (rng.random() - 0.5), y + 2 * (rng.random() - 0.5)))
    # neck + shoulder (right side)
    for i in range(30):
        u = i / 30
        x = 370 + u * 60
        y = 200 + u * 180
        pts.append((x + 3 * math.sin(u * 8 * math.pi), y + 2 * (rng.random() - 0.5)))
    # body right side (down)
    for i in range(50):
        u = i / 50
        x = 430 + 60 * math.sin(u * 1.6 * math.pi)
        y = 380 + u * 480
        pts.append((x, y))
    # bottom (curved)
    for i in range(30):
        u = i / 30
        theta = u * math.pi
        x = 500 + 95 * math.cos(math.pi - theta)
        y = 860 + 30 * math.sin(theta)
        pts.append((x, y))
    # body left side (up)
    for i in range(50):
        u = i / 50
        x = 580 - 60 * math.sin(u * 1.6 * math.pi)
        y = 860 - u * 480
        pts.append((x, y))
    # shoulder (left)
    for i in range(n - len(pts) - 1):
        u = i / max(1, (n - len(pts) - 1))
        x = 540 - u * 60
        y = 380 - u * 180
        pts.append((x + 3 * math.sin(u * 8 * math.pi), y + 2 * (rng.random() - 0.5)))
    return pts[:n]


# --------------------- 5. angular_path ---------------------

def make_angular_path(n=100, seed=13):
    """Path alternating sharp corners and smooth arcs.
    Tests preservation of sharp features by the simplifier."""
    pts = []
    n_segments = 8
    for seg in range(n_segments):
        seg_n = n // n_segments
        if seg % 2 == 0:
            # smooth quarter arc
            cx = 200 + (seg // 2) * 200
            cy = 500
            r = 150
            theta_start = seg * 0.5
            for i in range(seg_n):
                u = i / seg_n
                theta = theta_start + u * (math.pi / 2)
                pts.append((cx + r * math.cos(theta), cy + r * math.sin(theta)))
        else:
            # sharp zigzag (alternating up/down)
            cx = 250 + (seg // 2) * 200
            for i in range(seg_n):
                u = i / seg_n
                x = cx + u * 100
                y = 350 if i % 2 == 0 else 650
                pts.append((x, y))
    # close
    while len(pts) < n:
        pts.append(pts[-1])
    return pts[:n]


# --------------------- 6. star_curved ---------------------

def make_star_curved(n=160, seed=31):
    """Five-point star with curved sides — sharp points + smooth bezier-like sides."""
    cx, cy = 500, 500
    outer_r = 380
    inner_r = 150
    pts = []
    n_points = 5
    per_segment = n // (n_points * 2)
    angles = []
    for k in range(n_points * 2):
        a = (k / (n_points * 2)) * 2 * math.pi - math.pi / 2
        r = outer_r if k % 2 == 0 else inner_r
        angles.append((a, r))
    for k in range(n_points * 2):
        a1, r1 = angles[k]
        a2, r2 = angles[(k + 1) % (n_points * 2)]
        for i in range(per_segment):
            u = i / per_segment
            # blend linearly in (theta, r)
            # but curve the radius using a quadratic to make sides bow outward
            ub = u * (1 - u) * 4  # 0 → 1 → 0
            a = a1 + u * ((a2 - a1) if abs(a2 - a1) < math.pi else (a2 - a1 - 2 * math.pi))
            r = (1 - u) * r1 + u * r2 + 30 * ub * (-1 if k % 2 else 1)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts[:n]


# --------------------- 7. spiral ---------------------

def make_spiral(n=200):
    """Equiangular spiral. Vertex density should be reducible heavily on
    outer turns (low curvature) and less so on inner turns."""
    cx, cy = 500, 500
    pts = []
    a = 8
    b = 0.15
    for i in range(n):
        t = i * 0.18
        r = a * math.exp(b * t)
        if r > 480:
            break
        x = cx + r * math.cos(t)
        y = cy + r * math.sin(t)
        pts.append((x, y))
    # close back to start (radial line)
    pts.append(pts[0])
    return pts


# --------------------- 8. heart ---------------------

def make_heart(n=140):
    """Classic heart outline. Sharp cleft at top, smooth bottom."""
    cx, cy = 500, 480
    pts = []
    for i in range(n):
        t = (i / n) * 2 * math.pi
        # parametric heart curve, scaled
        x = 16 * math.sin(t) ** 3
        y = -(13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t))
        pts.append((cx + 22 * x, cy + 22 * y))
    return pts


# --------------------- main ---------------------

SHAPES = [
    ("noisy_circle_120", make_noisy_circle),
    ("signature_curve_180", make_signature_curve),
    ("organic_blob_150", make_organic_blob),
    ("dense_silhouette_240", make_dense_silhouette),
    ("angular_path_100", make_angular_path),
    ("star_curved_160", make_star_curved),
    ("spiral_200", make_spiral),
    ("heart_140", make_heart),
]


def main():
    for name, factory in SHAPES:
        pts = factory()
        d = _path_d_from_points(pts, closed=True)
        svg = _svg_wrap(d, name, len(pts))
        out = OUT_DIR / f"{name}.svg"
        out.write_text(svg)
        print(f"wrote {out.name}  ({len(pts)} vertices)")


if __name__ == "__main__":
    main()
