#!/usr/bin/env python3
"""Policy/smoke checks for bbsolver --diagnostics JSONL lifecycle.

The DiagnosticsWriter emits a JSON-Lines stream when bbsolver is invoked
with `--diagnostics PATH`. This policy locks the operator-visible
contract that:

- diagnostics are absent by default (no file created unless requested);
- a happy-path solve emits the full envelope set
  (`solve_start` -> `parallel_runtime` -> `solve_mode_capabilities` ->
  `cancellation_status` -> `solve_done`), every row carrying
  `schema_version` and `request_id`, with `solve_done` carrying the
  lifecycle metadata Phase 4 CEP consumers parse (properties,
  total_keys, solve_time_ms, total_samples_input);
- a cancelled solve produces `solve_cancelled` instead of `solve_done`,
  with `cancellation_status.cancel_file_exists` flipped to true and the
  process returning the documented exit code 5.

The policy uses the smallest committed fixture (`color_pulse.bbsm.json`,
73 samples, 1 property) so the wall time per case stays well under a
second. All cases short-circuit cleanly when bbsolver is not built,
keeping the script safe to invoke from `tools/p3_refactor_guard.py`'s
quick tier on a fresh checkout.
"""

from __future__ import annotations

import json
import os
import subprocess
import tempfile
from pathlib import Path

from _solver_policy_paths import find_solver_layout


ROOT, SOLVER = find_solver_layout(__file__)
BBSOLVER = Path(
    os.environ.get(
        "BBSOLVER_TEST_BINARY",
        str(SOLVER / "build" / "bbsolver"),
    )
)
FIXTURE = SOLVER / "tests" / "fixtures" / "color_pulse.bbsm.json"

HAPPY_PATH_EVENT_ORDER = [
    "solve_start",
    "parallel_runtime",
    "solve_mode_capabilities",
    "cancellation_status",
    "solve_done",
]
HAPPY_PATH_EVENT_NAMES = set(HAPPY_PATH_EVENT_ORDER)
CANCELLED_PATH_EVENT_ORDER = [
    "solve_start",
    "parallel_runtime",
    "solve_mode_capabilities",
    "cancellation_status",
    "solve_cancelled",
]
CANCELLED_PATH_EVENT_NAMES = set(CANCELLED_PATH_EVENT_ORDER)


def _bbsolver_missing() -> bool:
    if not BBSOLVER.exists():
        print(f"[SKIP] bbsolver not built at {BBSOLVER}")
        return True
    return False


def _read_jsonl_events(path: Path) -> list[dict]:
    events: list[dict] = []
    for raw in path.read_text(encoding="utf-8").splitlines():
        raw = raw.strip()
        if raw:
            events.append(json.loads(raw))
    return events


def _fixture_request_id() -> str:
    with FIXTURE.open(encoding="utf-8") as f:
        return json.load(f)["request_id"]


def test_diagnostics_absent_by_default_when_built() -> None:
    """A solve without --diagnostics must not create diagnostics artifacts."""
    if _bbsolver_missing():
        return
    with tempfile.TemporaryDirectory(prefix="bb_diagnostics_default_") as tmp:
        tmp_path = Path(tmp)
        out_path = tmp_path / "out.bbky.json"
        absent_path = tmp_path / "must-not-exist.diagnostics.jsonl"
        proc = subprocess.run(
            [
                str(BBSOLVER),
                "solve",
                str(FIXTURE),
                str(out_path),
                "--tolerance",
                "0.5",
                "--jobs",
                "1",
            ],
            text=True,
            capture_output=True,
            timeout=20,
            check=True,
        )
        assert proc.returncode == 0, proc.stderr
        assert out_path.exists(), "solver must produce its primary output"
        produced = sorted(p.name for p in tmp_path.iterdir())
        assert produced == [out_path.name], (
            "default solve must not produce a diagnostics file; "
            f"found {produced!r}"
        )
        assert not absent_path.exists(), (
            "default solve must not write to a diagnostics-shaped path"
        )


def test_diagnostics_lifecycle_when_built() -> None:
    """An enabled diagnostics file must contain the full successful lifecycle."""
    if _bbsolver_missing():
        return
    with tempfile.TemporaryDirectory(prefix="bb_diagnostics_lifecycle_") as tmp:
        tmp_path = Path(tmp)
        out_path = tmp_path / "out.bbky.json"
        diag_path = tmp_path / "diag.jsonl"
        proc = subprocess.run(
            [
                str(BBSOLVER),
                "solve",
                str(FIXTURE),
                str(out_path),
                "--tolerance",
                "0.5",
                "--jobs",
                "1",
                "--diagnostics",
                str(diag_path),
            ],
            text=True,
            capture_output=True,
            timeout=20,
            check=True,
        )
        assert proc.returncode == 0, proc.stderr
        assert diag_path.exists(), "--diagnostics path must be created"
        events = _read_jsonl_events(diag_path)
        names = [event.get("event") for event in events]
        assert names == HAPPY_PATH_EVENT_ORDER, names
        assert set(names) == HAPPY_PATH_EVENT_NAMES, names

        fixture_request_id = _fixture_request_id()
        for event in events:
            assert event.get("schema_version") == 1, event
            assert event.get("request_id") == fixture_request_id, event

        by_name = {event["event"]: event for event in events}

        start = by_name["solve_start"]
        assert start["properties"] == 1
        assert start["tolerance"] == 0.5
        assert isinstance(start.get("input"), str) and start["input"]
        assert isinstance(start.get("output"), str) and start["output"]

        parallel = by_name["parallel_runtime"]
        assert parallel["requested_jobs"] == 1
        assert parallel["resolved_jobs"] == 1
        assert int(parallel["detected_jobs"]) >= 1
        assert int(parallel["hard_cap"]) >= 1
        assert isinstance(parallel["tbb_available"], bool)
        assert isinstance(parallel.get("phase"), str) and parallel["phase"]

        mode = by_name["solve_mode_capabilities"]
        assert mode["mode"] == "full"
        assert mode["allows_temporal"] is True
        assert mode["allows_vertex"] is True
        assert mode["allows_spatial_topology"] is True
        assert mode["is_motion_smooth"] is False

        cancel_status = by_name["cancellation_status"]
        assert cancel_status["cancel_file_set"] is False
        assert cancel_status["cancel_file_exists"] is False
        assert cancel_status["cancel_file_path"] == ""
        assert cancel_status["partial_write_exit_code"] == 5

        done = by_name["solve_done"]
        assert done["properties"] == 1
        assert int(done["total_keys"]) >= 1, "solver must keep at least one key"
        assert float(done["solve_time_ms"]) >= 0.0
        assert int(done["total_samples_input"]) >= 1


def test_diagnostics_cancellation_when_built() -> None:
    """A pre-existing cancel sentinel must emit solve_cancelled and exit 5."""
    if _bbsolver_missing():
        return
    with tempfile.TemporaryDirectory(prefix="bb_diagnostics_cancel_") as tmp:
        tmp_path = Path(tmp)
        out_path = tmp_path / "out.bbky.json"
        diag_path = tmp_path / "diag.jsonl"
        cancel_path = tmp_path / "cancel.flag"
        cancel_path.touch()
        proc = subprocess.run(
            [
                str(BBSOLVER),
                "solve",
                str(FIXTURE),
                str(out_path),
                "--tolerance",
                "0.5",
                "--jobs",
                "1",
                "--diagnostics",
                str(diag_path),
                "--cancel-file",
                str(cancel_path),
            ],
            text=True,
            capture_output=True,
            timeout=20,
            check=False,
        )
        assert proc.returncode == 5, (
            "cancelled solve must return the documented exit code 5; "
            f"got {proc.returncode}: {proc.stderr}"
        )
        assert diag_path.exists(), "--diagnostics path must be created"
        events = _read_jsonl_events(diag_path)
        names = [event.get("event") for event in events]
        assert names == CANCELLED_PATH_EVENT_ORDER, names
        assert set(names) == CANCELLED_PATH_EVENT_NAMES, names
        assert "solve_done" not in names, (
            "cancelled solve must not emit solve_done"
        )

        fixture_request_id = _fixture_request_id()
        for event in events:
            assert event.get("schema_version") == 1, event
            assert event.get("request_id") == fixture_request_id, event

        by_name = {event["event"]: event for event in events}
        cancel_status = by_name["cancellation_status"]
        assert cancel_status["cancel_file_set"] is True
        assert cancel_status["cancel_file_exists"] is True
        assert cancel_status["cancel_file_path"].endswith("cancel.flag"), (
            cancel_status["cancel_file_path"]
        )
        assert cancel_status["partial_write_exit_code"] == 5

        cancelled = by_name["solve_cancelled"]
        assert isinstance(cancelled.get("phase"), str) and cancelled["phase"]
        assert int(cancelled["property_index"]) >= 0
        assert int(cancelled["properties_completed"]) >= 0
        assert float(cancelled["solve_time_ms"]) >= 0.0
        assert cancelled["partial_write_exit_code"] == 5


def test_diagnostics_path_is_truncated_when_enabled() -> None:
    """The CLI must replace stale diagnostics files, not append to them."""
    if _bbsolver_missing():
        return
    with tempfile.TemporaryDirectory(prefix="bb_diagnostics_truncate_") as tmp:
        tmp_path = Path(tmp)
        out_path = tmp_path / "out.bbky.json"
        diag_path = tmp_path / "diag.jsonl"
        diag_path.write_text(
            '{"event":"stale","schema_version":0,"request_id":"old"}\n',
            encoding="utf-8",
        )
        proc = subprocess.run(
            [
                str(BBSOLVER),
                "solve",
                str(FIXTURE),
                str(out_path),
                "--tolerance",
                "0.5",
                "--jobs",
                "1",
                "--diagnostics",
                str(diag_path),
            ],
            text=True,
            capture_output=True,
            timeout=20,
            check=True,
        )
        assert proc.returncode == 0, proc.stderr
        events = _read_jsonl_events(diag_path)
        names = [event.get("event") for event in events]
        assert names == HAPPY_PATH_EVENT_ORDER, names
        assert "stale" not in names, "diagnostics file must be truncated"
        assert all(
            event.get("request_id") == _fixture_request_id() for event in events
        )


def main() -> int:
    tests = [
        test_diagnostics_absent_by_default_when_built,
        test_diagnostics_lifecycle_when_built,
        test_diagnostics_cancellation_when_built,
        test_diagnostics_path_is_truncated_when_enabled,
    ]
    for test in tests:
        test()
        print(f"[PASS] {test.__name__}")
    print(f"summary: {len(tests)} passed, 0 failed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
