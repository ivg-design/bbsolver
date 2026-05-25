#!/usr/bin/env python3
"""main.cpp dispatch-only lock.

`solver/src/app/main.cpp` is a pure command-dispatch entry point that:

  * includes only the command headers (`solve_command.hpp`,
    `bbsolver/verify/verify_dump_commands.hpp`) plus `cli_options.hpp` for the
    help/version path
  * delegates solve/verify/dump to their respective command entry functions in
    the bbsolver namespace
  * holds zero diagnostics, lifecycle, progress, or solver orchestration code

This policy locks those dispatch-only structural invariants of main.cpp.
It is intentionally complementary to
`tests/policies/solver_diagnostics_boundary_policy.py`:

  * `solver_diagnostics_boundary_policy.py` treats main.cpp as a leaf and
    enforces the general leaf rule: no `#include "bbsolver/diagnostics/solver_diagnostics.hpp"`, no
    `DiagnosticsWriter` symbol, and no `diagnostics.Emit(` calls.
  * This policy locks dispatcher-specific structural rules: an LOC ceiling
    tied to the dispatch-only invariant ("main.cpp only shrinks"), forbidden
    lifecycle/progress/reporting includes, forbidden lifecycle/progress emit
    symbols, and required delegation to RunSolve.

The DiagnosticsWriter identifier and the `#include "bbsolver/diagnostics/solver_diagnostics.hpp"`
directive are deliberately not in the forbidden lists here; the diagnostics
boundary policy owns those checks. This keeps the two policies free of redundant
assertions while preserving full coverage: any regression on main.cpp fires from
exactly one policy.

Pure source-text checks. No subprocess calls; runs in well under a second.
Domain-header agnostic: does not reference `domain.h` or `domain.hpp` directly.
"""

from __future__ import annotations

from pathlib import Path

from _solver_policy_paths import find_solver_layout


ROOT, SOLVER = find_solver_layout(__file__)
MAIN_CPP = SOLVER / "src" / "app" / "main.cpp"


# Current main.cpp is 38 LOC; the ceiling allows headroom for future command
# additions without making the policy chase every trivial growth. A regression
# toward orchestration-in-main trips this well before main.cpp re-monolithizes.
MAIN_CPP_LINE_CEILING = 60

# Includes that signal main.cpp has acquired orchestration concerns other than
# the diagnostics writer itself, which is governed by
# solver_diagnostics_boundary_policy.py.
FORBIDDEN_MAIN_CPP_INCLUDES = (
    "bbsolver/diagnostics/solver_diagnostic_events.hpp",
    "bbsolver/progress/progress.hpp",
    "bbsolver/progress/solve_cancellation.hpp",
    "solver_diagnostic_events.hpp",
    "solve_lifecycle_reporting.hpp",
    "progress.hpp",
    "solve_cancellation.hpp",
    "solver_reporting.hpp",
)

# Identifiers whose appearance in main.cpp means the dispatch boundary has been
# violated. DiagnosticsWriter is covered by the diagnostics-boundary policy's
# leaf checks. ProgressWriter is dispatcher-specific because main.cpp does not
# pass anything down; it only dispatches.
FORBIDDEN_MAIN_CPP_IDENTIFIERS = (
    "ProgressWriter",
    "EmitSolveStartLifecycle",
    "EmitSolveDoneLifecycle",
    "EmitSolveCancelledLifecycle",
    "BuildSolveStartProgressEvent",
    "BuildParallelConfigProgressEvent",
    "BuildSolveDoneProgressEvent",
    "AppendSolverNote",
    "AccuracyGateOptimizationNote",
)

# RunSolve is the canonical solve-command dispatch target; absence means the
# solve subcommand was either inlined back into main or renamed without updating
# this policy.
REQUIRED_MAIN_CPP_DELEGATIONS = (
    "RunSolve",
)


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def _strip_line_comments(text: str) -> str:
    return "\n".join(line.split("//", 1)[0] for line in text.splitlines())


def test_main_cpp_remains_dispatch_only_size() -> None:
    """main.cpp must stay near its post-Slice-67 size."""
    assert MAIN_CPP.exists(), (
        f"{MAIN_CPP} not found; has the source layout changed?"
    )
    line_count = len(_read(MAIN_CPP).splitlines())
    assert line_count <= MAIN_CPP_LINE_CEILING, (
        f"solver/src/app/main.cpp is {line_count} LOC; ceiling is "
        f"{MAIN_CPP_LINE_CEILING}. main.cpp must remain dispatch-only "
        "('main.cpp only shrinks'). Extract new logic into a command module."
    )


def test_main_cpp_does_not_acquire_lifecycle_or_progress_includes() -> None:
    """main.cpp must not include lifecycle, progress, or reporting headers."""
    text = _strip_line_comments(_read(MAIN_CPP))
    findings = []
    for forbidden in FORBIDDEN_MAIN_CPP_INCLUDES:
        directive = f'#include "{forbidden}"'
        if directive in text:
            findings.append(
                f"`{directive}` is forbidden in main.cpp; move the consumer "
                "into solve_command.cpp or its downstream orchestrator"
            )
    assert not findings, (
        "main.cpp acquired orchestration includes:\n  "
        + "\n  ".join(findings)
    )


def test_main_cpp_does_not_reference_lifecycle_or_progress() -> None:
    """main.cpp must not name progress, lifecycle, or reporting builders."""
    text = _strip_line_comments(_read(MAIN_CPP))
    findings = []
    for forbidden in FORBIDDEN_MAIN_CPP_IDENTIFIERS:
        if forbidden in text:
            findings.append(
                f"`{forbidden}` appears in main.cpp; the symbol belongs in "
                "solve_command.cpp or downstream"
            )
    assert not findings, (
        "main.cpp acquired lifecycle / progress / note-builder references:\n  "
        + "\n  ".join(findings)
    )


def test_main_cpp_delegates_via_known_command_entry_points() -> None:
    """main.cpp must continue to delegate via the documented entry points."""
    text = _strip_line_comments(_read(MAIN_CPP))
    findings = []
    for symbol in REQUIRED_MAIN_CPP_DELEGATIONS:
        if symbol not in text:
            findings.append(
                f"main.cpp does not delegate via `{symbol}`; the solve command "
                "entry point lives in solve_command.cpp"
            )
    assert not findings, (
        "main.cpp delegation contract violated:\n  " + "\n  ".join(findings)
    )


def main() -> int:
    tests = [
        test_main_cpp_remains_dispatch_only_size,
        test_main_cpp_does_not_acquire_lifecycle_or_progress_includes,
        test_main_cpp_does_not_reference_lifecycle_or_progress,
        test_main_cpp_delegates_via_known_command_entry_points,
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
