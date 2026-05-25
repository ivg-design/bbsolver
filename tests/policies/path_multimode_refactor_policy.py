#!/usr/bin/env python3
"""Source-level policy for the path multimode module."""

from __future__ import annotations

from pathlib import Path

from _solver_policy_paths import find_solver_layout


ROOT, SOLVER = find_solver_layout(__file__)
SOLVER_SRC = SOLVER / "src"
# Slice 86: path_multimode moved into the Rive-style standalone layout.
# Headers live under solver/include/bbsolver/path/multimode/, sources
# under solver/src/path/multimode/.
PATH_MULTIMODE_PUBLIC_DIR = SOLVER / "include" / "bbsolver" / "path" / "multimode"
PATH_MULTIMODE_SRC_DIR = SOLVER_SRC / "path" / "multimode"
COORDINATOR = PATH_MULTIMODE_SRC_DIR / "path_multimode_solver.cpp"
MAX_COORDINATOR_LINES = 250

PURE_HELPER_FORBIDDEN_TOKENS = (
    "DiagnosticsWriter",
    "solver_diagnostics",
    "ProgressWriter",
    "ProgressEvent",
    "progress_events",
    "progress_writer",
    "WriteProgress",
    "EmitProgress",
    "JsonProgress",
)

EXTRACTED_BOUNDARY_DEFINITIONS = (
    "ShapeFlatInputValidation ValidateShapeFlatMultiModeInputs(",
    "ShapeFlatInputValidation ValidateShapeFlatLandmarkInput(",
    "std::string MultiModeRegionBudgetExceededNote(",
    "std::string MultiModeValidationBudgetExceededNote(",
    "std::string MultiModeCandidateKeyBudgetExceededNote(",
    "std::string MultiModeCandidateNote(",
)


def _text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def _line_count(path: Path) -> int:
    return len(_text(path).splitlines())


def _path_multimode_sources() -> list[Path]:
    paths = list(PATH_MULTIMODE_SRC_DIR.glob("path_multimode_*.cpp"))
    paths.extend(PATH_MULTIMODE_PUBLIC_DIR.glob("path_multimode_*.hpp"))
    return sorted(paths)


def _pure_helper_sources() -> list[Path]:
    return [
        path
        for path in _path_multimode_sources()
        if path.name not in {"path_multimode_solver.cpp", "path_multimode_solver.hpp"}
    ]


def test_path_multimode_solver_stays_under_coordinator_target() -> None:
    lines = _line_count(COORDINATOR)
    assert lines <= MAX_COORDINATOR_LINES, (
        f"{COORDINATOR.relative_to(ROOT)} has {lines} lines; "
        f"the M16 coordinator target is {MAX_COORDINATOR_LINES}"
    )


def test_solver_keeps_public_coordinator_entrypoints() -> None:
    text = _text(COORDINATOR)
    assert "PropertyKeys SolveShapeFlatMultiModeTemporal(" in text
    assert "std::vector<PropertyKeys> EmitShapeFlatLandmarkSubpathKeys(" in text
    assert "ValidateShapeFlatMultiModeInputs(original, reduced)" in text
    assert "ValidateShapeFlatLandmarkInput(reduced)" in text
    assert "MultiModeCandidateNote(" in text


def test_extracted_boundary_definitions_stay_out_of_coordinator() -> None:
    text = _text(COORDINATOR)
    for definition in EXTRACTED_BOUNDARY_DEFINITIONS:
        assert definition not in text, (
            f"{definition} must remain in an extracted path_multimode module"
        )


def test_pure_multimode_helpers_do_not_emit_runtime_diagnostics() -> None:
    for path in _pure_helper_sources():
        text = _text(path)
        for token in PURE_HELPER_FORBIDDEN_TOKENS:
            assert token not in text, (
                f"{path.relative_to(ROOT)} must stay pure; found {token!r}"
            )


def test_path_multimode_headers_use_hpp() -> None:
    # Check both the post-Slice-86 public-header location and the legacy
    # flat layout (the legacy check ensures no stale `.h` re-appears).
    legacy_flat = sorted(SOLVER_SRC.glob("path_multimode_*.h"))
    legacy_public = sorted(PATH_MULTIMODE_PUBLIC_DIR.glob("path_multimode_*.h"))
    legacy_headers = legacy_flat + legacy_public
    assert not legacy_headers, (
        "path_multimode headers must use .hpp: "
        + ", ".join(path.relative_to(ROOT).as_posix() for path in legacy_headers)
    )


def main() -> int:
    tests = [
        test_path_multimode_solver_stays_under_coordinator_target,
        test_solver_keeps_public_coordinator_entrypoints,
        test_extracted_boundary_definitions_stay_out_of_coordinator,
        test_pure_multimode_helpers_do_not_emit_runtime_diagnostics,
        test_path_multimode_headers_use_hpp,
    ]
    for test in tests:
        test()
        print(f"[PASS] {test.__name__}")
    print(f"summary: {len(tests)} passed, 0 failed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
