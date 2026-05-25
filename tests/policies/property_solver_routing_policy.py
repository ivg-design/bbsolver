#!/usr/bin/env python3
"""
Source policy for the RunSolve property-routing boundary.

The route priority is intentionally small and unit-tested in C++; this policy
keeps that priority ladder and the route execution switch outside
solver/src/app/main.cpp.
"""

from __future__ import annotations

from pathlib import Path

from _solver_policy_paths import find_solver_layout


ROOT, SOLVER = find_solver_layout(__file__)
MAIN_CPP = SOLVER / "src" / "app" / "main.cpp"
SOLVE_COMMAND_CPP = SOLVER / "src" / "solve" / "solve_command.cpp"
ROUTING_H = SOLVER / "include" / "bbsolver" / "routing" / "property_solver_routing.hpp"
ROUTING_CPP = SOLVER / "src" / "routing" / "property_solver_routing.cpp"
ROUTE_SOLVER_H = SOLVER / "include" / "bbsolver" / "routing" / "property_route_solver.hpp"
ROUTE_SOLVER_CPP = SOLVER / "src" / "routing" / "property_route_solver.cpp"
TEMPORAL_PRELUDE_H = SOLVER / "include" / "bbsolver" / "solve" / "solve_property_temporal_prelude.hpp"
TEMPORAL_PRELUDE_CPP = SOLVER / "src" / "solve" / "solve_property_temporal_prelude.cpp"
ROUTING_TEST = SOLVER / "tests" / "solver_unit" / "test_property_solver_routing.cpp"


def _source_between(text: str, start_marker: str, end_marker: str) -> str:
    start = text.find(start_marker)
    assert start >= 0, f"missing start marker: {start_marker!r}"
    end = text.find(end_marker, start + len(start_marker))
    assert end > start, f"missing end marker {end_marker!r}"
    return text[start:end]


def test_routing_boundary_files_exist():
    for path in (
        ROUTING_H,
        ROUTING_CPP,
        ROUTE_SOLVER_H,
        ROUTE_SOLVER_CPP,
        TEMPORAL_PRELUDE_H,
        TEMPORAL_PRELUDE_CPP,
        ROUTING_TEST,
    ):
        assert path.is_file(), f"missing property routing boundary file: {path}"


def test_solve_command_uses_extracted_route_decision():
    main = MAIN_CPP.read_text()
    solve_command = SOLVE_COMMAND_CPP.read_text()
    assert '#include "bbsolver/routing/property_route_solver.hpp"' in solve_command
    assert '#include "bbsolver/solve/solve_property_temporal_prelude.hpp"' in solve_command
    assert "PropertyRouteSolveRequest route_solve_request" not in main
    dispatch_body = _source_between(
        solve_command,
        "PropertyTemporalPreludeRequest temporal_prelude_request;",
        "ApplyFinalStaticTrimNote(property_keys, final_static_trim_note)",
    )
    assert "temporal_prelude.property_solve_route" in dispatch_body
    assert "bbsolver::ChoosePropertySolveRoute(route_input)" not in dispatch_body
    assert "PropertyRouteSolveRequest route_solve_request" in dispatch_body
    assert "SolvePropertyRoute(route_solve_request)" in dispatch_body
    assert "switch (property_solve_route)" not in dispatch_body

    temporal_prelude = TEMPORAL_PRELUDE_CPP.read_text()
    prelude_body = _source_between(
        temporal_prelude,
        "PropertySolveRouteInput route_input;",
        "return state;",
    )
    assert "ChoosePropertySolveRoute(route_input)" in prelude_body
    assert "switch (state.property_solve_route)" not in prelude_body


def test_priority_ladder_lives_in_routing_module():
    main = MAIN_CPP.read_text()
    solve_command = SOLVE_COMMAND_CPP.read_text()
    routing = ROUTING_CPP.read_text()
    route_solver = ROUTE_SOLVER_CPP.read_text()
    assert "PropertySolveRoute ChoosePropertySolveRoute" not in main
    assert "PropertySolveRoute ChoosePropertySolveRoute" not in solve_command
    assert "PropertySolveRoute ChoosePropertySolveRoute" in routing
    assert "switch (request.route)" not in main
    assert "switch (request.route)" not in solve_command
    assert "switch (request.route)" in route_solver
    for route in (
        "PreserveSourceKeys",
        "MotionSmooth",
        "FrameKeyFallback",
        "ReplacementShapeFlatTemporal",
        "PathDecomposed",
        "PlainTemporal",
    ):
        assert f"PropertySolveRoute::{route}" in route_solver

    ordered_tokens = [
        "input.preserve_source_keys",
        "input.motion_smooth_enabled",
        "!input.temporal_optimization_enabled",
        "input.path_temporal_reduced_by_fit",
        "input.decompose_paths && input.decompose_candidate_is_shape_flat",
        "PropertySolveRoute::PlainTemporal",
    ]
    cursor = -1
    for token in ordered_tokens:
        index = routing.find(token)
        assert index > cursor, f"routing priority token out of order: {token}"
        cursor = index


def main() -> int:
    tests = [
        test_routing_boundary_files_exist,
        test_solve_command_uses_extracted_route_decision,
        test_priority_ladder_lives_in_routing_module,
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
