#!/usr/bin/env python3
"""Source-level policy for the temporal-refit module."""

from __future__ import annotations

import subprocess
from pathlib import Path

from _solver_policy_paths import find_solver_layout, solver_path


ROOT, SOLVER = find_solver_layout(__file__)
SOLVER_SRC = SOLVER / "src"
PUBLIC_REFIT_ROOT = SOLVER / "include" / "bbsolver" / "temporal" / "refit"
SRC_REFIT_ROOT = SOLVER_SRC / "temporal" / "refit"
CHECKLIST = ROOT / "tests" / "temporal_refit_integration_checklist.md"
FACADE = SRC_REFIT_ROOT / "temporal_refit.cpp"
PUBLIC_HEADER = PUBLIC_REFIT_ROOT / "temporal_refit.hpp"
MAX_FACADE_LINES = 180
MAX_PUBLIC_HEADER_LINES = 250
MAX_HELPER_CPP_LINES = 180
MAX_HELPER_HPP_LINES = 80
HELPER_PREFIX = "temporal_refit_"
HELPER_EXEMPT = {"temporal_refit.cpp", "temporal_refit.hpp"}

EXPECTED_PUBLIC_HEADER_EXPORTS = (
    "struct TemporalRefitOptions",
    "struct TemporalRefitResult",
    "TemporalRefitResult TryTemporalRefitKeyReduction(",
    "PropertySamples ResampleAcceptedAtSourceTimes(",
    "bool ValidateRefitAgainstSource(",
)

FORBIDDEN_PUBLIC_HEADER_EXPORTS = (
    "StrictPropertyCeiling(",
    "RelativeCeilingFromBaseline(",
    "TemporalRefitIsCustomProperty(",
    "TemporalRefitExpectedDimensions(",
    "TemporalRefitValuesMatchDimensions(",
    "TemporalRefitIsShapeFlatProperty(",
    "IsValidTemporalRefitShapeFlatValue(",
    "TemporalRefitShapeFlatTopologyMatches(",
    "AllTemporalRefitSourceSamplesAreValidShapeFlat(",
    "AllTemporalRefitShapeFlatKeysHaveStableTopology(",
    "RefitStructuralRejection(",
    "TwoEndpointCandidate(",
    "BuildTemporalRefitNotes(",
    "TemporalRefitValidationNote(",
)

FORBIDDEN_DIAGNOSTIC_TOKENS = (
    "solve_lifecycle_reporting.hpp",
    "solver_diagnostics.hpp",
    "solver_reporting.hpp",
    "DiagnosticsWriter",
    "diagnostics.Emit(",
    "BuildBridgePrune",
    "BuildSolve",
    "nlohmann::json",
)

FORBIDDEN_HELPER_ORCHESTRATION_INCLUDES = (
    '#include "main.cpp"',
    '#include "bbsolver/path/replacement/path_replacement_decision_apply.hpp"',
    '#include "bbsolver/path/replacement/path_replacement_post_temporal.hpp"',
    '#include "bbsolver/path/replacement/path_replacement_retry_loop.hpp"',
    '#include "bbsolver/path/frame_fit/path_frame_fit_types.hpp"',
    '#include "bbsolver/fit/segment_fitter.hpp"',
    '#include "bbsolver/solve/solve_command.hpp"',
    '#include "bbsolver/solve/solve_lifecycle_reporting.hpp"',
    '#include "bbsolver/runtime/solve_parallel_runtime_scope.hpp"',
    '#include "bbsolver/solve/solve_property_completion.hpp"',
    '#include "bbsolver/diagnostics/solver_diagnostic_events.hpp"',
    '#include "bbsolver/diagnostics/solver_diagnostics.hpp"',
    '#include "bbsolver/solve/solver_reporting.hpp"',
)

FORBIDDEN_PUBLIC_HEADER_INCLUDES = (
    '#include "main.cpp"',
    '#include "bbsolver/motion_smooth/motion_smooth_solver.hpp"',
    '#include "bbsolver/path/replacement/path_replacement_decision_apply.hpp"',
    '#include "bbsolver/path/replacement/path_replacement_post_temporal.hpp"',
    '#include "bbsolver/path/replacement/path_replacement_retry_loop.hpp"',
    '#include "bbsolver/path/frame_fit/path_frame_fit_types.hpp"',
    '#include "bbsolver/fit/segment_fitter.hpp"',
    '#include "bbsolver/solve/solve_command.hpp"',
    '#include "bbsolver/solve/solve_lifecycle_reporting.hpp"',
    '#include "bbsolver/runtime/solve_parallel_runtime_scope.hpp"',
    '#include "bbsolver/solve/solve_property_completion.hpp"',
    '#include "bbsolver/solve/solve_property_post_processing.hpp"',
    '#include "bbsolver/diagnostics/solver_diagnostic_events.hpp"',
    '#include "bbsolver/diagnostics/solver_diagnostics.hpp"',
    '#include "bbsolver/solve/solver_reporting.hpp"',
    '#include "bbsolver/temporal/refit/temporal_refit_budget.hpp"',
    '#include "bbsolver/temporal/refit/temporal_refit_candidate.hpp"',
    '#include "bbsolver/temporal/refit/temporal_refit_dimensions.hpp"',
    '#include "bbsolver/temporal/refit/temporal_refit_resample.hpp"',
    '#include "bbsolver/temporal/refit/temporal_refit_shape.hpp"',
    '#include "bbsolver/temporal/refit/temporal_refit_structural.hpp"',
    '#include "bbsolver/temporal/refit/temporal_refit_support.hpp"',
    '#include "bbsolver/temporal/refit/temporal_refit_validation.hpp"',
)

EXPECTED_HELPER_FILES = (
    "solver/src/temporal/refit/temporal_refit_budget.cpp",
    "solver/include/bbsolver/temporal/refit/temporal_refit_budget.hpp",
    "solver/src/temporal/refit/temporal_refit_candidate.cpp",
    "solver/include/bbsolver/temporal/refit/temporal_refit_candidate.hpp",
    "solver/src/temporal/refit/temporal_refit_dimensions.cpp",
    "solver/include/bbsolver/temporal/refit/temporal_refit_dimensions.hpp",
    "solver/src/temporal/refit/temporal_refit_gate.cpp",
    "solver/include/bbsolver/temporal/refit/temporal_refit_gate.hpp",
    "solver/src/temporal/refit/temporal_refit_resample.cpp",
    "solver/include/bbsolver/temporal/refit/temporal_refit_resample.hpp",
    "solver/src/temporal/refit/temporal_refit_shape.cpp",
    "solver/include/bbsolver/temporal/refit/temporal_refit_shape.hpp",
    "solver/src/temporal/refit/temporal_refit_structural.cpp",
    "solver/include/bbsolver/temporal/refit/temporal_refit_structural.hpp",
    "solver/src/temporal/refit/temporal_refit_support.cpp",
    "solver/include/bbsolver/temporal/refit/temporal_refit_support.hpp",
    "solver/src/temporal/refit/temporal_refit_validation.cpp",
    "solver/include/bbsolver/temporal/refit/temporal_refit_validation.hpp",
)

ALLOWED_READINESS_DIFF_PATHS = frozenset(
    {
        "solver/src/temporal/refit/temporal_refit.cpp",
        "solver/include/bbsolver/temporal/refit/temporal_refit.hpp",
        "solver/src/solve/solve_property_post_processing.cpp",
        "solver/tests/solver_unit/test_temporal_refit.cpp",
        "solver/tests/solver_unit/test_temporal_refit_gate.cpp",
        "docs/project/P3_REFACTOR_GUIDELINES.md",
        "progressRerport.md",
        "tools/p3_refactor_guard.py",
        "tests/p3_refactor_guard_policy.py",
        "tests/solver_diagnostics_boundary_policy.py",
        "tests/temporal_refit_integration_checklist.md",
        "tests/temporal_refit_refactor_policy.py",
    }
    | set(EXPECTED_HELPER_FILES)
)
IGNORED_LOCAL_WORKTREE_PATHS = frozenset({"AGENTS.md"})
IGNORED_LOCAL_WORKTREE_PREFIXES = (".maestri/",)
TEMPORAL_RELATED_PREFIXES = (
    "solver/src/temporal/refit/temporal_refit",
    "solver/include/bbsolver/temporal/refit/temporal_refit",
    "solver/tests/solver_unit/test_temporal_refit.cpp",
    "solver/tests/solver_unit/test_temporal_refit_gate.cpp",
    "tests/temporal_refit",
)
STANDALONE_NAMESPACE_MIGRATION_MARKERS = frozenset(
    {
        "solver/include/bbsolver/domain.hpp",
        "solver/include/bbsolver/app/cli_options.hpp",
        "solver/src/app/cli_options.cpp",
    }
)
STANDALONE_PACKAGE_REMEDIATION_MARKERS = frozenset(
    {
        "solver/CMakeLists.txt",
        "solver/include/bbsolver/app/cli_options.hpp",
        "solver/src/app/cli_options.cpp",
        "solver/protocol/samples.fbs",
        "solver/tests/package_smoke/main.cpp",
    }
)
STANDALONE_SURFACE_COMMENT_CLEANUP_PATHS = frozenset(
    {
        "progressRerport.md",
        "solver/src/motion_smooth/motion_smooth_shape_quality.cpp",
        "solver/src/path/geometry/path_outline_error.cpp",
        "solver/src/temporal/refit/temporal_refit.cpp",
        "solver/tests/policies/main_dispatch_only_policy.py",
        "solver/tests/policies/motion_smooth_facade_lock_policy.py",
        "solver/tests/policies/solver_layout_policy.py",
        "solver/tests/policies/temporal_refit_refactor_policy.py",
    }
)

EXPECTED_FACADE_INCLUDES = (
    '#include "bbsolver/fit/segment_fitter.hpp"',
    '#include "bbsolver/temporal/refit/temporal_refit_budget.hpp"',
    '#include "bbsolver/temporal/refit/temporal_refit_candidate.hpp"',
    '#include "bbsolver/temporal/refit/temporal_refit_resample.hpp"',
    '#include "bbsolver/temporal/refit/temporal_refit_structural.hpp"',
    '#include "bbsolver/temporal/refit/temporal_refit_support.hpp"',
    '#include "bbsolver/temporal/refit/temporal_refit_validation.hpp"',
)

EXTRACTED_DEFINITIONS = (
    "double StrictPropertyCeiling(",
    "bool TemporalRefitScreenGateEnabled(",
    "double StrictScreenCeiling(",
    "double RelativeCeilingFromBaseline(",
    "bool TemporalRefitIsCustomProperty(",
    "std::size_t TemporalRefitExpectedDimensions(",
    "bool TemporalRefitValuesMatchDimensions(",
    "bool AllTemporalRefitCandidateKeysMatchDimensions(",
    "bool TemporalRefitIsShapeFlatProperty(",
    "bool IsValidTemporalRefitShapeFlatValue(",
    "bool TemporalRefitShapeFlatTopologyMatches(",
    "bool AllTemporalRefitSourceSamplesAreValidShapeFlat(",
    "bool AllTemporalRefitShapeFlatKeysHaveStableTopology(",
    "PropertySamples ResampleShapeFlatAcceptedAtSourceTimes(",
    "PropertySamples ResampleAcceptedAtSourceTimes(",
    "std::string RefitStructuralRejection(",
    "bool ValidateShapeRefitAgainstSource(",
    "bool ValidateRefitAgainstSource(",
    "PropertyKeys TwoEndpointCandidate(",
)

FACADE_FORBIDDEN_BODY_TOKENS = (
    "namespace {",
    "std::numeric_limits<double>::infinity()",
    "ValidateKeys(source, candidate.keys",
    "PathTemporalValidationOptions",
    "EvalKeysAt(accepted_keys.keys",
    "source.samples_per_frame != 1",
    "ineligible_custom_property",
    "ineligible_shape_flat_source_malformed",
    "ineligible_shape_flat_key_topology",
)

EXPECTED_BOUNDARY_OWNERS = {
    "solver/src/temporal/refit/temporal_refit_budget.cpp": (
        "double StrictPropertyCeiling(",
        "bool TemporalRefitScreenGateEnabled(",
        "double StrictScreenCeiling(",
        "double RelativeCeilingFromBaseline(",
    ),
    "solver/src/temporal/refit/temporal_refit_candidate.cpp": (
        "PropertyKeys TwoEndpointCandidate(",
    ),
    "solver/src/temporal/refit/temporal_refit_dimensions.cpp": (
        "bool TemporalRefitIsCustomProperty(",
        "std::size_t TemporalRefitExpectedDimensions(",
        "bool TemporalRefitValuesMatchDimensions(",
        "bool AllTemporalRefitCandidateKeysMatchDimensions(",
    ),
    "solver/src/temporal/refit/temporal_refit_gate.cpp": (
        "bool PipelineAllowsTemporalRefit(",
    ),
    "solver/src/temporal/refit/temporal_refit_resample.cpp": (
        "PropertySamples ResampleAcceptedAtSourceTimes(",
    ),
    "solver/src/temporal/refit/temporal_refit_shape.cpp": (
        "bool TemporalRefitIsShapeFlatProperty(",
        "bool IsValidTemporalRefitShapeFlatValue(",
        "bool TemporalRefitShapeFlatTopologyMatches(",
        "bool AllTemporalRefitSourceSamplesAreValidShapeFlat(",
        "bool AllTemporalRefitShapeFlatKeysHaveStableTopology(",
        "PropertySamples ResampleShapeFlatAcceptedAtSourceTimes(",
        "bool ValidateShapeRefitAgainstSource(",
    ),
    "solver/src/temporal/refit/temporal_refit_structural.cpp": (
        "std::string RefitStructuralRejection(",
    ),
    "solver/src/temporal/refit/temporal_refit_validation.cpp": (
        "bool ValidateRefitAgainstSource(",
    ),
}

EXPECTED_HEADER_OWNERS = {
    "solver/include/bbsolver/temporal/refit/temporal_refit_budget.hpp": (
        "double StrictPropertyCeiling(",
        "bool TemporalRefitScreenGateEnabled(",
        "double StrictScreenCeiling(",
        "double RelativeCeilingFromBaseline(",
    ),
    "solver/include/bbsolver/temporal/refit/temporal_refit_candidate.hpp": (
        "PropertyKeys TwoEndpointCandidate(",
    ),
    "solver/include/bbsolver/temporal/refit/temporal_refit_dimensions.hpp": (
        "bool TemporalRefitIsCustomProperty(",
        "std::size_t TemporalRefitExpectedDimensions(",
        "bool TemporalRefitValuesMatchDimensions(",
        "bool AllTemporalRefitCandidateKeysMatchDimensions(",
    ),
    "solver/include/bbsolver/temporal/refit/temporal_refit_gate.hpp": (
        "bool PipelineAllowsTemporalRefit(",
    ),
    "solver/include/bbsolver/temporal/refit/temporal_refit_resample.hpp": (
        "PropertySamples ResampleAcceptedAtSourceTimes(",
    ),
    "solver/include/bbsolver/temporal/refit/temporal_refit_shape.hpp": (
        "bool TemporalRefitIsShapeFlatProperty(",
        "bool IsValidTemporalRefitShapeFlatValue(",
        "bool TemporalRefitShapeFlatTopologyMatches(",
        "bool AllTemporalRefitSourceSamplesAreValidShapeFlat(",
        "bool AllTemporalRefitShapeFlatKeysHaveStableTopology(",
        "PropertySamples ResampleShapeFlatAcceptedAtSourceTimes(",
        "bool ValidateShapeRefitAgainstSource(",
    ),
    "solver/include/bbsolver/temporal/refit/temporal_refit_structural.hpp": (
        "std::string RefitStructuralRejection(",
    ),
    "solver/include/bbsolver/temporal/refit/temporal_refit_support.hpp": (
        "void EmitTemporalRefitProgress(",
        "bool TemporalRefitCancelled(",
        "std::string BuildTemporalRefitNotes(",
        "std::string TemporalRefitValidationNote(",
    ),
    "solver/include/bbsolver/temporal/refit/temporal_refit_validation.hpp": (
        "bool ValidateRefitAgainstSource(",
    ),
}

EXPECTED_HEADER_INCLUDES = {
    "solver/include/bbsolver/temporal/refit/temporal_refit_budget.hpp": (
        '#include "bbsolver/domain.hpp"',
    ),
    "solver/include/bbsolver/temporal/refit/temporal_refit_candidate.hpp": (
        '#include "bbsolver/domain.hpp"',
    ),
    "solver/include/bbsolver/temporal/refit/temporal_refit_dimensions.hpp": (
        '#include "bbsolver/domain.hpp"',
        "#include <cstddef>",
        "#include <vector>",
    ),
    "solver/include/bbsolver/temporal/refit/temporal_refit_gate.hpp": (
        '#include "bbsolver/domain.hpp"',
    ),
    "solver/include/bbsolver/temporal/refit/temporal_refit_resample.hpp": (
        '#include "bbsolver/domain.hpp"',
    ),
    "solver/include/bbsolver/temporal/refit/temporal_refit_shape.hpp": (
        '#include "bbsolver/domain.hpp"',
        '#include "bbsolver/temporal/refit/temporal_refit.hpp"',
        "#include <vector>",
    ),
    "solver/include/bbsolver/temporal/refit/temporal_refit_structural.hpp": (
        '#include "bbsolver/domain.hpp"',
        "#include <string>",
    ),
    "solver/include/bbsolver/temporal/refit/temporal_refit_support.hpp": (
        '#include "bbsolver/temporal/refit/temporal_refit.hpp"',
        "#include <string>",
    ),
    "solver/include/bbsolver/temporal/refit/temporal_refit_validation.hpp": (
        '#include "bbsolver/domain.hpp"',
        '#include "bbsolver/temporal/refit/temporal_refit.hpp"',
    ),
}

EXPECTED_SOURCE_INCLUDES = {
    "solver/src/temporal/refit/temporal_refit_budget.cpp": (
        '#include "bbsolver/domain.hpp"',
        "#include <algorithm>",
    ),
    "solver/src/temporal/refit/temporal_refit_candidate.cpp": (
        '#include "bbsolver/domain.hpp"',
        "#include <utility>",
    ),
    "solver/src/temporal/refit/temporal_refit_dimensions.cpp": (
        '#include "bbsolver/domain.hpp"',
        "#include <algorithm>",
        "#include <cstddef>",
        "#include <vector>",
    ),
    "solver/src/temporal/refit/temporal_refit_gate.cpp": (
        '#include "bbsolver/domain.hpp"',
        '#include "bbsolver/routing/property_classification.hpp"',
        '#include "bbsolver/routing/solve_mode_policy.hpp"',
    ),
    "solver/src/temporal/refit/temporal_refit_resample.cpp": (
        '#include "bbsolver/domain.hpp"',
        '#include "bbsolver/temporal/refit/temporal_refit_dimensions.hpp"',
        '#include "bbsolver/temporal/refit/temporal_refit_shape.hpp"',
        '#include "bbsolver/verify/verifier.hpp"',
        "#include <utility>",
    ),
    "solver/src/temporal/refit/temporal_refit_shape.cpp": (
        '#include "bbsolver/domain.hpp"',
        '#include "bbsolver/path/temporal/path_temporal_validation.hpp"',
        '#include "bbsolver/temporal/refit/temporal_refit.hpp"',
        '#include "bbsolver/verify/verifier.hpp"',
        "#include <algorithm>",
        "#include <cmath>",
        "#include <limits>",
        "#include <utility>",
        "#include <vector>",
    ),
    "solver/src/temporal/refit/temporal_refit_structural.cpp": (
        '#include "bbsolver/domain.hpp"',
        '#include "bbsolver/temporal/refit/temporal_refit_dimensions.hpp"',
        '#include "bbsolver/temporal/refit/temporal_refit_shape.hpp"',
        "#include <cmath>",
        "#include <string>",
    ),
    "solver/src/temporal/refit/temporal_refit_support.cpp": (
        '#include "bbsolver/domain.hpp"',
        '#include "bbsolver/dp/dp_placer.hpp"',
        '#include "bbsolver/temporal/refit/temporal_refit.hpp"',
        "#include <string>",
    ),
    "solver/src/temporal/refit/temporal_refit_validation.cpp": (
        '#include "bbsolver/domain.hpp"',
        '#include "bbsolver/metrics/error_metrics.hpp"',
        '#include "bbsolver/temporal/refit/temporal_refit.hpp"',
        '#include "bbsolver/temporal/refit/temporal_refit_budget.hpp"',
        '#include "bbsolver/temporal/refit/temporal_refit_dimensions.hpp"',
        '#include "bbsolver/temporal/refit/temporal_refit_shape.hpp"',
        '#include "bbsolver/verify/verifier.hpp"',
        "#include <limits>",
    ),
}

EXPECTED_PUBLIC_HEADER_INCLUDES = (
    '#include "bbsolver/domain.hpp"',
    "#include <functional>",
    "#include <string>",
)


def _text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def _line_count(path: Path) -> int:
    return len(_text(path).splitlines())


def _helper_paths() -> list[Path]:
    return sorted(
        path
        for root in (SRC_REFIT_ROOT, PUBLIC_REFIT_ROOT)
        for path in root.glob(f"{HELPER_PREFIX}*")
        if path.name not in HELPER_EXEMPT and path.suffix in {".cpp", ".hpp"}
    )


def _without_line_comments(text: str) -> str:
    return "\n".join(line.split("//", 1)[0] for line in text.splitlines())


def _first_nonblank_line(path: Path) -> str:
    for line in _text(path).splitlines():
        if line.strip():
            return line.strip()
    return ""


def _git_paths(*args: str) -> set[str]:
    if not (ROOT / ".git").exists():
        return set()
    completed = subprocess.run(
        ["git", *args],
        cwd=ROOT,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
    )
    return {line.strip() for line in completed.stdout.splitlines() if line.strip()}


def _standalone_namespace_migration_active(changed_paths: set[str]) -> bool:
    """Allow global legacy-to-bbsolver structural migrations through this lane guard."""
    if STANDALONE_NAMESPACE_MIGRATION_MARKERS.issubset(changed_paths):
        return True
    if STANDALONE_PACKAGE_REMEDIATION_MARKERS.issubset(changed_paths):
        return True
    if changed_paths and changed_paths <= STANDALONE_SURFACE_COMMENT_CLEANUP_PATHS:
        return True
    rive_style_layout_markers = {
        "tests/solver_layout_policy.py",
        "docs/project/P3_REFACTOR_GUIDELINES.md",
    }
    migrated_area_header = any(
        path.startswith("solver/include/bbsolver/io/")
        or path.startswith("solver/include/bbsolver/diagnostics/")
        or path.startswith("solver/include/bbsolver/fit/")
        or path.startswith("solver/include/bbsolver/progress/")
        or path.startswith("solver/include/bbsolver/routing/")
        or path.startswith("solver/include/bbsolver/runtime/")
        or path.startswith("solver/include/bbsolver/samples/")
        or path.startswith("solver/include/bbsolver/shape/")
        or path.startswith("solver/include/bbsolver/solve/")
        or path.startswith("solver/include/bbsolver/metrics/")
        or path.startswith("solver/include/bbsolver/temporal/refit/")
        or path.startswith("solver/include/bbsolver/motion_smooth/")
        or path.startswith("solver/include/bbsolver/verify/")
        or path.startswith("solver/include/bbsolver/path/temporal/")
        or path.startswith("solver/include/bbsolver/path/decompose/")
        or path.startswith("solver/include/bbsolver/path/frame_fit/")
        or path.startswith("solver/include/bbsolver/path/multimode/")
        or path.startswith("solver/include/bbsolver/path/dense/")
        or path.startswith("solver/include/bbsolver/path/bridge_prune/")
        or path.startswith("solver/include/bbsolver/path/config/")
        or path.startswith("solver/include/bbsolver/path/reduction/")
        or path.startswith("solver/include/bbsolver/dp/")
        or path.startswith("solver/include/bbsolver/path/geometry/")
        or path.startswith("solver/include/bbsolver/path/fit/")
        or path.startswith("solver/include/bbsolver/path/replacement/")
        or path.startswith("solver/include/bbsolver/replacement_temporal/")
        for path in changed_paths
    )
    return rive_style_layout_markers.issubset(changed_paths) and migrated_area_header


def test_temporal_refit_readiness_diff_stays_scoped() -> None:
    changed_paths = (
        _git_paths("diff", "--name-only")
        | _git_paths("diff", "--cached", "--name-only")
        | _git_paths("ls-files", "--others", "--exclude-standard")
    )
    if _standalone_namespace_migration_active(changed_paths):
        return
    if not any(path.startswith(TEMPORAL_RELATED_PREFIXES) for path in changed_paths):
        return
    unexpected = sorted(
        path
        for path in changed_paths - ALLOWED_READINESS_DIFF_PATHS
        if path not in IGNORED_LOCAL_WORKTREE_PATHS
        and not path.startswith(IGNORED_LOCAL_WORKTREE_PREFIXES)
    )
    assert not unexpected, (
        "temporal-refit readiness lane must not touch non-temporal files: "
        + ", ".join(unexpected)
    )


def test_temporal_refit_facade_stays_compact() -> None:
    lines = _line_count(FACADE)
    assert lines <= MAX_FACADE_LINES, (
        f"{FACADE.relative_to(ROOT)} has {lines} lines; "
        f"temporal-refit facade target is {MAX_FACADE_LINES}"
    )


def test_temporal_refit_public_header_stays_under_header_cap() -> None:
    path = PUBLIC_HEADER
    lines = _line_count(path)
    assert lines <= MAX_PUBLIC_HEADER_LINES, (
        f"{path.relative_to(ROOT)} has {lines} lines; "
        f"public header target is {MAX_PUBLIC_HEADER_LINES}"
    )


def test_temporal_refit_public_header_exports_stay_narrow() -> None:
    text = _text(PUBLIC_HEADER)
    for token in EXPECTED_PUBLIC_HEADER_EXPORTS:
        assert token in text, f"temporal_refit.hpp must export {token}"
    for token in FORBIDDEN_PUBLIC_HEADER_EXPORTS:
        assert token not in text, (
            f"temporal_refit.hpp must not expose helper-only symbol {token}"
        )


def test_temporal_refit_public_header_includes_direct_dependencies() -> None:
    text = _text(PUBLIC_HEADER)
    assert "#pragma once" in text, "temporal_refit.hpp must use #pragma once"
    for include in EXPECTED_PUBLIC_HEADER_INCLUDES:
        assert include in text, (
            f"temporal_refit.hpp must include direct dependency {include}"
        )


def test_temporal_refit_public_header_stays_orchestration_free() -> None:
    text = _without_line_comments(_text(PUBLIC_HEADER))
    for include in FORBIDDEN_PUBLIC_HEADER_INCLUDES:
        assert include not in text, (
            f"temporal_refit.hpp must stay facade/API-only; found {include}"
        )


def test_temporal_refit_facade_keeps_orchestration_only() -> None:
    text = _text(FACADE)
    assert "TemporalRefitResult TryTemporalRefitKeyReduction(" in text
    assert "SolveProperty(" in text
    assert 'EmitTemporalRefitProgress(options, "temporal_refit_start"' in text
    assert 'EmitTemporalRefitProgress(options, "temporal_refit_done"' in text
    for include in EXPECTED_FACADE_INCLUDES:
        assert include in text, f"{FACADE.relative_to(ROOT)} must include {include}"
    for definition in EXTRACTED_DEFINITIONS:
        assert definition not in text, (
            f"{definition} must remain in a temporal_refit helper module"
        )
    for token in FACADE_FORBIDDEN_BODY_TOKENS:
        assert token not in text, (
            f"{FACADE.relative_to(ROOT)} must stay compact; found moved body token "
            f"{token!r}"
        )


def test_temporal_refit_helper_modules_are_present_compact_and_hpp_only() -> None:
    for rel_path in EXPECTED_HELPER_FILES:
        assert solver_path(SOLVER, rel_path).exists(), (
            f"missing temporal-refit helper {rel_path}"
        )

    legacy_headers = sorted(SOLVER_SRC.glob("temporal_refit_*.h"))
    legacy_headers.extend(PUBLIC_REFIT_ROOT.glob("temporal_refit_*.h"))
    assert not legacy_headers, (
        "temporal_refit headers must use.hpp: "
        + ", ".join(path.relative_to(ROOT).as_posix() for path in legacy_headers)
    )

    for path in _helper_paths():
        lines = _line_count(path)
        if path.suffix == ".cpp":
            assert lines <= MAX_HELPER_CPP_LINES, (
                f"{path.relative_to(ROOT)} has {lines} lines; "
                f"helper target is {MAX_HELPER_CPP_LINES}"
            )
        else:
            assert lines <= MAX_HELPER_HPP_LINES, (
                f"{path.relative_to(ROOT)} has {lines} lines; "
                f"header target is {MAX_HELPER_HPP_LINES}"
            )


def test_temporal_refit_helper_sources_include_own_header_first() -> None:
    for rel_path in EXPECTED_HELPER_FILES:
        path = solver_path(SOLVER, rel_path)
        if path.suffix != ".cpp":
            continue
        expected = f'#include "bbsolver/temporal/refit/{path.stem}.hpp"'
        assert _first_nonblank_line(path) == expected, (
            f"{rel_path} must include its own header first"
        )


def test_temporal_refit_helper_headers_own_expected_declarations() -> None:
    for rel_path, tokens in EXPECTED_HEADER_OWNERS.items():
        text = _text(solver_path(SOLVER, rel_path))
        for token in tokens:
            assert token in text, f"{token} must stay declared in {rel_path}"

    all_helper_header_text = "\n".join(
        _text(path) for path in _helper_paths() if path.suffix == ".hpp"
    )
    for rel_path, tokens in EXPECTED_HEADER_OWNERS.items():
        header_text = _text(solver_path(SOLVER, rel_path))
        for token in tokens:
            assert all_helper_header_text.count(token) == header_text.count(token), (
                f"{token} must be declared only by {rel_path}"
            )


def test_temporal_refit_helper_headers_include_direct_dependencies() -> None:
    for rel_path, includes in EXPECTED_HEADER_INCLUDES.items():
        text = _text(solver_path(SOLVER, rel_path))
        assert "#pragma once" in text, f"{rel_path} must use #pragma once"
        for include in includes:
            assert include in text, f"{rel_path} must include direct dependency {include}"


def test_temporal_refit_helper_sources_include_direct_dependencies() -> None:
    for rel_path, includes in EXPECTED_SOURCE_INCLUDES.items():
        text = _text(solver_path(SOLVER, rel_path))
        for include in includes:
            assert include in text, f"{rel_path} must include direct dependency {include}"


def test_temporal_refit_boundary_owners_are_explicit() -> None:
    for rel_path, tokens in EXPECTED_BOUNDARY_OWNERS.items():
        text = _text(solver_path(SOLVER, rel_path))
        for token in tokens:
            assert token in text, f"{token} must stay in {rel_path}"

    validation_text = _text(SRC_REFIT_ROOT / "temporal_refit_validation.cpp")
    for token in (
        "double StrictPropertyCeiling(",
        "double RelativeCeilingFromBaseline(",
        "std::string RefitStructuralRejection(",
        "PropertyKeys TwoEndpointCandidate(",
    ):
        assert token not in validation_text, (
            f"{token} must not drift back into temporal_refit_validation.cpp"
        )


def test_temporal_refit_helpers_respect_diagnostics_boundary() -> None:
    for path in _helper_paths() + [FACADE]:
        text = _without_line_comments(_text(path))
        for token in FORBIDDEN_DIAGNOSTIC_TOKENS:
            assert token not in text, (
                f"{path.relative_to(ROOT)} must not own diagnostics emission; "
                f"found {token!r}"
            )


def test_temporal_refit_helpers_do_not_absorb_orchestration_includes() -> None:
    for path in _helper_paths():
        text = _without_line_comments(_text(path))
        for include in FORBIDDEN_HELPER_ORCHESTRATION_INCLUDES:
            assert include not in text, (
                f"{path.relative_to(ROOT)} must stay pure; found {include}"
            )


def test_temporal_refit_facade_and_helpers_do_not_include_motion_smooth_modules() -> None:
    for path in _helper_paths() + [FACADE, PUBLIC_HEADER]:
        for line in _without_line_comments(_text(path)).splitlines():
            include = line.strip()
            assert not include.startswith('#include "motion_smooth'), (
                f"{path.relative_to(ROOT)} must not depend on motion_smooth "
                f"modules; found {include}"
            )


def test_temporal_refit_uses_domain_hpp_only() -> None:
    paths = (
        _helper_paths()
        + [FACADE, PUBLIC_HEADER]
        + [SOLVER / "tests" / "solver_unit" / "test_temporal_refit.cpp"]
    )
    for path in paths:
        text = _without_line_comments(_text(path))
        assert '#include "domain.h"' not in text
        assert '#include "../../solver/src/domain.h"' not in text


def test_temporal_refit_readiness_checklist_records_merge_contract() -> None:
    if not CHECKLIST.exists():
        return
    text = _text(CHECKLIST)
    for phrase in (
        "TR16",
        "TR18",
        "TR19",
        "TR20",
        "TR21",
        "TR22",
        "TR23-TR26",
        "TR27",
        "TR28-TR31",
        "TR32",
        "TR33-TR36",
        "TR37",
        "TR38-TR41",
        "TR42",
        "TR43-TR46",
        "TR47",
        "TR48-TR51",
        "TR52",
        "TR53-TR56",
        "TR57",
        "TR58-TR61",
        "TR62",
        "TR63-TR66",
        "TR67",
        "TR68-TR71",
        "TR72",
        "TR73-TR76",
        "TR77",
        "TR78-TR81",
        "TR82",
        "TR83-TR86",
        "TR87",
        "TR88-TR91",
        "TR92",
        "TR93-TR96",
        "TR97",
        "TR98-TR101",
        "TR102",
        "TR103-TR106",
        "TR107",
        "TR108-TR111",
        "TR112",
        "TR113-TR116",
        "TR117",
        "TR118-TR121",
        "TR122",
        "TR123-TR126",
        "TR127",
        "TR128-TR131",
        "TR132",
        "TR133-TR136",
        "TR142",
        "TR143-TR146",
        "TR147",
        "TR148-TR151",
        "TR152",
        "TR153-TR156",
        "TR157",
        "TR158-TR161",
        "TR162",
        "TR163-TR166",
        "TR167",
        "TR168-TR171",
        "TR172",
        "TR173",
        "TR174",
        "7a3eacc",
        "30c193d",
        "1c3fcec",
        "738521a",
        "80faad4",
        "7e51827",
        "cdeaa3b",
        "5a499b4",
        "320eccd",
        "b9daa58",
        "038be2e",
        "7ca745b",
        "b828bea",
        "9b4a8be",
        "8e69081",
        "e199a2d",
        "b59991e",
        "c914f9c",
        "4485457",
        "68f5a7c",
        "3e0d090",
        "7bc0f56",
        "9b1464d",
        "079cb9f",
        "8c8c2dc",
        "4022588",
        "201ea25",
        "4577ca0",
        "5751807",
        "539d633",
        "061c77c",
        "d16f3c8",
        "a2cb90d",
        "20a9529",
        "2230183",
        "7cbd52d",
        "cb975e5",
        "d2f92cd",
        "7468b56",
        "e493921",
        "599a77e",
        "9940264",
        "774ab29",
        "origin/feature/multicore-path-prune",
        "frozen worktree",
        "Do not integrate this readiness lane by editing",
        "Incoming off-limits paths are `main.cpp`, `path_fit_pipeline.*`, `path_replacement_notes.*`, `path_replacement_progress.*`, `path_replacement_preference.*`, `path_solver_config.*`, `path_frame_fit.hpp`, `path_replacement_target_ladder.*`",
        "Diagnostics decision remains caller-owned notes/progress only",
        "support-module cancellation, progress event defaults, note token formatting",
        "accepted refit progress coverage locks stage remapping",
        "pre-eligibility custom-property rejection returns the original accepted keys unchanged",
        "cancellation rejection preserves the original accepted keys unchanged",
        "degenerate no-op rejection preserves the original accepted keys unchanged",
        "structural subframe rejection preserves the original accepted keys unchanged",
        "structural dimension rejection preserves the original accepted keys unchanged",
        "structural endpoint-mismatch rejection preserves the original accepted keys unchanged",
        "shape-flat malformed-source rejection preserves the original accepted keys unchanged",
        "attempted over-budget rejection preserves the original accepted keys unchanged",
        "attempted no-gain rejection preserves the original accepted keys unchanged",
        "attempted over-budget and no-gain rejection checks share a single temporal-refit assertion helper",
        "attempted rejection helper now locks temporal-refit done progress payload",
        "accepted scalar and shape-flat refits propagate converged status plus max-error bookkeeping",
        "accepted shape-flat refits emit the same temporal_refit_start, temporal_refit_validate, and temporal_refit_done progress sequence",
        "attempted rejected refits now share a lifecycle assertion",
        "allowed temporal_refit progress-stage whitelist",
        "attempted rejected refits now require exactly one temporal_refit_start, one temporal_refit_validate, and one final temporal_refit_done event",
        "validation before done",
        "attempted cancellation after temporal_refit_start now locks the two-event lifecycle payload",
        "start step 0/1 followed by done step 1/1",
        "pre-eligibility no-attempt exits now share assertions for preserved input/output key counts",
        "input/output count note tokens",
        "attempted refused refits now share assertions for input/output key-count note tokens",
        "temporal_refit_max_err and temporal_refit_max_err_screen_px note tokens",
        "latest-origin awareness remains fetch-only",
        "no `path_validation_done` extraction is visible",
        "5a499b4` replacement baseline progress",
        "320eccd` replacement validation start progress",
        "no-attempt rejection preservation checks share a single temporal-refit assertion helper",
        "b9daa58` path solver config validation-options extraction",
        "038be2e` replacement validation summary",
        "Slice 44/45 path solver config and replacement-validation extraction files",
        "7ca745b` replacement retry eligibility",
        "local Slice 47 path-fit/main/docs/policy work is not assumed",
        "b828bea` path replacement vertex preference",
        "Slice 47 `path_replacement_preference.*` extraction",
        "9b4a8be` replacement vertex note input",
        "local Slice 49 path-replacement-notes/main/docs/policy work is not assumed",
        "8e69081` replacement source validation note input",
        "e199a2d` replacement retry result note input",
        "b59991e` replacement retry eligibility input",
        "c914f9c` replacement retry target ladder helper",
        "4485457` replacement validation summary application",
        "68f5a7c` replacement note append helper",
        "incoming off-limits paths now also include `path_frame_fit.hpp` and `path_replacement_target_ladder.*`",
        "worktree fast-forwarded to `3e0d090` Clean clangd include diagnostics",
        "pre-update temporal-refit dirty work was preserved in stash and reapplied",
        "off-limits `.clangd`, VS Code, CMake, domain, fallback, motion-smooth, guard, path-panel, progress-policy, and P3 guideline files stayed untouched",
        "worktree fast-forwarded to `9b1464d` Force compile commands export for clangd",
        "incoming `7bc0f56` clangd LSP diagnostic checker",
        "incoming `7bc0f56` clangd LSP diagnostic checker and `9b1464d` compile-command export were accepted as base content only",
        "no TR edits to off-limits CMake, clangd, progress, fallback, motion-smooth, guard, path-panel, progress-policy, or P3 guideline files",
        "worktree fast-forwarded to `8c8c2dc` Add clangd config policy",
        "incoming `079cb9f` clangd LSP checker policy",
        "incoming `079cb9f` clangd LSP checker policy and `8c8c2dc` clangd config policy were accepted as base content only",
        "no TR edits to off-limits progress, docs, clangd config, VS Code, CMake, domain, fallback, motion-smooth, path replacement, guard, path-panel, or progress-policy files",
        "worktree fast-forwarded to `4022588` Extract replacement fast vertex acceptance",
        "incoming replacement fast-vertex extraction was accepted as base content only",
        "no TR edits to off-limits main, progress, docs, path-replacement, diagnostics-boundary, fallback, motion-smooth, guard, path-panel, or progress-policy files",
        "worktree fast-forwarded to `201ea25` Extract replacement baseline solve",
        "then `4577ca0` Extract replacement candidate validation",
        "then `5751807` Extract replacement decision application",
        "incoming path-replacement baseline-solve extraction was accepted as base content only",
        "incoming path-replacement baseline-solve and candidate-validation extractions were accepted as base content only",
        "incoming path-replacement baseline-solve, candidate-validation, and decision-application extractions were accepted as base content only",
        "no TR edits to off-limits main, progress, docs, path-replacement, diagnostics-boundary, path-panel, fallback, motion-smooth, guard, or progress-policy files",
        "accepted scalar and shape-flat refits now share assertions for input/output key-count note tokens",
        "temporal_refit_refactor_policy now locks direct include-cleaner readiness for temporal_refit helper source dependencies",
        "fetched origin shows `539d633` replacement retry skipped notes and `061c77c` replacement retry loop as off-limits base content not applied in this dirty worktree",
        "BuildTemporalRefitNotes now has exact-string coverage for accepted and attempted-refused note token order",
        "fetched origin shows `d16f3c8` solve parallel runtime scope as off-limits base content not applied in this dirty worktree",
        "incoming replacement/runtime/main/progress/docs/policy paths remain merge-base-only for this TR lane",
        "temporal-refit budget helpers now have direct tests for strict property tolerance clamp",
        "fetched origin shows `a2cb90d` unified spatial warning reporting as off-limits base content not applied in this dirty worktree",
        "temporal_refit_refactor_policy now forbids solver_reporting.hpp in temporal helpers/facade",
        "incoming reporting/replacement/runtime/main/progress/docs/policy paths remain merge-base-only for this TR lane",
        "TwoEndpointCandidate now has direct tests for empty and one-sample no-op behavior",
        "endpoint property id preservation, endpoint time/value copy, linear interpolation defaults, and independent value copies",
        "orchestrator local reporting/static-trim WIP is not assumed by this dirty TR stack",
        "reporting/main/progress/docs changes remain off-limits base content for future integration",
        "temporal helpers/facade still own no reporting emission surface",
        "helper policy forbids reporting/runtime/replacement orchestration includes",
        "temporal_refit.hpp stays free of helper/reporting/runtime/replacement orchestration includes",
        "temporal_refit.hpp direct dependencies stay explicit for include-cleaner",
        "solve_lifecycle_reporting.hpp",
        "fetched origin shows `20a9529` solve lifecycle output helpers as off-limits base content not applied in this dirty worktree",
        "ResampleAcceptedAtSourceTimes now directly covers unsupported Custom properties preserving metadata",
        "path_replacement_post_temporal.hpp is forbidden from temporal helpers/facade",
        "fetched origin shows `2230183` post temporal replacement orchestration as off-limits base content not applied in this dirty worktree",
        "ResampleAcceptedAtSourceTimes now directly covers numeric source-time replay metadata",
        "preserving property id/kind/dimensions/units and t_start/t_end plus every source sample timestamp",
        "solve_property_completion.hpp is forbidden from temporal helpers/facade",
        "fetched origin shows `7cbd52d` solved property completion as off-limits base content not applied in this dirty worktree",
        "unsupported Custom source-time replay metadata now preserves dimensions, t_start/t_end, samples_per_frame, and hash_of_expression while returning an empty sample stream",
        "solve_command.hpp is forbidden from temporal helpers/facade",
        "fetched origin advanced from `7cbd52d` solved property completion to `cb975e5` solve command module during this dirty worktree lane",
        "path_frame_fit_types.hpp is forbidden from temporal helpers/facade",
        "fetched origin shows `d2f92cd` path frame fit type header as off-limits base content not applied in this dirty worktree",
        "motion-smooth integration work in progress is not assumed by this dirty TR stack",
        "motion_smooth module includes are forbidden from temporal helpers/facade",
        "fetched origin shows `7468b56` motion-smooth module split integration as off-limits base content not applied in this dirty worktree",
        "no incoming temporal_refit paths are visible against `7468b56`",
        "temporal_refit includes and policy expectations now use domain.hpp only",
        "worktree fast-forwarded to `e493921` Rename domain header to hpp",
        "domain.h must not return to temporal helpers, facade, or focused temporal tests",
        "worktree fast-forwarded to `9940264` Lock main dispatch boundary",
        "incoming `599a77e` diagnostics boundary and `9940264` main dispatch policy changes were accepted as base content only",
        "worktree fast-forwarded to `774ab29` Split path replacement acceptance module",
        "incoming path_fit_pipeline and path_replacement_acceptance changes were accepted as base content only",
        "temporal_refit.hpp stays below the public header line target",
        "local Slice 49 path-replacement-notes/main/docs/policy work is still not assumed",
        "Public `temporal_refit.hpp` exports stay limited",
        "Helper declarations stay one-to-one",
        "python3 tests/temporal_refit_refactor_policy.py",
        "python3 tools/clangd_lsp_check.py",
        "python3 tools/p3_refactor_guard.py --tier quick --no-build",
    ):
        assert phrase in text, f"readiness checklist missing {phrase!r}"


def main() -> int:
    tests = [
        test_temporal_refit_facade_stays_compact,
        test_temporal_refit_public_header_stays_under_header_cap,
        test_temporal_refit_public_header_exports_stay_narrow,
        test_temporal_refit_public_header_includes_direct_dependencies,
        test_temporal_refit_public_header_stays_orchestration_free,
        test_temporal_refit_facade_keeps_orchestration_only,
        test_temporal_refit_helper_modules_are_present_compact_and_hpp_only,
        test_temporal_refit_helper_sources_include_own_header_first,
        test_temporal_refit_helper_headers_own_expected_declarations,
        test_temporal_refit_helper_headers_include_direct_dependencies,
        test_temporal_refit_helper_sources_include_direct_dependencies,
        test_temporal_refit_boundary_owners_are_explicit,
        test_temporal_refit_helpers_respect_diagnostics_boundary,
        test_temporal_refit_helpers_do_not_absorb_orchestration_includes,
        test_temporal_refit_facade_and_helpers_do_not_include_motion_smooth_modules,
        test_temporal_refit_uses_domain_hpp_only,
        test_temporal_refit_readiness_checklist_records_merge_contract,
        test_temporal_refit_readiness_diff_stays_scoped,
    ]
    for test in tests:
        test()
        print(f"[PASS] {test.__name__}")
    print(f"summary: {len(tests)} passed, 0 failed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
