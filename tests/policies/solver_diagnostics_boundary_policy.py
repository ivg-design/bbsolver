#!/usr/bin/env python3
"""Source-level policy for the Phase 3 diagnostics boundary.

Diagnostics are intentionally opt-in and orchestration-owned. Pure policy,
math, formatting, and helper modules may produce notes/progress payloads, but
they must not depend on DiagnosticsWriter or write diagnostics directly. This
policy is standalone for D6; a later governance slice may add it to
tools/p3_refactor_guard.py after active guard/header work settles.
"""

from __future__ import annotations

import re
from pathlib import Path

from _solver_policy_paths import find_solver_layout


ROOT, SOLVER = find_solver_layout(__file__)
SOLVER_SRC = SOLVER / "src"
PUBLIC_ROOT = SOLVER / "include" / "bbsolver"
GUIDELINES = ROOT / "docs" / "project" / "P3_REFACTOR_GUIDELINES.md"


# Maintained allowlist for modules that own command orchestration or the
# diagnostics implementation itself. All other solver modules are treated as
# pure/leaf from the DiagnosticsWriter boundary perspective.
DIAGNOSTICS_BOUNDARY_ALLOWLIST = {
    "solver/include/bbsolver/diagnostics/solver_diagnostics.hpp",
    "solver/include/bbsolver/diagnostics/solver_diagnostic_events.hpp",
    "solver/include/bbsolver/solve/solve_lifecycle_reporting.hpp",
    "solver/src/diagnostics/solver_diagnostics.cpp",
    "solver/src/diagnostics/solver_diagnostic_events.cpp",
    "solver/src/solve/solve_command.cpp",
    "solver/src/solve/solve_lifecycle_reporting.cpp",
}

DIAGNOSTIC_EVENT_MODULES = {
    "solver/include/bbsolver/diagnostics/solver_diagnostic_events.hpp",
    "solver/src/diagnostics/solver_diagnostic_events.cpp",
}

FORBIDDEN_LEAF_PATTERNS = {
    '#include "bbsolver/diagnostics/solver_diagnostics.hpp"': "leaf modules must not include the diagnostics writer",
    "DiagnosticsWriter": "leaf modules must not instantiate or name DiagnosticsWriter",
    "diagnostics.Emit(": "leaf modules must not emit diagnostics directly",
}

EVENT_BUILDER_RE = re.compile(
    r"(?m)^\s*nlohmann::json\s+"
    r"(Build(?![A-Za-z0-9_]*ProgressEvent\b)[A-Za-z0-9_]*Event)\s*\("
)


def _rel(path: Path) -> str:
    if path.is_relative_to(SOLVER):
        return "solver/" + path.relative_to(SOLVER).as_posix()
    return path.relative_to(ROOT).as_posix()


def _solver_sources() -> list[Path]:
    return sorted(
        path
        for root in (SOLVER_SRC, PUBLIC_ROOT)
        for path in root.rglob("*")
        if path.suffix in {".cpp", ".hpp", ".h"}
    )


def _without_line_comments(text: str) -> str:
    return "\n".join(line.split("//", 1)[0] for line in text.splitlines())


def _section(text: str, heading: str, next_heading_level: str = "### ") -> str:
    start = text.find(heading)
    assert start >= 0, f"missing guideline heading: {heading}"
    end = text.find("\n" + next_heading_level, start + len(heading))
    if end < 0:
        end = len(text)
    return text[start:end]


def test_leaf_modules_do_not_use_diagnostics_writer() -> None:
    offenders: list[str] = []
    for path in _solver_sources():
        rel = _rel(path)
        if rel in DIAGNOSTICS_BOUNDARY_ALLOWLIST:
            continue
        text = _without_line_comments(path.read_text(encoding="utf-8"))
        for token, reason in FORBIDDEN_LEAF_PATTERNS.items():
            if token in text:
                offenders.append(f"{rel}: {reason}: {token!r}")
    assert not offenders, "diagnostics boundary violations:\n" + "\n".join(offenders)


def test_nested_solver_modules_are_part_of_diagnostics_boundary() -> None:
    rel_paths = {_rel(path) for path in _solver_sources()}
    assert "solver/src/io/io_json.cpp" in rel_paths, (
        "diagnostics boundary policy must recurse into solver/src/<area>/ modules"
    )
    assert "solver/src/app/cli_options.cpp" in rel_paths, (
        "diagnostics boundary policy must cover nested app modules"
    )


def test_diagnostic_writer_and_event_modules_stay_separate() -> None:
    diagnostics_impl = (
        (SOLVER_SRC / "diagnostics" / "solver_diagnostics.cpp").read_text(encoding="utf-8")
        + "\n"
        + (
            PUBLIC_ROOT / "diagnostics" / "solver_diagnostics.hpp"
        ).read_text(encoding="utf-8")
    )
    event_impl = (
        (
            SOLVER_SRC / "diagnostics" / "solver_diagnostic_events.cpp"
        ).read_text(encoding="utf-8")
        + "\n"
        + (
            PUBLIC_ROOT / "diagnostics" / "solver_diagnostic_events.hpp"
        ).read_text(encoding="utf-8")
    )

    assert "class DiagnosticsWriter" in diagnostics_impl
    assert "DiagnosticsWriter::ToFile" in diagnostics_impl
    assert "DiagnosticsWriter::Emit" in diagnostics_impl
    assert "BuildSolveStartEvent" not in diagnostics_impl, (
        "DiagnosticsWriter module must not accumulate event-builder ownership"
    )

    assert '#include "bbsolver/diagnostics/solver_diagnostics.hpp"' not in event_impl, (
        "diagnostic event builders must stay pure and not include the writer"
    )
    assert "DiagnosticsWriter" not in event_impl, (
        "diagnostic event builders must not instantiate the writer"
    )
    assert ".Emit(" not in event_impl, (
        "diagnostic event builders must not emit diagnostics directly"
    )


def test_diagnostic_event_builders_live_only_in_event_modules() -> None:
    found: dict[str, list[str]] = {}
    for path in _solver_sources():
        text = path.read_text(encoding="utf-8")
        names = EVENT_BUILDER_RE.findall(text)
        if names:
            found[_rel(path)] = names

    assert found, "diagnostic event builders must exist"
    unexpected = {
        rel: names
        for rel, names in found.items()
        if rel not in DIAGNOSTIC_EVENT_MODULES
    }
    assert not unexpected, (
        "Build*Event diagnostic builders must stay in named diagnostics/event "
        f"modules, found drift: {unexpected!r}"
    )

    all_names = {name for names in found.values() for name in names}
    expected_names = {
        "BuildSolveStartEvent",
        "BuildParallelRuntimeEvent",
        "BuildSolveModeCapabilitiesEvent",
        "BuildCancellationStatusEvent",
        "BuildSolveCancelledEvent",
        "BuildSolveDoneEvent",
        "BuildBridgePruneResultEvent",
        "BuildBridgePrunePhaseEvent",
    }
    assert expected_names <= all_names, (
        "missing expected diagnostic event builders: "
        f"{sorted(expected_names - all_names)!r}"
    )


def test_guidelines_record_diagnostics_boundary_rule() -> None:
    if not GUIDELINES.exists():
        return
    text = GUIDELINES.read_text(encoding="utf-8")
    rule = _section(text, "10. Diagnostics are explicit at module boundaries.", "\n## ")
    for phrase in (
        "Pure policy/math/formatting modules must not emit diagnostics directly.",
        "Orchestration modules must declare their diagnostic surface",
        "`DiagnosticsWriter` calls stay at command/orchestration boundaries",
        "Diagnostic event builders must be side-effect free",
        "New modules that affect runtime behavior, cancellation, parallelism, progress cadence, validation, or solver acceptance",
    ):
        assert phrase in rule, f"diagnostics boundary rule missing phrase: {phrase!r}"


def test_bridge_prune_slices_record_diagnostics_decision_surface() -> None:
    if not GUIDELINES.exists():
        return
    text = GUIDELINES.read_text(encoding="utf-8")
    slice_7h = _section(
        text,
        "### Slice 7h - Post-Temporal Bridge-Prune Orchestrator",
    )
    slice_7i = _section(
        text,
        "### Slice 7i - Bridge-Prune Module Compaction",
    )

    for phrase in (
        "event payload formatting and timing accumulation",
        "unless a later diagnostics module consumes it",
        "progress JSON events",
        "telemetry note tokens",
        "final accepted/rejected note strings",
    ):
        assert phrase in slice_7h, (
            f"Slice 7h must record progress/notes/event-helper diagnostics "
            f"decision wording: {phrase!r}"
        )

    for phrase in (
        "progress JSON cadence",
        "telemetry note token spelling",
        "named companion module",
        "planning/progress helper",
    ):
        assert phrase in slice_7i, (
            f"Slice 7i must record progress/notes/event-helper diagnostics "
            f"decision wording: {phrase!r}"
        )


def test_main_command_extraction_slices_record_diagnostics_decisions() -> None:
    if not GUIDELINES.exists():
        return
    text = GUIDELINES.read_text(encoding="utf-8")
    expected: dict[str, tuple[str, ...]] = {
        "### Slice 28 - Solved Property Output Append": (
            "Diagnostics decision:",
            "caller-owned `ProgressWriter`",
            "emits no diagnostics directly",
            "adds no diagnostic schemas",
        ),
        "### Slice 29 - Weak Fallback Property Solver Header": (
            "Diagnostics decision:",
            "Diagnostics ownership: none",
            "does not",
            "emit diagnostics or progress events",
        ),
        "### Slice 30 - Path Solve Preparation Extraction": (
            "Diagnostics decision:",
            "caller-owned `ProgressWriter`",
            "no `DiagnosticsWriter` rows",
            "diagnostics remain owned by the command/lifecycle",
        ),
        "### Slice 31 - Property Temporal Prelude Extraction": (
            "Diagnostics decision:",
            "`property_start` progress row",
            "no `DiagnosticsWriter` rows",
            "adds no diagnostic event schema",
        ),
        "### Slice 32 - Property Post-Solve Processing Extraction": (
            "Diagnostics decision:",
            "progress and note emission only",
            "solve_command.cpp` remains the `DiagnosticsWriter`",
            "lifecycle owner",
            "emits no diagnostic event schemas directly",
        ),
        "### Slice 33 - Property Temporal Result Reporting Extraction": (
            "Diagnostics decision:",
            "`temporal_solve_done` progress row",
            "returns the cancel phase",
            "solve_command.cpp` remains the `DiagnosticsWriter`",
            "adds no diagnostic event schema",
        ),
        "### Slice 34 - Replacement Host-Equivalent Note Extraction": (
            "Diagnostics decision:",
            "result-note text only",
            "does not emit progress JSON",
            "does not call `DiagnosticsWriter`",
            "adds no diagnostic event schema",
        ),
        "### Slice 35 - Replacement Fast Vertex Preference Gate Extraction": (
            "Diagnostics decision:",
            "pure scalar acceptance policy",
            "emits no progress JSON",
            "does not call `DiagnosticsWriter`",
            "adds no diagnostic event schema",
        ),
        "### Slice 36 - Replacement Fast Vertex Preference Note Extraction": (
            "Diagnostics decision:",
            "result-note text only",
            "emits no progress JSON",
            "does not call `DiagnosticsWriter`",
            "adds no diagnostic event schema",
        ),
        "### Slice 37 - Replacement Source Validation Note Extraction": (
            "Diagnostics decision:",
            "result-note text only",
            "emits no progress JSON",
            "does not call `DiagnosticsWriter`",
            "adds no diagnostic event schema",
        ),
        "### Slice 38 - Replacement Initial Verdict Note Extraction": (
            "Diagnostics decision:",
            "result-note text only",
            "emits no progress JSON",
            "does not call `DiagnosticsWriter`",
            "adds no diagnostic event schema",
        ),
        "### Slice 39 - Replacement Retry Note Extraction": (
            "Diagnostics decision:",
            "result-note text only",
            "emits no progress JSON",
            "does not call `DiagnosticsWriter`",
            "adds no diagnostic event schema",
        ),
        "### Slice 40 - Replacement Retry Progress Event Extraction": (
            "Diagnostics decision:",
            "progress JSON event builders only",
            "caller-owned `ProgressWriter`",
            "do not call `ProgressWriter::Emit`",
            "do not call `DiagnosticsWriter`",
            "solve_command.cpp` remains the `DiagnosticsWriter`",
            "add no diagnostic event schema",
        ),
        "### Slice 41 - Replacement Validation Progress Event Extraction": (
            "Diagnostics decision:",
            "progress JSON event builders only",
            "caller-owned",
            "`ProgressWriter`",
            "do not call `ProgressWriter::Emit`",
            "do not call `DiagnosticsWriter`",
            "solve_command.cpp` remains",
            "`DiagnosticsWriter` owner",
            "add no diagnostic event schema",
        ),
        "### Slice 42 - Replacement Baseline Progress Event Extraction": (
            "Diagnostics decision:",
            "progress JSON event builders only",
            "caller-owned",
            "`ProgressWriter`",
            "do not call `ProgressWriter::Emit`",
            "do not call `DiagnosticsWriter`",
            "solve_command.cpp` remains the `DiagnosticsWriter`",
            "add no diagnostic event schema",
        ),
        "### Slice 43 - Replacement Validation Start Progress Event Extraction": (
            "Diagnostics decision:",
            "progress JSON event builder only",
            "caller-owned",
            "`ProgressWriter`",
            "does not call `ProgressWriter::Emit`",
            "does not call `DiagnosticsWriter`",
            "solve_command.cpp` remains the `DiagnosticsWriter`",
            "adds no diagnostic event schema",
        ),
        "### Slice 44 - Replacement Temporal Validation Options Extraction": (
            "Diagnostics decision:",
            "pure validation configuration helper only",
            "emits no progress JSON",
            "performs no cancellation checks",
            "does not call `ProgressWriter::Emit`",
            "does not call `DiagnosticsWriter`",
            "adds no diagnostic event schema",
            "C++ post-build diagnostics/lifecycle helpers",
        ),
        "### Slice 45 - Replacement Validation Summary Extraction": (
            "Diagnostics decision:",
            "pure scalar validation summary helper only",
            "emits no progress JSON",
            "performs no cancellation checks",
            "does not call `ProgressWriter::Emit`",
            "does not call `DiagnosticsWriter`",
            "adds no diagnostic event schema",
            "C++ post-build diagnostics/lifecycle helpers",
        ),
        "### Slice 46 - Replacement Retry Eligibility Extraction": (
            "Diagnostics decision:",
            "pure scalar retry-eligibility helper only",
            "emits no progress JSON",
            "performs no cancellation checks",
            "does not call `ProgressWriter::Emit`",
            "does not call `DiagnosticsWriter`",
            "adds no diagnostic event schema",
            "C++ post-build diagnostics/lifecycle helpers",
        ),
        "### Slice 47 - Replacement Fast Vertex Preference Input Extraction": (
            "Diagnostics decision:",
            "pure scalar fast-vertex preference input builder only",
            "emits no progress JSON",
            "performs no cancellation checks",
            "does not call `ProgressWriter::Emit`",
            "does not call `DiagnosticsWriter`",
            "adds no diagnostic event schema",
            "C++ post-build diagnostics/lifecycle helpers",
        ),
        "### Slice 48 - Replacement Fast Vertex Preference Note Input Extraction": (
            "Diagnostics decision:",
            "pure fast-vertex preference note-input builder only",
            "emits no progress JSON",
            "performs no cancellation checks",
            "does not call `ProgressWriter::Emit`",
            "does not call `DiagnosticsWriter`",
            "adds no diagnostic event schema",
            "C++ post-build diagnostics/lifecycle helpers",
        ),
        "### Slice 49 - Replacement Source Validation Note Input Extraction": (
            "Diagnostics decision:",
            "pure source-validation note-input builder only",
            "emits no progress JSON",
            "performs no cancellation checks",
            "does not call `ProgressWriter::Emit`",
            "does not call `DiagnosticsWriter`",
            "adds no diagnostic event schema",
            "C++ post-build diagnostics/lifecycle helpers",
        ),
        "### Slice 50 - Replacement Retry Result Note Input Extraction": (
            "Diagnostics decision:",
            "pure retry-result note-input builder only",
            "emits no progress JSON",
            "performs no cancellation checks",
            "does not call `ProgressWriter::Emit`",
            "does not call `DiagnosticsWriter`",
            "adds no diagnostic event schema",
            "C++ post-build diagnostics/lifecycle helpers",
        ),
        "### Slice 51 - Replacement Retry Eligibility Input Extraction": (
            "Diagnostics decision:",
            "pure scalar retry-eligibility input builder only",
            "emits no progress JSON",
            "performs no cancellation checks",
            "does not call `ProgressWriter::Emit`",
            "does not call `DiagnosticsWriter`",
            "adds no diagnostic event schema",
            "C++ post-build diagnostics/lifecycle helpers",
        ),
        "### Slice 52 - Replacement Retry Target Ladder Helper Extraction": (
            "Diagnostics decision:",
            "pure retry-target ladder helper only",
            "emits no progress JSON",
            "performs no cancellation checks",
            "does not call `ProgressWriter::Emit`",
            "does not call `DiagnosticsWriter`",
            "adds no diagnostic event schema",
            "C++ post-build diagnostics/lifecycle helpers",
        ),
        "### Slice 53 - Replacement Validation Summary Application Extraction": (
            "Diagnostics decision:",
            "validation-summary bookkeeping helper only",
            "emits no progress JSON",
            "performs no cancellation checks",
            "does not call `ProgressWriter::Emit`",
            "does not call `DiagnosticsWriter`",
            "adds no diagnostic event schema",
            "C++ post-build diagnostics/lifecycle helpers",
        ),
        "### Slice 54 - Replacement Note Append Helper Usage": (
            "Diagnostics decision:",
            "mainline note-append cleanup only",
            "emits no progress JSON",
            "performs no cancellation checks",
            "does not call `ProgressWriter::Emit`",
            "does not call `DiagnosticsWriter`",
            "adds no diagnostic event schema",
            "C++ post-build diagnostics/lifecycle helpers",
        ),
        "### Slice 55 - Replacement Fast Vertex Acceptance Extraction": (
            "Diagnostics decision:",
            "replacement fast-vertex acceptance helper only",
            "caller-owned `ProgressWriter`",
            "`replacement_fast_vertex_validation_done` progress row",
            "performs no cancellation checks",
            "does not call `DiagnosticsWriter`",
            "adds no diagnostic event schema",
            "C++ post-build diagnostics/lifecycle helpers",
        ),
        "### Slice 56 - Replacement Baseline Solve Extraction": (
            "Diagnostics decision:",
            "replacement baseline solve helper only",
            "caller-owned `ProgressWriter`",
            "`replacement_baseline_start`, placement, and done progress rows",
            "`path_replacement_baseline` cancel phase",
            "does not call `DiagnosticsWriter`",
            "adds no diagnostic event schema",
            "C++ post-build diagnostics/lifecycle helpers",
        ),
        "### Slice 57 - Replacement Candidate Validation Extraction": (
            "Diagnostics decision:",
            "replacement candidate validation helper only",
            "caller-owned `ProgressWriter`",
            "`replacement_validation_start` and `replacement_validation_done` progress rows",
            "performs no cancellation checks",
            "does not call `DiagnosticsWriter`",
            "adds no diagnostic event schema",
            "C++ post-build diagnostics/lifecycle helpers",
        ),
        "### Slice 58 - Replacement Initial/Fallback Decision Application Extraction": (
            "Diagnostics decision:",
            "replacement initial/fallback decision helper only",
            "emits no progress JSON",
            "performs no cancellation checks",
            "does not call `DiagnosticsWriter`",
            "adds no diagnostic event schema",
            "C++ post-build diagnostics/lifecycle helpers",
        ),
        "### Slice 59 - Replacement Retry Skipped Note Decision Extraction": (
            "Diagnostics decision:",
            "replacement retry-skipped note helper only",
            "emits no progress JSON",
            "performs no cancellation checks",
            "does not call `DiagnosticsWriter`",
            "adds no diagnostic event schema",
            "C++ post-build diagnostics/lifecycle helpers",
        ),
        "### Slice 60 - Replacement Retry Loop Extraction": (
            "Diagnostics decision:",
            "replacement retry-loop helper only",
            "caller-owned `ProgressWriter`",
            "`replacement_retry_start` and `replacement_retry_done` progress rows",
            "caller-owned cancel callback",
            "`path_replacement_retry` cancel phase",
            "does not call `DiagnosticsWriter`",
            "adds no diagnostic event schema",
            "C++ post-build diagnostics/lifecycle helpers",
        ),
        "### Slice 61 - Solve Parallel Runtime Scope Extraction": (
            "Diagnostics decision:",
            "solve parallel runtime scope helper only",
            "emits no progress JSON",
            "performs no cancellation checks",
            "does not call `DiagnosticsWriter`",
            "adds no diagnostic event schema",
            "`parallel_config` progress",
            "`parallel_runtime`",
            "C++ post-build diagnostics/lifecycle helpers",
        ),
        "### Slice 62 - Unified Spatial Warning Reporting Extraction": (
            "Diagnostics decision:",
            "solver reporting warning helper only",
            "pre-existing operator stderr warning",
            "caller-owned stream",
            "emits no progress JSON",
            "performs no cancellation checks",
            "does not call `DiagnosticsWriter`",
            "adds no diagnostic event schema",
            "C++ post-build diagnostics/lifecycle helpers",
        ),
        "### Slice 63 - Final Static Trim Note Reporting Extraction": (
            "Diagnostics decision:",
            "solver reporting final-static-trim helper only",
            "emits no stderr warning",
            "emits no progress JSON",
            "performs no cancellation checks",
            "does not call `DiagnosticsWriter`",
            "adds no diagnostic event schema",
            "C++ post-build diagnostics/lifecycle helpers",
        ),
        "### Slice 64 - Solve Lifecycle Output Helper Extraction": (
            "Diagnostics decision:",
            "solve lifecycle output helpers only",
            "pre-existing `solve_cancelled` diagnostic event",
            "WriteCancelledPartial",
            "pre-existing done progress row",
            "`solve_done` diagnostic event",
            "add no progress fields",
            "add no diagnostic event schema",
            "C++ post-build diagnostics/lifecycle caller boundary",
        ),
        "### Slice 65 - Post-Temporal Replacement Orchestration Extraction": (
            "Diagnostics decision:",
            "post-temporal replacement orchestration helper only",
            "caller-owned `ProgressWriter`",
            "caller-owned cancel callback",
            "`path_replacement_baseline`",
            "`path_replacement_retry`",
            "does not call `DiagnosticsWriter`",
            "adds no diagnostic event schema",
            "C++ post-build diagnostics/lifecycle caller boundary",
        ),
        "### Slice 66 - Solved Property Completion Extraction": (
            "Diagnostics decision:",
            "solved-property completion helper only",
            "caller-owned `ProgressWriter`",
            "caller-owned cancel callback",
            "pre-existing post-solve",
            "does not call `DiagnosticsWriter`",
            "adds no diagnostic event schema",
            "C++ post-build diagnostics/lifecycle caller boundary",
        ),
        "### Slice 67 - Solve Command Module Extraction": (
            "Diagnostics decision:",
            "solve command orchestration module",
            "retains the existing `DiagnosticsWriter` owner",
            "pre-existing lifecycle events",
            "moves no diagnostic event builders",
            "adds no diagnostic event schema",
        ),
        "### Slice 68 - Path Frame Fit Type Header Split": (
            "Diagnostics decision:",
            "none/type declarations only",
            "emits no progress JSON",
            "performs no cancellation checks",
            "does not call `DiagnosticsWriter`",
            "adds no diagnostic event schema",
        ),
        "### Slice 73 - Path Replacement Acceptance Module Split": (
            "Diagnostics decision:",
            "pure scalar acceptance helpers only",
            "emits no progress JSON",
            "performs no cancellation checks",
            "does not call `ProgressWriter::Emit`",
            "does not call `DiagnosticsWriter`",
            "adds no diagnostic event schema",
            "C++ post-build diagnostics/lifecycle emission remains",
        ),
        "### Slice 74 - Temporal Refit Helper Module Split": (
            "Diagnostics decision:",
            "caller-owned notes/progress callbacks only",
            "temporal-refit placement-progress",
            "cancellation callback checks",
            "emit no progress JSON directly",
            "do not call `ProgressWriter::Emit`",
            "do not call `DiagnosticsWriter`",
            "do not include `solver_diagnostics.hpp`",
            "add no diagnostic event schema",
            "C++ post-build diagnostics/lifecycle emission remains",
        ),
        "### Slice 75 - Target Folder Structure Foundation": (
            "Diagnostics decision:",
            "build/include layout only",
            "progress JSON",
            "performs no cancellation checks",
            "`ProgressWriter::Emit`",
            "does not call `DiagnosticsWriter`",
            "diagnostic event schema",
            "C++ post-build diagnostics/lifecycle emission remains",
        ),
        "### Slice 76 - Replacement Temporal Relaxed Fit Split": (
            "Diagnostics decision:",
            "pure replacement-temporal relaxed fit helpers only",
            "`SegmentFitResult` fields",
            "existing reason strings",
            "emits no progress JSON",
            "performs no cancellation checks",
            "`ProgressWriter::Emit`",
            "does not call `DiagnosticsWriter`",
            "`solver_diagnostics.hpp`",
            "adds no diagnostic event schema",
            "C++ post-build",
        ),
        "### Slice 77 - Standalone bbsolver Namespace and First App/IO Layout": (
            "Diagnostics decision:",
            "structural namespace/include/layout migration only",
            "emit no progress JSON",
            "perform no cancellation",
            "`ProgressWriter::Emit`",
            "do not call `DiagnosticsWriter`",
            "`solver_diagnostics.hpp`",
            "add no diagnostic event schema",
            "recurses into",
        ),
    }
    for heading, phrases in expected.items():
        section = _section(text, heading)
        for phrase in phrases:
            assert phrase in section, (
                f"{heading} must record diagnostics boundary wording: "
                f"{phrase!r}"
            )


def main() -> int:
    tests = [
        test_leaf_modules_do_not_use_diagnostics_writer,
        test_nested_solver_modules_are_part_of_diagnostics_boundary,
        test_diagnostic_writer_and_event_modules_stay_separate,
        test_diagnostic_event_builders_live_only_in_event_modules,
        test_guidelines_record_diagnostics_boundary_rule,
        test_bridge_prune_slices_record_diagnostics_decision_surface,
        test_main_command_extraction_slices_record_diagnostics_decisions,
    ]
    failures = 0
    for test in tests:
        try:
            test()
        except AssertionError as exc:
            failures += 1
            print(f"[FAIL] {test.__name__}: {exc}")
        else:
            print(f"[PASS] {test.__name__}")
    print(f"summary: {len(tests) - failures} passed, {failures} failed")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
