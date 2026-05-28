"""Standalone port of robertjoosten's maya-keyframe-reduction algorithm.

Original source: https://github.com/robertjoosten/maya-keyframe-reduction
License: see THIRD_PARTY_LICENSE in this directory.

The original is a Maya plugin (Python + maya.cmds) that ports Paper.js's
Schneider-style cubic-Bezier fitting algorithm to animation curves. This
port strips the maya.cmds and OpenMaya dependencies so the algorithm can
run against a bbsm.json sample stream without Maya installed.

Public API:
    fit_bezier(points, error) -> list of Keyframe
    reduce_channel(samples_t, samples_v, error) -> (kept_keys, max_residual)
"""
from .fit import fit_bezier
from .runner import reduce_channel

__all__ = ["fit_bezier", "reduce_channel"]
