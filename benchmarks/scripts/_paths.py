"""Path-resolution helper shared by the reproducibility scripts.

The scripts in this directory drive comparisons against external bbsolver /
Blender installations and against the bundled benchmark corpus. The helper
below resolves each external dependency through a documented search order so
that a fresh clone of the repository can run the scripts without touching
hardcoded paths.

Search order for each resolved item:

  1. The CLI argument or environment variable named below (if set).
  2. A platform-appropriate default discovered via `shutil.which()` or a
     well-known install location.
  3. The author's local development path (last-resort fallback so the script
     keeps working on the author's machine without configuration).

If resolution fails the helper raises ``FileNotFoundError`` with a single,
copy-paste-ready hint telling the operator exactly which env var to set.

Environment variables consulted (override anything via CLI when available):

  BBSOLVER_BINARY        path to the bbsolver executable
  BBSOLVER_BENCHMARK_CORPUS  root containing shipped benchmark request_id dirs
                         (defaults to ``benchmarks/corpus`` in this repo)
  BBSOLVER_DEV_LIVE_RUNS extra search root for unshipped development runs
                         (defaults to ``~/github/bbsolver/artifacts/bbsolver
                         /corpus/live_runs`` then
                         ``(historical dev tree)``)
  BLENDER_APP            path to the Blender executable
  FBX_MOCAP_BBSM         path to retarget_full_size.bbsm.json (overrides the
                         bundled location under benchmarks/fbx_mocap_…/…)
"""
from __future__ import annotations

import os
import shutil
import sys
from pathlib import Path
from typing import Iterable


SCRIPTS_DIR = Path(__file__).resolve().parent
BENCHMARKS_ROOT = SCRIPTS_DIR.parent  # benchmarks/
REPO_ROOT = BENCHMARKS_ROOT.parent  # bbsolver repo root


# ---------------------------------------------------------------------------
# bbsolver binary
# ---------------------------------------------------------------------------

def resolve_bbsolver_binary(override: str | None = None) -> str:
    """Locate the bbsolver executable.

    Order: ``override`` argument → ``$BBSOLVER_BINARY`` → ``shutil.which()``
    → ``~/.bbsolver/bin/bbsolver`` (author install) → ``./build/bbsolver``
    (in-tree build).
    """
    candidates: list[Path] = []
    if override:
        candidates.append(Path(override))
    env = os.environ.get("BBSOLVER_BINARY")
    if env:
        candidates.append(Path(env))
    which = shutil.which("bbsolver")
    if which:
        candidates.append(Path(which))
    candidates.extend([
        Path.home() / ".bbsolver/bin/bbsolver",
        REPO_ROOT / "build/bbsolver",
        REPO_ROOT / "build/Release/bbsolver.exe",
    ])
    for c in candidates:
        if c.is_file() and os.access(c, os.X_OK):
            return str(c)
    raise FileNotFoundError(
        "Could not locate the bbsolver executable. Set $BBSOLVER_BINARY or "
        "place a build under ./build/bbsolver. Tried: "
        + ", ".join(str(c) for c in candidates)
    )


# ---------------------------------------------------------------------------
# Blender (used by the FBX cross-host comparison)
# ---------------------------------------------------------------------------

def resolve_blender_binary(override: str | None = None) -> str:
    """Locate the Blender CLI binary.

    Order: ``override`` → ``$BLENDER_APP`` → ``shutil.which("blender")`` →
    ``/Applications/Blender.app/Contents/MacOS/Blender`` (macOS default) →
    ``C:\\Program Files\\Blender Foundation\\Blender 4.5\\blender.exe``
    (Windows default).
    """
    candidates: list[Path] = []
    if override:
        candidates.append(Path(override))
    env = os.environ.get("BLENDER_APP")
    if env:
        candidates.append(Path(env))
    which = shutil.which("blender")
    if which:
        candidates.append(Path(which))
    candidates.extend([
        Path("/Applications/Blender.app/Contents/MacOS/Blender"),
        Path("C:/Program Files/Blender Foundation/Blender 4.5/blender.exe"),
    ])
    for c in candidates:
        if c.is_file() and os.access(c, os.X_OK):
            return str(c)
    raise FileNotFoundError(
        "Could not locate Blender. Set $BLENDER_APP or install Blender at "
        "the default OS location. Tried: " + ", ".join(str(c) for c in candidates)
    )


# ---------------------------------------------------------------------------
# Live-run corpora (shipped paper_corpus + author dev trees)
# ---------------------------------------------------------------------------

def _candidate_live_run_roots() -> Iterable[Path]:
    env = os.environ.get("BBSOLVER_BENCHMARK_CORPUS")
    if env:
        yield Path(env)
    # Backwards-compat env var name from an earlier layout.
    env_legacy = os.environ.get("BBSOLVER_PAPER_CORPUS")
    if env_legacy:
        yield Path(env_legacy)
    yield BENCHMARKS_ROOT / "corpus"
    env_dev = os.environ.get("BBSOLVER_DEV_LIVE_RUNS")
    if env_dev:
        yield Path(env_dev)
    yield Path.home() / "github/bbsolver/artifacts/bbsolver/corpus/live_runs"
    yield Path.home() / "github/bbsolver/artifacts/bbsolver/corpus/live_runs.alt"


def resolve_request_dir(req_id: str) -> str:
    """Locate a request_id directory across the shipped + author corpora.

    The shipped ``corpus`` wins when present so that a fresh clone
    runs purely against public artifacts.
    """
    candidates: list[Path] = []
    for root in _candidate_live_run_roots():
        candidates.append(root / req_id)
        # bbsolver dev corpus already contains <root>/<req_id>/ — same shape.
    for d in candidates:
        if (d / f"{req_id}_g1.bbky.json").exists():
            return str(d)
    raise FileNotFoundError(
        f"Could not locate request_id {req_id!r}. Tried: "
        + ", ".join(str(c) for c in candidates)
        + ". Set $BBSOLVER_PAPER_CORPUS to the live_runs/ root."
    )


# ---------------------------------------------------------------------------
# Fixture-specific files
# ---------------------------------------------------------------------------

def resolve_fbx_mocap_bbsm(override: str | None = None) -> str:
    """Locate the FBX mocap SampleBundle used by §5.3 cross-host validation."""
    candidates: list[Path] = []
    if override:
        candidates.append(Path(override))
    env = os.environ.get("FBX_MOCAP_BBSM")
    if env:
        candidates.append(Path(env))
    candidates.extend([
        BENCHMARKS_ROOT / "fbx_mocap_retarget_full_size/pose_sampled_blender_action/retarget_full_size.bbsm.json",
        Path.home() / "github/bbsolver/benchmarks/fbx_mocap_retarget_full_size/pose_sampled_blender_action/retarget_full_size.bbsm.json",
    ])
    for c in candidates:
        if c.is_file():
            return str(c)
    raise FileNotFoundError(
        "Could not locate retarget_full_size.bbsm.json. Set $FBX_MOCAP_BBSM "
        "or check out the bbsolver repo with the fbx_mocap_retarget_full_size "
        "fixture intact. Tried: " + ", ".join(str(c) for c in candidates)
    )


if __name__ == "__main__":
    # Print resolved paths for diagnostic purposes.
    print("scripts dir:", SCRIPTS_DIR)
    print("benchmarks root:", BENCHMARKS_ROOT)
    print("repo root:", REPO_ROOT)
    try:
        print("bbsolver:", resolve_bbsolver_binary())
    except FileNotFoundError as e:
        print("bbsolver:", e, file=sys.stderr)
    try:
        print("blender:", resolve_blender_binary())
    except FileNotFoundError as e:
        print("blender:", e, file=sys.stderr)
    try:
        print("fbx mocap bbsm:", resolve_fbx_mocap_bbsm())
    except FileNotFoundError as e:
        print("fbx mocap bbsm:", e, file=sys.stderr)
