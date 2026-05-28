"""Toolchefs keyReducer algorithm — standalone Python port.

Direct translation of /tmp/keyReducer/source/keyReducerCmd.cpp doKeyReduce()
and getValue() functions. Output: kept anchor indices; the deviation metric
is perpendicular distance from a candidate point to the line through its
flanking already-kept points (2D in (time, value) space, matching the C++
original which used MVector p1/p2/p3 in 2D).
"""
from __future__ import annotations

import math
from typing import List, Tuple


def _perpendicular_distance(x1, y1, x2, y2, x3, y3):
    """Toolchefs getValue(): perpendicular distance from (x3, y3) to the
    line through (x1, y1) and (x2, y2) in 2D."""
    vx = x2 - x1
    vy = y2 - y1
    seg_len = math.hypot(vx, vy)
    if seg_len < 1e-12:
        return math.hypot(x3 - x1, y3 - y1)
    # Normalize
    vx /= seg_len
    vy /= seg_len
    # Project (p3 - p1) onto unit vector
    t = vx * (x3 - x1) + vy * (y3 - y1)
    # Perpendicular component
    px = (x3 - x1) - t * vx
    py = (y3 - y1) - t * vy
    return math.hypot(px, py)


def _do_key_reduce(times: List[float], values: List[float], tolerance: float) -> List[int]:
    """Direct translation of KeyReducerCmd::doKeyReduce. Returns sorted
    list of indices into the source `times` / `values` that should be kept.
    """
    n = len(times)
    if n == 0:
        return []
    if n == 1:
        return [0]

    out_keys = [0, n - 1]  # first and last samples are always kept

    while True:
        current_deviation = 0.0
        current_index = -1
        # find sample with max deviation from its line segment
        for i in range(1, n - 1):
            if i in out_keys:
                continue
            # Find flanking kept indices
            # out_keys is maintained sorted; binary-search for the bracket
            lo, hi = 0, len(out_keys) - 1
            while lo + 1 < hi:
                mid = (lo + hi) // 2
                if out_keys[mid] < i:
                    lo = mid
                else:
                    hi = mid
            x1 = times[out_keys[lo]]
            y1 = values[out_keys[lo]]
            x2 = times[out_keys[hi]]
            y2 = values[out_keys[hi]]
            x3 = times[i]
            y3 = values[i]
            d = _perpendicular_distance(x1, y1, x2, y2, x3, y3)
            if d > current_deviation:
                current_deviation = d
                current_index = i

        if current_deviation > tolerance and current_index >= 0:
            # Insert sorted
            for j, k in enumerate(out_keys):
                if k > current_index:
                    out_keys.insert(j, current_index)
                    break
            else:
                out_keys.append(current_index)
        else:
            break

        if len(out_keys) == n:
            break

    return out_keys


def reduce_channel(
    sample_times: List[float],
    sample_values: List[float],
    error: float,
) -> Tuple[int, float, list]:
    """Run the Toolchefs algorithm on a scalar channel.

    Returns:
        (kept_keys, max_residual, kept_indices)
        kept_keys     : number of anchors kept
        max_residual  : maximum value-axis residual between piecewise-linear
                        reconstruction and source samples (true L∞ value error)
        kept_indices  : the indices into the source samples that were kept
    """
    if not sample_times:
        return 0, 0.0, []
    kept = _do_key_reduce(sample_times, sample_values, error)
    if len(kept) < 2:
        # Degenerate — single value
        const_v = sample_values[kept[0]] if kept else 0.0
        max_resid = max((abs(v - const_v) for v in sample_values), default=0.0)
        return len(kept), max_resid, kept

    # Compute max value-axis residual between piecewise-linear interpolation
    # through the kept anchors and the source samples
    max_resid = 0.0
    # Iterate samples in order, with a pointer into kept to find bracket
    kp = 0
    for i, (t, v) in enumerate(zip(sample_times, sample_values)):
        # Advance kp so that kept[kp] <= i <= kept[kp+1]
        while kp + 1 < len(kept) and kept[kp + 1] < i:
            kp += 1
        if i == kept[kp]:
            # exact anchor — residual zero by construction
            continue
        if kp + 1 >= len(kept):
            # past last anchor — use last anchor value
            d = abs(v - sample_values[kept[-1]])
        else:
            t0 = sample_times[kept[kp]]
            v0 = sample_values[kept[kp]]
            t1 = sample_times[kept[kp + 1]]
            v1 = sample_values[kept[kp + 1]]
            if t1 <= t0:
                interp_v = v0
            else:
                u = (t - t0) / (t1 - t0)
                interp_v = v0 + u * (v1 - v0)
            d = abs(v - interp_v)
        if d > max_resid:
            max_resid = d

    return len(kept), max_resid, kept
