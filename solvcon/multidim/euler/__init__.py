# Copyright (c) 2026, solvcon team <contact@solvcon.net>
# BSD 3-Clause License, see COPYING


"""
Multi-dimensional Euler-equation solver; currently the mesh builder,
boundary tagging, shock relations, and solver driver for the oblique-shock
reflection.
"""

from . import oblique

__all__ = [
    'oblique',
]

# vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4:
