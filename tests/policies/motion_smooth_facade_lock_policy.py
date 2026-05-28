#!/usr/bin/env python3
"""Motion Smooth façade lock policy.

This policy is the structural guard for the three façade-style splits
delivered by the motion-smooth refactor lane:

* `motion_smooth_shape_loop.hpp` façade re-exports four
  sub-headers (curve / adaptive / schedule / tangent-lock). The body
  TU `motion_smooth_shape_loop.cpp` shrank from 472 → 29 LOC.
* `motion_smooth_shape_schedule.hpp` façade re-exports
  three sub-headers (source_key_schedule / trajectory_smooth /
  rove_schedule). The body TU `motion_smooth_shape_schedule.cpp` was
  fully extracted and deleted.
* `motion_smooth_solver.hpp` façade re-exports four
  sub-headers (sample_points / bezier_ease / endpoint_keys /
  spatial_trajectory). The body TU `motion_smooth_solver.cpp` was
  fully extracted and deleted.

The cross-TU dedupe in `motion_smooth_shape_quality.cpp` (which
removed byte-identical duplicates of `ShapeFlatVertexPoint` and
`PointTurnDeg` so the public promotion would not collide at link
time) is also locked here.

Each check is a pure source-text read; the policy makes no subprocess
calls, mutates no git state, and runs in well under a second.

A regression that would silently break the build at integration time
— resurrecting a deleted.cpp file, dropping a façade re-include,
re-introducing a duplicate definition — will trip this policy first
so the failure surfaces at the standard quick-tier guard rather than
during a downstream merge.
"""

from __future__ import annotations

from pathlib import Path

from _solver_policy_paths import find_solver_layout, solver_path


ROOT, SOLVER = find_solver_layout(__file__)
# Post-motion-smooth-layout-migration paths. Headers live under the public
# include root, implementation files live under the area-rooted src tree. The
# `SRC` legacy constant is retained for the pre-existing leg checks that scan
# the broader `solver/src` tree (non-motion-smooth headers like `domain.hpp`
# and the pre-migration `motion_smooth_geometry.cpp` style locations), but
# every motion-smooth file lookup must go through `_ms_path()` so the policy
# resolves the correct post-migration location.
SRC = SOLVER / "src"
HEADER_DIR = SOLVER / "include" / "bbsolver" / "motion_smooth"
SRC_DIR = SOLVER / "src" / "motion_smooth"


def _ms_path(filename: str) -> Path:
    """Return the on-disk path for a motion_smooth file name.

    Headers live under `solver/include/bbsolver/motion_smooth/`; sources
    live under `solver/src/motion_smooth/`. This helper centralizes the
    extension-based routing so the rest of the policy does not need to
    know about the public/private split.
    """
    if filename.endswith(".hpp"):
        return HEADER_DIR / filename
    return SRC_DIR / filename


# ---------------------------------------------------------------------------
# Façade contracts. Each façade is keyed by its header path; the value lists
# the sibling sub-headers that MUST appear inside `#include "..."` directives
# in that header. Order is not checked (only presence) so future re-orderings
# do not trip the policy.
# ---------------------------------------------------------------------------

FACADE_INCLUDES = {
    # façade. NOTE: motion_smooth_shape_loop_curve.hpp is
    # intentionally NOT included here. Its symbols
    # (MotionSmoothCatmullRomValue / ShapeFlatVertexPoint / PointTurnDeg /
    # EvaluateClosedLoopShapeAtParam) were file-local in the original
    # motion_smooth_shape_loop.cpp's anonymous namespace and had no
    # pre-split caller. After, curve.hpp is consumed directly by
    # motion_smooth_shape_loop_adaptive.cpp and motion_smooth_shape_quality.cpp
    # (the dedupe site). Re-exporting it through the façade would
    # expand the public surface beyond what existed pre-refactor.
    "motion_smooth_shape_loop.hpp": [
        "motion_smooth_shape_loop_adaptive.hpp",
        "motion_smooth_shape_loop_schedule.hpp",
        "motion_smooth_shape_quality.hpp",
        "motion_smooth_shape_tangent_lock.hpp",
    ],
    "motion_smooth_shape_schedule.hpp": [
        "motion_smooth_shape_rove_schedule.hpp",
        "motion_smooth_shape_source_key_schedule.hpp",
        "motion_smooth_shape_trajectory_smooth.hpp",
    ],
    "motion_smooth_solver.hpp": [
        "motion_smooth_bezier_ease.hpp",
        "motion_smooth_endpoint_keys.hpp",
        "motion_smooth_sample_points.hpp",
        "motion_smooth_spatial_trajectory.hpp",
    ],
}

# Façades that are pure re-export shims (no residual declarations).
# motion_smooth_shape_loop.hpp is NOT in this set because it still
# carries the lone `EvenTimesForValueCount` declaration — the one
# public helper that did not fit any of the four the corresponding sub-modules cohesive cuts
# and remained at the façade alongside the re-includes.
PURE_SHIM_FACADES = (
    "motion_smooth_shape_schedule.hpp",
    "motion_smooth_solver.hpp",
)


# ---------------------------------------------------------------------------
# Each MS sub-module.hpp must declare the symbols listed here. The check
# is a substring scan (after stripping `//` line comments) so future
# whitespace / formatting tweaks do not trip the policy; the symbol must
# appear in code, not in a doc comment.
# ---------------------------------------------------------------------------

SUBMODULE_SYMBOLS = {
    # the corresponding sub-modules closed-loop sampler.
    "motion_smooth_shape_tangent_lock.hpp": [
        "struct ShapeTangentLockStats",
        "LockShapeFlatRotationalTangents",
        "LockShapeFlatRotationalTangentsExcept",
    ],
    "motion_smooth_shape_loop_curve.hpp": [
        "MotionSmoothCatmullRomValue",
        "ShapeFlatVertexPoint",
        "PointTurnDeg",
        "EvaluateClosedLoopShapeAtParam",
    ],
    "motion_smooth_shape_loop_adaptive.hpp": [
        "struct AdaptiveClosedLoopShapeSamples",
        "BuildAdaptiveClosedLoopShapeSamples",
    ],
    "motion_smooth_shape_loop_schedule.hpp": [
        "struct SourcePoseIntervalTimeSchedule",
        "TimesForClosedLoopParams",
        "TimesForClosedLoopParamsByIntervalTravel",
    ],
    # the corresponding sub-modules anchor schedule.
    "motion_smooth_shape_source_key_schedule.hpp": [
        "struct ShapeMotionSourceKeySchedule",
        "ShapeMotionSourceKeyRdpKeep",
        "BuildShapeMotionSourceKeySchedule",
    ],
    "motion_smooth_shape_trajectory_smooth.hpp": [
        "struct ShapeMotionTrajectorySmoothResult",
        "BuildShapeMotionTrajectorySmoothValues",
    ],
    "motion_smooth_shape_rove_schedule.hpp": [
        "struct ShapeMotionRoveSchedule",
        "BuildShapeMotionRoveScheduleFromValues",
    ],
    # the corresponding sub-modules solver orchestrator.
    "motion_smooth_sample_points.hpp": [
        "IsMotionSmoothSpatialProperty",
        "SegmentEndpointValueOrSample",
        "MotionSmoothSourceKeyTimes",
        "MotionSmoothRawPoints",
        "MotionSmoothInterpolatedVector",
    ],
    "motion_smooth_bezier_ease.hpp": [
        "ApplyMotionSmoothBezierEase",
    ],
    "motion_smooth_endpoint_keys.hpp": [
        "MotionSmoothEndpointKeys",
    ],
    "motion_smooth_spatial_trajectory.hpp": [
        "MotionSmoothSpatialTrajectoryKeys",
    ],
    # the shape-flat extraction sub-modules shape_flat orchestrator helpers. These are
    # orchestrator-internal (not re-exported via motion_smooth_shape_flat.hpp)
    # but exposed as their own headers so tests can exercise them and so
    # the orchestrator stays a thin control-flow function. They are NOT
    # added to FACADE_INCLUDES because motion_smooth_shape_flat.hpp is a
    # single-function declaration header, not a façade in the the corresponding sub-modules
    # sense.
    "motion_smooth_shape_flat_notes.hpp": [
        "struct MotionSmoothShapeFlatNotesInputs",
        "BuildMotionSmoothShapeFlatNotes",
    ],
    "motion_smooth_shape_flat_topology_gate.hpp": [
        "struct MotionSmoothShapeFlatTopologyGateResult",
        "ValidateMotionSmoothShapeFlatTopology",
    ],
    "motion_smooth_shape_flat_closed_loop.hpp": [
        "struct ClosedLoopAdaptiveResampleResult",
        "BuildShapeFlatClosedLoopAdaptiveResample",
    ],
    "motion_smooth_shape_flat_key_emission.hpp": [
        "EmitMotionSmoothShapeFlatKeysFromRoveSchedule",
    ],
}


#.cpp files that the  /  extractions deleted in full and that must
# stay deleted. Resurrecting either would cause duplicate-symbol link
# errors against the new sibling TUs.
DELETED_BODIES = (
    "motion_smooth_shape_schedule.cpp",
    "motion_smooth_solver.cpp",
)


# Symbols that the dedupe removed from motion_smooth_shape_quality.cpp.
# The canonical definitions live in motion_smooth_shape_loop_curve.cpp;
# re-introducing them in shape_quality.cpp would re-create the link-time
# collision fixed.
DEDUPED_FROM_SHAPE_QUALITY = (
    "ShapeFlatVertexPoint(",
    "PointTurnDeg(",
)


# Motion-smooth headers that pre-date the façade splits and are
# neither façades (FACADE_INCLUDES keys) nor MS-extracted sub-modules
# (SUBMODULE_SYMBOLS keys) nor sub-headers re-exported through a façade
# (FACADE_INCLUDES values). They are legacy public headers consumed
# directly by main.cpp / dispatch / reduction-gate. Listed explicitly so
# the orphan-header check can prove every motion_smooth_*.hpp on disk is
# accounted for. Adding a new motion_smooth header without registering
# it in one of the four buckets will trip the orphan check at quick-tier.
PRE_EXISTING_MOTION_SMOOTH_HEADERS = (
    "motion_smooth_dispatch.hpp",
    "motion_smooth_geometry.hpp",
    "motion_smooth_reduction_gate.hpp",
    "motion_smooth_shape_flat.hpp",
)


# Parallel allowlist for motion_smooth.cpp bodies that pre-date the
# the corresponding sub-modules extractions and are therefore not in MS_EXTRACTED_CPPS.
# motion_smooth_shape_flat.cpp / motion_smooth_shape_loop.cpp are NOT
# listed here: the shape-flat extraction sub-modules / the corresponding sub-modules extracted them, so they belong to
# MS_EXTRACTED_CPPS (shrunk shape_flat orchestrator and shape_loop
# façade body respectively). Listed here so the orphan-body check can
# prove every motion_smooth_*.cpp on disk is accounted for.
PRE_EXISTING_MOTION_SMOOTH_BODIES = (
    "motion_smooth_dispatch.cpp",
    "motion_smooth_geometry.cpp",
    "motion_smooth_reduction_gate.cpp",
    "motion_smooth_shape_quality.cpp",
)


# Anti-monolith LOC ceilings for MS-extracted files. Set generously
# above current observed sizes (current maxima: façade header 26,
# sub-header 56, MS body 183) so genuine growth doesn't trip the
# check, but tight enough that a sub-module regressing toward the
# original monolith dimensions (motion_smooth_shape_schedule.cpp was
# 382 LOC, motion_smooth_solver.cpp 360 LOC, motion_smooth_shape_loop.cpp
# 472 LOC before the split-) will fire before re-monolithization is far
# enough along to be hard to unwind.
#
# Pre-existing legacy files are out of scope: reduction_gate.cpp (350)
# and shape_quality.cpp (331) live at sizes the MS lane was not
# chartered to police.
MONOLITH_CEILING_FACADE_HEADER_LOC = 50
MONOLITH_CEILING_SUB_HEADER_LOC = 100
MONOLITH_CEILING_SUB_BODY_LOC = 300


# Stem prefixes of headers that represent orchestration subsystems
# motion_smooth must NOT acquire as dependencies. Motion-smooth is a
# downstream consumer of the solver pipeline: it produces samples,
# values, schedules and trajectories that the orchestrator (main.cpp,
# fallback_property_solver, post-processing) feeds through reporting,
# parallel-runtime, replacement, refit and similar cross-cutting
# subsystems. A motion_smooth_*.{hpp,cpp} acquiring a reverse-direction
# dependency on one of these would be an architectural smell that the
# MS lane was explicitly designed to prevent.
#
# motion_smooth_reduction_gate.cpp's existing solver_reporting.hpp
# include pre-dates and is grandfathered via the explicit map
# GRANDFATHERED_ORCHESTRATION_INCLUDES below. The wider lock
# scans every motion_smooth_*.{hpp,cpp} on disk and only the listed
# (file → include) pairs are permitted; anything else trips the check.
FORBIDDEN_ORCHESTRATION_STEM_PREFIXES = (
    "solver_reporting",
    "solve_lifecycle_reporting",
    "solve_parallel_runtime_scope",
    "path_replacement",
    "temporal_refit",
    "solve_property_post_processing",
    "fallback_property_solver",
)


# STL identifier → canonical header self-sufficiency map. Each entry
# is (identifier_substring, required_include_directive). The
# substring scan runs on line-comment-stripped source text; angle-
# bracket and quoted forms of the include both count. The set is
# intentionally conservative — only identifiers used by some
# motion_smooth file today, plus the canonical numeric/utility
# staples that would be obvious next-touch points.
#
# Applied uniformly to MS-extracted façade headers, sub-headers, and
# bodies so a sub-header that compiles only via transitive
# includes from its consumer is caught at policy time.
STL_SELF_SUFFICIENCY = (
    ("std::vector", "<vector>"),
    ("std::optional", "<optional>"),
    ("std::string", "<string>"),
    ("std::array", "<array>"),
    ("std::function", "<functional>"),
    ("std::variant", "<variant>"),
    ("std::pair", "<utility>"),
    ("std::tuple", "<tuple>"),
    ("std::map", "<map>"),
    ("std::unordered_map", "<unordered_map>"),
    ("std::set", "<set>"),
    ("std::span", "<span>"),
    ("std::size_t", "<cstddef>"),
)


# Explicit allowlist for pre-existing orchestration dependencies that
# pre-date and that the MS lane is not authorised to rewrite this
# round. Each key is a motion_smooth_*.{hpp,cpp} filename; each value
# is the set of orchestration headers (matching one of
# FORBIDDEN_ORCHESTRATION_STEM_PREFIXES) the file is permitted to
# `#include`. Reverse leg: every (file, include) entry here must
# actually appear in the source — otherwise the allowance is stale
# and should be deleted so the wider lock catches the next regression.
GRANDFATHERED_ORCHESTRATION_INCLUDES = {
    "motion_smooth_reduction_gate.cpp": frozenset({
        "solver_reporting.hpp",
    }),
}


def _strip_line_comments(text: str) -> str:
    """Return `text` with each line's `//` comment trailer removed."""
    lines = []
    for raw_line in text.splitlines():
        lines.append(raw_line.split("//", 1)[0])
    return "\n".join(lines)


def _read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def test_facades_reexport_their_subheaders() -> None:
    """Each façade header must re-include every sub-header listed in
    FACADE_INCLUDES. A regression that drops one of the includes would
    break callers that rely on the façade for symbol resolution
    (motion_smooth_shape_flat.cpp, motion_smooth_dispatch.cpp, main.cpp,
    test_motion_smooth_solver.cpp).
    """
    findings = []
    for facade_name, expected_includes in FACADE_INCLUDES.items():
        facade_path = _ms_path(facade_name)
        if not facade_path.exists():
            findings.append(f"missing façade: {facade_name}")
            continue
        text = _read_text(facade_path)
        for sub_header in expected_includes:
            include_token = f'#include "bbsolver/motion_smooth/{sub_header}"'
            if include_token not in text:
                findings.append(
                    f"{facade_name}: missing `{include_token}`"
                )
    assert not findings, (
        "façade re-export contract violated. The façades must "
        "re-include their sub-headers verbatim so existing consumers "
        "continue to resolve symbols through the façade. Findings:\n  "
        + "\n  ".join(findings)
    )


def test_submodule_headers_declare_expected_symbols() -> None:
    """Each MS sub-module.hpp must declare the symbols listed in
    SUBMODULE_SYMBOLS. The scan is comment-aware (strips `//` lines) so
    a symbol must appear in actual code, not just docstring prose.
    """
    findings = []
    for header_name, expected_symbols in SUBMODULE_SYMBOLS.items():
        header_path = _ms_path(header_name)
        if not header_path.exists():
            findings.append(f"missing sub-module header: {header_name}")
            continue
        text = _strip_line_comments(_read_text(header_path))
        for symbol in expected_symbols:
            if symbol not in text:
                findings.append(
                    f"{header_name}: missing symbol `{symbol}`"
                )
    assert not findings, (
        "MS sub-module headers must declare every expected symbol. "
        "Findings:\n  " + "\n  ".join(findings)
    )


def test_deleted_bodies_stay_deleted() -> None:
    """The.cpp files that and fully extracted into sibling
    modules must stay deleted. Resurrecting either file would cause
    duplicate-symbol link errors against the new / sibling TUs
    that now own the same symbols.

    A `git restore solver/src/motion_smooth_solver.cpp` would
    repopulate the file from HEAD; this policy catches that before
    the build hits ld.
    """
    findings = []
    for body_name in DELETED_BODIES:
        body_path = _ms_path(body_name)
        if body_path.exists():
            findings.append(
                f"{body_name}: must stay deleted (/ extracted "
                "every definition into sibling TUs; a stub copy would "
                "collide at link time)"
            )
    assert not findings, (
        "deleted-body contract violated. Findings:\n  "
        + "\n  ".join(findings)
    )


def test_shape_quality_does_not_redefine_curve_helpers() -> None:
    """The dedupe removed `ShapeFlatVertexPoint` and `PointTurnDeg`
    function definitions from motion_smooth_shape_quality.cpp; the
    canonical definitions now live in motion_smooth_shape_loop_curve.cpp
    (public bbsolver:: surface). A regression that re-introduces either
    function definition in shape_quality.cpp would re-create the
    duplicate-symbol link error fixed.

    The scanner looks for the function-definition pattern (`Name(` at
    line start or with a return type prefix), not for call-site usage.
    motion_smooth_shape_quality.cpp continues to *call* these symbols
    via the curve header — that is the post-dedupe contract.
    """
    shape_quality_path = _ms_path("motion_smooth_shape_quality.cpp")
    assert shape_quality_path.exists(), (
        "motion_smooth_shape_quality.cpp must exist"
    )
    text = _strip_line_comments(_read_text(shape_quality_path))
    findings = []
    for symbol in DEDUPED_FROM_SHAPE_QUALITY:
        # Definition pattern: `std::pair<double, double> ShapeFlatVertexPoint(`
        # or `double PointTurnDeg(`. Both forms start the symbol token at a
        # word boundary preceded by `> ` (template close) or ` ` (return
        # type). A call site like `PointDistance(ShapeFlatVertexPoint(...)` is
        # preceded by `(` not by a return-type token.
        definition_markers = (
            f"> {symbol}",   # std::pair<double, double> Name(
            f"double {symbol}",  # double Name(
            f"int {symbol}",     # int Name(  (defensive — no current return)
            f"void {symbol}",    # void Name(  (defensive — no current return)
        )
        if any(marker in text for marker in definition_markers):
            findings.append(
                f"motion_smooth_shape_quality.cpp redefines `{symbol}` "
                "—  dedupe required these to live only in "
                "motion_smooth_shape_loop_curve.cpp"
            )
    assert not findings, (
        " dedupe contract violated. Findings:\n  "
        + "\n  ".join(findings)
    )


def test_facade_headers_are_thin() -> None:
    """Each MS façade header is small. A regression that adds
    substantive declarations to a façade would re-couple consumers to
    the façade's symbol-table directly, which is the layering the corresponding sub-modules
    explicitly broke.

    Universal heuristic: every façade must be under 50 lines.

    Pure-shim façades (PURE_SHIM_FACADES) additionally must not
    contain a `namespace bbsolver` block — they re-export via
    `#include` only, and sub-headers own every declaration.
    `motion_smooth_shape_loop.hpp` is exempt from the namespace check
    because it still carries the lone `EvenTimesForValueCount`
    declaration that did not fit any the corresponding sub-modules cohesive cut.
    """
    findings = []
    line_cap = 50
    for facade_name in FACADE_INCLUDES:
        facade_path = _ms_path(facade_name)
        if not facade_path.exists():
            # Already reported by test_facades_reexport_their_subheaders.
            continue
        text = _read_text(facade_path)
        line_count = len(text.splitlines())
        if line_count > line_cap:
            findings.append(
                f"{facade_name}: {line_count} lines exceeds façade cap "
                f"({line_cap}). Façades must stay compact"
            )
        if (
            facade_name in PURE_SHIM_FACADES
            and "namespace bbsolver" in text
        ):
            findings.append(
                f"{facade_name}: pure-shim façade must not contain "
                "`namespace bbsolver` block — re-export via #include only"
            )
    assert not findings, (
        "façade-thinness contract violated. Findings:\n  "
        + "\n  ".join(findings)
    )


def test_motion_smooth_sources_not_excluded_from_cmake_glob() -> None:
    """ CMake source-list completeness lock.

    solver/CMakeLists.txt registers bbsolver_core via
    `file(GLOB_RECURSE... CONFIGURE_DEPENDS "src/*.cpp")` followed by zero or
    more `list(FILTER BBSOLVER_CORE_SOURCES EXCLUDE REGEX "...")`
    directives. The GLOB picks up every motion_smooth_*.cpp; the
    EXCLUDE patterns subtract entries from that list. Today only
    `/main\\.cpp$` is excluded — clean.

    A future refactor that broadened EXCLUDE (for example
    `EXCLUDE REGEX "motion_smooth_shape_flat"` to "temporarily disable
    the shape_flat orchestrator while migrating") would silently drop
    every motion_smooth file matching that pattern. The build would
    still succeed (no source → no compiled symbols → no compile error)
    but linkers consuming the library would fail with unresolved
    motion_smooth symbol errors — diagnosed away from the cause.

    This check reads solver/CMakeLists.txt, extracts every EXCLUDE
    REGEX pattern, and verifies no current motion_smooth_*.cpp file
    matches. CMake uses POSIX-ish regex syntax that is close enough to
    Python's `re` for substring/anchor patterns; complex character
    classes are not in scope.

    Note: this check is intentionally narrow. It does NOT verify the
    GLOB pattern itself (a future refactor that removed the GLOB
    entirely and replaced it with an explicit source list would
    require its own dedicated check). It catches the most common drift
    — over-broad EXCLUDE — surface-side.
    """
    import re
    cmake_path = SOLVER / "CMakeLists.txt"
    assert cmake_path.exists(), (
        "solver/CMakeLists.txt must exist for source-list validation"
    )
    cmake_text = cmake_path.read_text(encoding="utf-8")
    # Extract every `list(FILTER BBSOLVER_CORE_SOURCES EXCLUDE REGEX "<pattern>")`.
    exclude_re = re.compile(
        r'list\s*\(\s*FILTER\s+BBSOLVER_CORE_SOURCES\s+EXCLUDE\s+REGEX\s+'
        r'"((?:[^"\\]|\\.)*)"',
        re.IGNORECASE,
    )
    exclude_patterns = exclude_re.findall(cmake_text)

    # The current state has exactly one EXCLUDE (`/main\.cpp$`). If a
    # future refactor removes it entirely the build will simply add
    # main.cpp to bbsolver_core — annoying but not a motion_smooth
    # regression. We only flag entries that would match motion_smooth.
    motion_smooth_sources = sorted(SRC_DIR.glob("motion_smooth_*.cpp"))
    findings = []
    for pattern in exclude_patterns:
        # CMake regex uses `\\.` for a literal dot (escaped backslash
        # in a C-string); the captured group already unescapes that
        # to `\.`, which Python re reads as a literal dot — semantics
        # match.
        try:
            compiled = re.compile(pattern)
        except re.error as exc:
            findings.append(
                f'cannot compile EXCLUDE REGEX pattern `{pattern}` '
                f'(check syntax compatibility): {exc}'
            )
            continue
        for cpp_path in motion_smooth_sources:
            # CMake regex matches against the full path string. The
            # `file(GLOB_RECURSE...)` produces absolute paths, so we match
            # against the absolute path; an EXCLUDE anchored on
            # `/main\.cpp$` would NOT match
            # `solver/src/motion_smooth_*.cpp` because of the
            # filename mismatch.
            full_path_str = str(cpp_path)
            if compiled.search(full_path_str):
                findings.append(
                    f'CMake EXCLUDE REGEX `{pattern}` matches motion_smooth '
                    f'source `{cpp_path.relative_to(ROOT)}` — that file '
                    'would be silently dropped from bbsolver_core. '
                    'Tighten the EXCLUDE pattern to avoid motion_smooth '
                    'files.'
                )
    assert not findings, (
        'CMake source-list completeness contract violated. Findings:\n  '
        + '\n  '.join(findings)
    )


def test_motion_smooth_policy_file_registers_all_defined_checks() -> None:
    """ meta-policy integrity lock.

    The policy file defines policy checks as `def test_*` functions
    and dispatches them from `main()`'s `tests = [...]` runner list.
    Without this check, a future contributor could:
      * Add a new `def test_xxx(...)` function
      * Forget to add `test_xxx,` to the `tests` list in `main()`

    Result: the new check would never run. The file still parses and
    `summary: N passed, 0 failed` still reports success — silently
    masking a missing policy lock.

    This check reads the policy file's own source text, extracts
    every `def test_*` definition name, and asserts each is
    referenced in the `tests = [...]` list inside `main()`. It is
    intentionally bidirectional: it also verifies every entry in the
    `tests` list corresponds to a defined function (catching dangling
    references after a rename).

    This is the only policy check that reads its own host file.
    Implementation note: it parses source text directly rather than
    using `inspect`/`globals()` so it can be reasoned about purely as
    a text-level integrity contract.
    """
    import re
    self_path = SOLVER / "tests" / "policies" / "motion_smooth_facade_lock_policy.py"
    assert self_path.exists(), (
        "solver/tests/policies/motion_smooth_facade_lock_policy.py must exist"
    )
    text = self_path.read_text(encoding="utf-8")

    # Find every top-level `def test_NAME(` (not nested inside class).
    # The MS policy doesn't use classes; top-level matches all checks.
    defined_pattern = re.compile(r'^def (test_\w+)\s*\(', re.MULTILINE)
    defined_names = set(defined_pattern.findall(text))

    # Scan main()'s entire body for `test_*` identifier references.
    # The runner pattern is `for test in tests: test()`, so every
    # registered check must appear by name inside main() — whether in
    # the `tests = [...]` literal or any inline reference.
    #
    # Locate main()'s definition via the top-level `\ndef main(` token
    # (NOT a bare `find("def main()")` — that would match a string
    # literal of the same text inside *this* check's own body and
    # land in the wrong place). The `\n` anchor restricts matches to
    # column-0 declarations.
    main_decl_match = re.search(r'^def main\s*\(', text, re.MULTILINE)
    assert main_decl_match is not None, (
        "motion_smooth_facade_lock_policy.py must define a top-level main() runner"
    )
    main_start = main_decl_match.start()
    # Bound main() at the next top-level `def` or the if-__name__ guard.
    bound_candidates = [
        m.start() for m in re.finditer(
            r'^(def |if __name__)', text[main_start + 1:], re.MULTILINE
        )
    ]
    if bound_candidates:
        main_end = main_start + 1 + bound_candidates[0]
    else:
        main_end = len(text)
    main_body = text[main_start:main_end]
    registered_names = set(re.findall(r'\btest_\w+\b', main_body))

    findings = []
    missing_registrations = sorted(defined_names - registered_names)
    for name in missing_registrations:
        findings.append(
            f"`def {name}` is defined but not registered in "
            "main()'s tests list — the check would never run. "
            "Add `{name},` to the tests list in main()."
.replace("{name}", name)
        )
    dangling_registrations = sorted(registered_names - defined_names)
    for name in dangling_registrations:
        findings.append(
            f"main()'s tests list references `{name}` but no "
            "matching `def test_*` exists — likely a rename or "
            "deletion left a dangling reference."
        )

    assert not findings, (
        "motion_smooth_facade_lock_policy.py internal integrity "
        "contract violated. Findings:\n  " + "\n  ".join(findings)
    )


def test_motion_smooth_facades_have_exact_motion_smooth_include_count() -> None:
    """ exact-count lock for façade re-exports.

     `test_facades_reexport_their_subheaders` verifies that each
    expected sub-header IS included by its façade. But it doesn't
    flag SURPLUS includes: a future refactor could add a 5th
    `#include "bbsolver/motion_smooth/motion_smooth_..."` to `motion_smooth_solver.hpp` (e.g.,
    accidentally re-exporting a new helper) and the check
    would still pass — the original 4 are present.

    This check counts the `#include "bbsolver/motion_smooth/motion_smooth_*"` lines in each
    façade and asserts the count matches `len(FACADE_INCLUDES[name])`
    exactly. Combined with, that means:
      * Every expected sub-header is present ( forward check)
      * No unexpected sub-header is present (this reverse check)

    Bidirectional façade-content lock. The non-motion_smooth includes
    in `motion_smooth_shape_loop.hpp` (`<cstddef>`, `<vector>`) and in
    `motion_smooth_solver.hpp` (none) are exempted automatically
    because the count only inspects motion_smooth_* includes.
    """
    findings = []
    for facade_name, expected_includes in FACADE_INCLUDES.items():
        facade_path = _ms_path(facade_name)
        if not facade_path.exists():
            continue
        text = _read_text(facade_path)
        # Count #include "bbsolver/motion_smooth/motion_smooth_*" lines, excluding any line
        # that contains a // comment marker BEFORE the include token
        # (so commented-out includes don't count).
        actual = 0
        for raw_line in text.splitlines():
            # Strip line-comment trailers but keep the leading code.
            # If `//` precedes the `#include`, the line is itself
            # commented out and shouldn't count.
            stripped = raw_line.split("//", 1)[0]
            if '#include "bbsolver/motion_smooth/motion_smooth_' in stripped:
                actual += 1
        expected_count = len(expected_includes)
        if actual != expected_count:
            findings.append(
                f"{facade_name}: has {actual} motion_smooth_* "
                f"includes but FACADE_INCLUDES expects exactly "
                f"{expected_count}. Either update "
                f"FACADE_INCLUDES[{facade_name!r}] to match the new "
                "façade contract or remove the surplus include."
            )
    assert not findings, (
        "façade exact-count contract violated. Findings:\n  "
        + "\n  ".join(findings)
    )


def test_motion_smooth_policy_registered_in_quick_guard() -> None:
    """ registration-lock for the motion_smooth policy.

     added a one-line entry to `tools/p3_refactor_guard.py`'s
    quick-tier policy list:

        ("motion smooth facade lock policy",
         [_PYTHON, "solver/tests/policies/motion_smooth_facade_lock_policy.py"]),

    This entry makes the motion_smooth policy run as policy 11
    alongside the 10 pre-existing policies inside
    `python3 tools/p3_refactor_guard.py --tier quick --no-build`.

    Without this check, a future refactor of `p3_refactor_guard.py`
    (e.g., reordering the policy list, renaming `_PYTHON`, or a
    blanket-replace that drops the entry) could silently remove the
    registration. The motion_smooth policy would still exist as a
    standalone file but would no longer run in the standard quick
    tier — a regression that wouldn't surface until someone
    manually invoked the standalone runner.

    The check verifies the registration line is present with the
    expected label and target path. It does NOT modify
    `p3_refactor_guard.py` — the brief explicitly permits motion_smooth
    policies to *verify* the existing registration. This is read-only
    source-text inspection.
    """
    guard_path = ROOT / "tools" / "p3_refactor_guard.py"
    if not guard_path.exists():
        # Upstream-only guard registration check. When the package is checked
        # out alongside the development monorepo, this verifies that
        # the motion_smooth policy is listed in the monorepo's aggregate
        # quick-tier policy runner. In a standalone bbsolver checkout the
        # monorepo guard does not exist; skip rather than fail. Solver-owned
        # policies can still be invoked directly via:
        #     python3 solver/tests/policies/motion_smooth_facade_lock_policy.py
        return
    text = _read_text(guard_path)
    expected_label = '("motion smooth facade lock policy",'
    expected_target = '"solver/tests/policies/motion_smooth_facade_lock_policy.py"'
    findings = []
    if expected_label not in text:
        findings.append(
            f"tools/p3_refactor_guard.py: missing registration label "
            f"`{expected_label}` — the motion_smooth policy is no "
            "longer registered in the quick-tier policy list. "
            "Restore the single-line addition or update this "
            "check if the label was intentionally renamed."
        )
    if expected_target not in text:
        findings.append(
            f"tools/p3_refactor_guard.py: missing registration target "
            f"path `{expected_target}` — the registration may exist "
            "with the right label but the policy path was changed. "
            "Verify the file path is still solver/tests/policies/motion_smooth_facade_lock_policy.py."
        )
    assert not findings, (
        "motion_smooth policy registration contract violated. "
        "Findings:\n  " + "\n  ".join(findings)
    )


def test_ms_extracted_cpps_covers_every_submodule_body() -> None:
    """ meta-policy lock: cross-list consistency between
    SUBMODULE_SYMBOLS (the.hpp surface) and MS_EXTRACTED_CPPS (the
.cpp dependency-surface allowlist).

    Each entry in SUBMODULE_SYMBOLS is an MS-extracted.hpp; its
    sibling.cpp is one of the 15 sub-module bodies the lane added.
    MS_EXTRACTED_CPPS additionally lists 2 modified orchestrator
.cpp files (motion_smooth_shape_loop.cpp from,
    motion_smooth_shape_flat.cpp from the shape-flat extraction sub-modules). The union should
    be: {<basename>.cpp for each.hpp in SUBMODULE_SYMBOLS} ∪
    {motion_smooth_shape_loop.cpp, motion_smooth_shape_flat.cpp}.

    Without this check, a future MS lane could:
      * Add a new sub-module.hpp +.cpp pair
      * Add the.hpp to SUBMODULE_SYMBOLS (locking its surface)
      * Forget to add the.cpp to MS_EXTRACTED_CPPS

    Result: the new.cpp would be silently skipped by 
    `test_ms_extracted_cpps_only_include_allowed_non_motion_smooth_headers`,
    so a stray `#include "temporal_refit.hpp"` in the new file would
    not be flagged. This check prevents that drift mode.

    The check is bidirectional: every SUBMODULE_SYMBOLS.hpp must
    have its sibling.cpp in MS_EXTRACTED_CPPS, AND every
    MS_EXTRACTED_CPPS.cpp (except the 2 orchestrator exceptions)
    must have its sibling.hpp in SUBMODULE_SYMBOLS.
    """
    orchestrator_exceptions = frozenset({
        #  — motion_smooth_shape_loop.cpp retains only
        # EvenTimesForValueCount; its.hpp is the façade,
        # not a sub-module header in SUBMODULE_SYMBOLS.
        "motion_smooth_shape_loop.cpp",
        # the shape-flat extraction sub-modules — motion_smooth_shape_flat.cpp is the
        # orchestrator that consumes the 4 the shape-flat extraction sub-modules helpers; its
        #.hpp is a single-function declaration header, not a
        # sub-module header.
        "motion_smooth_shape_flat.cpp",
    })

    expected_cpps_from_submodules = {
        header[:-len(".hpp")] + ".cpp" for header in SUBMODULE_SYMBOLS
    }

    # Forward direction: every SUBMODULE_SYMBOLS.hpp must have its
    # sibling.cpp in MS_EXTRACTED_CPPS.
    findings = []
    for cpp_name in sorted(expected_cpps_from_submodules):
        if cpp_name not in MS_EXTRACTED_CPPS:
            findings.append(
                f"SUBMODULE_SYMBOLS lists `{cpp_name[:-4]}.hpp` but its "
                f"sibling `{cpp_name}` is not in MS_EXTRACTED_CPPS — "
                "the.cpp would be silently skipped by the  "
                "dependency-surface allowlist check. Add it to "
                "MS_EXTRACTED_CPPS."
            )

    # Reverse direction: every MS_EXTRACTED_CPPS entry (except the 2
    # orchestrator exceptions) must have its sibling.hpp in
    # SUBMODULE_SYMBOLS.
    for cpp_name in sorted(MS_EXTRACTED_CPPS):
        if cpp_name in orchestrator_exceptions:
            continue
        if cpp_name not in expected_cpps_from_submodules:
            findings.append(
                f"MS_EXTRACTED_CPPS lists `{cpp_name}` but its "
                f"sibling `{cpp_name[:-4]}.hpp` is not in "
                "SUBMODULE_SYMBOLS — the.hpp public surface is "
                "not locked. Either add the header to "
                "SUBMODULE_SYMBOLS or, if it is an orchestrator "
                "rather than a sub-module, add it to "
                "`orchestrator_exceptions` in this check with a "
                "justification comment."
            )

    assert not findings, (
        "MS extracted-files cross-list consistency contract "
        "violated. Findings:\n  " + "\n  ".join(findings)
    )


def test_motion_smooth_facade_includes_carry_iwyu_keep_pragma() -> None:
    """ IWYU-keep-pragma lock.

    the corresponding sub-modules added `// IWYU pragma: keep` to each re-export include
    in the 3 motion_smooth façade headers. Without the pragma,
    clangd's include-cleaner reports the includes as
    `unused-includes` (because the façade body uses none of the
    imported symbols directly — by design). The pragma tells clangd
    the include is intentional for transitive resolution by
    downstream consumers.

    Without this check, a future editor auto-cleanup or accidental
    blanket-replace could strip the pragma, re-introducing 11 clangd
    diagnostics on the façade surface and silently breaking the
    "clangd-clean motion_smooth surface" guarantee.

    The pragma's exact spelling is locked too: `// IWYU pragma: keep`
    (with single space after `//`, lowercase `pragma`, lowercase
    `keep`). Stripped variations (e.g. `// IWYU: keep`,
    `//IWYU pragma: keep` without leading space) are not recognized
    by clangd's include-cleaner. This check enforces the precise
    spelling.
    """
    iwyu_marker = "// IWYU pragma: keep"
    findings = []
    for facade_name, expected_includes in FACADE_INCLUDES.items():
        facade_path = _ms_path(facade_name)
        if not facade_path.exists():
            # Already reported by test_facades_reexport_their_subheaders.
            continue
        text = _read_text(facade_path)
        for include_name in expected_includes:
            include_token = f'#include "bbsolver/motion_smooth/{include_name}"'
            found_with_pragma = False
            found_at_all = False
            for line in text.splitlines():
                if include_token in line:
                    found_at_all = True
                    if iwyu_marker in line:
                        found_with_pragma = True
                        break
            if not found_at_all:
                # Already reported by test_facades_reexport_their_subheaders.
                continue
            if not found_with_pragma:
                findings.append(
                    f"{facade_name}: #include \"{include_name}\" "
                    f"is missing trailing `{iwyu_marker}` pragma. "
                    "Façade re-exports without the pragma would "
                    "trip clangd's include-cleaner with "
                    "`unused-includes` warnings, breaking the "
                    "clangd-clean motion_smooth surface guarantee."
                )
    assert not findings, (
        "motion_smooth façade IWYU-keep-pragma contract violated. "
        "Findings:\n  " + "\n  ".join(findings)
    )


def test_shape_flat_facade_does_not_re_export_internal_helpers() -> None:
    """ boundary lock: motion_smooth_shape_flat.hpp is intentionally
    a single-function declaration header (only declares
    `MotionSmoothShapeFlatTrajectoryKeys`). The the shape-flat extraction sub-modules helpers
    (notes / topology_gate / closed_loop / key_emission) are
    orchestrator-internal — they are exposed as their own headers so
    tests can call them, but they MUST NOT be re-exported via
    motion_smooth_shape_flat.hpp.

    This intent is documented inline in
    `motion_smooth_facade_lock_policy.py`'s SUBMODULE_SYMBOLS comment
    block ("These are NOT added to FACADE_INCLUDES because
    motion_smooth_shape_flat.hpp is a single-function declaration
    header, not a façade in the the corresponding sub-modules sense") and was reinforced by
    the the shape-flat extraction sub-modules docs entry. Without this check, a future refactor
    that added `#include "bbsolver/motion_smooth/motion_smooth_shape_flat_notes.hpp"` to the
    shape_flat header would silently expand the public API surface —
    every consumer of `motion_smooth_shape_flat.hpp` (main.cpp,
    motion_smooth_dispatch.cpp, the panel-side integration code) would
    suddenly see the internal helper symbols.

    The check is a simple substring scan over the shape_flat header.
    """
    facade_path = _ms_path("motion_smooth_shape_flat.hpp")
    assert facade_path.exists(), (
        "motion_smooth_shape_flat.hpp must exist"
    )
    text = facade_path.read_text(encoding="utf-8")
    forbidden_includes = (
        "motion_smooth_shape_flat_notes.hpp",
        "motion_smooth_shape_flat_topology_gate.hpp",
        "motion_smooth_shape_flat_closed_loop.hpp",
        "motion_smooth_shape_flat_key_emission.hpp",
    )
    findings = []
    for include_name in forbidden_includes:
        include_token = f'#include "bbsolver/motion_smooth/{include_name}"'
        if include_token in text:
            findings.append(
                f"motion_smooth_shape_flat.hpp includes `{include_name}` "
                "— that helper is orchestrator-internal (the shape-flat extraction sub-modules) and "
                "must not be re-exported through the public shape_flat "
                "header; move the include into "
                "motion_smooth_shape_flat.cpp instead"
            )
    assert not findings, (
        "shape_flat header internal-helper boundary contract violated. "
        "Findings:\n  " + "\n  ".join(findings)
    )


def _extract_top_level_declared_symbols(text: str) -> list[str]:
    """ helper: extract every top-level declaration in a header.

    A declaration is either:
      * `struct X {... };` at column 0 → emits "struct X"
      * `<return_type> Func(...);` at column 0 → emits "Func"

    Field declarations inside struct bodies are at indented columns
    (> 0), so the column-0 anchor excludes them automatically. The
    extractor is comment-aware: `//` line comments are stripped before
    scanning. Block comments (`/* */`) are not handled — the MS
    headers don't use them.

    The function-declaration regex matches a return-type token (which
    may contain `:`, `<`, `>`, `,`, `&`, `*`, and whitespace) followed
    by whitespace, an identifier, optional whitespace, and an opening
    paren at end-of-line. The captured identifier is the function
    name.
    """
    import re
    stripped = _strip_line_comments(text)
    symbols: list[str] = []
    # Column-0 struct declarations.
    for match in re.finditer(r'^struct\s+(\w+)', stripped, re.MULTILINE):
        symbols.append("struct " + match.group(1))
    # Column-0 function declarations.
    # Pattern: line starts with a return-type token (one or more
    # words optionally containing `:<>,&* `), followed by whitespace,
    # then the function name (captured), then optional whitespace,
    # then `(` at end of line. The non-greedy quantifier on the
    # return type prevents over-eating into the function name.
    func_pattern = re.compile(
        r'^[\w:<>,&\s\*]+?\s+(\w+)\s*\($',
        re.MULTILINE,
    )
    for match in func_pattern.finditer(stripped):
        symbols.append(match.group(1))
    return symbols


def test_motion_smooth_subheaders_only_declare_expected_symbols() -> None:
    """ symbol-drift detection.

    The existing `test_submodule_headers_declare_expected_symbols` verifies every entry in SUBMODULE_SYMBOLS is *present* in
    its header.  inverts the direction: it asserts every
    *top-level declaration* in each header is *expected* by
    SUBMODULE_SYMBOLS.

    Without this inverse check, a future refactor could add a new
    public function to a sub-header (say, `void NewHelperX(...)` in
    `motion_smooth_shape_flat_notes.hpp`) without updating the policy.
    The check would still pass (the expected symbols are still
    declared). But the architectural intent — that each MS sub-header
    has a tightly-scoped public surface enumerated by
    SUBMODULE_SYMBOLS — would silently break.

    Algorithm:
      1. For each (header, expected_symbols) in SUBMODULE_SYMBOLS.
      2. Extract every top-level declaration via the column-0 regex
         in `_extract_top_level_declared_symbols`.
      3. Compare against the expected set. Any declared symbol not in
         the expected set is a "drift" finding.

    The extractor is intentionally simple — it catches the common
    public surface (struct definitions and free function
    declarations). Templates, `using` aliases, and macros are not
    in scope; the MS sub-headers don't use them.
    """
    findings = []
    for header_name, expected_symbols in SUBMODULE_SYMBOLS.items():
        header_path = _ms_path(header_name)
        if not header_path.exists():
            # Already reported by other policy checks.
            continue
        text = _read_text(header_path)
        declared = _extract_top_level_declared_symbols(text)
        expected_set = set(expected_symbols)
        for symbol in declared:
            if symbol not in expected_set:
                findings.append(
                    f"{header_name}: declared top-level symbol "
                    f"`{symbol}` is not in SUBMODULE_SYMBOLS for this "
                    "header. Either add it to the expected list (if "
                    "the new public surface is intentional) or move "
                    "the declaration to a non-MS-tracked location."
                )
    assert not findings, (
        "motion_smooth sub-header symbol-drift contract violated. "
        "Findings:\n  " + "\n  ".join(findings)
    )


#  allowlist scaffolding ----------------------------------------------
#
# The files this lane owns (15 new MS-extracted sub-modules + 2 modified
# orchestrators whose bodies the MS work substantially rewrote). Each
# entry must either include only motion_smooth_*.hpp headers or one of
# the headers in MS_EXTRACTED_NON_MOTION_SMOOTH_ALLOWLIST.
#
# Pre-existing motion_smooth files (motion_smooth_dispatch.cpp,
# motion_smooth_geometry.cpp, motion_smooth_reduction_gate.cpp,
# motion_smooth_shape_quality.cpp) are intentionally NOT in this list —
# they have legitimate pre-existing dependencies on path_bridge_refit,
# solver_reporting, etc., that are outside MS-architectural scope.
MS_EXTRACTED_CPPS = frozenset({
    #  — tangent lock.
    "motion_smooth_shape_tangent_lock.cpp",
    #  — Catmull-Rom curve primitives.
    "motion_smooth_shape_loop_curve.cpp",
    #  — adaptive sampler.
    "motion_smooth_shape_loop_adaptive.cpp",
    #  — closed-loop schedule.
    "motion_smooth_shape_loop_schedule.cpp",
    #  — residual orchestrator (only EvenTimesForValueCount remains).
    "motion_smooth_shape_loop.cpp",
    #  — source-key schedule + RDP.
    "motion_smooth_shape_source_key_schedule.cpp",
    #  — trajectory smoother.
    "motion_smooth_shape_trajectory_smooth.cpp",
    #  — rove schedule.
    "motion_smooth_shape_rove_schedule.cpp",
    #  — sample/point primitives.
    "motion_smooth_sample_points.cpp",
    #  — Bezier ease.
    "motion_smooth_bezier_ease.cpp",
    #  — endpoint keys.
    "motion_smooth_endpoint_keys.cpp",
    #  — spatial trajectory.
    "motion_smooth_spatial_trajectory.cpp",
    # the shape-flat extraction sub-modules — shape_flat orchestrator helpers.
    "motion_smooth_shape_flat_notes.cpp",
    "motion_smooth_shape_flat_topology_gate.cpp",
    "motion_smooth_shape_flat_closed_loop.cpp",
    "motion_smooth_shape_flat_key_emission.cpp",
    # the shape-flat extraction sub-modules — shape_flat orchestrator (substantially reduced).
    "motion_smooth_shape_flat.cpp",
})


# Allowlist of non-motion_smooth headers that MS-extracted code may
# include. Built from current usage; expanding this set requires an
# explicit policy edit so the dependency-surface change is reviewed
# alongside the code change.
MS_EXTRACTED_NON_MOTION_SMOOTH_ALLOWLIST = frozenset({
    "bbsolver/domain.hpp",                    # Core types (PropertySamples, Sample, Key, etc.)
    "bbsolver/dp/dp_placer.hpp",   # SegmentFitResult (consumed by sample_points). Slice 90: dp_placer migrated to bbsolver/dp/.
    "bbsolver/routing/property_classification.hpp", # IsShapeFlatPath (consumed by, ).
    "bbsolver/samples/raw_frame_keys.hpp",       # ShapeFlatFrameKeyFallback (consumed by, ).
    "bbsolver/samples/sample_key_timing.hpp",    # DefaultEasesForProperty (consumed by,,, ).
    "bbsolver/samples/sample_value_helpers.hpp", # SampleVectorOrZeros (consumed by, ).
    "bbsolver/shape/shape_flat_topology.hpp", # ShapeFlatVertexCountFromValues (consumed by,, ).
})


def test_ms_extracted_cpps_only_include_allowed_non_motion_smooth_headers() -> None:
    """ dependency-surface lock for MS-extracted code.

    Every `#include "..."` directive in a file listed in
    MS_EXTRACTED_CPPS must point to either:

      (a) a `motion_smooth_*.hpp` sibling header, or
      (b) a header in MS_EXTRACTED_NON_MOTION_SMOOTH_ALLOWLIST.

    A regression that added e.g.
    `#include "bbsolver/path/replacement/path_replacement_notes.hpp"` or
    `#include "temporal_refit.hpp"` to a motion_smooth file would
    silently couple motion_smooth to those modules — creating a cycle
    that the build would tolerate but that ties motion_smooth's
    architectural boundary to unrelated lanes.

    Pre-existing motion_smooth files (dispatch, geometry,
    reduction_gate, shape_quality) are explicitly OUT of scope. They
    have legitimate pre-existing dependencies on path_bridge_refit,
    solver_reporting, etc., that aren't this lane's to police.

    This check completes the dependency-surface trifecta:
      *: no stale motion_smooth includes (every included
        motion_smooth header exists on disk).
      *: no unexpected public symbols in MS sub-headers.
      *: no unexpected non-motion_smooth dependencies in
        MS-extracted.cpp files.
    """
    findings = []
    for cpp_name in sorted(MS_EXTRACTED_CPPS):
        cpp_path = _ms_path(cpp_name)
        if not cpp_path.exists():
            findings.append(
                f"{cpp_name}: listed in MS_EXTRACTED_CPPS but file does "
                "not exist — update MS_EXTRACTED_CPPS to match the "
                "current MS-extracted source set"
            )
            continue
        text = _read_text(cpp_path)
        for line_no, raw_line in enumerate(text.splitlines(), start=1):
            stripped = raw_line.split("//", 1)[0]
            marker = '#include "'
            if marker not in stripped:
                continue
            start = stripped.index(marker) + len(marker)
            end = stripped.find('"', start)
            if end < 0:
                findings.append(
                    f"{cpp_name}:{line_no}: malformed #include directive"
                )
                continue
            header = stripped[start:end]
            header_stem = Path(header).name
            if header_stem.startswith("motion_smooth_"):
                continue  # Sibling MS header — always allowed.
            # Allowlist entries may be full-path form (e.g.
            # `bbsolver/domain.hpp` after Slice 70-style migration) or
            # legacy basename form (e.g. `dp_placer.hpp` still at flat
            # `solver/src/`). Accept both spellings.
            if header in MS_EXTRACTED_NON_MOTION_SMOOTH_ALLOWLIST:
                continue
            if header_stem in MS_EXTRACTED_NON_MOTION_SMOOTH_ALLOWLIST:
                continue  # Approved cross-module dependency.
            findings.append(
                f"{cpp_name}:{line_no}: #include \"{header}\" is not "
                "in MS_EXTRACTED_NON_MOTION_SMOOTH_ALLOWLIST. If the "
                "new dependency is intentional, add the header to the "
                "allowlist with a justification comment. If it is "
                "accidental, remove the include or restructure the "
                "code to reach the symbol via an existing allowed "
                "dependency."
            )
    assert not findings, (
        "MS-extracted dependency-surface contract violated. "
        "Findings:\n  " + "\n  ".join(findings)
    )


def test_submodule_bodies_pair_with_their_headers() -> None:
    """Every MS sub-module.hpp listed in SUBMODULE_SYMBOLS must have a
    sibling.cpp at the same path. A regression that orphans a header
    (removes the.cpp but leaves the.hpp) would silently produce
    unresolved-symbol link errors at integration; surface that here.
    """
    findings = []
    for header_name in SUBMODULE_SYMBOLS:
        body_name = header_name[:-4] + ".cpp"  # strip.hpp, add.cpp
        body_path = _ms_path(body_name)
        if not body_path.exists():
            findings.append(
                f"orphaned header: {header_name} has no sibling {body_name}"
            )
    assert not findings, (
        "sub-module body-pairing contract violated. Findings:\n  "
        + "\n  ".join(findings)
    )


# ---------------------------------------------------------------------------
#  integration-readiness checks. The the corresponding sub-modules contract locked the
# structure of the splits at the source-text layer.  closes the
# remaining gaps that would matter when the lane integrates upstream:
# stale include paths, duplicate symbol declarations across motion_smooth
# headers, header/body symbol-ownership drift, and the focused-test files
# that were added in  /  to lock previously-untested surfaces.
# ---------------------------------------------------------------------------


# Every file under solver/src/ or solver/tests/solver_unit/ that includes a
# motion_smooth_*.hpp header points to a path that must exist on disk.
# This is the "no stale include" smoke check: a regression that renamed
# a sub-header without updating its consumers would surface as a missing
# file here before the build attempts to compile.
SCAN_INCLUDE_ROOTS = ("solver/src", "solver/include/bbsolver/motion_smooth", "solver/tests/solver_unit")


#  /  focused-test files. These must remain present so the
# specific surfaces they lock (RDP keep-mask recursion, ApplyMotionSmoothBezierEase
# early-return guards, influence clamp extremes, anchor-pin contract,
# tolerance-clamp extremes) stay covered at the unit-test layer.
FOCUSED_TEST_FILES = (
    ("solver/tests/solver_unit/test_motion_smooth_source_key_schedule.cpp",
     " focused test for ShapeMotionSourceKeyRdpKeep + "
     "BuildShapeMotionSourceKeySchedule contracts"),
    ("solver/tests/solver_unit/test_motion_smooth_bezier_ease_guards.cpp",
     " focused test for ApplyMotionSmoothBezierEase early-return "
     "guards and min_influence/max_influence clamp extremes"),
    ("solver/tests/solver_unit/test_motion_smooth_curve_and_tangent_lock.cpp",
     " focused test for LockShapeFlatRotationalTangents + "
     " curve primitives (MotionSmoothCatmullRomValue, PointTurnDeg, "
     "ShapeFlatVertexPoint, EvaluateClosedLoopShapeAtParam)"),
    ("solver/tests/solver_unit/test_motion_smooth_shape_flat_notes.cpp",
     " focused test for BuildMotionSmoothShapeFlatNotes — "
     "locks the public note-token contract (always-on flags, "
     "closed-loop branch tokens, source-fidelity discriminator)"),
    ("solver/tests/solver_unit/test_motion_smooth_shape_flat_topology_gate.cpp",
     " focused test for ValidateMotionSmoothShapeFlatTopology — "
     "locks all five return paths (success, no_shape_motion_span, "
     "invalid_shape_topology, variable_shape_topology, "
     "no_source_key_schedule) and the populated success outputs"),
    ("solver/tests/solver_unit/test_motion_smooth_shape_flat_key_emission.cpp",
     " focused test for EmitMotionSmoothShapeFlatKeysFromRoveSchedule — "
     "locks edge-Linear/interior-Bezier interp choice, "
     "temporal_continuous mirror of use_ease, times/values round-trip "
     "from rove schedule, and ApplyMotionSmoothBezierEase invocation "
     "under use_ease=true"),
    ("solver/tests/solver_unit/test_motion_smooth_rove_schedule.cpp",
     " focused test for BuildShapeMotionRoveScheduleFromValues — "
     "locks the 1e-7 static-duplicate epsilon, endpoint preservation, "
     "travel-proportional retiming under apply_rove=true, and the "
     "apply_rove=false short-circuit that keeps interior times"),
    ("solver/tests/solver_unit/test_motion_smooth_shape_loop_schedule.cpp",
     " focused test for TimesForClosedLoopParams + "
     "TimesForClosedLoopParamsByIntervalTravel — locks linear-interp "
     "boundary clamps, size-mismatch fallback, endpoint pinning, "
     "chord-travel interior retiming, and applied threshold semantics"),
    ("solver/tests/solver_unit/test_motion_smooth_shape_flat_closed_loop.cpp",
     " focused test for BuildShapeFlatClosedLoopAdaptiveResample — "
     "locks the 4-cell (closed_loop × motion_smooth_source_fidelity) "
     "behaviour matrix: trivial pass-through choosing smoothed vs "
     "original_values, all-true constraint indices under fidelity, "
     "EvenTimesForValueCount derivation under closed-loop, and "
     "source_pose_interval_schedule population under closed-loop+fidelity"),
    ("solver/tests/solver_unit/test_motion_smooth_shape_trajectory_smooth.cpp",
     " focused test for BuildShapeMotionTrajectorySmoothValues — "
     "locks pre-LSQ scaffolding (smoothing_passes formula, "
     "smoothing_blend = clamp(strength/(strength+2), 0, 0.90)), "
     "early-return paths (size<=2 or dims<=2 → smoothed==original), "
     "main-path displacement_limit positivity, and source-fidelity "
     "flag round-trip from optional inputs"),
    ("solver/tests/solver_unit/test_motion_smooth_shape_loop_adaptive.cpp",
     " focused test for BuildAdaptiveClosedLoopShapeSamples — "
     "closes the last indirect-only motion_smooth surface. Locks three "
     "early-return paths (size<4, dims<=2, vertex_count<=0), the "
     "strength-derived formulas at non-source-pose and source-pose "
     "(target_turn_deg, chord_error_tolerance, max_keys), and the "
     "main-path structural invariants (refinement_passes <= 16, "
     "splits >= 0, quality valid, values.size() <= max_keys)"),
)


def test_no_stale_motion_smooth_includes() -> None:
    """Every `#include "bbsolver/motion_smooth/motion_smooth_*.hpp"` directive across the
    solver/src/ and solver/tests/solver_unit/ trees must point to a file that
    exists in solver/src/. A regression that renames a sub-header
    (e.g. drops the "_loop_" infix) without updating its consumers
    would otherwise surface as a build-time "file not found" — this
    catches it source-side, with a precise file:line.

    The scanner only follows `#include "..."` directives (quoted form,
    same-directory resolution). Angle-bracket includes are
    out-of-scope.
    """
    findings = []
    available_headers = {p.name for p in HEADER_DIR.glob("motion_smooth_*.hpp")}
    for root_name in SCAN_INCLUDE_ROOTS:
        root_path = solver_path(SOLVER, root_name)
        if not root_path.exists():
            findings.append(
                f"scan-root missing: {root_name} — adjust "
                "SCAN_INCLUDE_ROOTS"
            )
            continue
        for cpp_or_hpp in sorted(root_path.rglob("*.[ch]pp")):
            text = _read_text(cpp_or_hpp)
            for line_no, raw_line in enumerate(text.splitlines(), start=1):
                stripped = raw_line.split("//", 1)[0]
                # Match `#include "bbsolver/motion_smooth/motion_smooth_*.hpp"` precisely.
                marker = '#include "bbsolver/motion_smooth/motion_smooth_'
                if marker not in stripped:
                    continue
                # Extract the quoted header name.
                start = stripped.index(marker) + len('#include "')
                end = stripped.find('"', start)
                if end < 0:
                    findings.append(
                        f"{cpp_or_hpp.relative_to(ROOT)}:{line_no}: "
                        "malformed #include directive"
                    )
                    continue
                header = stripped[start:end]
                # `header` is the full include path (e.g.
                # `bbsolver/motion_smooth/motion_smooth_X.hpp`);
                # `available_headers` keys by basename.
                header_stem = Path(header).name
                if header_stem not in available_headers:
                    findings.append(
                        f"{cpp_or_hpp.relative_to(ROOT)}:{line_no}: "
                        f"#include \"{header}\" references a "
                        "motion_smooth header that does not exist under "
                        "solver/include/bbsolver/motion_smooth/"
                    )
    assert not findings, (
        "stale motion_smooth include detected. Findings:\n  "
        + "\n  ".join(findings)
    )


def test_motion_smooth_symbol_ownership_is_unique() -> None:
    """For every public symbol listed in SUBMODULE_SYMBOLS, the symbol's
    declaration must appear in exactly one motion_smooth header in
    solver/src/. A duplicate declaration in a sibling header (perhaps
    introduced by a partial copy-paste during a future refactor) would
    cause subtle ODR-adjacent issues at integration: header order
    becomes load-bearing, and downstream consumers' #include order
    silently affects which declaration the compiler binds to.

    The scanner walks all `solver/src/motion_smooth_*.hpp` files and
    counts how many declare each tracked symbol (after stripping `//`
    line comments).
    """
    headers = sorted(HEADER_DIR.glob("motion_smooth_*.hpp"))
    header_texts = {h.name: _strip_line_comments(_read_text(h)) for h in headers}
    findings = []
    for canonical_header, expected_symbols in SUBMODULE_SYMBOLS.items():
        for symbol in expected_symbols:
            owners = [
                name for name, text in header_texts.items() if symbol in text
            ]
            if len(owners) == 0:
                # Already reported by test_submodule_headers_declare_expected_symbols.
                continue
            if len(owners) > 1:
                # Exception: façades intentionally re-include sub-headers, so
                # the symbol token can appear inside an `#include "..."`
                # directive in the façade. We discriminate declarations from
                # includes by ensuring the symbol token is not bracketed by
                # quotes on its line. A simple heuristic: a true declaration
                # never appears inside a #include line.
                real_owners = []
                for owner_name in owners:
                    owner_text = header_texts[owner_name]
                    real_match = False
                    for line in owner_text.splitlines():
                        if symbol in line and "#include" not in line:
                            real_match = True
                            break
                    if real_match:
                        real_owners.append(owner_name)
                if len(real_owners) > 1:
                    findings.append(
                        f"symbol `{symbol}` declared in multiple "
                        f"motion_smooth headers: {real_owners} — "
                        f"canonical owner per SUBMODULE_SYMBOLS is "
                        f"{canonical_header}"
                    )
                elif len(real_owners) == 1 and real_owners[0] != canonical_header:
                    findings.append(
                        f"symbol `{symbol}` declared in "
                        f"{real_owners[0]} but SUBMODULE_SYMBOLS claims "
                        f"{canonical_header} owns it"
                    )
    assert not findings, (
        "motion_smooth symbol-ownership uniqueness contract violated. "
        "Findings:\n  " + "\n  ".join(findings)
    )


def test_submodule_bodies_define_their_declared_symbols() -> None:
    """For each function symbol declared in a sub-module.hpp, the
    sibling.cpp must contain a definition. A regression that moves a
    function definition out of its expected.cpp (e.g. into a stray
    "motion_smooth_misc.cpp") would surface as a link error downstream;
    catching it at the source-text layer keeps the failure local to
    the policy run.

    The scanner looks for the symbol as a function-token: it must
    appear in the body file in a context that is plausibly a
    definition (i.e., the line contains the symbol's name followed by
    `(`) rather than purely as a call-site argument. For struct
    symbols (those listed in SUBMODULE_SYMBOLS that start with
    `struct `), the check is skipped — structs don't have a "body
    file definition" the way functions do.
    """
    findings = []
    for header_name, expected_symbols in SUBMODULE_SYMBOLS.items():
        body_name = header_name[:-4] + ".cpp"
        body_path = _ms_path(body_name)
        if not body_path.exists():
            # Already reported by test_submodule_bodies_pair_with_their_headers.
            continue
        body_text = _strip_line_comments(_read_text(body_path))
        for symbol in expected_symbols:
            if symbol.startswith("struct "):
                continue
            token = symbol + "("
            if token not in body_text:
                findings.append(
                    f"{body_name}: missing definition of `{symbol}` "
                    f"(declared in {header_name})"
                )
    assert not findings, (
        "sub-module body symbol-ownership contract violated. "
        "Findings:\n  " + "\n  ".join(findings)
    )


# ---------------------------------------------------------------------------
#  merge-preflight checks. The the corresponding sub-modules contracts lock the structure
# and the include graph.  closes the remaining algorithmic-fidelity
# gaps: tolerance / clamp constants, public note-string tokens, header
# hygiene (#pragma once), and the self-include convention. A regression
# in any of these would compile cleanly but silently change downstream
# behaviour or break diagnostic consumers — exactly the kind of drift
# that's hardest to catch during merge review.
# ---------------------------------------------------------------------------


# Algorithmic constants that must remain in their owning TU. Each entry
# maps a body file (relative to solver/src/) to a list of (token,
# rationale) pairs. The token must appear in the body file's code
# (after stripping `//` line comments). A regression that changes the
# numeric value would fail this check; a regression that moves the
# constant to a different TU would also fail.
TOLERANCE_CONSTANTS = {
    "motion_smooth_shape_loop_adaptive.cpp": [
        ("48.0 - strength * 3.0",
         "base_target_turn_deg formula ( adaptive sampler)"),
        ("strength * 0.55",
         "regular-mode chord_error_tolerance multiplier"),
        ("strength * 0.35",
         "source-pose chord_error_tolerance multiplier"),
    ],
    "motion_smooth_spatial_trajectory.cpp": [
        ("* 0.45",
         "tangent length cap = 0.45 * adjacent-segment length"),
        ("1.0 / 3.0",
         "endpoint tangent scale (outward 1/3 delta)"),
        ("1.0 / 6.0",
         "interior tangent scale (central-difference 1/6 delta)"),
        ("strength * 0.75",
         "simplify_tolerance = max(0.75, strength * 0.75) RDP gate"),
    ],
    "motion_smooth_shape_rove_schedule.cpp": [
        ("1e-7",
         "static_eps for duplicate-key detection in rove schedule"),
    ],
    "motion_smooth_sample_points.cpp": [
        ("1e-6",
         "source-key time inclusion + dedup epsilon"),
    ],
    "motion_smooth_bezier_ease.cpp": [
        ("0.001, 0.999",
         "Bezier x1/x2 clamp range"),
        ("x1 * 100.0",
         "out_influence raw computation before clamp"),
    ],
    #  trajectory smoother constants (added in  — the 
    # report flagged these as untracked). The smoothing_blend formula
    # is the user-visible knob for how aggressively the cubic-Bezier
    # LSQ fit influences the output; its clamp ceiling at 0.90 is the
    # hard cap on smoothing strength. The displacement_limit
    # components govern per-control displacement capping. The 1e-12
    # determinant threshold guards the LSQ solve against singular
    # observation matrices (silently falling back to linear endpoint
    # interpolation when the threshold trips).
    "motion_smooth_shape_trajectory_smooth.cpp": [
        ("strength / (strength + 2.0)",
         "smoothing_blend raw formula before clamp"),
        ("0.90",
         "smoothing_blend clamp ceiling (also tested at )"),
        ("strength * 24.0",
         "displacement_limit raw component (strength-driven cap)"),
        ("extent * 0.04 * strength",
         "displacement_limit raw component (extent + strength)"),
        ("extent * 0.35",
         "displacement_limit upper bound (35% of trajectory extent)"),
        ("6.0",
         "displacement_limit lower bound (minimum displacement cap)"),
        ("1e-12",
         "determinant threshold guarding the LSQ solve against "
         "singular observation matrices"),
    ],
    # the shape-flat extraction sub-modules shape_flat orchestrator + helpers (added in to
    # close the policy gap left by the corresponding sub-modules — the helpers introduced
    # algorithmically significant constants that the existing per-file
    # entries above did not cover).
    "motion_smooth_shape_flat.cpp": [
        ("1e-6",
         "closed-loop gate epsilon: closed_loop iff <= max(1e-6, strength*0.01)"),
        ("strength * 0.01",
         "closed-loop gate strength factor"),
    ],
    "motion_smooth_shape_flat_closed_loop.cpp": [
        ("1e-9",
         "param round tolerance for source_pose_constraint_indices "
         "(detects original key positions in the resampled adaptive_loop.params)"),
    ],
    "motion_smooth_shape_flat_topology_gate.cpp": [
        ("vertex_count <= 0 || dims < 8",
         "minimum-topology gate: invalid topology iff vertex_count<=0 or dims<8 "
         "(anchor + 1 vertex with all 6 slots = 8 dims)"),
    ],
    #: lock the endpoint route's behavioural threshold. This
    # 1e-9 decides whether the endpoint route emits 1 key (first
    # sample only) or 2 keys (first + last). Unlike a defensive
    # degenerate-length guard, this threshold directly shapes the
    # output key count downstream consumers (panel, diagnostics)
    # see. A regression that changed it to 1e-3 or removed it would
    # silently change key counts for short-span properties.
    "motion_smooth_endpoint_keys.cpp": [
        ("first_sample.t_sec + 1e-9",
         "behavioural threshold: emit second endpoint key iff "
         "last_sample.t_sec > first_sample.t_sec + 1e-9"),
    ],
}


# Public note-string tokens that downstream diagnostic consumers
# (panel, AE, diagnostics analyzers) depend on. Each entry maps a body
# file to a list of (token, rationale). Token presence is checked
# in raw text (not stripped of comments) — these are user-facing
# string literals and renaming them silently breaks API contracts.
NOTE_STRING_TOKENS = {
    "motion_smooth_endpoint_keys.cpp": [
        ("solve_mode_motion_smooth",
         "primary mode token consumed by downstream diagnostic parsers"),
        ("endpoint_keys=",
         "endpoint count diagnostic"),
        ("motion_smooth_ease=",
         "ease on/off diagnostic"),
        ("source_error_not_evaluated=true",
         "documents that endpoint route does not evaluate source error"),
        ("motion_smooth_endpoint_interpolation",
         "segment.reason token for endpoint route"),
        ("endpoint_topology_mismatch",
         "shape-flat fallback note for vertex-count mismatch"),
        #: lock the empty-samples fallback note. Downstream
        # consumers parse this as the discriminator for "endpoint
        # route was invoked but had no samples to work with"; a rename
        # would silently break that classification.
        ("solve_mode_motion_smooth; no_samples",
         "fallback note when property_samples.samples.empty()"),
    ],
    "motion_smooth_spatial_trajectory.cpp": [
        ("solve_mode_motion_smooth",
         "primary mode token (shared with endpoint route)"),
        ("motion_smooth_spatial_trajectory_filter=true",
         "diagnostic flag for the spatial route"),
        ("key_schedule=source_keys",
         "diagnostic token when source-key times are used"),
        ("key_schedule=sample_rdp",
         "diagnostic token when RDP sample reduction is used"),
        ("source_error_not_constrained=true",
         "documents that spatial route does not constrain source error"),
        ("motion_smooth_spatial_trajectory_filter",
         "segment.reason token for spatial route"),
        #: lock the no-spatial-span fallback note. Downstream
        # consumers parse this as the discriminator for "spatial
        # route was invoked with fewer than 2 samples"; a rename
        # would silently break that classification.
        ("solve_mode_motion_smooth; no_spatial_span",
         "fallback note when property_samples.samples.size() < 2"),
    ],
    # extraction — the shape_flat notes block. After the
    # biggest motion-smooth notes string lives in this TU; previously
    # it sat inline in motion_smooth_shape_flat.cpp. Locking the
    # always-on flags + the closed-loop/source-fidelity discriminators
    # here so a future rename of any one of them surfaces at the
    # policy layer before downstream consumers fail.
    "motion_smooth_shape_flat_notes.cpp": [
        ("solve_mode_motion_smooth",
         "primary mode token (also covered in spatial_trajectory.cpp and "
         "endpoint_keys.cpp)"),
        ("motion_smooth_shape_rove_time=true",
         "always-on rove_time flag (shape-flat route)"),
        ("motion_smooth_shape_trajectory_filter=true",
         "always-on trajectory_filter flag (shape-flat route)"),
        ("motion_smooth_stable_topology=true",
         "always-on stable_topology flag (shape-flat route)"),
        ("motion_smooth_source_key_times=true",
         "always-on source_key_times flag (shape-flat route)"),
        # The "key_schedule=" prefix and its two value tokens
        # (source_key_times_spline / source_keys_roved) are concatenated
        # at runtime from two adjacent string literals; the source
        # carries them as separate literals so we lock each separately.
        ("source_key_times_spline",
         "fidelity-on key schedule value (concatenated with `key_schedule=`)"),
        ("source_keys_roved",
         "fidelity-off key schedule value (concatenated with `key_schedule=`)"),
        ("key_schedule=",
         "key_schedule token prefix"),
        ("shape_tangent_lock=true",
         "tangent-lock always-on flag"),
        ("motion_smooth_closed_loop=",
         "closed-loop discriminator token prefix"),
        ("source_error_not_constrained=",
         "source-error constraint token prefix"),
        ("closed_loop_resample=true",
         "closed-loop-only token appended when closed_loop branch fires"),
    ],
    # extraction — the four fallback skip strings that
    # downstream consumers parse to discriminate why the motion-smooth
    # route bailed. Rename of any of these would silently break the
    # diagnostic taxonomy.
    "motion_smooth_shape_flat_topology_gate.cpp": [
        ("solve_mode_motion_smooth; no_shape_motion_span",
         "fallback note for <2 samples (direct keys.notes set)"),
        ("solve_mode_motion_smooth_skipped: invalid_shape_topology",
         "fallback note for vertex_count<=0 or dims<8 "
         "(via ShapeFlatFrameKeyFallback)"),
        ("solve_mode_motion_smooth_skipped: variable_shape_topology",
         "fallback note for per-sample topology mismatch"),
        ("solve_mode_motion_smooth_skipped: no_source_key_schedule",
         "fallback note for <2 source key times"),
    ],
}


def test_motion_smooth_headers_have_pragma_once() -> None:
    """Every motion_smooth_*.hpp must begin with `#pragma once`. The
    project standardized on `#pragma once` rather than include-guard
    macros; a regression that dropped the directive would risk
    multiple-definition errors on consumers that include the header
    twice through different paths.

    The check looks at the first non-blank, non-`//` line of each
    header. Anything other than `#pragma once` fails.
    """
    findings = []
    for header_path in sorted(HEADER_DIR.glob("motion_smooth_*.hpp")):
        text = _read_text(header_path)
        first_directive = None
        for raw_line in text.splitlines():
            stripped = raw_line.strip()
            if not stripped:
                continue
            if stripped.startswith("//"):
                continue
            first_directive = stripped
            break
        if first_directive != "#pragma once":
            findings.append(
                f"{header_path.name}: first non-comment line is "
                f"`{first_directive}`, expected `#pragma once`"
            )
    assert not findings, (
        "motion_smooth header guard contract violated. Findings:\n  "
        + "\n  ".join(findings)
    )


def test_motion_smooth_tolerance_constants_remain_with_owning_tu() -> None:
    """Algorithmic tolerance / clamp constants must remain in their
    owning.cpp. A regression that changes (say) `48.0 - strength * 3.0`
    to `50.0 - strength * 3.0` in the adaptive sampler would compile
    cleanly and pass most behavioural tests numerically (the input
    space is large enough that random-input tests rarely hit the
    sensitive region), but would silently change motion-smooth output
    for downstream consumers.

    The check is comment-aware: each token must appear in code, not
    just in a docstring referring to the constant.
    """
    findings = []
    for body_name, expected_tokens in TOLERANCE_CONSTANTS.items():
        body_path = _ms_path(body_name)
        if not body_path.exists():
            findings.append(f"missing body file: {body_name}")
            continue
        text = _strip_line_comments(_read_text(body_path))
        for token, rationale in expected_tokens:
            if token not in text:
                findings.append(
                    f"{body_name}: missing token `{token}` "
                    f"({rationale})"
                )
    assert not findings, (
        "motion_smooth tolerance-constant ownership contract violated. "
        "Findings:\n  " + "\n  ".join(findings)
    )


def test_motion_smooth_notes_strings_remain_with_owning_tu() -> None:
    """Public note-string tokens that downstream diagnostic consumers
    parse must remain in their owning.cpp. A regression that renames
    `solve_mode_motion_smooth` to anything else would silently break
    the panel's mode detection and any diagnostic analyzer that keys
    off the token.

    Tokens are checked in raw text (no comment stripping) because they
    are string literals — the C++ source carries them as content of
    `+ "token"` expressions, and the policy verifies the literal is
    present in the file regardless of surrounding comment density.
    """
    findings = []
    for body_name, expected_tokens in NOTE_STRING_TOKENS.items():
        body_path = _ms_path(body_name)
        if not body_path.exists():
            findings.append(f"missing body file: {body_name}")
            continue
        text = _read_text(body_path)
        for token, rationale in expected_tokens:
            if token not in text:
                findings.append(
                    f"{body_name}: missing note token `{token}` "
                    f"({rationale})"
                )
    assert not findings, (
        "motion_smooth note-string ownership contract violated. "
        "Findings:\n  " + "\n  ".join(findings)
    )


def test_motion_smooth_cpp_files_self_include_their_header() -> None:
    """Every motion_smooth_*.cpp must `#include "<basename>.hpp"` as its
    first include directive. The convention serves two purposes:

    1. Forces the header to be self-contained (any missing include in
       the header surfaces immediately as a build error in the body
       file, not in some downstream consumer).
    2. Makes the implementation-vs-interface pairing explicit and
       grep-discoverable.

    A regression that lets a.cpp open with a different include first
    (e.g. a project header before the self-include) would silently
    weaken the header's self-containment invariant.
    """
    findings = []
    for body_path in sorted(SRC_DIR.glob("motion_smooth_*.cpp")):
        basename = body_path.stem  # motion_smooth_foo
        expected = f'#include "bbsolver/motion_smooth/{basename}.hpp"'
        text = _read_text(body_path)
        first_include = None
        for raw_line in text.splitlines():
            stripped = raw_line.strip()
            if stripped.startswith("#include"):
                first_include = stripped
                break
        if first_include != expected:
            findings.append(
                f"{body_path.name}: first #include is "
                f"`{first_include}`, expected `{expected}`"
            )
    assert not findings, (
        "motion_smooth self-include convention violated. Findings:\n  "
        + "\n  ".join(findings)
    )


def test_focused_test_files_remain_present() -> None:
    """The and focused-test files must remain on disk. These
    test files lock specific motion_smooth surfaces (RDP keep-mask
    recursion + builder anchor-pin/tolerance clamps from;
    ApplyMotionSmoothBezierEase early-return guards + influence-clamp
    extremes from ) that the higher-level integration tests don't
    cover.

    A regression that deleted either file would silently drop the
    surface coverage they encode; this check fails fast so the
    integration reviewer can decide whether the deletion was
    intentional or accidental.
    """
    findings = []
    for relative_path, purpose in FOCUSED_TEST_FILES:
        test_path = solver_path(SOLVER, relative_path)
        if not test_path.exists():
            findings.append(
                f"missing focused test: {relative_path} — covers {purpose}"
            )
            continue
        text = _read_text(test_path)
        if "int main(" not in text and "int main (" not in text:
            findings.append(
                f"{relative_path}: missing `int main(` entry point — "
                "test file is present but cannot be run as a binary"
            )
    assert not findings, (
        "/ focused-test presence contract violated. Findings:\n  "
        + "\n  ".join(findings)
    )


def test_no_orphan_motion_smooth_headers() -> None:
    """ orphan-header lock: every `motion_smooth_*.hpp` on disk in
    `solver/src/` must be accounted for by exactly one classification
    bucket in this policy:

      * FACADE_INCLUDES keys           — MS façade headers
      * FACADE_INCLUDES values         — sub-headers re-exported by a façade
      * SUBMODULE_SYMBOLS keys         — MS-extracted sub-module headers
      * PRE_EXISTING_MOTION_SMOOTH_HEADERS — legacy headers pre-dating 

    A new motion_smooth header that lands without being added to one of
    those buckets is an orphan: the policy does not enforce its symbol
    surface, its façade contract, or its sub-module pairing, so the
    integration reviewer would have no policy-backed signal that it was
    intentional. The reverse leg of this check also asserts every
    PRE_EXISTING_MOTION_SMOOTH_HEADERS entry still exists on disk so
    the allowlist cannot rot into a stale set of dangling names.
    """
    tracked: set[str] = set()
    tracked.update(FACADE_INCLUDES.keys())
    for subheaders in FACADE_INCLUDES.values():
        tracked.update(subheaders)
    tracked.update(SUBMODULE_SYMBOLS.keys())
    tracked.update(PRE_EXISTING_MOTION_SMOOTH_HEADERS)

    on_disk = sorted(p.name for p in HEADER_DIR.glob("motion_smooth_*.hpp"))
    assert on_disk, (
        "expected at least one motion_smooth_*.hpp under solver/src/; "
        "found none — has the directory layout changed?"
    )

    orphans = [name for name in on_disk if name not in tracked]
    assert not orphans, (
        "Untracked motion_smooth header(s) found on disk. Each header "
        "must be registered in FACADE_INCLUDES (as key or sub-header "
        "value), SUBMODULE_SYMBOLS, or PRE_EXISTING_MOTION_SMOOTH_HEADERS:"
        "\n  " + "\n  ".join(orphans)
    )

    missing_preexisting = [
        name
        for name in PRE_EXISTING_MOTION_SMOOTH_HEADERS
        if not _ms_path(name).exists()
    ]
    assert not missing_preexisting, (
        "PRE_EXISTING_MOTION_SMOOTH_HEADERS lists headers that no longer "
        "exist on disk; remove the stale allowlist entries:\n  "
        + "\n  ".join(missing_preexisting)
    )


def test_no_orphan_motion_smooth_bodies() -> None:
    """ orphan-body lock: parallel to `test_no_orphan_motion_smooth_headers`,
    every `motion_smooth_*.cpp` on disk in `solver/src/` must be
    accounted for by exactly one classification bucket:

      * MS_EXTRACTED_CPPS                  — the corresponding sub-modules extracted bodies
      * PRE_EXISTING_MOTION_SMOOTH_BODIES  — legacy bodies pre-dating 

    Three additional data-integrity asserts run on the allowlist itself:
      1. Every PRE_EXISTING_MOTION_SMOOTH_BODIES entry exists on disk
         (no stale dangling names).
      2. PRE_EXISTING_MOTION_SMOOTH_BODIES ∩ MS_EXTRACTED_CPPS is empty
         (a body cannot simultaneously be MS-extracted and pre-existing).
      3. DELETED_BODIES ∩ MS_EXTRACTED_CPPS is empty AND
         DELETED_BODIES ∩ PRE_EXISTING_MOTION_SMOOTH_BODIES is empty
         (a body that must stay deleted cannot also be classified as a
         live tracked body — the policy data would be self-contradictory).
    """
    tracked: set[str] = set()
    tracked.update(MS_EXTRACTED_CPPS)
    tracked.update(PRE_EXISTING_MOTION_SMOOTH_BODIES)

    on_disk = sorted(p.name for p in SRC_DIR.glob("motion_smooth_*.cpp"))
    assert on_disk, (
        "expected at least one motion_smooth_*.cpp under solver/src/; "
        "found none — has the directory layout changed?"
    )

    orphans = [name for name in on_disk if name not in tracked]
    assert not orphans, (
        "Untracked motion_smooth body file(s) found on disk. Each body "
        "must be registered in MS_EXTRACTED_CPPS or "
        "PRE_EXISTING_MOTION_SMOOTH_BODIES:\n  " + "\n  ".join(orphans)
    )

    missing_preexisting = [
        name
        for name in PRE_EXISTING_MOTION_SMOOTH_BODIES
        if not _ms_path(name).exists()
    ]
    assert not missing_preexisting, (
        "PRE_EXISTING_MOTION_SMOOTH_BODIES lists bodies that no longer "
        "exist on disk; remove the stale allowlist entries:\n  "
        + "\n  ".join(missing_preexisting)
    )

    overlap_extracted = sorted(
        set(PRE_EXISTING_MOTION_SMOOTH_BODIES) & MS_EXTRACTED_CPPS
    )
    assert not overlap_extracted, (
        "PRE_EXISTING_MOTION_SMOOTH_BODIES overlaps MS_EXTRACTED_CPPS — "
        "a body cannot be both MS-extracted and pre-existing:\n  "
        + "\n  ".join(overlap_extracted)
    )

    deleted_set = set(DELETED_BODIES)
    deleted_alive_extracted = sorted(deleted_set & MS_EXTRACTED_CPPS)
    deleted_alive_preexisting = sorted(
        deleted_set & set(PRE_EXISTING_MOTION_SMOOTH_BODIES)
    )
    assert not deleted_alive_extracted, (
        "DELETED_BODIES overlaps MS_EXTRACTED_CPPS — a body marked "
        "deleted cannot also be a tracked MS extraction:\n  "
        + "\n  ".join(deleted_alive_extracted)
    )
    assert not deleted_alive_preexisting, (
        "DELETED_BODIES overlaps PRE_EXISTING_MOTION_SMOOTH_BODIES — a "
        "body marked deleted cannot also be a tracked pre-existing "
        "body:\n  " + "\n  ".join(deleted_alive_preexisting)
    )


def test_ms_extracted_non_motion_smooth_allowlist_has_no_stale_entries() -> None:
    """ reverse-leg lock on MS_EXTRACTED_NON_MOTION_SMOOTH_ALLOWLIST:
    every header listed there must actually be `#include`d (quoted form)
    by at least one MS-extracted body (MS_EXTRACTED_CPPS) or sub-header
    (SUBMODULE_SYMBOLS keys).

     introduces a parallel forward leg on sub-headers, so the
    reverse-leg here must scan both bodies and sub-headers; otherwise
    an allowlist entry used only by a sub-header would be incorrectly
    flagged as stale.

    The forward legs
    (`test_ms_extracted_cpps_only_include_allowed_non_motion_smooth_headers`
    and `test_ms_extracted_headers_only_include_allowed_non_motion_smooth_headers`)
    assert every non-motion_smooth header an MS file includes is in
    the allowlist. Without this reverse leg, the allowlist could
    accumulate stale entries: a header that was removed from every MS
    body/sub-header's include set but kept in the allowlist would
    silently rot the integration signal — the policy would still PASS
    and reviewers would have no way to spot the dead allowance.

    Each entry is scanned via `#include "<name>"` substring match on
    line-comment-stripped source text. Angle-bracket includes are
    out-of-scope by design (the allowlist tracks quoted/in-tree headers).
    """
    usage_counts = {name: 0 for name in MS_EXTRACTED_NON_MOTION_SMOOTH_ALLOWLIST}
    scan_targets = list(MS_EXTRACTED_CPPS) + list(SUBMODULE_SYMBOLS.keys())
    for file_name in scan_targets:
        file_path = _ms_path(file_name)
        if not file_path.exists():
            # The orphan/missing-body case is enforced by
            # test_no_orphan_motion_smooth_bodies and
            # test_submodule_bodies_pair_with_their_headers; skip here
            # so this check stays focused on its single contract.
            continue
        text = _strip_line_comments(_read_text(file_path))
        for allowed in usage_counts:
            if f'#include "{allowed}"' in text:
                usage_counts[allowed] += 1

    stale = sorted(name for name, count in usage_counts.items() if count == 0)
    assert not stale, (
        "MS_EXTRACTED_NON_MOTION_SMOOTH_ALLOWLIST contains stale "
        "entries — no MS-extracted body or sub-header `#include`s them. "
        "Either an MS file needs to re-add the include, or the "
        "allowlist entry should be removed:\n  " + "\n  ".join(stale)
    )


def test_focused_test_files_reference_motion_smooth_surface() -> None:
    """ surface-coverage lock: every entry in FOCUSED_TEST_FILES
    must `#include` at least one `motion_smooth_*.hpp` header.

    The presence check (`test_focused_test_files_remain_present`) only
    asserts the file exists and has an `int main(` entry point — a
    test file that was renamed to look motion_smooth-relevant but no
    longer actually exercises motion_smooth code would still pass.
    This check catches that drift by requiring at least one quoted-form
    `#include "[…]motion_smooth_*.hpp"` directive. Relative-path
    includes (`../../solver/src/motion_smooth_*.hpp`) match too — the
    focused tests live in `solver/tests/solver_unit/` and reach into the
    solver tree via relative include, not via build-system include
    paths.
    """
    import re

    motion_smooth_include = re.compile(
        r'#include\s+"[^"]*motion_smooth_[^"]+\.hpp"'
    )
    findings = []
    for relative_path, purpose in FOCUSED_TEST_FILES:
        test_path = solver_path(SOLVER, relative_path)
        if not test_path.exists():
            # Presence is enforced by test_focused_test_files_remain_present.
            continue
        text = _strip_line_comments(_read_text(test_path))
        if not motion_smooth_include.search(text):
            findings.append(
                f"{relative_path}: contains no quoted-form "
                "`#include \"…motion_smooth_*.hpp\"` directive — file is "
                f"registered as covering `{purpose}` but does not "
                "include any motion_smooth header"
            )
    assert not findings, (
        "FOCUSED_TEST_FILES surface-coverage contract violated. "
        "Findings:\n  " + "\n  ".join(findings)
    )


def test_pure_shim_facades_are_facade_includes_keys() -> None:
    """ policy-data consistency: every entry in PURE_SHIM_FACADES
    must be a key in FACADE_INCLUDES.

    PURE_SHIM_FACADES is consumed by `test_facade_headers_are_thin` to
    enforce a stricter thinness rule on façades that have no residual
    declarations. If an entry there does not correspond to a tracked
    façade, the strict rule silently targets nothing — a data-typo
    would weaken the policy without any failing test. This check makes
    the constants self-consistent at policy load time.
    """
    missing = sorted(set(PURE_SHIM_FACADES) - set(FACADE_INCLUDES.keys()))
    assert not missing, (
        "PURE_SHIM_FACADES references names that are not keys in "
        "FACADE_INCLUDES. Either add the façade to FACADE_INCLUDES or "
        "remove the stray entry from PURE_SHIM_FACADES:\n  "
        + "\n  ".join(missing)
    )


def test_ms_extracted_headers_only_include_allowed_non_motion_smooth_headers() -> None:
    """ dependency-surface lock for MS-extracted sub-headers.

    Parallel to 's `.cpp` check: every quoted `#include "..."`
    directive in a sub-header listed in SUBMODULE_SYMBOLS must point
    to either:

      (a) a `motion_smooth_*.hpp` sibling header, or
      (b) a header in MS_EXTRACTED_NON_MOTION_SMOOTH_ALLOWLIST.

    Without this check, a sub-header could acquire e.g.
    `#include "bbsolver/domain.hpp"` without it being recorded in the allowlist,
    silently widening the dependency surface every downstream consumer
    of the sub-header pulls in. The current sub-headers reach for
    `domain.hpp` and `dp_placer.hpp` externally — both are already in
    MS_EXTRACTED_NON_MOTION_SMOOTH_ALLOWLIST, so this check passes
    cleanly and locks the surface.

    Façade headers (FACADE_INCLUDES keys) are out of scope; 
    enforces a stricter motion_smooth-only quoted-include rule on
    them. Pre-existing legacy headers (PRE_EXISTING_MOTION_SMOOTH_HEADERS)
    are also out of scope for the same reason excludes them.
    """
    findings = []
    for header_name in sorted(SUBMODULE_SYMBOLS.keys()):
        header_path = _ms_path(header_name)
        if not header_path.exists():
            findings.append(
                f"{header_name}: listed in SUBMODULE_SYMBOLS but file "
                "does not exist — update SUBMODULE_SYMBOLS to match "
                "the current MS-extracted header set"
            )
            continue
        text = _read_text(header_path)
        for line_no, raw_line in enumerate(text.splitlines(), start=1):
            stripped = raw_line.split("//", 1)[0]
            marker = '#include "'
            if marker not in stripped:
                continue
            start = stripped.index(marker) + len(marker)
            end = stripped.find('"', start)
            if end < 0:
                findings.append(
                    f"{header_name}:{line_no}: malformed #include directive"
                )
                continue
            header = stripped[start:end]
            header_stem = Path(header).name
            if header_stem.startswith("motion_smooth_"):
                continue  # Sibling MS header — always allowed.
            # Allowlist entries may be full-path form (e.g.
            # `bbsolver/domain.hpp` after Slice 70-style migration) or
            # legacy basename form (e.g. `dp_placer.hpp` still at flat
            # `solver/src/`). Accept both spellings.
            if header in MS_EXTRACTED_NON_MOTION_SMOOTH_ALLOWLIST:
                continue
            if header_stem in MS_EXTRACTED_NON_MOTION_SMOOTH_ALLOWLIST:
                continue  # Approved cross-module dependency.
            findings.append(
                f"{header_name}:{line_no}: #include \"{header}\" is not "
                "in MS_EXTRACTED_NON_MOTION_SMOOTH_ALLOWLIST. If the "
                "new dependency is intentional, add the header to the "
                "allowlist with a justification comment. If it is "
                "accidental, remove the include or reach the symbol "
                "via an existing allowed dependency."
            )
    assert not findings, (
        "MS-extracted sub-header dependency-surface contract violated. "
        "Findings:\n  " + "\n  ".join(findings)
    )


def test_motion_smooth_files_have_no_unapproved_orchestration_dependencies() -> None:
    """ architectural-boundary lock (wider than 's MS-extracted
    scope): every `motion_smooth_*.{hpp,cpp}` on disk in solver/src/
    may `#include` (quoted form) headers matching
    FORBIDDEN_ORCHESTRATION_STEM_PREFIXES only if the (file, include)
    pair appears in GRANDFATHERED_ORCHESTRATION_INCLUDES.

    Motion-smooth is downstream of those subsystems; they consume the
    motion_smooth output, not the other way around. The grandfather
    map captures the small set of pre-existing legitimate exceptions
    (currently just motion_smooth_reduction_gate.cpp →
    solver_reporting.hpp). Any new orchestration include — in any
    motion_smooth file, MS-extracted or pre-existing — trips this
    check.

    A reverse leg also runs: every entry in the grandfather map must
    actually appear in the source. If reduction_gate.cpp ever drops
    its solver_reporting include, the stale allowance must be removed
    so the wider lock starts catching that case immediately on the
    next regression.
    """
    import re

    include_re = re.compile(r'#include\s+"([^"]+)"')

    def is_forbidden(included_stem: str) -> bool:
        return any(
            included_stem.startswith(prefix)
            for prefix in FORBIDDEN_ORCHESTRATION_STEM_PREFIXES
        )

    findings: list[str] = []
    observed_grandfather_hits: dict[str, set[str]] = {}
    motion_smooth_files = sorted(
        list(HEADER_DIR.glob("motion_smooth_*.hpp"))
        + list(SRC_DIR.glob("motion_smooth_*.cpp"))
    )
    assert motion_smooth_files, (
        "expected motion_smooth_*.{hpp,cpp} files under solver/src/; "
        "found none — directory layout regressed"
    )

    for path in motion_smooth_files:
        text = _strip_line_comments(_read_text(path))
        allowed = GRANDFATHERED_ORCHESTRATION_INCLUDES.get(path.name, frozenset())
        for line_no, raw_line in enumerate(text.splitlines(), start=1):
            match = include_re.search(raw_line)
            if not match:
                continue
            included = match.group(1)
            stem = Path(included).name
            if not is_forbidden(stem):
                continue
            if stem in allowed:
                observed_grandfather_hits.setdefault(path.name, set()).add(stem)
                continue
            findings.append(
                f"{path.name}:{line_no}: unapproved orchestration "
                f'include `{included}` — motion_smooth must not '
                "depend on orchestration subsystems; if this is "
                "intentional, add the (file, include) pair to "
                "GRANDFATHERED_ORCHESTRATION_INCLUDES"
            )
    assert not findings, (
        "Motion_smooth files acquired unapproved orchestration "
        "dependencies:\n  " + "\n  ".join(findings)
    )

    # Reverse leg: every grandfathered (file, include) pair must
    # actually appear in the source. A stale allowance silently
    # widens the policy's permission surface.
    stale_grandfather_entries: list[str] = []
    for file_name, allowed_includes in GRANDFATHERED_ORCHESTRATION_INCLUDES.items():
        hits = observed_grandfather_hits.get(file_name, set())
        for inc in sorted(allowed_includes):
            if inc not in hits:
                stale_grandfather_entries.append(
                    f"{file_name} no longer `#include`s `{inc}` — "
                    "remove the stale entry from "
                    "GRANDFATHERED_ORCHESTRATION_INCLUDES"
                )
    assert not stale_grandfather_entries, (
        "GRANDFATHERED_ORCHESTRATION_INCLUDES contains stale entries:"
        "\n  " + "\n  ".join(stale_grandfather_entries)
    )


def test_motion_smooth_facade_quoted_includes_are_motion_smooth_only() -> None:
    """ façade-purity lock: every quoted-form `#include "..."`
    directive inside a façade header (FACADE_INCLUDES keys) must
    reference another `motion_smooth_*.hpp` file.

    Angle-bracket STL includes (e.g. `#include <cstddef>` on
    motion_smooth_shape_loop.hpp for the lone EvenTimesForValueCount
    declaration) are intentionally out of scope: those are direct
    dependencies of the façade's residual code, not architectural
    coupling. The check targets the quoted-include surface where
    cross-tree coupling would actually manifest.

    A façade that started `#include "bbsolver/domain.hpp"` or
    `#include "bbsolver/solve/solver_reporting.hpp"` would silently expand the
    transitive header surface every downstream consumer pulls in.
    This check fails fast on that pollution.
    """
    import re

    include_re = re.compile(r'#include\s+"([^"]+)"')
    findings = []
    for facade_name in FACADE_INCLUDES.keys():
        facade_path = _ms_path(facade_name)
        if not facade_path.exists():
            # Presence is enforced by test_facades_reexport_their_subheaders.
            continue
        text = _strip_line_comments(_read_text(facade_path))
        for line_no, raw_line in enumerate(text.splitlines(), start=1):
            match = include_re.search(raw_line)
            if not match:
                continue
            included = match.group(1)
            stem = Path(included).name
            if not stem.startswith("motion_smooth_"):
                findings.append(
                    f"{facade_name}:{line_no}: non-motion_smooth "
                    f'quoted include `{included}` — façade headers '
                    "must be pure motion_smooth re-export shims"
                )
    assert not findings, (
        "Façade quoted-include purity violated:\n  "
        + "\n  ".join(findings)
    )


def test_ms_extracted_files_are_stl_self_sufficient() -> None:
    """ direct-include / self-sufficient lock for every MS-extracted
    motion_smooth file (façade headers, sub-headers, and bodies).

    Wider than  (which originally covered only SUBMODULE_SYMBOLS
    sub-headers). Scope is now the union of FACADE_INCLUDES keys,
    SUBMODULE_SYMBOLS keys, and MS_EXTRACTED_CPPS. The principle:
    every TU should include what it uses directly. A sub-header that
    uses `std::vector<X>` in a signature but does not `#include
    <vector>` compiles today only because every current consumer also
    includes `<vector>` before reaching the sub-header. A body that
    uses `std::optional` but only inherits `<optional>` transitively
    through its `.hpp` is similarly brittle. Locking direct-include
    sufficiency here makes each file consumable in isolation — the
    include-cleaner contract every integration reviewer expects to be
    able to rely on.

    The check accepts both angle-bracket (`#include <vector>`) and
    quoted (`#include "vector"`) forms even though only the
    angle-bracket form is conventional for STL — a future tooling
    pass that rewrites includes would otherwise force a brittle
    coupled change.
    """
    scope: set[str] = set()
    scope.update(FACADE_INCLUDES.keys())
    scope.update(SUBMODULE_SYMBOLS.keys())
    scope.update(MS_EXTRACTED_CPPS)

    findings = []
    for file_name in sorted(scope):
        file_path = _ms_path(file_name)
        if not file_path.exists():
            continue  # Orphan/missing checks own this.
        text = _strip_line_comments(_read_text(file_path))
        for identifier, required_include in STL_SELF_SUFFICIENCY:
            if identifier not in text:
                continue
            stripped = required_include.strip("<>")
            angle_form = f"#include <{stripped}>"
            quoted_form = f'#include "{stripped}"'
            if angle_form in text or quoted_form in text:
                continue
            findings.append(
                f"{file_name}: references `{identifier}` but does "
                f"not `#include {required_include}` — the file is not "
                "direct-include self-sufficient for this identifier"
            )
    assert not findings, (
        "MS-extracted file STL self-sufficiency violated. "
        "Findings:\n  " + "\n  ".join(findings)
    )


def test_ms_extracted_non_motion_smooth_allowlist_entries_exist() -> None:
    """ allowlist-existence lock: every entry in
    MS_EXTRACTED_NON_MOTION_SMOOTH_ALLOWLIST must exist as a file
    under either solver/src/ or the namespaced solver/include/ root.

    The forward leg permits an MS file to `#include` a header
    in this allowlist; the reverse leg asserts each allowlist
    entry is actually `#include`d by some MS file. Neither check
    verifies the file referenced by the allowlist entry exists on
    disk. If `dp_placer.hpp` were renamed during a refactor and the
    allowlist not updated, MS files including it would fail to
    compile but the allowlist policy would still PASS — the data is
    pointing at nothing. This check catches that rot.
    """
    # Allowlist entries are non-motion-smooth headers; they may live in
    # flat `solver/src/` (legacy/pre-migration headers like dp_placer.hpp)
    # or under any namespaced area below `solver/include/bbsolver/`. The
    # existence check searches both roots recursively.
    def _allowlist_entry_exists(name: str) -> bool:
        if (SRC / name).exists():
            return True
        for p in (SOLVER / "include").rglob(name):
            if p.is_file():
                return True
        return False

    missing = sorted(
        name
        for name in MS_EXTRACTED_NON_MOTION_SMOOTH_ALLOWLIST
        if not _allowlist_entry_exists(name)
    )
    assert not missing, (
        "MS_EXTRACTED_NON_MOTION_SMOOTH_ALLOWLIST references headers "
        "that do not exist under solver/src/ or solver/include/. Either "
        "restore the file or remove the stale allowlist entry:\n  "
        + "\n  ".join(missing)
    )


def test_ms_extracted_subheaders_directly_include_cross_ms_symbol_owners() -> None:
    """ cross-MS direct-include lock.

    SUBMODULE_SYMBOLS maps each sub-header to the symbols it owns
    (declares as the canonical site). If sub-header A references a
    symbol owned by sub-header B in its source text, A must
    `#include "B"` — otherwise A compiles only because some
    transitive consumer happens to include B first, which is a
    fragile invariant the include-cleaner contract should not have
    to discover at downstream build time.

    The check builds an owner map from SUBMODULE_SYMBOLS (`struct X`
    entries normalize to identifier `X` so the scan matches both
    `struct X` and bare-identifier `X` references), then for each
    sub-header scans every OTHER sub-header's symbols. A hit without
    the required include fires.

    Currently 10 cross-MS references exist across 3 sub-headers
    (shape_flat_notes, shape_flat_closed_loop, shape_flat_key_emission
    consume the upstream the corresponding sub-modules schedule +  adaptive types) and
    every one has its direct include.  locks that clean state.
    """
    owner_map: dict[str, str] = {}
    for header, symbols in SUBMODULE_SYMBOLS.items():
        for raw in symbols:
            normalized = raw.replace("struct ", "").strip()
            owner_map[normalized] = header

    findings = []
    for header in sorted(SUBMODULE_SYMBOLS.keys()):
        header_path = SRC / header
        if not header_path.exists():
            continue  # Orphan/missing checks own this.
        text = _strip_line_comments(_read_text(header_path))
        for symbol, owning_header in owner_map.items():
            if owning_header == header:
                continue  # Self-owned; the header's own decl is fine.
            if symbol not in text:
                continue
            include_directive = f'#include "{owning_header}"'
            if include_directive in text:
                continue
            findings.append(
                f"{header}: references cross-MS symbol `{symbol}` "
                f"owned by `{owning_header}` but does not "
                f"`#include \"{owning_header}\"` — the sub-header is "
                "not direct-include self-sufficient for this symbol"
            )
    assert not findings, (
        "MS-extracted cross-MS direct-include contract violated. "
        "Findings:\n  " + "\n  ".join(findings)
    )


def test_ms_extracted_sub_modules_do_not_back_edge_their_facade() -> None:
    """ façade back-edge lock.

    For each sub-header / sub-body pair where the sub-header is
    re-exported by a façade in FACADE_INCLUDES, neither the
    sub-header nor its paired body may `#include` that façade. A
    back-edge (sub → façade → sub) creates a cycle in the include
    graph that compiles only because of include guards, and it
    architecturally inverts the leaf-vs-aggregator role: sub-modules
    should be consumable as leaves, the façade is the aggregator.

    Façade bodies themselves (e.g. motion_smooth_shape_loop.cpp, the
    29-LOC residual body of the shape_loop façade) DO include their
    own façade via the self-include convention — those are the
    façade's TU, not back-edges, and they are not in scope here
    because the orchestrators are not in SUBMODULE_SYMBOLS.
    """
    sub_to_facade: dict[str, str] = {}
    for facade, subheaders in FACADE_INCLUDES.items():
        for sub in subheaders:
            sub_to_facade[sub] = facade

    findings = []
    for header in sorted(SUBMODULE_SYMBOLS.keys()):
        facade = sub_to_facade.get(header)
        if not facade:
            continue  # Sub-header is not re-exported by any façade.
        directive = f'#include "{facade}"'

        header_path = SRC / header
        if header_path.exists():
            text = _strip_line_comments(_read_text(header_path))
            if directive in text:
                findings.append(
                    f"{header}: `#include \"{facade}\"` creates a "
                    "back-edge to its own façade — sub-headers must "
                    "remain leaves in the include graph"
                )

        body = header.replace(".hpp", ".cpp")
        if body not in MS_EXTRACTED_CPPS:
            continue
        body_path = SRC / body
        if body_path.exists():
            text = _strip_line_comments(_read_text(body_path))
            if directive in text:
                findings.append(
                    f"{body}: `#include \"{facade}\"` creates a "
                    "back-edge to its sub-module's façade — sub-module "
                    "bodies must include their own sub-header, not the "
                    "façade that re-exports them"
                )
    assert not findings, (
        "MS-extracted sub-module façade back-edge contract violated. "
        "Findings:\n  " + "\n  ".join(findings)
    )


def test_ms_extracted_files_remain_below_monolith_ceiling() -> None:
    """ anti-monolith guardrail: every MS-extracted motion_smooth
    file must remain below a category-specific LOC ceiling.

      * façade headers (FACADE_INCLUDES keys)
            ≤ MONOLITH_CEILING_FACADE_HEADER_LOC
      * sub-headers (SUBMODULE_SYMBOLS keys)
            ≤ MONOLITH_CEILING_SUB_HEADER_LOC
      * MS-extracted bodies (MS_EXTRACTED_CPPS)
            ≤ MONOLITH_CEILING_SUB_BODY_LOC

    The original pre-MS monoliths were 360-472 LOC. The ceilings are
    set generously above current observed sizes (max body LOC today
    is 183) so steady evolution does not trip the check, but tight
    enough that a sub-module regressing toward original monolith
    dimensions will fire before re-monolithization is hard to unwind.

    LOC is counted by `len(splitlines())` of the raw file (comments
    and blanks included). Substring-counting line breaks is sufficient
    for a coarse anti-monolith signal — a regression that bloats a
    file with comments to dodge the ceiling would be visible in code
    review anyway.

    Pre-existing legacy files (PRE_EXISTING_MOTION_SMOOTH_HEADERS /
    PRE_EXISTING_MOTION_SMOOTH_BODIES) are intentionally out of scope:
    reduction_gate.cpp (350) and shape_quality.cpp (331) live at
    sizes the MS lane is not chartered to police.
    """
    def loc(path: Path) -> int:
        return len(path.read_text(encoding="utf-8").splitlines())

    findings = []
    for facade_name in sorted(FACADE_INCLUDES.keys()):
        path = _ms_path(facade_name)
        if not path.exists():
            continue
        size = loc(path)
        if size > MONOLITH_CEILING_FACADE_HEADER_LOC:
            findings.append(
                f"{facade_name}: {size} LOC exceeds façade-header "
                f"ceiling of {MONOLITH_CEILING_FACADE_HEADER_LOC} — "
                "façade has grown beyond pure-shim role; split or "
                "demote residual code into a sub-header"
            )
    for sub_header in sorted(SUBMODULE_SYMBOLS.keys()):
        path = _ms_path(sub_header)
        if not path.exists():
            continue
        size = loc(path)
        if size > MONOLITH_CEILING_SUB_HEADER_LOC:
            findings.append(
                f"{sub_header}: {size} LOC exceeds sub-header ceiling "
                f"of {MONOLITH_CEILING_SUB_HEADER_LOC} — sub-header "
                "has accreted enough surface to suggest the sub-module "
                "is rejoining its façade siblings; consider a further "
                "split"
            )
    for body in sorted(MS_EXTRACTED_CPPS):
        path = SRC / body
        if not path.exists():
            continue
        size = loc(path)
        if size > MONOLITH_CEILING_SUB_BODY_LOC:
            findings.append(
                f"{body}: {size} LOC exceeds MS-body ceiling of "
                f"{MONOLITH_CEILING_SUB_BODY_LOC} — the original "
                "pre-MS monoliths were 360-472 LOC; this sub-module "
                "is regressing toward monolith dimensions and should "
                "be split before merging"
            )
    assert not findings, (
        "Anti-monolith ceiling violated. Findings:\n  "
        + "\n  ".join(findings)
    )


def main() -> int:
    tests = [
        #  structural checks.
        test_facades_reexport_their_subheaders,
        test_submodule_headers_declare_expected_symbols,
        test_deleted_bodies_stay_deleted,
        test_shape_quality_does_not_redefine_curve_helpers,
        test_facade_headers_are_thin,
        test_ms_extracted_cpps_covers_every_submodule_body,
        test_motion_smooth_policy_file_registers_all_defined_checks,
        test_motion_smooth_facades_have_exact_motion_smooth_include_count,
        test_motion_smooth_policy_registered_in_quick_guard,
        test_motion_smooth_facade_includes_carry_iwyu_keep_pragma,
        test_shape_flat_facade_does_not_re_export_internal_helpers,
        test_motion_smooth_sources_not_excluded_from_cmake_glob,
        test_motion_smooth_subheaders_only_declare_expected_symbols,
        test_ms_extracted_cpps_only_include_allowed_non_motion_smooth_headers,
        test_submodule_bodies_pair_with_their_headers,
        #  integration-readiness checks.
        test_no_stale_motion_smooth_includes,
        test_motion_smooth_symbol_ownership_is_unique,
        test_submodule_bodies_define_their_declared_symbols,
        test_focused_test_files_remain_present,
        #  merge-preflight checks.
        test_motion_smooth_headers_have_pragma_once,
        test_motion_smooth_tolerance_constants_remain_with_owning_tu,
        test_motion_smooth_notes_strings_remain_with_owning_tu,
        test_motion_smooth_cpp_files_self_include_their_header,
        test_no_orphan_motion_smooth_headers,
        test_no_orphan_motion_smooth_bodies,
        test_ms_extracted_non_motion_smooth_allowlist_has_no_stale_entries,
        test_focused_test_files_reference_motion_smooth_surface,
        test_pure_shim_facades_are_facade_includes_keys,
        test_motion_smooth_files_have_no_unapproved_orchestration_dependencies,
        test_motion_smooth_facade_quoted_includes_are_motion_smooth_only,
        test_ms_extracted_headers_only_include_allowed_non_motion_smooth_headers,
        test_ms_extracted_files_are_stl_self_sufficient,
        test_ms_extracted_non_motion_smooth_allowlist_entries_exist,
        test_ms_extracted_subheaders_directly_include_cross_ms_symbol_owners,
        test_ms_extracted_sub_modules_do_not_back_edge_their_facade,
        test_ms_extracted_files_remain_below_monolith_ceiling,
    ]
    failures = 0
    for test in tests:
        try:
            test()
            print(f"[PASS] {test.__name__}")
        except AssertionError as exc:
            failures += 1
            print(f"[FAIL] {test.__name__}: {exc}")
    print(f"summary: {len(tests) - failures} passed, {failures} failed")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
