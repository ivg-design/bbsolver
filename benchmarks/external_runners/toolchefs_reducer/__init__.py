"""Standalone Python port of Toolchefs Maya keyReducer algorithm.

Original source: https://github.com/toolchefs/keyReducer (LGPL v3 — see
THIRD_PARTY_LICENSE in this directory). The original is a C++ Maya plugin;
this port reproduces the *algorithm* (an iterative greedy RDP variant) in
pure Python so the comparison can run without Maya installed.

The algorithm:
    1. Start with only the first and last sample kept.
    2. Find the source sample whose perpendicular distance from the line
       between its two flanking kept samples is maximal.
    3. If that maximal distance exceeds the tolerance, insert that sample
       into the kept set.
    4. Repeat until no sample deviates more than the tolerance.

This is a classic Ramer-Douglas-Peucker variant; the output is a set of
linearly-interpolated anchors (no bezier handles). Toolchefs's Maya plugin
restores tangents from the source curve in a post-pass, but for the
comparison metric (max residual against source) only the anchor set
matters.
"""
from .runner import reduce_channel

__all__ = ["reduce_channel"]
