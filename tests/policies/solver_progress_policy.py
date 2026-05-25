#!/usr/bin/env python3
"""
Policy/smoke checks for bbsolver progress events.

The AE panel reads bbsolver progress JSON and turns it into operator-visible
status. This locks the contract that long solver stages emit both a numeric
progress value and a human-readable phase.
"""

from __future__ import annotations

import json
import math
import os
import subprocess
import tempfile
from pathlib import Path

from _solver_policy_paths import find_solver_layout


ROOT, SOLVER = find_solver_layout(__file__)
MAIN_CPP = SOLVER / "src" / "app" / "main.cpp"
SOLVE_COMMAND_CPP = SOLVER / "src" / "solve" / "solve_command.cpp"
SOLVE_COMMAND_HPP = SOLVER / "include" / "bbsolver" / "solve" / "solve_command.hpp"
PROGRESS_CPP = SOLVER / "src" / "progress" / "progress.cpp"
PROGRESS_H = SOLVER / "include" / "bbsolver" / "progress" / "progress.hpp"
RUNTIME_ENV_CPP = SOLVER / "src" / "runtime" / "runtime_env.cpp"
RUNTIME_ENV_HPP = SOLVER / "include" / "bbsolver" / "runtime" / "runtime_env.hpp"
SOLVE_COMMAND_CONFIG_CPP = SOLVER / "src" / "solve" / "solve_command_config.cpp"
SOLVE_COMMAND_CONFIG_HPP = SOLVER / "include" / "bbsolver" / "solve" / "solve_command_config.hpp"
SOLVE_LIFECYCLE_REPORTING_CPP = SOLVER / "src" / "solve" / "solve_lifecycle_reporting.cpp"
SOLVE_LIFECYCLE_REPORTING_HPP = SOLVER / "include" / "bbsolver" / "solve" / "solve_lifecycle_reporting.hpp"
SOLVE_PARALLEL_RUNTIME_SCOPE_CPP = (
    SOLVER / "src" / "runtime" / "solve_parallel_runtime_scope.cpp"
)
SOLVE_PARALLEL_RUNTIME_SCOPE_HPP = (
    SOLVER
    / "include"
    / "bbsolver"
    / "runtime"
    / "solve_parallel_runtime_scope.hpp"
)
SOLVE_PATH_PREPARATION_CPP = SOLVER / "src" / "solve" / "solve_path_preparation.cpp"
SOLVE_PATH_PREPARATION_HPP = SOLVER / "include" / "bbsolver" / "solve" / "solve_path_preparation.hpp"
SOLVE_PROPERTY_POST_PROCESSING_CPP = SOLVER / "src" / "solve" / "solve_property_post_processing.cpp"
SOLVE_PROPERTY_POST_PROCESSING_HPP = SOLVER / "include" / "bbsolver" / "solve" / "solve_property_post_processing.hpp"
SOLVE_PROPERTY_TEMPORAL_PRELUDE_CPP = SOLVER / "src" / "solve" / "solve_property_temporal_prelude.cpp"
SOLVE_PROPERTY_TEMPORAL_PRELUDE_HPP = SOLVER / "include" / "bbsolver" / "solve" / "solve_property_temporal_prelude.hpp"
SOLVE_PROPERTY_TEMPORAL_RESULT_CPP = SOLVER / "src" / "solve" / "solve_property_temporal_result.cpp"
SOLVE_PROPERTY_TEMPORAL_RESULT_HPP = SOLVER / "include" / "bbsolver" / "solve" / "solve_property_temporal_result.hpp"
SOLVE_PROPERTY_OUTPUT_CPP = SOLVER / "src" / "solve" / "solve_property_output.cpp"
SOLVE_PROPERTY_OUTPUT_HPP = SOLVER / "include" / "bbsolver" / "solve" / "solve_property_output.hpp"
PATH_BRIDGE_PRUNE_CPP = SOLVER / "src" / "path" / "bridge_prune" / "path_bridge_prune.cpp"
PATH_BRIDGE_PRUNE_HPP = SOLVER / "include" / "bbsolver" / "path" / "bridge_prune" / "path_bridge_prune.hpp"
PATH_BRIDGE_PRUNE_BATCH_CPP = SOLVER / "src" / "path" / "bridge_prune" / "path_bridge_prune_batch.cpp"
PATH_BRIDGE_PRUNE_BATCH_HPP = SOLVER / "include" / "bbsolver" / "path" / "bridge_prune" / "path_bridge_prune_batch.hpp"
PATH_BRIDGE_PRUNE_BATCH_ATTEMPT_CPP = SOLVER / "src" / "path" / "bridge_prune" / "path_bridge_prune_batch_attempt.cpp"
PATH_BRIDGE_PRUNE_BATCH_ATTEMPT_HPP = SOLVER / "include" / "bbsolver" / "path" / "bridge_prune" / "path_bridge_prune_batch_attempt.hpp"
PATH_BRIDGE_PRUNE_CANDIDATE_CPP = SOLVER / "src" / "path" / "bridge_prune" / "path_bridge_prune_candidate.cpp"
PATH_BRIDGE_PRUNE_CANDIDATE_HPP = SOLVER / "include" / "bbsolver" / "path" / "bridge_prune" / "path_bridge_prune_candidate.hpp"
PATH_BRIDGE_PRUNE_NOTES_CPP = SOLVER / "src" / "path" / "bridge_prune" / "path_bridge_prune_notes.cpp"
PATH_BRIDGE_PRUNE_NOTES_HPP = SOLVER / "include" / "bbsolver" / "path" / "bridge_prune" / "path_bridge_prune_notes.hpp"
PATH_BRIDGE_PRUNE_PLAN_CPP = SOLVER / "src" / "path" / "bridge_prune" / "path_bridge_prune_plan.cpp"
PATH_BRIDGE_PRUNE_PLAN_HPP = SOLVER / "include" / "bbsolver" / "path" / "bridge_prune" / "path_bridge_prune_plan.hpp"
PATH_BRIDGE_PRUNE_PROGRESS_CPP = SOLVER / "src" / "path" / "bridge_prune" / "path_bridge_prune_progress.cpp"
PATH_BRIDGE_PRUNE_PROGRESS_HPP = SOLVER / "include" / "bbsolver" / "path" / "bridge_prune" / "path_bridge_prune_progress.hpp"
PATH_BRIDGE_PRUNE_RESULT_CPP = SOLVER / "src" / "path" / "bridge_prune" / "path_bridge_prune_result.cpp"
PATH_BRIDGE_PRUNE_RESULT_HPP = SOLVER / "include" / "bbsolver" / "path" / "bridge_prune" / "path_bridge_prune_result.hpp"
PATH_BRIDGE_PRUNE_ROUND_CPP = SOLVER / "src" / "path" / "bridge_prune" / "path_bridge_prune_round.cpp"
PATH_BRIDGE_PRUNE_ROUND_HPP = SOLVER / "include" / "bbsolver" / "path" / "bridge_prune" / "path_bridge_prune_round.hpp"
PATH_BRIDGE_PRUNE_SELECTION_CPP = SOLVER / "src" / "path" / "bridge_prune" / "path_bridge_prune_selection.cpp"
PATH_BRIDGE_PRUNE_SELECTION_HPP = SOLVER / "include" / "bbsolver" / "path" / "bridge_prune" / "path_bridge_prune_selection.hpp"
PATH_DECOMPOSED_SOLVER_CPP = (
    SOLVER / "src" / "path" / "decompose" / "path_decomposed_solver.cpp"
)
PATH_DECOMPOSED_SOLVER_HPP = (
    SOLVER
    / "include"
    / "bbsolver"
    / "path"
    / "decompose"
    / "path_decomposed_solver.hpp"
)
PROPERTY_ROUTE_SOLVER_CPP = SOLVER / "src" / "routing" / "property_route_solver.cpp"
PROPERTY_ROUTE_SOLVER_HPP = SOLVER / "include" / "bbsolver" / "routing" / "property_route_solver.hpp"
PATH_REPLACEMENT_FRACTION_TRIAL_CPP = SOLVER / "src" / "path" / "replacement" / "path_replacement_fraction_trial.cpp"
PATH_REPLACEMENT_FRACTION_TRIAL_HPP = SOLVER / "include" / "bbsolver" / "path" / "replacement" / "path_replacement_fraction_trial.hpp"
PATH_REPLACEMENT_PHASE2_FIT_CPP = SOLVER / "src" / "path" / "replacement" / "path_replacement_phase2_fit.cpp"
PATH_REPLACEMENT_PHASE2_FIT_HPP = SOLVER / "include" / "bbsolver" / "path" / "replacement" / "path_replacement_phase2_fit.hpp"
PATH_REPLACEMENT_SOLVER_CPP = SOLVER / "src" / "path" / "replacement" / "path_replacement_solver.cpp"
PATH_REPLACEMENT_SOLVER_HPP = SOLVER / "include" / "bbsolver" / "path" / "replacement" / "path_replacement_solver.hpp"
PATH_REPLACEMENT_PROGRESS_CPP = SOLVER / "src" / "path" / "replacement" / "path_replacement_progress.cpp"
PATH_REPLACEMENT_PROGRESS_HPP = SOLVER / "include" / "bbsolver" / "path" / "replacement" / "path_replacement_progress.hpp"
DP_PLACER_CPP = SOLVER / "src" / "dp" / "dp_placer.cpp"
DP_PLACER_HPP = SOLVER / "include" / "bbsolver" / "dp" / "dp_placer.hpp"
DP_FORWARD_PLACEMENT_CPP = SOLVER / "src" / "dp" / "dp_forward_placement.cpp"
DP_FORWARD_PLACEMENT_HPP = SOLVER / "include" / "bbsolver" / "dp" / "dp_forward_placement.hpp"
DP_PLACEMENT_PROGRESS_CPP = SOLVER / "src" / "dp" / "dp_placement_progress.cpp"
DP_PLACEMENT_PROGRESS_HPP = SOLVER / "include" / "bbsolver" / "dp" / "dp_placement_progress.hpp"
REPLACEMENT_TEMPORAL_CPP = SOLVER / "src" / "replacement_temporal" / "replacement_temporal_solver.cpp"
REPLACEMENT_TEMPORAL_HPP = SOLVER / "include" / "bbsolver" / "replacement_temporal" / "replacement_temporal_solver.hpp"
BBSOLVER = Path(os.environ.get(
    "BBSOLVER_TEST_BINARY",
    str(SOLVER / "build" / "bbsolver"),
))
FIXTURE = SOLVER / "tests" / "fixtures" / "color_pulse.bbsm.json"


def test_progress_source_contract():
    src = MAIN_CPP.read_text()
    solve_command_src = (
        SOLVE_COMMAND_CPP.read_text()
        + "\n"
        + SOLVE_COMMAND_HPP.read_text()
    )
    solve_command_config_src = (
        SOLVE_COMMAND_CONFIG_CPP.read_text()
        + "\n"
        + SOLVE_COMMAND_CONFIG_HPP.read_text()
    )
    solve_lifecycle_reporting_src = (
        SOLVE_LIFECYCLE_REPORTING_CPP.read_text()
        + "\n"
        + SOLVE_LIFECYCLE_REPORTING_HPP.read_text()
    )
    solve_parallel_runtime_scope_src = (
        SOLVE_PARALLEL_RUNTIME_SCOPE_CPP.read_text()
        + "\n"
        + SOLVE_PARALLEL_RUNTIME_SCOPE_HPP.read_text()
    )
    solve_path_preparation_src = (
        SOLVE_PATH_PREPARATION_CPP.read_text()
        + "\n"
        + SOLVE_PATH_PREPARATION_HPP.read_text()
    )
    solve_property_post_processing_src = (
        SOLVE_PROPERTY_POST_PROCESSING_CPP.read_text()
        + "\n"
        + SOLVE_PROPERTY_POST_PROCESSING_HPP.read_text()
    )
    solve_property_temporal_prelude_src = (
        SOLVE_PROPERTY_TEMPORAL_PRELUDE_CPP.read_text()
        + "\n"
        + SOLVE_PROPERTY_TEMPORAL_PRELUDE_HPP.read_text()
    )
    solve_property_temporal_result_src = (
        SOLVE_PROPERTY_TEMPORAL_RESULT_CPP.read_text()
        + "\n"
        + SOLVE_PROPERTY_TEMPORAL_RESULT_HPP.read_text()
    )
    solve_property_output_src = (
        SOLVE_PROPERTY_OUTPUT_CPP.read_text()
        + "\n"
        + SOLVE_PROPERTY_OUTPUT_HPP.read_text()
    )
    progress_src = PROGRESS_CPP.read_text() + "\n" + PROGRESS_H.read_text()
    runtime_env_src = RUNTIME_ENV_CPP.read_text() + "\n" + RUNTIME_ENV_HPP.read_text()
    path_bridge_prune_src = (
        PATH_BRIDGE_PRUNE_CPP.read_text()
        + "\n"
        + PATH_BRIDGE_PRUNE_HPP.read_text()
        + "\n"
        + PATH_BRIDGE_PRUNE_BATCH_CPP.read_text()
        + "\n"
        + PATH_BRIDGE_PRUNE_BATCH_HPP.read_text()
        + "\n"
        + PATH_BRIDGE_PRUNE_BATCH_ATTEMPT_CPP.read_text()
        + "\n"
        + PATH_BRIDGE_PRUNE_BATCH_ATTEMPT_HPP.read_text()
        + "\n"
        + PATH_BRIDGE_PRUNE_CANDIDATE_CPP.read_text()
        + "\n"
        + PATH_BRIDGE_PRUNE_CANDIDATE_HPP.read_text()
        + "\n"
        + PATH_BRIDGE_PRUNE_NOTES_CPP.read_text()
        + "\n"
        + PATH_BRIDGE_PRUNE_NOTES_HPP.read_text()
        + "\n"
        + PATH_BRIDGE_PRUNE_PLAN_CPP.read_text()
        + "\n"
        + PATH_BRIDGE_PRUNE_PLAN_HPP.read_text()
        + "\n"
        + PATH_BRIDGE_PRUNE_PROGRESS_CPP.read_text()
        + "\n"
        + PATH_BRIDGE_PRUNE_PROGRESS_HPP.read_text()
        + "\n"
        + PATH_BRIDGE_PRUNE_RESULT_CPP.read_text()
        + "\n"
        + PATH_BRIDGE_PRUNE_RESULT_HPP.read_text()
        + "\n"
        + PATH_BRIDGE_PRUNE_ROUND_CPP.read_text()
        + "\n"
        + PATH_BRIDGE_PRUNE_ROUND_HPP.read_text()
        + "\n"
        + PATH_BRIDGE_PRUNE_SELECTION_CPP.read_text()
        + "\n"
        + PATH_BRIDGE_PRUNE_SELECTION_HPP.read_text()
    )
    path_vertex_reduction_src = (
        (
            SOLVER
            / "src"
            / "path"
            / "reduction"
            / "path_vertex_reduction.cpp"
        ).read_text()
        + "\n"
        + (
            SOLVER
            / "include"
            / "bbsolver"
            / "path"
            / "reduction"
            / "path_vertex_reduction.hpp"
        ).read_text()
    )
    path_decomposed_solver_src = (
        PATH_DECOMPOSED_SOLVER_CPP.read_text()
        + "\n"
        + PATH_DECOMPOSED_SOLVER_HPP.read_text()
    )
    property_route_solver_src = (
        PROPERTY_ROUTE_SOLVER_CPP.read_text()
        + "\n"
        + PROPERTY_ROUTE_SOLVER_HPP.read_text()
    )
    path_replacement_phase2_fit_src = (
        PATH_REPLACEMENT_PHASE2_FIT_CPP.read_text()
        + "\n"
        + PATH_REPLACEMENT_PHASE2_FIT_HPP.read_text()
    )
    path_replacement_fraction_trial_src = (
        PATH_REPLACEMENT_FRACTION_TRIAL_CPP.read_text()
        + "\n"
        + PATH_REPLACEMENT_FRACTION_TRIAL_HPP.read_text()
    )
    path_replacement_solver_src = (
        PATH_REPLACEMENT_SOLVER_CPP.read_text()
        + "\n"
        + PATH_REPLACEMENT_SOLVER_HPP.read_text()
    )
    path_replacement_progress_src = (
        PATH_REPLACEMENT_PROGRESS_CPP.read_text()
        + "\n"
        + PATH_REPLACEMENT_PROGRESS_HPP.read_text()
    )
    progress_contract_src = (
        src
        + "\n"
        + solve_command_src
        + "\n"
        + progress_src
        + "\n"
        + path_bridge_prune_src
        + "\n"
        + path_decomposed_solver_src
        + "\n"
        + property_route_solver_src
        + "\n"
        + path_replacement_phase2_fit_src
        + "\n"
        + path_replacement_fraction_trial_src
        + "\n"
        + path_replacement_solver_src
        + "\n"
        + path_replacement_progress_src
        + "\n"
        + solve_lifecycle_reporting_src
        + "\n"
        + solve_path_preparation_src
        + "\n"
        + solve_property_post_processing_src
        + "\n"
        + solve_property_temporal_prelude_src
        + "\n"
        + solve_property_temporal_result_src
        + "\n"
        + solve_property_output_src
    )
    dp_src = (
        DP_PLACER_CPP.read_text()
        + "\n"
        + DP_PLACER_HPP.read_text()
        + "\n"
        + DP_FORWARD_PLACEMENT_CPP.read_text()
        + "\n"
        + DP_FORWARD_PLACEMENT_HPP.read_text()
        + "\n"
        + DP_PLACEMENT_PROGRESS_CPP.read_text()
        + "\n"
        + DP_PLACEMENT_PROGRESS_HPP.read_text()
    )
    dp_h = DP_PLACER_HPP.read_text()
    replacement_src = REPLACEMENT_TEMPORAL_CPP.read_text()
    replacement_h = REPLACEMENT_TEMPORAL_HPP.read_text()
    assert "PropertyProgressEvent" in progress_src, "progress helper must exist"
    assert "PlacementProgressEvent" in progress_src, "placement progress bridge must exist"
    assert '#include "bbsolver/progress/progress.hpp"' in solve_command_src, (
        "solve command must use progress module"
    )
    assert '#include "bbsolver/runtime/runtime_env.hpp"' in solve_lifecycle_reporting_src, (
        "solve lifecycle reporting must use runtime env module"
    )
    assert "BridgePruneLocalProgress" in path_vertex_reduction_src, (
        "bridge prune progress helper must exist"
    )
    assert "0.080 * vertex_progress" in path_vertex_reduction_src, (
        "bridge prune must use a visible progress span"
    )
    assert "BridgePruneProgressChunkSize" in path_vertex_reduction_src, (
        "bridge prune must chunk candidate progress"
    )
    assert '#include "bbsolver/path/bridge_prune/path_bridge_prune.hpp"' in solve_command_src, (
        "solve command must use bridge-prune module"
    )
    bridge_worker_lambda = PATH_BRIDGE_PRUNE_ROUND_CPP.read_text().split(
        "const auto evaluate_candidate =", 1
    )[1].split(
        "const std::size_t progress_chunk_size", 1
    )[0]
    assert "progress->Emit" not in bridge_worker_lambda, (
        "bridge prune worker lambda must not emit progress"
    )
    assert "std::min(0.895" in path_vertex_reduction_src, (
        "bridge prune progress must reach the high-80s before done"
    )
    assert "request.property_count, 0.90" in solve_property_post_processing_src, (
        "bridge prune done event must advance visibly"
    )
    assert "ReportPropertyTemporalSolveResult" in solve_property_temporal_result_src, (
        "temporal solve result reporting must stay in its named module"
    )
    assert '"temporal_solve_done"' in solve_property_temporal_result_src, (
        "temporal solve done progress event must stay covered after extraction"
    )
    assert '"temporal_solve"' in solve_property_temporal_result_src, (
        "temporal solve cancellation phase must remain stable after extraction"
    )
    assert "attempts % 8" not in path_bridge_prune_src, "bridge prune progress must not wait several candidates"
    assert '"segment_checks"' in progress_src, "placement progress must expose segment-check counts"
    assert '"dp_candidate_slots"' in progress_src, "placement progress must expose DP candidate slots"
    assert '"dp_fit_wall_ms"' in progress_src, "placement progress must expose DP fit wall time"
    assert '"dp_reduction_wall_ms"' in progress_src, "placement progress must expose DP reduction wall time"
    assert '"dp_final_anchor_fit_wall_ms"' in progress_src, "placement progress must expose final-anchor widening timing"
    assert '"fit_segment_hold_attempts"' in progress_src, "placement progress must expose ordinary hold-fit attempts"
    assert '"fit_segment_linear_attempts"' in progress_src, "placement progress must expose ordinary linear-fit attempts"
    assert '"fit_segment_hold_units_evaluated"' in progress_src, "placement progress must expose ordinary hold-fit evaluated units"
    assert '"fit_segment_linear_units_evaluated"' in progress_src, "placement progress must expose ordinary linear-fit evaluated units"
    assert '"fit_segment_hold_fail_fast_exits"' in progress_src, "placement progress must expose ordinary hold-fit fail-fast exits"
    assert '"fit_segment_linear_fail_fast_exits"' in progress_src, "placement progress must expose ordinary linear-fit fail-fast exits"
    assert '"fit_segment_hold_shape_outline_wall_ms"' in progress_src, "placement progress must expose ordinary hold shape-outline timing"
    assert '"fit_segment_linear_shape_outline_wall_ms"' in progress_src, "placement progress must expose ordinary linear shape-outline timing"
    assert '"fit_shape_temporal_attempts"' in progress_src, "placement progress must expose shape-temporal attempts"
    assert '"fit_shape_temporal_outline_wall_ms"' in progress_src, "placement progress must expose shape-temporal outline timing"
    assert '"fit_replacement_oracle_calls"' in progress_src, "placement progress must expose replacement fit oracle calls"
    assert '"fit_replacement_outline_wall_ms"' in progress_src, "placement progress must expose replacement outline-error timing"
    assert '"fit_replacement_relaxed_wall_ms"' in progress_src, "placement progress must expose relaxed endpoint timing"
    assert '"bridge_prune_accepted_fit_wall_ms"' in path_bridge_prune_src, "bridge prune progress must expose accepted fit timing"
    assert '"bridge_prune_accepted_validation_wall_ms"' in path_bridge_prune_src, "bridge prune progress must expose accepted validation timing"
    assert '"bridge_prune_rejected_validation_wall_ms"' in path_bridge_prune_src, "bridge prune progress must expose rejected validation timing"
    assert '"bridge_prune_rejected_sharp_wall_ms"' in path_bridge_prune_src, "bridge prune progress must expose rejected sharp-corner timing"
    assert '"bridge_prune_round_accepted_validation_wall_ms"' in path_bridge_prune_src, "bridge prune progress must expose round accepted validation timing"
    assert '"bridge_prune_batch_accepted_validation_wall_ms"' in path_bridge_prune_src, "bridge prune progress must expose batch accepted validation timing"
    assert "BridgePruneAcceptedBatchRemovalEvent(" in path_bridge_prune_src, "bridge prune batch replay must emit accepted-removal progress"
    assert "ResolveParallelJobs" in solve_command_config_src, "solver must resolve a bounded worker count"
    assert "SolveParallelRuntimeScope parallel_runtime_scope" in solve_command_src, (
        "--jobs must construct a solve-scoped runtime limiter"
    )
    assert "tbb::global_control::max_allowed_parallelism" in solve_parallel_runtime_scope_src, (
        "--jobs must constrain TBB instead of being a passive config field"
    )
    assert "kParallelJobsHardCap" in runtime_env_src, "parallel worker count must have a hard safety cap"
    assert "ParallelRuntimePhase" in runtime_env_src, "solver must log the effective parallel mode"
    assert "PlacementProgressFn" in dp_h, "DP placement must expose optional progress callback"
    assert "placement_progress_fn" in replacement_h, "replacement temporal solve must accept progress callback"
    assert "placement_progress_fn" in replacement_src, "replacement temporal solve must forward progress callback"
    assert "EmitPlacementProgress" in dp_src, "DP placement must emit inner progress"
    assert '"progress"' in progress_contract_src, "progress events must include numeric progress"
    assert '"phase"' in progress_contract_src, "progress events must include human-readable phase"
    for event in (
        "solve_start",
        "parallel_config",
        "property_prepare",
        "path_replacement_fit_start",
        "path_replacement_target_start",
        "path_replacement_target_phase2_start",
        "path_replacement_target_phase2_progress",
        "path_replacement_target_phase2_done",
        "path_replacement_target_layout_progress",
        "path_replacement_target_layout_done",
        "path_replacement_target_done",
        "path_replacement_target_rejected",
        "path_replacement_baseline_start",
        "path_replacement_baseline_progress",
        "path_replacement_baseline_done",
        "replacement_retry_start",
        "replacement_retry_done",
        "replacement_temporal_solve_progress",
        "temporal_solve_progress",
        "post_solve_vertex_reduction_start",
        "post_solve_vertex_bridge_prune_candidate",
        "post_solve_vertex_bridge_prune_progress",
        "post_solve_vertex_reduction_done",
        "static_key_run_collapse",
        "vert_done",
        "temporal_solve_done",
        "optimization_diagnostic",
        "path_validation_start",
        "path_validation_done",
        "landmark_subpaths_start",
        "property_done",
        "done",
    ):
        assert event in progress_contract_src, f"missing progress event: {event}"
    for stage in (
        "dp_start",
        "dp_anchor",
        "dp_done",
        "forward_start",
        "forward_anchor",
        "forward_done",
    ):
        assert stage in dp_src, f"missing placement progress stage: {stage}"


def test_bbsolver_progress_smoke_when_built():
    if not BBSOLVER.exists():
        print(f"[SKIP] bbsolver not built at {BBSOLVER}")
        return
    with tempfile.TemporaryDirectory(prefix="bb_progress_policy_") as tmp:
        out_path = Path(tmp) / "out.bbky.json"
        proc = subprocess.run(
            [
                str(BBSOLVER),
                "solve",
                str(FIXTURE),
                str(out_path),
                "--progress-fd",
                "1",
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
    events = []
    for raw in proc.stdout.splitlines():
        raw = raw.strip()
        if not raw.startswith("{"):
            continue
        events.append(json.loads(raw))
    assert events, "bbsolver must emit progress JSON"
    event_names = {event.get("event") for event in events}
    assert "solve_start" in event_names
    assert "parallel_config" in event_names
    assert "temporal_solve_progress" in event_names
    assert "temporal_solve_done" in event_names
    assert "done" in event_names
    plain_progress = [
        event for event in events
        if event.get("event") == "temporal_solve_progress"
    ]
    assert plain_progress, "plain temporal solve must emit placement progress"
    assert any(
        event.get("placement_stage") == "dp_anchor" for event in plain_progress
    ), "plain temporal progress must surface DP anchor placement stages"
    dp_anchor_events = [
        event for event in plain_progress
        if event.get("placement_stage") == "dp_anchor"
    ]
    dp_metric_events = [
        event for event in dp_anchor_events
        if "dp_candidate_slots" in event
    ]
    assert dp_metric_events, "DP anchor progress must include attribution metrics"
    last_dp_metric = dp_metric_events[-1]
    assert int(last_dp_metric["dp_candidate_slots"]) >= int(last_dp_metric["segment_checks"])
    assert float(last_dp_metric.get("dp_fit_wall_ms", 0.0)) >= 0.0
    assert float(last_dp_metric.get("dp_reduction_wall_ms", 0.0)) >= 0.0
    parallel_events = [
        event for event in events
        if event.get("event") == "parallel_config"
    ]
    assert parallel_events, "solver must report effective parallel runtime config"
    parallel_event = parallel_events[0]
    assert parallel_event.get("parallel_jobs_requested") == 1
    assert parallel_event.get("parallel_jobs_resolved") == 1
    assert int(parallel_event.get("parallel_jobs_detected", 0)) >= 1
    assert int(parallel_event.get("parallel_jobs_hard_cap", 0)) >= 1
    for event in events:
        assert "phase" in event, f"missing phase in {event}"
        assert "progress" in event, f"missing progress in {event}"
        assert 0.0 <= float(event["progress"]) <= 1.0, event


def test_bbsolver_progress_monotone_and_ordered_when_built():
    """Phase-2 readiness invariant: progress events emitted by bbsolver
    must (1) report `progress` as a non-decreasing sequence over the
    course of a single solve, and (2) emit the four pipeline-stage
    bracket events in the order `solve_start` -> `parallel_config` ->
    `temporal_solve_done` -> `done`.

    Locking this now means a future parallel work-stealing change that
    accidentally emits progress out of order, or that swaps the bracket
    sequence, fails the gate before it reaches AE. Verified against
    build097 on `color_pulse.bbsm.json` (8 events, 0 decreases).
    """
    if not BBSOLVER.exists():
        print(f"[SKIP] bbsolver not built at {BBSOLVER}")
        return
    with tempfile.TemporaryDirectory(prefix="bb_progress_monotone_") as tmp:
        out_path = Path(tmp) / "out.bbky.json"
        proc = subprocess.run(
            [
                str(BBSOLVER),
                "solve",
                str(FIXTURE),
                str(out_path),
                "--progress-fd",
                "1",
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
    events = []
    for raw in proc.stdout.splitlines():
        raw = raw.strip()
        if not raw.startswith("{"):
            continue
        events.append(json.loads(raw))
    assert events, "bbsolver must emit progress JSON"

    # (1) Monotonicity: no event's `progress` may be less than the
    # previous event's. Equal values are fine (multiple events per
    # phase-tick share progress).
    progress_values = [float(event["progress"]) for event in events]
    decreases = [
        (idx, prev, cur)
        for idx, (prev, cur) in enumerate(zip(progress_values, progress_values[1:]))
        if cur < prev
    ]
    assert not decreases, (
        f"progress went backwards at indices {decreases[:4]!r}; "
        f"phase-2 parallelization must not emit out-of-order progress"
    )

    # (2) Stage-bracket ordering: when each of the four stage events is
    # present, their first-occurrence indices must be strictly increasing.
    bracket_events = [
        "solve_start",
        "parallel_config",
        "temporal_solve_done",
        "done",
    ]
    first_index = {}
    for idx, event in enumerate(events):
        name = event.get("event")
        if name in bracket_events and name not in first_index:
            first_index[name] = idx
    present = [name for name in bracket_events if name in first_index]
    assert present == bracket_events, (
        f"expected all four bracket events in stream; got {present!r}"
    )
    ordered_indices = [first_index[name] for name in bracket_events]
    assert ordered_indices == sorted(ordered_indices), (
        f"bracket events out of order: "
        f"{list(zip(bracket_events, ordered_indices))!r}"
    )


def _duplicate_terminal_square_sample(tx: float) -> list[float]:
    return _duplicate_terminal_square_sample_xy(tx, 0.0)


def _duplicate_terminal_square_sample_xy(tx: float, ty: float) -> list[float]:
    coords = [
        (0.0 + tx, 0.0 + ty),
        (100.0 + tx, 0.0 + ty),
        (100.0 + tx, 100.0 + ty),
        (0.0 + tx, 100.0 + ty),
        (0.0 + tx, 0.0 + ty),
    ]
    flat: list[float] = [1.0, float(len(coords))]
    for x, y in coords:
        flat.extend([x, y, 0.0, 0.0, 0.0, 0.0])
    return flat


def _with_unlocked_shape_tangents(flat: list[float]) -> list[float]:
    out = list(flat)
    n = int(round(float(out[1])))
    for i in range(n):
        base = 2 + i * 6
        out[base + 2] = -10.0
        out[base + 3] = 4.0 + i
        out[base + 4] = 11.0
        out[base + 5] = 2.0
    return out


def _shape_tangent_max_deviation_from_180(flat: list[float]) -> float:
    n = int(round(float(flat[1])))
    worst = 0.0
    for i in range(n):
        base = 2 + i * 6
        ix, iy = float(flat[base + 2]), float(flat[base + 3])
        ox, oy = float(flat[base + 4]), float(flat[base + 5])
        il = math.hypot(ix, iy)
        ol = math.hypot(ox, oy)
        if il <= 1e-9 or ol <= 1e-9:
            continue
        cosv = max(-1.0, min(1.0, (ix * ox + iy * oy) / (il * ol)))
        worst = max(worst, abs(180.0 - math.degrees(math.acos(cosv))))
    return worst


def _duplicate_terminal_pentagon_sample(tx: float) -> list[float]:
    coords = [
        (0.0 + tx, 0.0),
        (100.0 + tx, 0.0),
        (120.0 + tx, 70.0),
        (50.0 + tx, 120.0),
        (-20.0 + tx, 70.0),
        (0.0 + tx, 0.0),
    ]
    flat: list[float] = [1.0, float(len(coords))]
    for x, y in coords:
        flat.extend([x, y, 0.0, 0.0, 0.0, 0.0])
    return flat


def _sharp_pentagon_sample(tx: float) -> list[float]:
    coords = [
        (50.0 + tx, -10.0),
        (85.267 + tx, 98.541),
        (-7.063 + tx, 31.459),
        (107.063 + tx, 31.459),
        (14.733 + tx, 98.541),
    ]
    flat: list[float] = [1.0, float(len(coords))]
    for x, y in coords:
        flat.extend([x, y, 0.0, 0.0, 0.0, 0.0])
    return flat


def _duplicate_terminal_soft_hexagon_sample(tx: float) -> list[float]:
    coords: list[tuple[float, float]] = []
    for i in range(6):
        theta = 2.0 * math.pi * i / 6.0
        coords.append((100.0 * math.cos(theta) + tx, 60.0 * math.sin(theta)))
    coords.append(coords[0])
    flat: list[float] = [1.0, float(len(coords))]
    for x, y in coords:
        flat.extend([x, y, 0.0, 0.0, 0.0, 0.0])
    return flat


def _low_vertex_hexagon_sample(tx: float) -> list[float]:
    coords: list[tuple[float, float]] = []
    for i in range(6):
        theta = 2.0 * math.pi * i / 6.0
        coords.append((80.0 * math.cos(theta) + tx, 55.0 * math.sin(theta)))
    flat: list[float] = [1.0, float(len(coords))]
    for x, y in coords:
        flat.extend([x, y, 0.0, 0.0, 0.0, 0.0])
    return flat


def _duplicate_terminal_collapsed_sample(vertex_count: int) -> list[float]:
    flat: list[float] = [1.0, float(vertex_count)]
    for _ in range(vertex_count):
        flat.extend([0.0, 0.0, 0.0, 0.0, 0.0, 0.0])
    return flat


def _duplicate_terminal_fixture(samples: list[dict[str, object]]) -> dict[str, object]:
    max_dimensions = max(len(sample["v"]) for sample in samples)
    variable_topology = any(len(sample["v"]) != max_dimensions for sample in samples)
    return {
        "_schema": "samples",
        "schema_version": 1,
        "request_id": "duplicate-terminal-closure-policy",
        "comp": {
            "fps": 30.0,
            "duration_sec": 1.0,
            "width": 1920,
            "height": 1080,
            "pixel_aspect": 1.0,
            "shutter_angle_deg": 180.0,
            "shutter_phase_deg": -90.0,
            "motion_blur_enabled": False,
            "work_area_start_sec": 0.0,
            "work_area_end_sec": 1.0,
        },
        "properties": [
            {
                "property": {
                    "id": "shape/path",
                    "match_name": "ADBE Vector Shape",
                    "display_name": "Path",
                    "kind": "Custom",
                    "dimensions": max_dimensions,
                    "is_spatial": False,
                    "is_separated": False,
                    "units_label": "shape_flat",
                    "shape_variable_topology": variable_topology,
                    "min_value": [],
                    "max_value": [],
                },
                "t_start_sec": 0.0,
                "t_end_sec": 1.0,
                "samples_per_frame": 1,
                "samples": samples,
            }
        ],
        "config": {
            "tolerance": 0.5,
            "tolerance_screen_px": 0.0,
            "weight_pos": 1.0,
            "weight_vel": 0.1,
            "weight_acc": 0.01,
            "weight_curv": 0.0,
            "weight_screen": 0.0,
            "allow_hold": True,
            "allow_linear": True,
            "allow_bezier": True,
            "allow_shape_temporal_bezier": True,
            "allow_path_spatial_fit": False,
            "allow_path_replacement_fit": False,
            "path_replacement_prefer_vertices": False,
            "path_preserve_sharp_corners": True,
            "path_sharp_corner_angle_deg": 90.0,
            "path_sharp_corner_tolerance": 1.5,
            "path_replacement_min_vertices": 4,
            "path_replacement_max_vertices": 0,
            "min_influence": 0.1,
            "max_influence": 100.0,
            "max_iters_per_segment": 100,
            "min_segment_frames": 2,
            "max_keys_hint": 0,
            "parallel_jobs": 0,
            "verbose": False,
        },
    }


def _timed_duplicate_terminal_samples() -> list[dict[str, object]]:
    return [
        {
            "t_sec": 0.0,
            "v": _duplicate_terminal_soft_hexagon_sample(0.0),
            "interp_in": "Linear",
            "interp_out": "Bezier",
            "temporal_ease_in": [{"speed": 0.0, "influence": 33.3}],
            "temporal_ease_out": [{"speed": 12.0, "influence": 72.0}],
            "temporal_continuous": True,
            "temporal_auto_bezier": False,
        },
        {
            "t_sec": 1.0,
            "v": _duplicate_terminal_soft_hexagon_sample(10.0),
            "interp_in": "Bezier",
            "interp_out": "Hold",
            "temporal_ease_in": [{"speed": 9.0, "influence": 64.0}],
            "temporal_ease_out": [{"speed": 0.0, "influence": 33.3}],
            "temporal_continuous": False,
            "temporal_auto_bezier": True,
        },
    ]


def _near_optimal_shape_samples() -> tuple[list[dict[str, object]], list[float], list[float]]:
    key_times = [0.0, 0.25, 0.5, 0.75, 1.0]
    key_offsets = [0.0, 18.0, -6.0, 24.0, 10.0]
    samples: list[dict[str, object]] = []
    sample_count = 601
    for i in range(sample_count):
        t = i / float(sample_count - 1)
        segment = 0
        while segment + 1 < len(key_times) and t > key_times[segment + 1]:
            segment += 1
        if segment + 1 >= len(key_times):
            tx = key_offsets[-1]
        else:
            span = key_times[segment + 1] - key_times[segment]
            u = (t - key_times[segment]) / span if span > 0 else 0.0
            tx = key_offsets[segment] * (1.0 - u) + key_offsets[segment + 1] * u
        samples.append({"t_sec": t, "v": _low_vertex_hexagon_sample(tx)})
    return samples, key_times, key_offsets


def _run_solve_fixture(
    fixture: dict[str, object],
    tolerance: float = 0.5,
    jobs: int | None = None,
) -> tuple[str, dict[str, object]]:
    with tempfile.TemporaryDirectory(prefix="bb_duplicate_terminal_") as tmp:
        tmp_path = Path(tmp)
        in_path = tmp_path / "in.bbsm.json"
        out_path = tmp_path / "out.bbky.json"
        in_path.write_text(json.dumps(fixture), encoding="utf-8")
        cmd = [
            str(BBSOLVER),
            "solve",
            str(in_path),
            str(out_path),
            "--progress-fd",
            "1",
            "--tolerance",
            str(tolerance),
        ]
        if jobs is not None:
            cmd.extend(["--jobs", str(jobs)])
        proc = subprocess.run(
            cmd,
            text=True,
            capture_output=True,
            timeout=20,
            check=True,
        )
        return proc.stdout, json.loads(out_path.read_text())


def test_duplicate_terminal_closure_vertex_reduction_when_built():
    if not BBSOLVER.exists():
        print(f"[SKIP] bbsolver not built at {BBSOLVER}")
        return

    fixture = _duplicate_terminal_fixture(
        [
            {"t_sec": 0.0, "v": _duplicate_terminal_square_sample(0.0)},
            {"t_sec": 1.0, "v": _duplicate_terminal_square_sample(10.0)},
        ]
    )
    stdout, bundle = _run_solve_fixture(fixture)

    events = [
        json.loads(raw)
        for raw in stdout.splitlines()
        if raw.strip().startswith("{")
    ]
    assert any(
        event.get("event") == "post_solve_vertex_reduction_done"
        and event.get("accepted") is True
        and event.get("source_vertices") == 5
        and event.get("fitted_vertices") == 4
        for event in events
    ), stdout
    result = bundle["property_results"][0]
    assert "post_solve_vertex_reduction_accepted" in result["notes"]
    assert "mode=duplicate_terminal_closure" in result["notes"]
    assert {int(round(key["v"][1])) for key in result["keys"]} == {4}
    assert len(result["keys"]) == 2
    assert float(result["max_err"]) <= 0.5


def test_bridge_prune_runs_after_duplicate_terminal_prepass_when_enabled():
    if not BBSOLVER.exists():
        print(f"[SKIP] bbsolver not built at {BBSOLVER}")
        return

    fixture = _duplicate_terminal_fixture(
        [
            {"t_sec": 0.0, "v": _duplicate_terminal_soft_hexagon_sample(0.0)},
            {"t_sec": 1.0, "v": _duplicate_terminal_soft_hexagon_sample(10.0)},
        ]
    )
    fixture["config"]["tolerance"] = 50.0
    fixture["config"]["tolerance_screen_px"] = 50.0
    fixture["config"]["allow_path_replacement_fit"] = False
    fixture["config"]["path_replacement_prefer_vertices"] = True
    fixture["config"]["path_preserve_sharp_corners"] = True
    stdout, bundle = _run_solve_fixture(fixture, tolerance=50.0)
    events = [
        json.loads(raw)
        for raw in stdout.splitlines()
        if raw.strip().startswith("{")
    ]
    assert any(
        event.get("event") == "post_solve_vertex_bridge_prune_candidate"
        for event in events
    ), stdout
    assert any(
        event.get("event") == "post_solve_vertex_reduction_done"
        and event.get("accepted") is True
        and event.get("source_vertices") == 7
        and event.get("fitted_vertices") < 6
        for event in events
    ), stdout
    result = bundle["property_results"][0]
    notes = result["notes"]
    assert "mode=post_temporal_bridge_prune" in notes
    assert "prepass=duplicate_terminal_closure" in notes
    assert "sharp_corner_preserve=on" in notes
    for field in (
        "bridge_prune_fit_failures",
        "bridge_prune_validation_failures",
        "bridge_prune_sharp_failures",
        "bridge_prune_accepted_candidates",
    ):
        assert field + "=" in notes
    assert _note_int(notes, "bridge_prune_accepted_candidates") > 0
    assert max(int(round(key["v"][1])) for key in result["keys"]) < 6
    assert "keys=" + str(len(result["keys"])) in notes
    assert float(result["max_err"]) <= 50.0


def test_bridge_prune_accepts_collapsed_shape_frames_when_enabled():
    if not BBSOLVER.exists():
        print(f"[SKIP] bbsolver not built at {BBSOLVER}")
        return

    fixture = _duplicate_terminal_fixture(
        [
            {"t_sec": 0.0, "v": _duplicate_terminal_collapsed_sample(7)},
            {"t_sec": 1.0, "v": _duplicate_terminal_collapsed_sample(7)},
        ]
    )
    fixture["config"]["tolerance"] = 3.0
    fixture["config"]["tolerance_screen_px"] = 3.0
    fixture["config"]["path_replacement_prefer_vertices"] = True
    fixture["config"]["path_preserve_sharp_corners"] = True
    stdout, bundle = _run_solve_fixture(fixture, tolerance=3.0)
    events = [
        json.loads(raw)
        for raw in stdout.splitlines()
        if raw.strip().startswith("{")
    ]
    bridge_progress = [
        float(event["progress"])
        for event in events
        if event.get("event") in (
            "post_solve_vertex_bridge_prune_candidate",
            "post_solve_vertex_bridge_prune_progress",
        )
    ]
    assert bridge_progress, stdout
    assert any(
        event.get("event") == "post_solve_vertex_bridge_prune_progress"
        and "candidate_count" in event
        and "candidates_checked" in event
        for event in events
    ), stdout
    assert any(
        event.get("event") == "post_solve_vertex_bridge_prune_progress"
        and int(event.get("candidates_checked", 0)) > 0
        and int(event.get("candidate_count", 0)) >= int(event.get("candidates_checked", 0))
        for event in events
    ), stdout
    decreases = [
        (prev, cur)
        for prev, cur in zip(bridge_progress, bridge_progress[1:])
        if cur + 1e-12 < prev
    ]
    assert not decreases, f"bridge prune progress went backwards: {decreases[:4]!r}"
    assert max(bridge_progress) - min(bridge_progress) >= 0.03, stdout
    assert any(
        event.get("event") == "post_solve_vertex_reduction_done"
        and event.get("accepted") is True
        and event.get("source_vertices") == 7
        and event.get("fitted_vertices") == 4
        for event in events
    ), stdout
    result = bundle["property_results"][0]
    notes = result["notes"]
    assert "mode=post_temporal_bridge_prune" in notes
    assert "prepass=duplicate_terminal_closure" in notes
    assert _note_int(notes, "bridge_prune_accepted_candidates") > 0
    assert _note_int(notes, "bridge_prune_fit_failures") >= 0
    assert _note_int(notes, "bridge_prune_validation_failures") >= 0
    assert _note_int(notes, "bridge_prune_sharp_failures") >= 0
    assert {int(round(key["v"][1])) for key in result["keys"]} == {4}
    assert float(result["max_err"]) == 0.0


def test_sharp_corner_preservation_blocks_high_tolerance_corner_deletion_when_built():
    if not BBSOLVER.exists():
        print(f"[SKIP] bbsolver not built at {BBSOLVER}")
        return

    fixture = _duplicate_terminal_fixture(
        [
            {"t_sec": 0.0, "v": _sharp_pentagon_sample(0.0)},
            {"t_sec": 1.0, "v": _sharp_pentagon_sample(10.0)},
        ]
    )
    fixture["config"]["tolerance"] = 100.0
    fixture["config"]["allow_path_replacement_fit"] = True
    fixture["config"]["path_replacement_prefer_vertices"] = True
    fixture["config"]["path_preserve_sharp_corners"] = True
    stdout, bundle = _run_solve_fixture(fixture, tolerance=100.0)

    events = [
        json.loads(raw)
        for raw in stdout.splitlines()
        if raw.strip().startswith("{")
    ]
    assert any(
        event.get("event") == "post_solve_vertex_reduction_done"
        and event.get("accepted") is False
        for event in events
    ), stdout
    result = bundle["property_results"][0]
    notes = result["notes"]
    assert "sharp_corner_preserve" in notes
    assert "post_solve_vertex_reduction_accepted" not in notes
    assert _note_int(notes, "bridge_prune_fit_failures") >= 0
    assert _note_int(notes, "bridge_prune_validation_failures") >= 0
    assert _note_int(notes, "bridge_prune_sharp_failures") >= 0
    assert _note_int(notes, "bridge_prune_accepted_candidates") == 0
    assert "protected_corner_skips=4" in notes
    assert {int(round(key["v"][1])) for key in result["keys"]} == {5}


def test_duplicate_terminal_reduction_skips_mixed_key_topology_when_built():
    if not BBSOLVER.exists():
        print(f"[SKIP] bbsolver not built at {BBSOLVER}")
        return

    fixture = _duplicate_terminal_fixture(
        [
            {"t_sec": 0.0, "v": _duplicate_terminal_square_sample(0.0)},
            {"t_sec": 1.0, "v": _duplicate_terminal_pentagon_sample(10.0)},
        ]
    )
    stdout, bundle = _run_solve_fixture(fixture)
    events = [
        json.loads(raw)
        for raw in stdout.splitlines()
        if raw.strip().startswith("{")
    ]
    assert any(
        event.get("event") == "post_solve_vertex_reduction_done"
        and event.get("accepted") is False
        for event in events
    ), stdout
    result = bundle["property_results"][0]
    assert "post_solve_vertex_reduction_skipped: mixed_key_topology" in result["notes"]
    assert "post_solve_vertex_reduction_accepted" not in result["notes"]
    assert {int(round(key["v"][1])) for key in result["keys"]} == {5, 6}


def test_second_pass_bridge_prune_mixed_key_topology_when_enabled():
    if not BBSOLVER.exists():
        print(f"[SKIP] bbsolver not built at {BBSOLVER}")
        return

    fixture = _duplicate_terminal_fixture(
        [
            {"t_sec": 0.0, "v": _duplicate_terminal_square_sample(0.0)},
            {"t_sec": 1.0, "v": _duplicate_terminal_pentagon_sample(10.0)},
        ]
    )
    fixture["config"]["allow_path_replacement_fit"] = True
    fixture["config"]["path_replacement_prefer_vertices"] = True
    stdout, bundle = _run_solve_fixture(fixture)
    events = [
        json.loads(raw)
        for raw in stdout.splitlines()
        if raw.strip().startswith("{")
    ]
    assert any(
        event.get("event") == "post_solve_vertex_bridge_prune_candidate"
        for event in events
    ), stdout
    result = bundle["property_results"][0]
    assert {int(round(key["v"][1])) for key in result["keys"]} == {5}
    assert len(result["keys"]) == 2
    assert float(result["max_err"]) <= 0.5
    direct_replacement = "path_replacement_fit;" in result["notes"]
    accepted_post_prune = "post_solve_vertex_reduction_accepted" in result["notes"]
    assert direct_replacement or accepted_post_prune, result["notes"]
    if accepted_post_prune:
        assert "mode=post_temporal_bridge_prune" in result["notes"]
        assert "bridge_prune_steps=" in result["notes"]


def test_parallel_jobs_preserve_bridge_prune_output_when_built():
    if not BBSOLVER.exists():
        print(f"[SKIP] bbsolver not built at {BBSOLVER}")
        return

    fixture = _duplicate_terminal_fixture(
        [
            {"t_sec": 0.0, "v": _duplicate_terminal_soft_hexagon_sample(0.0)},
            {"t_sec": 1.0, "v": _duplicate_terminal_soft_hexagon_sample(10.0)},
        ]
    )
    fixture["config"]["tolerance"] = 50.0
    fixture["config"]["tolerance_screen_px"] = 50.0
    fixture["config"]["allow_path_replacement_fit"] = False
    fixture["config"]["path_replacement_prefer_vertices"] = True
    fixture["config"]["path_preserve_sharp_corners"] = True

    serial_stdout, serial_bundle = _run_solve_fixture(
        fixture, tolerance=50.0, jobs=1)
    parallel_stdout, parallel_bundle = _run_solve_fixture(
        fixture, tolerance=50.0, jobs=4)

    serial_result = serial_bundle["property_results"][0]
    parallel_result = parallel_bundle["property_results"][0]
    assert serial_result["keys"] == parallel_result["keys"]
    assert serial_result["notes"] == parallel_result["notes"]
    assert serial_result["max_err"] == parallel_result["max_err"]

    serial_parallel_events = [
        json.loads(raw) for raw in serial_stdout.splitlines()
        if raw.strip().startswith("{")
        and json.loads(raw).get("event") == "parallel_config"
    ]
    parallel_parallel_events = [
        json.loads(raw) for raw in parallel_stdout.splitlines()
        if raw.strip().startswith("{")
        and json.loads(raw).get("event") == "parallel_config"
    ]
    assert serial_parallel_events[0]["parallel_jobs_resolved"] == 1
    assert parallel_parallel_events[0]["parallel_jobs_resolved"] >= 1


def test_temporal_only_mode_disables_vertex_reduction_when_built():
    if not BBSOLVER.exists():
        print(f"[SKIP] bbsolver not built at {BBSOLVER}")
        return

    fixture = _duplicate_terminal_fixture(
        [
            {"t_sec": 0.0, "v": _duplicate_terminal_soft_hexagon_sample(0.0)},
            {"t_sec": 1.0, "v": _duplicate_terminal_soft_hexagon_sample(10.0)},
        ]
    )
    fixture["config"]["tolerance"] = 50.0
    fixture["config"]["tolerance_screen_px"] = 50.0
    fixture["config"]["path_replacement_prefer_vertices"] = True
    fixture["config"]["solve_optimization_mode"] = "temporal_only"

    stdout, bundle = _run_solve_fixture(fixture, tolerance=50.0)
    events = [
        json.loads(raw)
        for raw in stdout.splitlines()
        if raw.strip().startswith("{")
    ]
    assert not any(
        event.get("event") == "post_solve_vertex_reduction_done"
        for event in events
    ), stdout
    result = bundle["property_results"][0]
    assert "post_solve_vertex_reduction_accepted" not in result["notes"]
    assert {int(round(key["v"][1])) for key in result["keys"]} == {7}


def test_shape_flat_near_optimal_fast_path_preserves_source_keys_when_built():
    if not BBSOLVER.exists():
        print(f"[SKIP] bbsolver not built at {BBSOLVER}")
        return

    samples, source_key_times, source_key_offsets = _near_optimal_shape_samples()
    fixture = _duplicate_terminal_fixture(samples)
    fixture["request_id"] = "shape-flat-near-optimal-fast-path"
    fixture["properties"][0]["property"]["source_key_times"] = source_key_times
    fixture["config"]["allow_shape_temporal_bezier"] = True
    fixture["config"]["allow_path_spatial_fit"] = True
    fixture["config"]["allow_path_replacement_fit"] = False
    fixture["config"]["path_replacement_prefer_vertices"] = True

    stdout, bundle = _run_solve_fixture(fixture, tolerance=0.5, jobs=1)
    events = [
        json.loads(raw)
        for raw in stdout.splitlines()
        if raw.strip().startswith("{")
    ]
    assert any(
        event.get("event") == "optimization_diagnostic"
        and "Motion Smooth" in event.get("phase", "")
        and "shape_flat_already_near_optimal=true" in event.get("notes", "")
        for event in events
    ), stdout
    assert not any(
        event.get("event") in {
            "path_replacement_fit_start",
            "path_replacement_fit",
            "path_fit_start",
            "path_fit",
            "post_solve_vertex_reduction_start",
            "post_solve_vertex_reduction_done",
        }
        for event in events
    ), stdout
    assert not any(
        "bridge_prune" in str(event.get("event", ""))
        for event in events
    ), stdout

    result = bundle["property_results"][0]
    notes = result["notes"]
    assert "shape_flat_already_near_optimal=true" in notes
    assert "source_key_preservation_fast_path=true" in notes
    assert "source_key_count=5" in notes
    assert "source_vertices=6" in notes
    assert "input_samples=601" in notes
    assert "motion_smooth_recommended=true" in notes
    assert "Motion Smooth" in notes
    assert "replacement_path_applied" not in notes
    assert "post_temporal_bridge_prune" not in notes
    assert len(result["keys"]) == len(source_key_times)
    for key, t_sec, tx in zip(result["keys"], source_key_times, source_key_offsets):
        assert abs(float(key["t_sec"]) - t_sec) <= 1e-9
        assert _vectors_close(key["v"], _low_vertex_hexagon_sample(tx))


def test_vertex_only_mode_skips_temporal_and_runs_vertex_reduction_when_built():
    if not BBSOLVER.exists():
        print(f"[SKIP] bbsolver not built at {BBSOLVER}")
        return

    fixture = _duplicate_terminal_fixture(_timed_duplicate_terminal_samples())
    fixture["config"]["tolerance"] = 50.0
    fixture["config"]["tolerance_screen_px"] = 50.0
    fixture["config"]["path_replacement_prefer_vertices"] = True
    fixture["config"]["solve_optimization_mode"] = "vertex_only"

    stdout, bundle = _run_solve_fixture(fixture, tolerance=50.0)
    events = [
        json.loads(raw)
        for raw in stdout.splitlines()
        if raw.strip().startswith("{")
    ]
    assert any(
        event.get("event") == "post_solve_vertex_reduction_done"
        and event.get("accepted") is True
        for event in events
    ), stdout
    assert not any(
        event.get("event") == "optimization_diagnostic"
        for event in events
    ), stdout
    result = bundle["property_results"][0]
    assert "solve_mode_vertex_only" in result["notes"]
    assert "source_key_timing_preserved=true" in result["notes"]
    assert "post_solve_vertex_reduction_accepted" in result["notes"]
    assert max(int(round(key["v"][1])) for key in result["keys"]) < 7
    assert result["keys"][0]["interp_out"] == "Bezier"
    assert result["keys"][1]["interp_in"] == "Bezier"
    assert result["keys"][1]["interp_out"] == "Hold"
    assert result["keys"][0]["temporal_ease_out"][0]["influence"] == 72.0
    assert result["keys"][1]["temporal_ease_in"][0]["speed"] == 9.0
    assert result["keys"][0]["temporal_continuous"] is True
    assert result["keys"][1]["temporal_auto_bezier"] is True


def test_motion_smooth_mode_smooths_shape_paths_without_endpoint_deletion_when_built():
    if not BBSOLVER.exists():
        print(f"[SKIP] bbsolver not built at {BBSOLVER}")
        return

    start = _duplicate_terminal_square_sample(0.0)
    noisy_middle = _duplicate_terminal_square_sample(80.0)
    end = _duplicate_terminal_square_sample(10.0)
    fixture = _duplicate_terminal_fixture(
        [
            {"t_sec": 0.0, "v": start},
            {"t_sec": 0.5, "v": noisy_middle},
            {"t_sec": 1.0, "v": end},
        ]
    )
    fixture["config"]["solve_optimization_mode"] = "motion_smooth"
    fixture["config"]["motion_smooth_use_ease"] = True
    fixture["config"]["allow_path_replacement_fit"] = True
    fixture["config"]["path_replacement_prefer_vertices"] = True
    fixture["properties"][0]["property"]["source_key_times"] = [0.0, 0.5, 1.0]

    stdout, bundle = _run_solve_fixture(fixture)
    events = [
        json.loads(raw)
        for raw in stdout.splitlines()
        if raw.strip().startswith("{")
    ]
    assert not any(
        event.get("event") == "path_replacement_fit_start"
        for event in events
    ), stdout
    assert not any(
        event.get("event") == "post_solve_vertex_reduction_done"
        for event in events
    ), stdout
    result = bundle["property_results"][0]
    assert "solve_mode_motion_smooth" in result["notes"]
    assert "motion_smooth_shape_rove_time=true" in result["notes"]
    assert "motion_smooth_shape_trajectory_filter=true" in result["notes"]
    assert "key_schedule=source_keys_roved" in result["notes"]
    assert "max_smoothing_displacement=" in result["notes"]
    assert "max_control_smoothing_displacement=" in result["notes"]
    assert "trajectory_turn_before_deg=" in result["notes"]
    assert "trajectory_turn_after_deg=" in result["notes"]
    assert len(result["keys"]) == 3
    assert result["keys"][0]["t_sec"] == 0.0
    assert result["keys"][0]["t_sec"] < result["keys"][1]["t_sec"] < result["keys"][2]["t_sec"]
    assert result["keys"][2]["t_sec"] == 1.0
    assert result["keys"][0]["v"] == start
    assert result["keys"][2]["v"] == end
    assert result["keys"][1]["v"] != noisy_middle
    assert result["keys"][0]["interp_out"] == "Bezier"


def test_motion_smooth_shape_paths_default_to_linear_roved_timing_when_built():
    if not BBSOLVER.exists():
        print(f"[SKIP] bbsolver not built at {BBSOLVER}")
        return

    start = _duplicate_terminal_square_sample(0.0)
    noisy_middle = _duplicate_terminal_square_sample(80.0)
    end = _duplicate_terminal_square_sample(10.0)
    fixture = _duplicate_terminal_fixture(
        [
            {"t_sec": 0.0, "v": start},
            {"t_sec": 0.5, "v": noisy_middle},
            {"t_sec": 1.0, "v": end},
        ]
    )
    fixture["config"]["solve_optimization_mode"] = "motion_smooth"
    fixture["config"]["motion_smooth_use_ease"] = False
    fixture["properties"][0]["property"]["source_key_times"] = [0.0, 0.5, 1.0]

    _stdout, bundle = _run_solve_fixture(fixture)
    result = bundle["property_results"][0]
    assert "solve_mode_motion_smooth" in result["notes"]
    assert "motion_smooth_shape_rove_time=true" in result["notes"]
    assert "motion_smooth_shape_trajectory_filter=true" in result["notes"]
    assert "key_schedule=source_keys_roved" in result["notes"]
    assert "rove_applied=true" in result["notes"]
    assert "motion_smooth_ease=off" in result["notes"]
    assert "motion_smooth_shape_ease_default" not in result["notes"]
    assert len(result["keys"]) == 3
    assert result["keys"][0]["v"] == start
    assert result["keys"][2]["v"] == end
    assert result["keys"][1]["v"] != noisy_middle
    assert result["keys"][0]["t_sec"] < result["keys"][1]["t_sec"] < result["keys"][2]["t_sec"]
    assert result["keys"][0]["interp_out"] == "Linear"
    assert result["keys"][1]["interp_in"] == "Linear"
    assert result["keys"][0]["temporal_continuous"] is False


def test_motion_smooth_shape_path_reduces_key_trajectory_turn_when_built():
    if not BBSOLVER.exists():
        print(f"[SKIP] bbsolver not built at {BBSOLVER}")
        return

    source = [
        (0.0, 0.0, 0.0),
        (0.25, 120.0, 0.0),
        (0.5, 80.0, 140.0),
        (0.75, -60.0, 110.0),
        (1.0, 0.0, 0.0),
    ]
    fixture = _duplicate_terminal_fixture(
        [
            {
                "t_sec": t,
                "v": _with_unlocked_shape_tangents(
                    _duplicate_terminal_square_sample_xy(tx, ty)
                ),
            }
            for t, tx, ty in source
        ]
    )
    fixture["config"]["solve_optimization_mode"] = "motion_smooth"
    fixture["config"]["motion_smooth_use_ease"] = False
    fixture["config"]["motion_smooth_tolerance"] = 3.0
    fixture["properties"][0]["property"]["source_key_times"] = [
        t for t, _tx, _ty in source
    ]

    _stdout, bundle = _run_solve_fixture(fixture)
    result = bundle["property_results"][0]
    notes = result["notes"]
    assert "motion_smooth_shape_trajectory_filter=true" in notes
    assert "key_schedule=source_keys_roved" in notes
    assert "motion_smooth_closed_loop=true" in notes
    assert "closed_loop_resample=true" in notes
    assert "adaptive_closed_loop_resample=true" in notes
    assert "shape_tangent_lock=true" in notes
    assert _note_float(notes, "shape_tangent_pairs_locked") > 0
    assert _note_float(notes, "shape_tangent_lock_max_deviation_before_deg") > 1.0
    assert "motion_smooth_ease=off" in notes
    assert _note_float(notes, "trajectory_turn_after_deg") < _note_float(
        notes, "trajectory_turn_before_deg"
    )
    assert _note_float(notes, "max_control_smoothing_displacement") > 0.0
    assert len(result["keys"]) > len(source)
    assert result["keys"][-1]["v"] == result["keys"][0]["v"]
    assert _shape_tangent_max_deviation_from_180(result["keys"][0]["v"]) <= 1e-6
    assert result["keys"][0]["interp_out"] == "Linear"
    assert result["keys"][1]["interp_in"] == "Linear"


def test_motion_smooth_shape_path_ignores_redundant_source_keys_when_built():
    if not BBSOLVER.exists():
        print(f"[SKIP] bbsolver not built at {BBSOLVER}")
        return

    real_keys = [
        (0.0, 0.0, 0.0),
        (0.2, 120.0, 0.0),
        (0.45, 80.0, 140.0),
        (0.75, -60.0, 110.0),
        (1.0, 0.0, 0.0),
    ]
    redundant_source_key_times = [
        0.0,
        0.15,
        0.2,
        0.235,
        0.42,
        0.45,
        0.48,
        0.72,
        0.75,
        0.78,
        0.97,
        1.0,
    ]

    def value_at(t: float) -> list[float]:
        segment = 0
        while segment + 1 < len(real_keys) and t > real_keys[segment + 1][0]:
            segment += 1
        if segment + 1 >= len(real_keys):
            tx, ty = real_keys[-1][1], real_keys[-1][2]
        else:
            left_t, left_x, left_y = real_keys[segment]
            right_t, right_x, right_y = real_keys[segment + 1]
            span = right_t - left_t
            u = (t - left_t) / span if span > 0 else 0.0
            tx = left_x * (1.0 - u) + right_x * u
            ty = left_y * (1.0 - u) + right_y * u
        return _with_unlocked_shape_tangents(
            _duplicate_terminal_square_sample_xy(tx, ty)
        )

    sample_count = 121
    fixture = _duplicate_terminal_fixture(
        [
            {"t_sec": i / float(sample_count - 1), "v": value_at(i / float(sample_count - 1))}
            for i in range(sample_count)
        ]
    )
    fixture["request_id"] = "motion-smooth-redundant-source-key-policy"
    fixture["config"]["solve_optimization_mode"] = "motion_smooth"
    fixture["config"]["motion_smooth_use_ease"] = False
    fixture["config"]["motion_smooth_tolerance"] = 3.0
    fixture["properties"][0]["property"]["source_key_times"] = (
        redundant_source_key_times
    )

    _stdout, bundle = _run_solve_fixture(fixture)
    result = bundle["property_results"][0]
    notes = result["notes"]
    assert "solve_mode_motion_smooth" in notes
    assert "motion_smooth_shape_trajectory_filter=true" in notes
    assert "source_key_anchor_simplification=true" in notes
    assert "source_key_count_raw=12" in notes
    assert "source_key_count=5" in notes
    assert "source_key_simplified_count=5" in notes
    assert "redundant_source_keys_removed=7" in notes
    assert "motion_smooth_source_fidelity=false" in notes
    assert "source_fidelity_samples=0" in notes
    assert "motion_smooth_closed_loop=true" in notes
    assert len(result["keys"]) < 90
    assert result["keys"][-1]["v"] == result["keys"][0]["v"]

    fidelity_fixture = json.loads(json.dumps(fixture))
    fidelity_fixture["config"]["motion_smooth_source_fidelity"] = True
    _stdout_fidelity, fidelity_bundle = _run_solve_fixture(fidelity_fixture)
    fidelity_result = fidelity_bundle["property_results"][0]
    fidelity_notes = fidelity_result["notes"]
    assert "source_key_anchor_simplification=true" in fidelity_notes
    assert "source_key_count_raw=12" in fidelity_notes
    assert "source_key_count=5" in fidelity_notes
    assert "redundant_source_keys_removed=7" in fidelity_notes
    assert "motion_smooth_source_fidelity=true" in fidelity_notes
    assert "source_fidelity_samples=5" in fidelity_notes
    assert "source_fidelity_mode=source_key_pose_constraints" in fidelity_notes
    assert "source_pose_constraints=5" in fidelity_notes
    assert "source_pose_constraint_keys=5" in fidelity_notes
    assert "shape_tangent_lock_skipped_source_pose_constraints=5" in fidelity_notes
    assert "key_schedule=source_key_times_spline" in fidelity_notes
    assert "rove_applied=false" in fidelity_notes
    assert "source_pose_interval_rove=true" in fidelity_notes
    assert _note_float(fidelity_notes, "source_pose_interval_rove_max_time_shift_sec") > 0.0
    assert _note_float(fidelity_notes, "loop_target_turn_deg") < _note_float(
        notes, "loop_target_turn_deg"
    )
    assert "source_error_not_constrained=false" in fidelity_notes
    assert len(fidelity_result["keys"]) > len(real_keys)
    assert len(fidelity_result["keys"]) <= 120
    for t_sec, tx, ty in real_keys:
        assert _vectors_close(
            _key_at_time(fidelity_result["keys"], t_sec)["v"],
            value_at(t_sec),
        )


def test_motion_smooth_source_fidelity_preserves_source_key_poses_when_built():
    if not BBSOLVER.exists():
        print(f"[SKIP] bbsolver not built at {BBSOLVER}")
        return

    source = [
        (0.0, 0.0, 0.0),
        (0.2, 120.0, 0.0),
        (0.45, 80.0, 140.0),
        (0.75, -60.0, 110.0),
        (1.0, 0.0, 0.0),
    ]

    def value_at(t: float) -> list[float]:
        segment = 0
        while segment + 1 < len(source) and t > source[segment + 1][0]:
            segment += 1
        if segment + 1 >= len(source):
            tx, ty = source[-1][1], source[-1][2]
        else:
            left_t, left_x, left_y = source[segment]
            right_t, right_x, right_y = source[segment + 1]
            span = right_t - left_t
            u = (t - left_t) / span if span > 0 else 0.0
            tx = left_x * (1.0 - u) + right_x * u
            ty = left_y * (1.0 - u) + right_y * u
        return _with_unlocked_shape_tangents(
            _duplicate_terminal_square_sample_xy(tx, ty)
        )

    sample_count = 121
    fixture = _duplicate_terminal_fixture(
        [
            {
                "t_sec": i / float(sample_count - 1),
                "v": value_at(i / float(sample_count - 1)),
            }
            for i in range(sample_count)
        ]
    )
    fixture["request_id"] = "motion-smooth-sampled-source-fidelity-policy"
    fixture["config"]["solve_optimization_mode"] = "motion_smooth"
    fixture["config"]["motion_smooth_use_ease"] = False
    fixture["config"]["motion_smooth_tolerance"] = 3.0
    fixture["config"]["motion_smooth_source_fidelity"] = True
    fixture["properties"][0]["property"]["source_key_times"] = [
        t for t, _tx, _ty in source
    ]

    _stdout, bundle = _run_solve_fixture(fixture)
    result = bundle["property_results"][0]
    notes = result["notes"]
    assert "solve_mode_motion_smooth" in notes
    assert "source_key_count_raw=5" in notes
    assert "source_key_count=5" in notes
    assert "redundant_source_keys_removed=0" in notes
    assert "motion_smooth_source_fidelity=true" in notes
    assert "source_fidelity_samples=5" in notes
    assert "source_fidelity_mode=source_key_pose_constraints" in notes
    assert "source_pose_constraints=5" in notes
    assert "source_pose_constraint_keys=5" in notes
    assert "shape_tangent_lock_skipped_source_pose_constraints=5" in notes
    assert "key_schedule=source_key_times_spline" in notes
    assert "rove_applied=false" in notes
    assert "source_pose_interval_rove=true" in notes
    assert _note_float(notes, "source_pose_interval_rove_max_time_shift_sec") > 0.0
    assert "source_error_not_constrained=false" in notes
    assert "motion_smooth_closed_loop=true" in notes
    for t_sec, tx, ty in source:
        assert _vectors_close(
            _key_at_time(result["keys"], t_sec)["v"],
            value_at(t_sec),
        )
    assert result["keys"][-1]["v"] == result["keys"][0]["v"]


def _position_motion_smooth_fixture(samples, request_id):
    fixture = _duplicate_terminal_fixture(samples)
    fixture["request_id"] = request_id
    prop = fixture["properties"][0]["property"]
    prop["id"] = "layer/Position"
    prop["match_name"] = "ADBE Position"
    prop["display_name"] = "Position"
    prop["kind"] = "TwoD_Spatial"
    prop["dimensions"] = 2
    prop["is_spatial"] = True
    prop["units_label"] = ""
    fixture["config"]["solve_optimization_mode"] = "motion_smooth"
    fixture["config"]["motion_smooth_use_ease"] = True
    fixture["config"]["allow_path_replacement_fit"] = True
    fixture["config"]["path_replacement_prefer_vertices"] = True
    return fixture


def _vec_is_nonzero(values, eps=1e-9):
    return isinstance(values, list) and any(abs(float(x)) > eps for x in values)


def _vectors_close(left, right, eps=1e-9):
    return (
        isinstance(left, list)
        and isinstance(right, list)
        and len(left) == len(right)
        and all(abs(float(a) - float(b)) <= eps for a, b in zip(left, right))
    )


def _key_at_time(keys, t_sec, eps=1e-9):
    for key in keys:
        if abs(float(key["t_sec"]) - float(t_sec)) <= eps:
            return key
    raise AssertionError(f"missing key at {t_sec}")


def _note_float(notes: str, name: str) -> float:
    prefix = name + "="
    for part in notes.split(";"):
        stripped = part.strip()
        if stripped.startswith(prefix):
            return float(stripped[len(prefix):])
    raise AssertionError(f"missing note field {name!r} in {notes}")


def _note_int(notes: str, name: str) -> int:
    prefix = name + "="
    for part in notes.split(";"):
        stripped = part.strip()
        if stripped.startswith(prefix):
            return int(stripped[len(prefix):])
    raise AssertionError(f"missing note field {name!r} in {notes}")


def test_motion_smooth_position_trajectory_smoothing_when_built():
    """Motion Smooth on a unified spatial Position bake must produce a
    trajectory-smoothing result, not the old endpoint-only blind mode and
    not the strict source-error two-key spatial fit. Endpoints stay pinned;
    interior keys (if any) carry explicit spatial tangents.
    """
    if not BBSOLVER.exists():
        print(f"[SKIP] bbsolver not built at {BBSOLVER}")
        return

    start = [10.0, 20.0]
    middle = [25.0, 100.0]
    end = [90.0, 35.0]
    fixture = _position_motion_smooth_fixture(
        [
            {"t_sec": 0.0, "v": start},
            {"t_sec": 0.5, "v": middle},
            {"t_sec": 1.0, "v": end},
        ],
        "motion-smooth-position-trajectory-policy",
    )
    fixture["properties"][0]["property"]["source_key_times"] = [0.0, 0.5, 1.0]
    fixture["config"]["motion_smooth_bezier_x1"] = 0.25
    fixture["config"]["motion_smooth_bezier_y1"] = 0.1
    fixture["config"]["motion_smooth_bezier_x2"] = 0.75
    fixture["config"]["motion_smooth_bezier_y2"] = 0.9

    stdout, bundle = _run_solve_fixture(fixture)
    events = [
        json.loads(raw)
        for raw in stdout.splitlines()
        if raw.strip().startswith("{")
    ]
    assert not any(
        event.get("event") == "path_replacement_fit_start"
        for event in events
    ), stdout
    result = bundle["property_results"][0]
    notes = result["notes"]
    # New trajectory-smoothing surface markers.
    assert "solve_mode_motion_smooth" in notes
    assert "motion_smooth_spatial_trajectory_filter=true" in notes
    assert "motion_smooth_source_key_times=true" in notes
    assert "key_schedule=source_keys" in notes
    assert "source_key_count=3" in notes
    assert "source_error_not_constrained=true" in notes
    assert "smoothing_strength=3.000000" in notes
    assert "smoothing_passes=" in notes
    assert "max_smoothing_displacement=" in notes
    assert "motion_smooth_bezier=0.250000,0.100000,0.750000,0.900000" in notes
    # The two old surfaces this fix replaces — strict source-error fit and
    # source-error-not-evaluated endpoint-only blind mode — must NOT appear.
    assert "motion_smooth_spatial_path_fit=true" not in notes
    assert "source_error_evaluated=true" not in notes
    assert "source_error_not_evaluated=true" not in notes
    # Range endpoints stay pinned.
    assert len(result["keys"]) == 3
    assert result["keys"][0]["t_sec"] == 0.0
    assert result["keys"][1]["t_sec"] == 0.5
    assert result["keys"][-1]["t_sec"] == 1.0
    assert result["keys"][0]["v"] == start
    assert result["keys"][-1]["v"] == end
    assert result["keys"][0]["temporal_ease_out"][0]["influence"] == 25.0
    assert result["keys"][1]["temporal_ease_in"][0]["influence"] == 25.0
    # All keys are explicit-tangent (not auto-bezier) and carry at least one
    # non-zero spatial tangent (in or out) — not the empty/zero tangent
    # array that the old endpoint-only mode produced.
    for i, key in enumerate(result["keys"]):
        assert key["spatial_continuous"] is True, (i, key)
        assert key["spatial_auto_bezier"] is False, (i, key)
        spatial_out = key.get("spatial_out", [])
        spatial_in = key.get("spatial_in", [])
        assert len(spatial_out) == 2 and len(spatial_in) == 2, (i, key)
        if i == 0:
            assert _vec_is_nonzero(spatial_out), (i, key)
        elif i == len(result["keys"]) - 1:
            assert _vec_is_nonzero(spatial_in), (i, key)
        else:
            # Interior keys are roving control points with both tangents set.
            assert key.get("roving") is True, (i, key)
            assert _vec_is_nonzero(spatial_out), (i, key)
            assert _vec_is_nonzero(spatial_in), (i, key)


def test_motion_smooth_position_smooths_jittery_arc_when_built():
    """A jittery arc whose deviation exceeds tolerance must still be
    smoothed (output keys deviate from source samples by more than the
    user's tolerance) instead of being honestly fit with many strict-error
    keys. This locks the trajectory-smoothing contract: smoothing > fitting.
    """
    if not BBSOLVER.exists():
        print(f"[SKIP] bbsolver not built at {BBSOLVER}")
        return

    import random
    random.seed(1)
    samples = []
    for i in range(31):
        t = i / 30.0
        base_x = 50.0 * math.cos(math.pi * t)
        base_y = 50.0 * math.sin(math.pi * t)
        samples.append({
            "t_sec": t,
            "v": [base_x + random.uniform(-3.0, 3.0),
                  base_y + random.uniform(-3.0, 3.0)],
        })
    fixture = _position_motion_smooth_fixture(
        samples, "motion-smooth-position-jitter-policy"
    )
    fixture["properties"][0]["property"]["source_key_times"] = [
        samples[i]["t_sec"] for i in (0, 5, 10, 15, 20, 25, 30)
    ]
    # Tight tolerance: strict fitting would emit many keys to honor noise.
    # The trajectory smoother must NOT respect this gate.
    fixture["config"]["tolerance"] = 0.5
    fixture["config"]["tolerance_screen_px"] = 0.0
    fixture["config"]["motion_smooth_tolerance"] = 5.0
    sample_count = len(samples)
    start_v = list(samples[0]["v"])
    end_v = list(samples[-1]["v"])

    _stdout, bundle = _run_solve_fixture(fixture)
    result = bundle["property_results"][0]
    notes = result["notes"]
    assert "motion_smooth_spatial_trajectory_filter=true" in notes
    assert "source_error_not_constrained=true" in notes
    assert "smoothing_strength=5.000000" in notes
    assert f"input_samples={sample_count}" in notes
    # Endpoints pinned to first/last raw sample, even though those samples
    # are themselves jittered (smoothing pass preserves boundaries).
    assert result["keys"][0]["v"] == start_v
    assert result["keys"][-1]["v"] == end_v
    # The smoother preserves the source key schedule instead of creating a
    # new sampled-frame key set.
    assert len(result["keys"]) == 7, result["keys"]
    assert "motion_smooth_source_key_times=true" in notes
    assert "key_schedule=source_keys" in notes
    # Max smoothing displacement must exceed the user's tolerance — that
    # is precisely what proves this is trajectory smoothing rather than
    # strict source-error fitting.
    max_err = float(result["max_err"])
    tolerance = float(fixture["config"]["tolerance"])
    assert max_err > tolerance, (
        f"trajectory smoother max_err={max_err} did not exceed "
        f"tolerance={tolerance}; smoothing contract is to give up "
        f"strict source-error in exchange for a smoother path"
    )
    # Interior keys must be roving with both tangents populated; endpoint
    # keys carry the appropriate one-sided tangent.
    interior = result["keys"][1:-1]
    assert interior, "smoother must keep at least one interior control"
    for i, key in enumerate(interior, start=1):
        assert key["spatial_auto_bezier"] is False, (i, key)
        assert key.get("roving") is True, (i, key)
        assert _vec_is_nonzero(key.get("spatial_in", [])), (i, key)
        assert _vec_is_nonzero(key.get("spatial_out", [])), (i, key)
    assert _vec_is_nonzero(result["keys"][0].get("spatial_out", []))
    assert _vec_is_nonzero(result["keys"][-1].get("spatial_in", []))


def main():
    tests = [
        test_progress_source_contract,
        test_bbsolver_progress_smoke_when_built,
        test_bbsolver_progress_monotone_and_ordered_when_built,
        test_duplicate_terminal_closure_vertex_reduction_when_built,
        test_bridge_prune_runs_after_duplicate_terminal_prepass_when_enabled,
        test_bridge_prune_accepts_collapsed_shape_frames_when_enabled,
        test_sharp_corner_preservation_blocks_high_tolerance_corner_deletion_when_built,
        test_duplicate_terminal_reduction_skips_mixed_key_topology_when_built,
        test_second_pass_bridge_prune_mixed_key_topology_when_enabled,
        test_parallel_jobs_preserve_bridge_prune_output_when_built,
        test_temporal_only_mode_disables_vertex_reduction_when_built,
        test_shape_flat_near_optimal_fast_path_preserves_source_keys_when_built,
        test_vertex_only_mode_skips_temporal_and_runs_vertex_reduction_when_built,
        test_motion_smooth_mode_smooths_shape_paths_without_endpoint_deletion_when_built,
        test_motion_smooth_shape_paths_default_to_linear_roved_timing_when_built,
        test_motion_smooth_shape_path_reduces_key_trajectory_turn_when_built,
        test_motion_smooth_shape_path_ignores_redundant_source_keys_when_built,
        test_motion_smooth_source_fidelity_preserves_source_key_poses_when_built,
        test_motion_smooth_position_trajectory_smoothing_when_built,
        test_motion_smooth_position_smooths_jittery_arc_when_built,
    ]
    for test in tests:
        test()
        print(f"[PASS] {test.__name__}")
    print(f"summary: {len(tests)} passed, 0 failed")


if __name__ == "__main__":
    main()
