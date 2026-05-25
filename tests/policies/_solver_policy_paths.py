#!/usr/bin/env python3
"""Shared path discovery for solver-local policy checks."""

from __future__ import annotations

from pathlib import Path


def _looks_like_monorepo_root(path: Path) -> bool:
    return (
        (path / "tests" / "solver_layout_policy.py").is_file()
        or (path / "docs" / "project").is_dir()
        or (path / ".vscode" / "settings.json").is_file()
    )


def find_solver_layout(anchor: str | Path) -> tuple[Path, Path]:
    """Return (repository_root, solver_root) for monorepo or standalone use."""
    parents = Path(anchor).resolve().parents
    for parent in parents:
        monorepo_solver = parent / "solver"
        if (
            (monorepo_solver / "CMakeLists.txt").is_file()
            and _looks_like_monorepo_root(parent)
        ):
            return parent, monorepo_solver
    for parent in parents:
        if (
            (parent / "CMakeLists.txt").is_file()
            and (parent / "include" / "bbsolver").is_dir()
            and (parent / "src").is_dir()
        ):
            return parent, parent
    raise RuntimeError("Could not locate bbsolver repository root")


def solver_path(solver_root: Path, relative: str | Path) -> Path:
    """Resolve a historical solver/... relative path under solver_root."""
    rel = Path(relative)
    if rel.parts and rel.parts[0] == "solver":
        rel = Path(*rel.parts[1:])
    return solver_root / rel
