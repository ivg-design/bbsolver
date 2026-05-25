#!/usr/bin/env python3
"""Public-header dependency policy for the bbsolver layout.

Public headers under `solver/include/bbsolver/` should include other public
headers through `bbsolver/...` paths. A short grandfather list remains while
core private headers in flat solver/src/ are still
flat in `solver/src/`; this policy makes those exceptions explicit and blocks
new public-to-private dependencies. Past migrations removed
`path_frame_fit.hpp` from the grandfather list by moving it into
`solver/include/bbsolver/path/frame_fit/`;
the metrics layout migration removes `error_metrics.hpp` by moving it into
`solver/include/bbsolver/metrics/`.
"""

from __future__ import annotations

import re
from pathlib import Path

from _solver_policy_paths import find_solver_layout


ROOT, SOLVER = find_solver_layout(__file__)
SRC_ROOT = SOLVER / "src"
PUBLIC_ROOT = SOLVER / "include" / "bbsolver"

INCLUDE_RE = re.compile(r'^\s*#\s*include\s+"([^"]+)"')

# These are existing layout debt, not a precedent. Remove entries as the
# referenced headers move into `solver/include/bbsolver/<area>/`.
#
# Slice 90 (this slice) moved the entire dp/* family to
# solver/include/bbsolver/dp/, dropping 24 prior dp_placer.hpp grandfather
# entries that spanned fit, motion_smooth, path/multimode, progress,
# replacement_temporal, and solve. The remaining 6 entries below cover
# the path/bridge_prune surface's transient path_vertex_reduction.hpp
# dependency (Slice 88), to be cleaned by a future vertex-reduction
# migration.
GRANDFATHERED_PRIVATE_INCLUDES: dict[str, set[str]] = {}


def _private_flat_headers() -> set[str]:
    return {path.name for path in SRC_ROOT.glob("*.hpp")}


def _direct_private_includes() -> dict[str, set[str]]:
    private_headers = _private_flat_headers()
    findings: dict[str, set[str]] = {}
    for header in sorted(PUBLIC_ROOT.rglob("*.hpp")):
        rel = header.relative_to(ROOT).as_posix()
        for line in header.read_text(encoding="utf-8").splitlines():
            match = INCLUDE_RE.match(line)
            if not match:
                continue
            include = match.group(1)
            if include in private_headers:
                findings.setdefault(rel, set()).add(include)
    return findings


def test_public_headers_do_not_add_private_flat_source_dependencies() -> None:
    actual = _direct_private_includes()
    assert actual == GRANDFATHERED_PRIVATE_INCLUDES, (
        "Public headers must not add direct includes of flat private "
        "`solver/src/*.hpp` headers. Move the dependency to a public "
        "`bbsolver/<area>/...` header or, for an intentional temporary "
        "exception, update GRANDFATHERED_PRIVATE_INCLUDES with the owning "
        "migration rationale.\n"
        f"Expected: {GRANDFATHERED_PRIVATE_INCLUDES}\n"
        f"Actual: {actual}"
    )


def test_grandfathered_private_headers_still_exist() -> None:
    private_headers = _private_flat_headers()
    missing = sorted(
        include
        for includes in GRANDFATHERED_PRIVATE_INCLUDES.values()
        for include in includes
        if include not in private_headers
    )
    assert not missing, (
        "Grandfathered public-header exceptions must be removed when their "
        "private flat headers move out of solver/src/. Missing headers:\n  "
        + "\n  ".join(missing)
    )


def main() -> int:
    tests = [
        test_public_headers_do_not_add_private_flat_source_dependencies,
        test_grandfathered_private_headers_still_exist,
    ]
    for test in tests:
        test()
        print(f"[PASS] {test.__name__}")
    print(f"summary: {len(tests)} passed, 0 failed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
