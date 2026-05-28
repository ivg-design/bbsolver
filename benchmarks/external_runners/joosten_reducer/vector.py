"""Pure-Python 2D vector replacing the maya.OpenMaya.MVector dependency in
the original robertjoosten/maya-keyframe-reduction code. The original
extended MVector; we provide the subset of operations the algorithm uses:
addition, subtraction, scalar multiplication, dot product (* between
vectors), length, normalisation, distanceBetween.
"""
from __future__ import annotations

import math


class Vector2D:
    __slots__ = ("x", "y")

    def __init__(self, x, y=None):
        if y is None:
            # Construct from another Vector2D
            self.x = float(x.x)
            self.y = float(x.y)
        else:
            self.x = float(x)
            self.y = float(y)

    def __repr__(self):
        return f"Vector2D({self.x}, {self.y})"

    def __add__(self, other):
        return Vector2D(self.x + other.x, self.y + other.y)

    def __sub__(self, other):
        return Vector2D(self.x - other.x, self.y - other.y)

    def __mul__(self, other):
        """Vector * scalar OR vector dot vector (matches Paper.js convention)."""
        if isinstance(other, Vector2D):
            return self.x * other.x + self.y * other.y
        return Vector2D(self.x * other, self.y * other)

    def __rmul__(self, other):
        return self.__mul__(other)

    def __truediv__(self, other):
        return Vector2D(self.x / other, self.y / other)

    def __neg__(self):
        return Vector2D(-self.x, -self.y)

    def length(self):
        return math.hypot(self.x, self.y)

    def normal(self):
        n = self.length()
        if n < 1e-12:
            return Vector2D(0.0, 0.0)
        return Vector2D(self.x / n, self.y / n)

    def distanceBetween(self, other):
        return (self - other).length()
