#!/usr/bin/env python3
"""Source-level schema policy for diagnostic event builders.

This policy locks the side-effect-free event-builder JSON surface without
running bbsolver command behavior. It inspects solver_diagnostic_events.hpp
and solver_diagnostic_events.cpp directly so any wire-format change must be
made deliberately alongside this contract.
"""

from __future__ import annotations

import re
import sys
from dataclasses import dataclass
from pathlib import Path

from _solver_policy_paths import find_solver_layout


ROOT, SOLVER = find_solver_layout(__file__)
EVENT_HPP = (
    SOLVER
    / "include"
    / "bbsolver"
    / "diagnostics"
    / "solver_diagnostic_events.hpp"
)
EVENT_CPP = SOLVER / "src" / "diagnostics" / "solver_diagnostic_events.cpp"


@dataclass(frozen=True)
class EventSchema:
    builder: str
    event_name: str
    fields: tuple[str, ...]


EVENT_SCHEMAS = (
    EventSchema(
        "BuildSolveStartEvent",
        "solve_start",
        (
            "event",
            "schema_version",
            "request_id",
            "input",
            "output",
            "properties",
            "tolerance",
            "screen_px",
            "decompose_paths",
            "fit_canonical_paths",
            "fit_replacement_paths",
            "emit_landmark_subpaths",
        ),
    ),
    EventSchema(
        "BuildParallelRuntimeEvent",
        "parallel_runtime",
        (
            "event",
            "schema_version",
            "requested_jobs",
            "detected_jobs",
            "hard_cap",
            "tbb_available",
            "resolved_jobs",
            "phase",
            "error",
        ),
    ),
    EventSchema(
        "BuildSolveModeCapabilitiesEvent",
        "solve_mode_capabilities",
        (
            "event",
            "schema_version",
            "mode",
            "allows_temporal",
            "allows_vertex",
            "allows_spatial_topology",
            "is_motion_smooth",
            "is_motion_path_smooth",
            "uses_motion_smoothing",
        ),
    ),
    EventSchema(
        "BuildCancellationStatusEvent",
        "cancellation_status",
        (
            "event",
            "schema_version",
            "cancel_file_set",
            "cancel_file_exists",
            "partial_write_exit_code",
            "cancel_file_path",
        ),
    ),
    EventSchema(
        "BuildSolveCancelledEvent",
        "solve_cancelled",
        (
            "event",
            "schema_version",
            "request_id",
            "phase",
            "property_index",
            "properties_completed",
            "solve_time_ms",
            "partial_write_exit_code",
        ),
    ),
    EventSchema(
        "BuildSolveDoneEvent",
        "solve_done",
        (
            "event",
            "schema_version",
            "request_id",
            "properties",
            "total_keys",
            "total_samples_input",
            "solve_time_ms",
        ),
    ),
    EventSchema(
        "BuildBridgePruneResultEvent",
        "post_temporal_bridge_prune_result",
        (
            "event",
            "schema_version",
            "request_id",
            "property_id",
            "property_name",
            "property_index",
            "property_count",
            "accepted",
            "attempted",
            "source_vertices",
            "fitted_vertices",
            "max_outline_error",
            "notes",
        ),
    ),
    EventSchema(
        "BuildBridgePrunePhaseEvent",
        "post_temporal_bridge_prune_phase",
        (
            "event",
            "schema_version",
            "request_id",
            "property_id",
            "property_name",
            "property_index",
            "property_count",
            "phase",
            "target_vertices",
            "removed_index",
            "candidate_count",
            "candidates_checked",
            "attempt",
            "accepted",
            "batch",
        ),
    ),
)


BUILDER_RE = re.compile(
    r"\bnlohmann::json\s+(Build[A-Za-z0-9_]*Event)\s*\("
)


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def _matching_brace(text: str, open_index: int) -> int:
    depth = 0
    in_string = False
    escaped = False
    for i in range(open_index, len(text)):
        ch = text[i]
        if in_string:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == '"':
                in_string = False
            continue
        if ch == '"':
            in_string = True
        elif ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return i
    raise AssertionError("unterminated C++ function body")


def _function_body(source: str, builder: str) -> str:
    match = re.search(
        rf"\bnlohmann::json\s+{re.escape(builder)}\s*\(",
        source,
    )
    assert match, f"missing implementation for {builder}"
    open_brace = source.find("{", match.end())
    assert open_brace >= 0, f"missing body for {builder}"
    close_brace = _matching_brace(source, open_brace)
    return source[open_brace : close_brace + 1]


def _header_contract(header: str, builder: str) -> str:
    builder_index = header.find(builder)
    assert builder_index >= 0, f"missing declaration for {builder}"
    start = header.rfind("// Event name:", 0, builder_index)
    assert start >= 0, f"missing event-name comment for {builder}"
    end = header.find(";", builder_index)
    assert end >= 0, f"missing declaration terminator for {builder}"
    return header[start : end + 1]


def _ordered_tokens(text: str, tokens: tuple[str, ...]) -> bool:
    cursor = -1
    for token in tokens:
        cursor = text.find(token, cursor + 1)
        if cursor < 0:
            return False
    return True


def _expected_builder_names() -> set[str]:
    return {schema.builder for schema in EVENT_SCHEMAS}


def test_event_builder_set_is_explicitly_pinned() -> None:
    header = _read(EVENT_HPP)
    source = _read(EVENT_CPP)
    expected = _expected_builder_names()
    header_builders = set(BUILDER_RE.findall(header))
    source_builders = set(BUILDER_RE.findall(source))
    assert header_builders == expected, (
        "diagnostic event declarations changed; update schema policy. "
        f"expected={sorted(expected)!r} actual={sorted(header_builders)!r}"
    )
    assert source_builders == expected, (
        "diagnostic event implementations changed; update schema policy. "
        f"expected={sorted(expected)!r} actual={sorted(source_builders)!r}"
    )


def test_header_documents_each_event_name() -> None:
    header = _read(EVENT_HPP)
    for schema in EVENT_SCHEMAS:
        contract = _header_contract(header, schema.builder)
        assert f'Event name: "{schema.event_name}"' in contract, (
            f"{schema.builder} must document stable event name "
            f"{schema.event_name!r}"
        )
        assert "Pure:" in contract, (
            f"{schema.builder} must document side-effect-free builder intent"
        )


def test_every_event_stamps_schema_version_and_required_fields() -> None:
    source = _read(EVENT_CPP)
    for schema in EVENT_SCHEMAS:
        body = _function_body(source, schema.builder)
        assert f'"{schema.event_name}"' in body, (
            f"{schema.builder} must emit event name {schema.event_name!r}"
        )
        assert "kSolverDiagnosticEventSchemaVersion" in body, (
            f"{schema.builder} must stamp schema version constant"
        )
        for field in schema.fields:
            assert f'"{field}"' in body, (
                f"{schema.builder} missing required JSON field {field!r}"
            )


def test_schema_version_constant_is_stable() -> None:
    header = _read(EVENT_HPP)
    assert "constexpr int kSolverDiagnosticEventSchemaVersion = 1;" in header, (
        "diagnostic event schema version changed; update consumers and policy"
    )


def test_parallel_runtime_negative_request_error_surface_is_pinned() -> None:
    source = _read(EVENT_CPP)
    body = _function_body(source, "BuildParallelRuntimeEvent")
    assert 'out["resolved_jobs"] = nullptr;' in body
    assert 'out["phase"] = nullptr;' in body
    assert 'out["error"] = "requested_jobs_negative";' in body


def test_cancellation_event_publishes_documented_exit_code() -> None:
    source = _read(EVENT_CPP)
    assert "constexpr int kCancelledPartialExitCode = 5;" in source
    for builder in ("BuildCancellationStatusEvent", "BuildSolveCancelledEvent"):
        body = _function_body(source, builder)
        assert "kCancelledPartialExitCode" in body, (
            f"{builder} must publish the documented partial-write exit code"
        )


def test_bridge_prune_event_uses_bridge_result_contract() -> None:
    header = _read(EVENT_HPP)
    source = _read(EVENT_CPP)
    assert "struct PostSolvePathVertexReductionResult;" in header, (
        "bridge-prune event builder should forward-declare the result type"
    )
    assert '#include "bbsolver/path/bridge_prune/path_bridge_prune.hpp"' in source, (
        "bridge-prune event implementation must consume path_bridge_prune.hpp"
    )
    contract = _header_contract(header, "BuildBridgePruneResultEvent")
    assert "const PostSolvePathVertexReductionResult& result" in contract, (
        "bridge-prune event builder must consume the canonical result type"
    )
    body = _function_body(source, "BuildBridgePruneResultEvent")
    for member in (
        "accepted",
        "attempted",
        "source_vertices",
        "fitted_vertices",
        "max_outline_error",
        "notes",
    ):
        assert f"result.{member}" in body, (
            f"bridge-prune event must read result.{member}"
        )
    assert "bool accepted" not in contract, (
        "event module must not duplicate bridge-prune result state"
    )


def test_bridge_prune_event_is_summary_only() -> None:
    source = _read(EVENT_CPP)
    body = _function_body(source, "BuildBridgePruneResultEvent")
    for forbidden in (
        'result.keys',
        '"keys"',
        '"progress"',
        '"cancelled"',
        "ProgressWriter",
        "DiagnosticsWriter",
    ):
        assert forbidden not in body, (
            "bridge-prune diagnostic result event must stay a summary-only "
            f"payload and avoid {forbidden!r}"
        )


def test_bridge_prune_phase_event_is_progress_free() -> None:
    source = _read(EVENT_CPP)
    body = _function_body(source, "BuildBridgePrunePhaseEvent")
    for forbidden in (
        '"progress"',
        '"notes"',
        '"cancelled"',
        "ProgressWriter",
        "DiagnosticsWriter",
    ):
        assert forbidden not in body, (
            "bridge-prune diagnostic phase event must stay side-effect-free "
            f"and avoid {forbidden!r}"
        )
    assert "BridgePrunePhaseDiagnosticInput" in _read(EVENT_HPP), (
        "bridge-prune phase event must use a named scalar input contract"
    )


def test_bridge_prune_property_name_fallback_order_is_pinned() -> None:
    source = _read(EVENT_CPP)
    assert "DiagnosticPropertyName" in source, (
        "bridge-prune event must publish the diagnostic property name"
    )
    assert _ordered_tokens(
        source,
        (
            "DiagnosticPropertyName",
            "display_name",
            "match_name",
            "property.id",
            '"<unnamed>"',
        ),
    ), "property name fallback must be display_name -> match_name -> id -> <unnamed>"


def main() -> int:
    tests = [
        test_event_builder_set_is_explicitly_pinned,
        test_header_documents_each_event_name,
        test_every_event_stamps_schema_version_and_required_fields,
        test_schema_version_constant_is_stable,
        test_parallel_runtime_negative_request_error_surface_is_pinned,
        test_cancellation_event_publishes_documented_exit_code,
        test_bridge_prune_event_uses_bridge_result_contract,
        test_bridge_prune_event_is_summary_only,
        test_bridge_prune_phase_event_is_progress_free,
        test_bridge_prune_property_name_fallback_order_is_pinned,
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
