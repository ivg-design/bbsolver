"""Runner that drives the ported joosten reducer against a bbsm-style
scalar curve and measures residual against source samples."""
from __future__ import annotations

import math
from typing import List, Tuple

from .fit import fit_bezier
from .vector import Vector2D


def _eval_cubic_bezier(p0, p1, p2, p3, t):
    """Evaluate a cubic Bezier at parameter t in [0,1] given four Vector2D
    control points; returns Vector2D."""
    u = 1.0 - t
    a = u * u * u
    b = 3.0 * u * u * t
    c = 3.0 * u * t * t
    d = t * t * t
    return Vector2D(
        a * p0.x + b * p1.x + c * p2.x + d * p3.x,
        a * p0.y + b * p1.y + c * p2.y + d * p3.y,
    )


def _solve_t_for_x(p0x, p1x, p2x, p3x, target_x, tol=1e-7, iters=64):
    """Solve for t in [0,1] such that the Bezier x-coordinate equals target_x.

    Uses bisection — robust for monotonic time axis."""
    lo, hi = 0.0, 1.0
    for _ in range(iters):
        mid = 0.5 * (lo + hi)
        u = 1.0 - mid
        x = (u * u * u) * p0x + 3 * (u * u) * mid * p1x + 3 * u * (mid * mid) * p2x + (mid * mid * mid) * p3x
        if abs(x - target_x) < tol:
            return mid
        if x < target_x:
            lo = mid
        else:
            hi = mid
    return 0.5 * (lo + hi)


def _segment_value_at(prev_kf, kf, t_target):
    """Given two adjacent Keyframes from the joosten reducer, evaluate the
    cubic Bezier segment between them at time t_target and return the y-value."""
    p0 = prev_kf.point
    p3 = kf.point
    # joosten's keyframes carry out-handle vectors relative to anchor
    # (prev.outHandle is the vector from prev.point to its outgoing control)
    p1 = p0 + (prev_kf.outHandle if prev_kf.outHandle is not None else Vector2D(0.0, 0.0))
    p2 = p3 + (kf.inHandle if kf.inHandle is not None else Vector2D(0.0, 0.0))
    # Find Bezier parameter that corresponds to x == t_target
    t = _solve_t_for_x(p0.x, p1.x, p2.x, p3.x, t_target)
    pt = _eval_cubic_bezier(p0, p1, p2, p3, t)
    return pt.y


def reduce_channel(
    sample_times: List[float],
    sample_values: List[float],
    error: float,
) -> Tuple[int, float, list]:
    """Run the ported joosten reducer on a scalar channel.

    Args:
        sample_times: sorted list of t values (e.g. frame*1/fps)
        sample_values: matching list of values at those times
        error: max allowed deviation per the algorithm's internal metric

    Returns:
        (kept_keys, max_residual_against_source, raw_keyframes)
            kept_keys             : number of keyframes emitted by the fit
            max_residual          : max |sample_value - bezier_evaluated_value|
                                    over every source sample time (true L∞
                                    residual against source)
            raw_keyframes         : the joosten keyframe objects (Vector2D
                                    anchors + handles), for inspection
    """
    if len(sample_times) == 0:
        return 0, 0.0, []
    if len(sample_times) == 1:
        return 1, 0.0, [None]

    points = [Vector2D(t, v) for t, v in zip(sample_times, sample_values)]
    keyframes = fit_bezier(points, error=error, weighted_tangents=True)

    # Evaluate the fitted curve at every source sample time and measure residual
    max_resid = 0.0
    if len(keyframes) < 2:
        # degenerate — single keyframe, treat as constant value
        const_v = keyframes[0].point.y if keyframes else 0.0
        for v in sample_values:
            d = abs(v - const_v)
            if d > max_resid:
                max_resid = d
        return len(keyframes), max_resid, keyframes

    # Build a list of segment boundaries (anchor x-coordinates)
    boundaries = [kf.point.x for kf in keyframes]
    for st, sv in zip(sample_times, sample_values):
        # find segment containing st
        # boundaries is monotonic increasing in x (= time)
        if st <= boundaries[0]:
            eval_v = keyframes[0].point.y
        elif st >= boundaries[-1]:
            eval_v = keyframes[-1].point.y
        else:
            # binary search for the segment
            lo, hi = 0, len(boundaries) - 1
            while lo + 1 < hi:
                mid = (lo + hi) // 2
                if boundaries[mid] <= st:
                    lo = mid
                else:
                    hi = mid
            eval_v = _segment_value_at(keyframes[lo], keyframes[hi], st)
        d = abs(sv - eval_v)
        if d > max_resid:
            max_resid = d

    return len(keyframes), max_resid, keyframes
