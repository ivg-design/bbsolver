#!/usr/bin/env python3
"""Solver folder-layout policy.

The target layout is: stable/public headers live under
`solver/include/bbsolver/<area>/` and implementation files live under
`solver/src/<area>/`. This policy keeps the build and clangd-ready layout
from regressing now that all C++ files have moved into area folders.
"""

from __future__ import annotations

import json
from pathlib import Path

from _solver_policy_paths import find_solver_layout


ROOT, SOLVER = find_solver_layout(__file__)
SOLVER = SOLVER
CMAKE = SOLVER / "CMakeLists.txt"
CMAKE_PRESETS = SOLVER / "CMakePresets.json"
SOLVER_CI_WORKFLOW = SOLVER / ".github" / "workflows" / "ci.yml"
INCLUDE_ROOT = SOLVER / "include"
PUBLIC_ROOT = INCLUDE_ROOT / "bbsolver"
SRC_ROOT = SOLVER / "src"
PROTOCOL_ROOT = SOLVER / "protocol"
PACKAGE_CONFIG_TEMPLATE = SOLVER / "cmake" / "bbsolverConfig.cmake.in"
PACKAGE_SMOKE_ROOT = SOLVER / "tests" / "package_smoke"
AE_EXAMPLES_ROOT = SOLVER / "examples" / "after-effects"
SOLVER_FIXTURE_ROOT = SOLVER / "tests" / "fixtures"
SOLVER_TEST_ROOT = SOLVER / "tests" / "solver_unit"
LEGACY_SOLVER_TEST_ROOT = ROOT / "tests" / "solver_unit"
THIRD_PARTY_ARCHIVE_ROOT = SOLVER / "third_party" / "archive"
ROOT_VSCODE_SETTINGS = ROOT / ".vscode" / "settings.json"
SOLVER_VSCODE_SETTINGS = SOLVER / ".vscode" / "settings.json"
SOLVER_GITIGNORE = SOLVER / ".gitignore"


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def _read_json(path: Path) -> dict:
    return json.loads(_read(path))


def _source_between(text: str, start_marker: str, end_marker: str) -> str:
    start = text.find(start_marker)
    assert start >= 0, f"missing start marker: {start_marker!r}"
    end = text.find(end_marker, start + len(start_marker))
    assert end > start, f"missing end marker {end_marker!r}"
    return text[start:end]


def test_cmake_recurses_implementation_sources() -> None:
    text = _read(CMAKE)
    assert "file(GLOB_RECURSE BBSOLVER_CORE_SOURCES CONFIGURE_DEPENDS" in text, (
        "bbsolver_core must use GLOB_RECURSE so solver/src/<area>/*.cpp "
        "files are compiled during the folder migration"
    )
    assert '"${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"' in text, (
        "recursive source glob must continue to cover solver/src/*.cpp and "
        "solver/src/<area>/*.cpp"
    )
    assert 'list(FILTER BBSOLVER_CORE_SOURCES EXCLUDE REGEX "/main\\\\.cpp$")' in text, (
        "main.cpp must stay excluded from bbsolver_core after recursive globbing"
    )


def test_cmake_exports_public_include_root() -> None:
    text = _read(CMAKE)
    include_marker = "${CMAKE_CURRENT_LIST_DIR}/include"
    src_marker = '"${CMAKE_CURRENT_LIST_DIR}/src"'
    generated_marker = "${BBSOLVER_GENERATED_DIR}"
    assert include_marker in text, (
        "bbsolver_core must expose solver/include for public headers"
    )
    assert generated_marker in text, (
        "bbsolver_core must expose generated protocol headers"
    )
    include_block = _source_between(
        text,
        "target_include_directories(bbsolver_core",
        "foreach(BBSOLVER_DEPENDENCY_TARGET",
    )
    assert "PUBLIC" in include_block and "PRIVATE" in include_block, (
        "bbsolver_core must separate public include roots from private "
        "implementation include roots"
    )
    public_block = include_block.split("PRIVATE", 1)[0]
    private_block = include_block.split("PRIVATE", 1)[1]
    assert include_marker in public_block, (
        "solver/include must be a PUBLIC include root"
    )
    assert generated_marker in public_block, (
        "generated protocol headers must stay a PUBLIC include root"
    )
    assert src_marker not in public_block, (
        "solver/src must not be a PUBLIC include root after the standalone "
        "header layout is complete"
    )
    assert src_marker in private_block, (
        "solver/src must remain a PRIVATE include root for implementation "
        "translation units"
    )


def test_cmake_uses_solver_local_protocol_schemas() -> None:
    text = _read(CMAKE)
    assert (PROTOCOL_ROOT / "samples.fbs").is_file(), (
        "SampleBundle schema must be owned by solver/protocol/"
    )
    assert (PROTOCOL_ROOT / "keys.fbs").is_file(), (
        "KeyBundle schema must be owned by solver/protocol/"
    )
    assert 'set(BBSOLVER_PROTOCOL_DIR "${CMAKE_CURRENT_LIST_DIR}/protocol")' in text, (
        "solver CMake must generate protocol headers from solver/protocol/"
    )
    legacy_root_var = "BAKER" "BOY_ROOT"
    legacy_protocol_var = "BAKER" "BOY_PROTOCOL_DIR"
    assert f'set({legacy_root_var} "${{CMAKE_CURRENT_LIST_DIR}}/..")' not in text, (
        "standalone solver CMake must not depend on the monorepo root"
    )
    assert f'set({legacy_protocol_var} "${{{legacy_root_var}}}/protocol")' not in text, (
        "standalone solver CMake must not generate from root protocol/"
    )
    assert '../protocol' not in text, (
        "standalone solver CMake must not reference root-relative protocol paths"
    )
    assert '"${BBSOLVER_PROTOCOL_DIR}/samples.fbs"' in text, (
        "SampleBundle flatc input must use solver-local protocol directory"
    )
    assert '"${BBSOLVER_PROTOCOL_DIR}/keys.fbs"' in text, (
        "KeyBundle flatc input must use solver-local protocol directory"
    )


def test_flatbuffers_schemas_are_not_linted_as_cpp() -> None:
    for settings_path in (ROOT_VSCODE_SETTINGS, SOLVER_VSCODE_SETTINGS):
        assert settings_path.is_file(), (
            f"{settings_path.relative_to(ROOT)} must exist for editor lint setup"
        )
        text = _read(settings_path)
        assert '"*.fbs": "cpp"' not in text, (
            f"{settings_path.relative_to(ROOT)} must not map FlatBuffers schemas "
            "to C++; clangd reports false C++ diagnostics for.fbs files"
        )
        assert '"*.fbs": "flatbuffers"' in text, (
            f"{settings_path.relative_to(ROOT)} must associate.fbs files with "
            "the FlatBuffers language id"
        )


def test_solver_package_carries_source_control_hygiene_files() -> None:
    assert SOLVER_GITIGNORE.is_file(), (
        "solver package must carry its own.gitignore before monorepo extraction"
    )
    text = _read(SOLVER_GITIGNORE)
    for pattern in ("/build/", "/build-*/", ".DS_Store", "__pycache__/"):
        assert pattern in text, f"solver/.gitignore must ignore {pattern}"
    assert "!.vscode/settings.json" in text, (
        "solver/.gitignore must keep the solver-local editor config tracked"
    )


def test_solver_package_carries_standalone_ci() -> None:
    assert SOLVER_CI_WORKFLOW.is_file(), (
        "solver package must carry standalone CI under.github/workflows"
    )
    workflow = _read(SOLVER_CI_WORKFLOW)
    for token in (
        "ubuntu-latest",
        "macos-14",
        "windows-latest",
        "ctest --test-dir build",
        "-LE slow",
        "validate_standalone_package.py",
        "tests/package_smoke",
    ):
        assert token in workflow, (
            f"standalone CI workflow must include {token!r}"
        )


def test_cmake_presets_lock_fast_validation_workflows() -> None:
    assert CMAKE_PRESETS.is_file(), (
        "solver/CMakePresets.json must exist so developer validation loops "
        "are repeatable from the standalone solver root"
    )
    presets = _read_json(CMAKE_PRESETS)
    expected = {
        "dev": "${sourceDir}/out/dev",
        "focused-test": "${sourceDir}/out/focused-test",
        "package-smoke": "${sourceDir}/out/package-smoke",
        "release-validation": "${sourceDir}/out/release-validation",
    }

    configure_presets = {
        preset["name"]: preset
        for preset in presets.get("configurePresets", [])
    }
    build_presets = {
        preset["name"]: preset
        for preset in presets.get("buildPresets", [])
    }
    test_presets = {
        preset["name"]: preset
        for preset in presets.get("testPresets", [])
    }
    assert set(configure_presets) >= set(expected), (
        "configure presets must include the standard validation loops"
    )
    assert set(build_presets) >= set(expected), (
        "build presets must include the standard validation loops"
    )
    assert set(test_presets) >= set(expected), (
        "test presets must include the standard validation loops"
    )

    for name, binary_dir in expected.items():
        configure = configure_presets[name]
        assert configure.get("binaryDir") == binary_dir, (
            f"{name} preset must build under solver-local out/; got "
            f"{configure.get('binaryDir')!r}"
        )
        assert configure.get("generator") == "Ninja", (
            f"{name} preset must use Ninja for fast incremental validation"
        )
        cache = configure.get("cacheVariables", {})
        assert cache.get("BBSOLVER_BUILD_TESTS") == "ON", (
            f"{name} preset must enable solver tests"
        )
        assert cache.get("CMAKE_EXPORT_COMPILE_COMMANDS") == "ON", (
            f"{name} preset must emit compile_commands.json for clangd"
        )
        assert build_presets[name].get("configurePreset") == name, (
            f"{name} build preset must target its matching configure preset"
        )
        assert test_presets[name].get("configurePreset") == name, (
            f"{name} test preset must target its matching configure preset"
        )

    package_targets = build_presets["package-smoke"].get("targets", [])
    assert {"bbsolver", "test_package_smoke_source"} <= set(package_targets), (
        "package-smoke build preset must build the CLI and in-tree smoke test"
    )
    package_filter = (
        test_presets["package-smoke"]
.get("filter", {})
.get("include", {})
.get("name")
    )
    assert package_filter == "^test_package_smoke_source$", (
        "package-smoke test preset must run only the package smoke source test"
    )
    release_cache = configure_presets["release-validation"].get("cacheVariables", {})
    assert release_cache.get("BBSOLVER_FORCE_THIRD_PARTY_ARCHIVES") == "ON", (
        "release-validation preset must force shipped archive dependency resolution"
    )
    gitignore_text = _read(SOLVER_GITIGNORE)
    assert "/out/" in gitignore_text, (
        "solver/.gitignore must ignore preset build directories under solver/out/"
    )


def test_cmake_installs_and_exports_standalone_package() -> None:
    text = _read(CMAKE)
    assert PACKAGE_CONFIG_TEMPLATE.is_file(), (
        "standalone package config template must live under solver/cmake/"
    )
    assert "include(GNUInstallDirs)" in text, (
        "install destinations must use GNUInstallDirs"
    )
    assert "include(CMakePackageConfigHelpers)" in text, (
        "package config/version files must use CMakePackageConfigHelpers"
    )
    assert "configure_package_config_file(" in text, (
        "bbsolver must generate a relocatable package config"
    )
    assert "write_basic_package_version_file(" in text, (
        "bbsolver must install a package version file"
    )
    assert "install(\n TARGETS bbsolver_core" in text, (
        "bbsolver library target must be installable"
    )
    assert "install(\n TARGETS bbsolver" in text, (
        "bbsolver CLI target must be installable when enabled"
    )
    assert "EXPORT bbsolverTargets" in text, (
        "install rules must export bbsolver targets"
    )
    assert "NAMESPACE bbsolver::" in text, (
        "installed CMake targets must use the bbsolver:: namespace"
    )
    assert 'install(\n DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/include/"' in text, (
        "public headers must be installed"
    )
    assert 'install(\n DIRECTORY "${BBSOLVER_GENERATED_NS_DIR}/"' in text, (
        "only bbsolver generated protocol headers must be installed"
    )
    assert 'DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/bbsolver"' in text, (
        "generated protocol headers must install under include/bbsolver"
    )
    assert 'install(\n DIRECTORY "${BBSOLVER_GENERATED_DIR}/"' not in text, (
        "install rules must not copy the whole generated tree; stale namespaces "
        "from dirty builds must not leak into packages"
    )
    assert 'DESTINATION "${CMAKE_INSTALL_DATADIR}/bbsolver/protocol"' in text, (
        "source FlatBuffers schemas must be installed as package data"
    )
    assert 'DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/examples/"' in text, (
        "standalone examples must be installed with the package"
    )
    assert 'DESTINATION "${CMAKE_INSTALL_DATADIR}/bbsolver/examples"' in text, (
        "examples must install under share/bbsolver/examples"
    )
    assert 'PATTERN ".DS_Store" EXCLUDE' in text, (
        "install rules must exclude Finder metadata from package archives"
    )
    assert '"${CMAKE_CURRENT_LIST_DIR}/LICENSE"' in text, (
        "solver license must be installed with package documentation"
    )


def test_solver_tree_contains_no_finder_metadata() -> None:
    leaked = sorted(
        path.relative_to(ROOT).as_posix()
        for path in SOLVER.rglob(".DS_Store")
        if path.is_file()
    )
    assert not leaked, (
        "solver package tree must not contain Finder metadata files: "
        + ", ".join(leaked)
    )


def test_solver_ctest_exports_source_dir_for_fixtures() -> None:
    text = _read(CMAKE)
    assert "BBSOLVER_TEST_SOURCE_DIR=${CMAKE_CURRENT_LIST_DIR}" in text, (
        "CTest must provide the solver source root so fixture-backed tests "
        "remain relocatable in copied/out-of-tree package validation builds"
    )


def test_cmake_dependency_lookup_ignores_user_package_registry() -> None:
    cmake_text = _read(CMAKE)
    config_text = _read(PACKAGE_CONFIG_TEMPLATE)
    assert cmake_text.count("NO_CMAKE_PACKAGE_REGISTRY") >= 5, (
        "source builds must not resolve dependencies from stale CMake user "
        "package-registry entries"
    )
    assert config_text.count("NO_CMAKE_PACKAGE_REGISTRY") >= 5, (
        "installed package configs must not resolve dependencies from stale "
        "CMake user package-registry entries"
    )


def test_after_effects_harness_support_folder_is_packaged_and_used() -> None:
    harness = AE_EXAMPLES_ROOT / "bbsolver-test-harness.jsx"
    support = AE_EXAMPLES_ROOT / "bbsolver-test-harness"
    samples_schema = PROTOCOL_ROOT / "samples.fbs"
    assert harness.is_file(), "AE ScriptUI harness must be packaged"
    assert support.is_dir(), "AE harness support folder must be packaged"
    harness_text = _read(harness)
    samples_text = _read(samples_schema)
    assert '//@include "bbsolver-test-harness/serialize_json.jsx"' in harness_text, (
        "AE harness must include the packaged SampleBundle writer"
    )
    assert '//@include "bbsolver-test-harness/parse_keys.jsx"' in harness_text, (
        "AE harness must include the packaged KeyBundle reader"
    )
    assert (support / "serialize_json.jsx").is_file(), (
        "AE harness support folder must include SampleBundle writer"
    )
    assert (support / "parse_keys.jsx").is_file(), (
        "AE harness support folder must include KeyBundle reader"
    )
    assert "writeSampleBundleJson(bundle," in harness_text, (
        "AE harness must write SampleBundles through the packaged writer"
    )
    assert "readKeyBundleJson(" in harness_text, (
        "AE harness must read KeyBundles through the packaged reader"
    )
    assert "ae/jsx/serialize_json.jsx" not in samples_text, (
        "standalone schema comments must not point to non-packaged AE paths"
    )
    assert "examples/after-effects/bbsolver-test-harness/" in samples_text, (
        "SampleBundle schema must point to the packaged AE support folder"
    )


def test_package_config_template_is_dependency_aware() -> None:
    text = _read(PACKAGE_CONFIG_TEMPLATE)
    assert "@PACKAGE_INIT@" in text, (
        "package config must be generated with configure_package_config_file"
    )
    assert "BBSOLVER_USE_BUNDLED_DEPS" in text, (
        "package config must allow installed archive dependency fallback"
    )
    assert "BBSOLVER_FORCE_BUNDLED_DEPS" in text, (
        "package config must allow consumers/tests to skip package-registry deps"
    )
    assert "_BBSOLVER_THIRD_PARTY_ARCHIVE_DIR" in text, (
        "package config must resolve dependency archives from the install prefix"
    )
    for dependency in (
        "nlohmann-json-3.11.3.tar.xz",
        "eigen-3.4.0.tar.gz",
        "ceres-solver-2.2.0.tar.gz",
        "onetbb-2021.13.0.tar.gz",
        "flatbuffers-24.3.25.tar.gz",
    ):
        assert dependency in text, (
            f"package config must know installed archive: {dependency}"
        )
    for target in (
        "nlohmann_json::nlohmann_json",
        "Eigen3::Eigen",
        "Ceres::ceres",
        "TBB::tbb",
        "flatbuffers::flatbuffers",
    ):
        assert target in text, (
            f"package config must expose/link public dependency target: {target}"
        )
    assert 'include("${CMAKE_CURRENT_LIST_DIR}/bbsolverTargets.cmake")' in text, (
        "package config must load the exported bbsolver targets"
    )
    assert "target_link_libraries(bbsolver::core" in text, (
        "package config must attach public dependency targets to bbsolver::core"
    )
    assert "check_required_components(bbsolver)" in text, (
        "package config must check requested components"
    )


def test_package_smoke_project_locks_exported_targets() -> None:
    top_cmake_text = _read(CMAKE)
    cmake_text = _read(PACKAGE_SMOKE_ROOT / "CMakeLists.txt")
    smoke_cpp = PACKAGE_SMOKE_ROOT / "main.cpp"
    assert smoke_cpp.is_file(), (
        "package smoke project must include a tiny optional core-link source"
    )
    assert "test_package_smoke_source" in top_cmake_text, (
        "top-level solver build must compile package smoke source so clangd "
        "does not infer a command from an unrelated main.cpp"
    )
    assert "tests/package_smoke/main.cpp" in top_cmake_text, (
        "package smoke source must be present in solver/build/compile_commands.json"
    )
    assert "find_package(bbsolver CONFIG REQUIRED)" in cmake_text, (
        "package smoke project must consume the installed package config"
    )
    assert "TARGET bbsolver::bbsolver" in cmake_text, (
        "package smoke project must verify the CLI imported target"
    )
    assert "TARGET bbsolver::core" in cmake_text, (
        "package smoke project must verify the core imported target"
    )
    assert "BBSOLVER_PACKAGE_SMOKE_LINK_CORE" in cmake_text, (
        "package smoke project must keep core-link validation configurable"
    )
    assert (
        'BBSOLVER_PACKAGE_SMOKE_LINK_CORE\n'
        '  "Build and link a small executable against bbsolver::core."\n'
        '  ON)' in cmake_text
    ), (
        "package smoke must link bbsolver::core by default"
    )


def test_solver_unit_tests_live_under_solver_tree() -> None:
    text = _read(CMAKE)
    assert SOLVER_TEST_ROOT.is_dir(), (
        "solver C++ unit tests must live under solver/tests/solver_unit/"
    )
    if ROOT != SOLVER:
        assert not LEGACY_SOLVER_TEST_ROOT.exists(), (
            "root tests/solver_unit/ must not return; compiled solver tests "
            "belong under solver/tests/solver_unit/"
        )
    assert '"${CMAKE_CURRENT_LIST_DIR}/tests/solver_unit/*.cpp"' in text, (
        "CMake must discover solver unit tests from solver/tests/solver_unit/"
    )
    legacy_root_var = "BAKER" "BOY_ROOT"
    assert f'"${{{legacy_root_var}}}/tests/solver_unit/*.cpp"' not in text, (
        "CMake must not discover compiled solver tests from root tests/"
    )
    assert 'WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"' in text, (
        "CTest unit tests must run from the solver source root so out-of-tree "
        "builds can find solver-local fixtures"
    )


def test_solver_fixtures_are_solver_local() -> None:
    replacement_test = SOLVER_TEST_ROOT / "test_replacement_temporal_solver.cpp"
    diagnostics_policy = SOLVER / "tests" / "policies" / "solver_diagnostics_policy.py"
    progress_policy = SOLVER / "tests" / "policies" / "solver_progress_policy.py"
    assert (SOLVER_FIXTURE_ROOT / "path_keydumps").is_dir(), (
        "fixture-backed solver tests must own fixtures under solver/tests/fixtures"
    )
    assert (SOLVER_FIXTURE_ROOT / "color_pulse.bbsm.json").is_file(), (
        "diagnostics/progress policy smokes must own color_pulse under "
        "solver/tests/fixtures for standalone-root validation"
    )
    text = _read(replacement_test)
    assert "tests/fixtures/path_keydumps" in text, (
        "replacement temporal tests must use solver-local fixtures"
    )
    for policy in (diagnostics_policy, progress_policy):
        policy_text = _read(policy)
        assert 'SOLVER / "tests" / "fixtures" / "color_pulse.bbsm.json"' in policy_text, (
            f"{policy.name} must use the solver-local color_pulse fixture"
        )
    # Guard: tests must not refer to root-level fixture paths from any
    # historical sibling project (we build the forbidden strings in pieces
    # so the literals never appear verbatim in source).
    _forbidden_legacy_dir = "tests/" + "b" + "akerboy_fixtures"
    assert _forbidden_legacy_dir not in text, (
        "standalone solver tests must not depend on root-level fixtures"
    )
    _forbidden_legacy_keydump = "b" + "akerBoy" + "_selected_path_keydump"
    assert _forbidden_legacy_keydump not in text, (
        "standalone solver tests must not reference product-named fixture files"
    )


def test_third_party_backup_archives_are_hash_locked() -> None:
    text = _read(CMAKE)
    expected_archives = {
        "nlohmann-json-3.11.3.tar.xz":
            "d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d",
        "eigen-3.4.0.tar.gz":
            "8586084f71f9bde545ee7fa6d00288b264a2b7ac3607b974e54d13e7162c1c72",
        "ceres-solver-2.2.0.tar.gz":
            "12efacfadbfdc1bbfa203c236e96f4d3c210bed96994288b3ff0c8e7c6f350d4",
        "onetbb-2021.13.0.tar.gz":
            "3ad5dd08954b39d113dc5b3f8a8dc6dc1fd5250032b7c491eb07aed5c94133e1",
        "flatbuffers-24.3.25.tar.gz":
            "4157c5cacdb59737c5d627e47ac26b140e9ee28b1102f812b36068aab728c1ed",
    }
    assert "BBSOLVER_THIRD_PARTY_ARCHIVE_FALLBACK" in text, (
        "CMake must expose the fallback flag for third-party backup archives"
    )
    assert "BBSOLVER_FORCE_THIRD_PARTY_ARCHIVES" in text, (
        "CMake must expose a force-local flag for offline fallback validation"
    )
    assert "BBSOLVER_REMOTE_PROBE_TIMEOUT_SECONDS" in text, (
        "CMake must bound remote archive availability probes"
    )
    assert "FetchContent_Populate(\n eigen" in text, (
        "fetched Eigen must be populate-only so it cannot register third-party "
        "tests or install rules in the solver package"
    )
    assert 'bbsolver_fetch_header_only_target(Eigen3::Eigen "${eigen_SOURCE_DIR}")' in text, (
        "fetched Eigen must be exposed as a header-only imported target"
    )
    assert "FetchContent_Populate(\n ceres_solver" in text, (
        "fetched Ceres must be populate-only so bbsolver controls its install surface"
    )
    assert (
        'add_subdirectory(\n    "${ceres_solver_SOURCE_DIR}"\n'
        '    "${ceres_solver_BINARY_DIR}"\n EXCLUDE_FROM_ALL)'
    ) in text, (
        "fetched Ceres must be excluded from bbsolver package install rules"
    )
    assert "set(TBB_INSTALL OFF CACHE BOOL \"\" FORCE)" in text, (
        "fetched oneTBB install rules must be disabled"
    )
    assert "FetchContent_Populate(\n onetbb" in text, (
        "fetched oneTBB must be populate-only so bbsolver controls its install surface"
    )
    assert (
        'add_subdirectory(\n    "${onetbb_SOURCE_DIR}"\n'
        '    "${onetbb_BINARY_DIR}"\n EXCLUDE_FROM_ALL)'
    ) in text, (
        "fetched oneTBB must be excluded from bbsolver package install rules"
    )
    assert "--head" in text and "--fail" in text and "remote_probe_result" in text, (
        "CMake must probe remote archive availability before selecting the "
        "backup archive"
    )
    assert 'set(selected_urls "${local_archive}")' in text, (
        "CMake must use the local archive as the sole URL when the remote "
        "probe fails"
    )
    for archive_name, sha256 in expected_archives.items():
        archive = THIRD_PARTY_ARCHIVE_ROOT / archive_name
        assert archive.is_file(), f"missing third-party backup archive: {archive_name}"
        assert archive_name in text, (
            f"CMake must reference third-party backup archive {archive_name}"
        )
        assert f"URL_HASH SHA256={sha256}" in text, (
            f"CMake must hash-lock {archive_name}"
        )


def test_public_headers_live_under_bbsolver_namespace_root() -> None:
    public_headers = sorted(INCLUDE_ROOT.rglob("*.hpp"))
    assert public_headers, (
        "expected at least one public solver header under solver/include"
    )
    findings = []
    for header in public_headers:
        if PUBLIC_ROOT not in header.parents:
            findings.append(header.relative_to(ROOT).as_posix())
    assert not findings, (
        "public solver headers must live under solver/include/bbsolver/. "
        "Findings:\n  " + "\n  ".join(findings)
    )


def test_public_root_only_exposes_domain_header_and_area_dirs() -> None:
    top_level_headers = [
        path.name for path in sorted(PUBLIC_ROOT.iterdir())
        if path.is_file() and path.suffix in {".hpp", ".h"}
    ]
    assert top_level_headers == ["domain.hpp"], (
        "Only the core domain contract may live directly under "
        "solver/include/bbsolver/. Other public headers must live in an area "
        "folder. Findings: " + ", ".join(top_level_headers)
    )

    expected_area_dirs = {
        "app",
        "diagnostics",
        "dp",
        "fit",
        "io",
        "metrics",
        "motion_smooth",
        "path",
        "progress",
        "replacement_temporal",
        "routing",
        "runtime",
        "samples",
        "shape",
        "solve",
        "temporal",
        "verify",
    }
    area_dirs = {
        path.name for path in PUBLIC_ROOT.iterdir()
        if path.is_dir()
    }
    assert area_dirs == expected_area_dirs, (
        "solver/include/bbsolver/ area directories drifted. "
        f"Expected {sorted(expected_area_dirs)}, got {sorted(area_dirs)}"
    )


def test_no_legacy_h_headers_in_solver_layout() -> None:
    findings = [
        path.relative_to(ROOT).as_posix()
        for root in (SRC_ROOT, INCLUDE_ROOT)
        for path in sorted(root.rglob("*.h"))
    ]
    assert not findings, (
        "Solver headers must use.hpp. Findings:\n  "
        + "\n  ".join(findings)
    )


def test_no_root_cxx_files_remain_in_solver_src() -> None:
    findings = [
        path.relative_to(ROOT).as_posix()
        for path in sorted(SRC_ROOT.iterdir())
        if path.is_file() and path.suffix in {".cpp", ".hpp", ".h"}
    ]
    assert not findings, (
        "The package layout must keep C++ source/header files out of the "
        "solver/src root. Use solver/src/<area>/ for implementations and "
        "solver/include/bbsolver/<area>/ for public headers. Findings:\n  "
        + "\n  ".join(findings)
    )


def test_cmake_dp_feature_probe_uses_target_dp_layout() -> None:
    text = _read(CMAKE)
    assert '"${CMAKE_CURRENT_LIST_DIR}/src/dp/dp_placer.cpp"' in text, (
        "BBSOLVER_HAVE_DP_PLACER must probe the migrated DP source path"
    )
    assert '"${CMAKE_CURRENT_LIST_DIR}/src/dp_placer.cpp"' not in text, (
        "legacy flat DP feature probe must not return"
    )


def test_cli_options_uses_target_app_layout() -> None:
    public_header = PUBLIC_ROOT / "app" / "cli_options.hpp"
    implementation = SRC_ROOT / "app" / "cli_options.cpp"
    legacy_header = SRC_ROOT / "cli_options.hpp"
    legacy_implementation = SRC_ROOT / "cli_options.cpp"

    assert public_header.is_file(), (
        "cli_options.hpp must live under solver/include/bbsolver/app/"
    )
    assert implementation.is_file(), (
        "cli_options.cpp must live under solver/src/app/"
    )
    assert not legacy_header.exists(), (
        "legacy solver/src/cli_options.hpp must not return after Slice 75"
    )
    assert not legacy_implementation.exists(), (
        "legacy solver/src/cli_options.cpp must not return after Slice 75"
    )
    assert (
        '#include "bbsolver/app/cli_options.hpp"'
        in _read(implementation)
    ), (
        "cli_options.cpp must include the public CLI header via the target "
        "bbsolver/app path"
    )


def test_main_entrypoint_uses_target_app_layout() -> None:
    text = _read(CMAKE)
    app_main = SRC_ROOT / "app" / "main.cpp"
    legacy_main = SRC_ROOT / "main.cpp"

    assert app_main.is_file(), (
        "bbsolver command-dispatch entry point must live under solver/src/app/"
    )
    assert not legacy_main.exists(), (
        "legacy solver/src/main.cpp must not return after the app layout move"
    )
    assert '"${CMAKE_CURRENT_LIST_DIR}/src/app/main.cpp"' in text, (
        "bbsolver executable must compile the app-layout main.cpp"
    )


def test_routing_modules_use_target_routing_layout() -> None:
    migrated = (
        "property_classification",
        "property_route_solver",
        "property_solver_routing",
        "solve_mode_policy",
    )
    for stem in migrated:
        header = PUBLIC_ROOT / "routing" / f"{stem}.hpp"
        implementation = SRC_ROOT / "routing" / f"{stem}.cpp"
        legacy_header = SRC_ROOT / f"{stem}.hpp"
        legacy_area_header = SRC_ROOT / "routing" / f"{stem}.hpp"
        legacy_implementation = SRC_ROOT / f"{stem}.cpp"

        assert header.is_file(), (
            f"{stem}.hpp must live under solver/include/bbsolver/routing/"
        )
        assert implementation.is_file(), (
            f"{stem}.cpp must live under solver/src/routing/"
        )
        assert not legacy_header.exists(), (
            f"legacy solver/src/{stem}.hpp must not return after the routing "
            "layout migration"
        )
        assert not legacy_area_header.exists(), (
            f"legacy solver/src/routing/{stem}.hpp must not return after the "
            "Rive-style header/source split"
        )
        assert not legacy_implementation.exists(), (
            f"legacy solver/src/{stem}.cpp must not return after the routing "
            "layout migration"
        )
        assert f'#include "bbsolver/routing/{stem}.hpp"' in _read(implementation), (
            f"{stem}.cpp must include its own routing header via the public "
            "bbsolver/routing path"
        )


def test_io_modules_use_target_io_header_layout() -> None:
    migrated = (
        "io_json",
        "key_bundle_io",
        "sample_bundle_io",
        "sample_bundle_validation",
        "sample_json_timing_io",
        "sample_json_value_io",
        "sample_property_io",
        "solver_config_io",
    )
    for stem in migrated:
        header = PUBLIC_ROOT / "io" / f"{stem}.hpp"
        implementation = SRC_ROOT / "io" / f"{stem}.cpp"
        legacy_header = SRC_ROOT / "io" / f"{stem}.hpp"

        assert header.is_file(), (
            f"{stem}.hpp must live under solver/include/bbsolver/io/"
        )
        assert implementation.is_file(), (
            f"{stem}.cpp must live under solver/src/io/"
        )
        assert not legacy_header.exists(), (
            f"legacy solver/src/io/{stem}.hpp must not return after the "
            "Rive-style header/source split"
        )
        assert f'#include "bbsolver/io/{stem}.hpp"' in _read(implementation), (
            f"{stem}.cpp must include its own IO header via the public "
            "bbsolver/io path"
        )


def test_runtime_modules_use_target_runtime_layout() -> None:
    migrated = (
        "runtime_env",
        "solve_parallel_runtime_scope",
    )
    for stem in migrated:
        header = PUBLIC_ROOT / "runtime" / f"{stem}.hpp"
        implementation = SRC_ROOT / "runtime" / f"{stem}.cpp"
        legacy_header = SRC_ROOT / f"{stem}.hpp"
        legacy_area_header = SRC_ROOT / "runtime" / f"{stem}.hpp"
        legacy_implementation = SRC_ROOT / f"{stem}.cpp"

        assert header.is_file(), (
            f"{stem}.hpp must live under solver/include/bbsolver/runtime/"
        )
        assert implementation.is_file(), (
            f"{stem}.cpp must live under solver/src/runtime/"
        )
        assert not legacy_header.exists(), (
            f"legacy solver/src/{stem}.hpp must not return after the runtime "
            "layout migration"
        )
        assert not legacy_area_header.exists(), (
            f"legacy solver/src/runtime/{stem}.hpp must not return after the "
            "Rive-style header/source split"
        )
        assert not legacy_implementation.exists(), (
            f"legacy solver/src/{stem}.cpp must not return after the runtime "
            "layout migration"
        )
        assert f'#include "bbsolver/runtime/{stem}.hpp"' in _read(implementation), (
            f"{stem}.cpp must include its own runtime header via the public "
            "bbsolver/runtime path"
        )


def test_diagnostics_modules_use_target_diagnostics_layout() -> None:
    migrated = (
        "solver_diagnostics",
        "solver_diagnostic_events",
    )
    for stem in migrated:
        header = PUBLIC_ROOT / "diagnostics" / f"{stem}.hpp"
        implementation = SRC_ROOT / "diagnostics" / f"{stem}.cpp"
        legacy_header = SRC_ROOT / f"{stem}.hpp"
        legacy_area_header = SRC_ROOT / "diagnostics" / f"{stem}.hpp"
        legacy_implementation = SRC_ROOT / f"{stem}.cpp"

        assert header.is_file(), (
            f"{stem}.hpp must live under solver/include/bbsolver/diagnostics/"
        )
        assert implementation.is_file(), (
            f"{stem}.cpp must live under solver/src/diagnostics/"
        )
        assert not legacy_header.exists(), (
            f"legacy solver/src/{stem}.hpp must not return after the "
            "diagnostics layout migration"
        )
        assert not legacy_area_header.exists(), (
            f"legacy solver/src/diagnostics/{stem}.hpp must not return after "
            "the Rive-style header/source split"
        )
        assert not legacy_implementation.exists(), (
            f"legacy solver/src/{stem}.cpp must not return after the "
            "diagnostics layout migration"
        )
        assert f'#include "bbsolver/diagnostics/{stem}.hpp"' in _read(implementation), (
            f"{stem}.cpp must include its own diagnostics header via the public "
            "bbsolver/diagnostics path"
        )


def test_progress_modules_use_target_progress_layout() -> None:
    migrated = (
        "progress",
        "solve_cancellation",
    )
    for stem in migrated:
        header = PUBLIC_ROOT / "progress" / f"{stem}.hpp"
        implementation = SRC_ROOT / "progress" / f"{stem}.cpp"
        legacy_header = SRC_ROOT / f"{stem}.hpp"
        legacy_area_header = SRC_ROOT / "progress" / f"{stem}.hpp"
        legacy_implementation = SRC_ROOT / f"{stem}.cpp"

        assert header.is_file(), (
            f"{stem}.hpp must live under solver/include/bbsolver/progress/"
        )
        assert implementation.is_file(), (
            f"{stem}.cpp must live under solver/src/progress/"
        )
        assert not legacy_header.exists(), (
            f"legacy solver/src/{stem}.hpp must not return after the progress "
            "layout migration"
        )
        assert not legacy_area_header.exists(), (
            f"legacy solver/src/progress/{stem}.hpp must not return after the "
            "Rive-style header/source split"
        )
        assert not legacy_implementation.exists(), (
            f"legacy solver/src/{stem}.cpp must not return after the progress "
            "layout migration"
        )
        assert f'#include "bbsolver/progress/{stem}.hpp"' in _read(implementation), (
            f"{stem}.cpp must include its own progress header via the public "
            "bbsolver/progress path"
        )


def test_samples_modules_use_target_samples_layout() -> None:
    migrated = (
        "raw_frame_keys",
        "sample_key_timing",
        "sample_value_helpers",
        "source_key_preservation",
    )
    for stem in migrated:
        header = PUBLIC_ROOT / "samples" / f"{stem}.hpp"
        implementation = SRC_ROOT / "samples" / f"{stem}.cpp"
        legacy_header = SRC_ROOT / f"{stem}.hpp"
        legacy_area_header = SRC_ROOT / "samples" / f"{stem}.hpp"
        legacy_implementation = SRC_ROOT / f"{stem}.cpp"

        assert header.is_file(), (
            f"{stem}.hpp must live under solver/include/bbsolver/samples/"
        )
        assert implementation.is_file(), (
            f"{stem}.cpp must live under solver/src/samples/"
        )
        assert not legacy_header.exists(), (
            f"legacy solver/src/{stem}.hpp must not return after the samples "
            "layout migration"
        )
        assert not legacy_area_header.exists(), (
            f"legacy solver/src/samples/{stem}.hpp must not return after the "
            "Rive-style header/source split"
        )
        assert not legacy_implementation.exists(), (
            f"legacy solver/src/{stem}.cpp must not return after the samples "
            "layout migration"
        )
        assert f'#include "bbsolver/samples/{stem}.hpp"' in _read(implementation), (
            f"{stem}.cpp must include its own samples header via the public "
            "bbsolver/samples path"
        )


def test_motion_smooth_modules_use_target_motion_smooth_layout() -> None:
    # All 23 motion_smooth public headers under bbsolver/motion_smooth/. Two are
    # facade-only (no paired.cpp): motion_smooth_shape_schedule (deleted body)
    # and motion_smooth_solver (deleted body). The remaining 21 stems have
    # both a header and an implementation.
    facade_only_stems = {
        "motion_smooth_shape_schedule",
        "motion_smooth_solver",
    }
    migrated = (
        "motion_smooth_bezier_ease",
        "motion_smooth_dispatch",
        "motion_smooth_endpoint_keys",
        "motion_smooth_geometry",
        "motion_smooth_reduction_gate",
        "motion_smooth_sample_points",
        "motion_smooth_shape_flat",
        "motion_smooth_shape_flat_closed_loop",
        "motion_smooth_shape_flat_key_emission",
        "motion_smooth_shape_flat_notes",
        "motion_smooth_shape_flat_topology_gate",
        "motion_smooth_shape_loop",
        "motion_smooth_shape_loop_adaptive",
        "motion_smooth_shape_loop_curve",
        "motion_smooth_shape_loop_schedule",
        "motion_smooth_shape_quality",
        "motion_smooth_shape_rove_schedule",
        "motion_smooth_shape_schedule",
        "motion_smooth_shape_source_key_schedule",
        "motion_smooth_shape_tangent_lock",
        "motion_smooth_shape_trajectory_smooth",
        "motion_smooth_solver",
        "motion_smooth_spatial_trajectory",
    )
    for stem in migrated:
        header = PUBLIC_ROOT / "motion_smooth" / f"{stem}.hpp"
        legacy_header = SRC_ROOT / f"{stem}.hpp"
        legacy_area_header = SRC_ROOT / "motion_smooth" / f"{stem}.hpp"

        assert header.is_file(), (
            f"{stem}.hpp must live under solver/include/bbsolver/motion_smooth/"
        )
        assert not legacy_header.exists(), (
            f"legacy solver/src/{stem}.hpp must not return after the "
            "motion_smooth layout migration"
        )
        assert not legacy_area_header.exists(), (
            f"legacy solver/src/motion_smooth/{stem}.hpp must not return after "
            "the Rive-style header/source split"
        )

        if stem in facade_only_stems:
            continue  # No paired.cpp expected.

        implementation = SRC_ROOT / "motion_smooth" / f"{stem}.cpp"
        legacy_implementation = SRC_ROOT / f"{stem}.cpp"
        assert implementation.is_file(), (
            f"{stem}.cpp must live under solver/src/motion_smooth/"
        )
        assert not legacy_implementation.exists(), (
            f"legacy solver/src/{stem}.cpp must not return after the "
            "motion_smooth layout migration"
        )
        assert f'#include "bbsolver/motion_smooth/{stem}.hpp"' in _read(implementation), (
            f"{stem}.cpp must include its own motion_smooth header via the "
            "public bbsolver/motion_smooth path"
        )


def test_segment_fit_modules_use_target_fit_layout() -> None:
    migrated = (
        "segment_fit_bezier",
        "segment_fit_ceres",
        "segment_fit_diagnostic_events",
        "segment_fit_policy",
        "segment_fit_samples",
        "segment_fit_shape_temporal",
        "segment_fit_unified_spatial",
        "segment_fitter",
    )
    for stem in migrated:
        header = PUBLIC_ROOT / "fit" / f"{stem}.hpp"
        implementation = SRC_ROOT / "fit" / f"{stem}.cpp"
        legacy_header = SRC_ROOT / f"{stem}.hpp"
        legacy_area_header = SRC_ROOT / "fit" / f"{stem}.hpp"
        legacy_implementation = SRC_ROOT / f"{stem}.cpp"

        assert header.is_file(), (
            f"{stem}.hpp must live under solver/include/bbsolver/fit/"
        )
        assert implementation.is_file(), (
            f"{stem}.cpp must live under solver/src/fit/"
        )
        assert not legacy_header.exists(), (
            f"legacy solver/src/{stem}.hpp must not return after the segment "
            "fit layout migration"
        )
        assert not legacy_area_header.exists(), (
            f"legacy solver/src/fit/{stem}.hpp must not return after the "
            "Rive-style header/source split"
        )
        assert not legacy_implementation.exists(), (
            f"legacy solver/src/{stem}.cpp must not return after the segment "
            "fit layout migration"
        )
        assert f'#include "bbsolver/fit/{stem}.hpp"' in _read(implementation), (
            f"{stem}.cpp must include its own fit header via the public "
            "bbsolver/fit path"
        )


def test_metrics_modules_use_target_metrics_layout() -> None:
    migrated = (
        "ae_curve",
        "error_metrics",
        "unified_spatial",
    )
    for stem in migrated:
        header = PUBLIC_ROOT / "metrics" / f"{stem}.hpp"
        implementation = SRC_ROOT / "metrics" / f"{stem}.cpp"
        legacy_header = SRC_ROOT / f"{stem}.hpp"
        legacy_area_header = SRC_ROOT / "metrics" / f"{stem}.hpp"
        legacy_implementation = SRC_ROOT / f"{stem}.cpp"

        assert header.is_file(), (
            f"{stem}.hpp must live under solver/include/bbsolver/metrics/"
        )
        assert implementation.is_file(), (
            f"{stem}.cpp must live under solver/src/metrics/"
        )
        assert not legacy_header.exists(), (
            f"legacy solver/src/{stem}.hpp must not return after the metrics "
            "layout migration"
        )
        assert not legacy_area_header.exists(), (
            f"legacy solver/src/metrics/{stem}.hpp must not return after the "
            "Rive-style header/source split"
        )
        assert not legacy_implementation.exists(), (
            f"legacy solver/src/{stem}.cpp must not return after the metrics "
            "layout migration"
        )
        assert f'#include "bbsolver/metrics/{stem}.hpp"' in _read(implementation), (
            f"{stem}.cpp must include its own metrics header via the public "
            "bbsolver/metrics path"
        )


def test_verify_modules_use_target_verify_layout() -> None:
    migrated = (
        "verifier",
        "verify_dump_commands",
    )
    for stem in migrated:
        header = PUBLIC_ROOT / "verify" / f"{stem}.hpp"
        implementation = SRC_ROOT / "verify" / f"{stem}.cpp"
        legacy_header = SRC_ROOT / f"{stem}.hpp"
        legacy_area_header = SRC_ROOT / "verify" / f"{stem}.hpp"
        legacy_implementation = SRC_ROOT / f"{stem}.cpp"

        assert header.is_file(), (
            f"{stem}.hpp must live under solver/include/bbsolver/verify/"
        )
        assert implementation.is_file(), (
            f"{stem}.cpp must live under solver/src/verify/"
        )
        assert not legacy_header.exists(), (
            f"legacy solver/src/{stem}.hpp must not return after the verify "
            "layout migration"
        )
        assert not legacy_area_header.exists(), (
            f"legacy solver/src/verify/{stem}.hpp must not return after the "
            "Rive-style header/source split"
        )
        assert not legacy_implementation.exists(), (
            f"legacy solver/src/{stem}.cpp must not return after the verify "
            "layout migration"
        )
        assert f'#include "bbsolver/verify/{stem}.hpp"' in _read(implementation), (
            f"{stem}.cpp must include its own verify header via the public "
            "bbsolver/verify path"
        )


def test_solve_modules_use_target_solve_layout() -> None:
    migrated = (
        "fallback_property_solver",
        "plain_property_solver",
        "static_key_cleanup",
        "solve_command",
        "solve_command_config",
        "solve_lifecycle_reporting",
        "solve_path_preparation",
        "solve_property_completion",
        "solve_property_output",
        "solve_property_post_processing",
        "solve_property_temporal_prelude",
        "solve_property_temporal_result",
        "solver_observability",
        "solver_reporting",
    )
    for stem in migrated:
        header = PUBLIC_ROOT / "solve" / f"{stem}.hpp"
        implementation = SRC_ROOT / "solve" / f"{stem}.cpp"
        legacy_header = SRC_ROOT / f"{stem}.hpp"
        legacy_area_header = SRC_ROOT / "solve" / f"{stem}.hpp"
        legacy_implementation = SRC_ROOT / f"{stem}.cpp"

        assert header.is_file(), (
            f"{stem}.hpp must live under solver/include/bbsolver/solve/"
        )
        assert implementation.is_file(), (
            f"{stem}.cpp must live under solver/src/solve/"
        )
        assert not legacy_header.exists(), (
            f"legacy solver/src/{stem}.hpp must not return after the solve "
            "layout migration"
        )
        assert not legacy_area_header.exists(), (
            f"legacy solver/src/solve/{stem}.hpp must not return after the "
            "Rive-style header/source split"
        )
        assert not legacy_implementation.exists(), (
            f"legacy solver/src/{stem}.cpp must not return after the solve "
            "layout migration"
        )
        assert f'#include "bbsolver/solve/{stem}.hpp"' in _read(implementation), (
            f"{stem}.cpp must include its own solve header via the public "
            "bbsolver/solve path"
        )


def test_temporal_refit_modules_use_target_temporal_refit_layout() -> None:
    migrated = (
        "temporal_refit",
        "temporal_refit_budget",
        "temporal_refit_candidate",
        "temporal_refit_dimensions",
        "temporal_refit_gate",
        "temporal_refit_resample",
        "temporal_refit_shape",
        "temporal_refit_structural",
        "temporal_refit_support",
        "temporal_refit_validation",
    )
    for stem in migrated:
        header = PUBLIC_ROOT / "temporal" / "refit" / f"{stem}.hpp"
        implementation = SRC_ROOT / "temporal" / "refit" / f"{stem}.cpp"
        legacy_header = SRC_ROOT / f"{stem}.hpp"
        legacy_implementation = SRC_ROOT / f"{stem}.cpp"

        assert header.is_file(), (
            f"{stem}.hpp must live under solver/include/bbsolver/temporal/refit/"
        )
        assert implementation.is_file(), (
            f"{stem}.cpp must live under solver/src/temporal/refit/"
        )
        assert not legacy_header.exists(), (
            f"legacy solver/src/{stem}.hpp must not return after the "
            "temporal/refit layout migration"
        )
        assert not legacy_implementation.exists(), (
            f"legacy solver/src/{stem}.cpp must not return after the "
            "temporal/refit layout migration"
        )
        assert f'#include "bbsolver/temporal/refit/{stem}.hpp"' in _read(
            implementation
        ), (
            f"{stem}.cpp must include its own temporal/refit header via the "
            "public bbsolver/temporal/refit path"
        )


def test_path_temporal_modules_use_target_path_temporal_layout() -> None:
    # Path-level temporal helpers live under the nested
    # `solver/include/bbsolver/path/temporal/` subarea to avoid a giant
    # single `path/` folder.
    migrated = (
        "path_temporal_band_helpers",
        "path_temporal_influence",
        "path_temporal_progress",
        "path_temporal_validation",
    )
    for stem in migrated:
        header = PUBLIC_ROOT / "path" / "temporal" / f"{stem}.hpp"
        implementation = SRC_ROOT / "path" / "temporal" / f"{stem}.cpp"
        legacy_header = SRC_ROOT / f"{stem}.hpp"
        legacy_implementation = SRC_ROOT / f"{stem}.cpp"

        assert header.is_file(), (
            f"{stem}.hpp must live under solver/include/bbsolver/path/temporal/"
        )
        assert implementation.is_file(), (
            f"{stem}.cpp must live under solver/src/path/temporal/"
        )
        assert not legacy_header.exists(), (
            f"legacy solver/src/{stem}.hpp must not return after the "
            "path/temporal layout migration"
        )
        assert not legacy_implementation.exists(), (
            f"legacy solver/src/{stem}.cpp must not return after the "
            "path/temporal layout migration"
        )
        assert f'#include "bbsolver/path/temporal/{stem}.hpp"' in _read(implementation), (
            f"{stem}.cpp must include its own header via the public "
            "bbsolver/path/temporal path"
        )


def test_path_decompose_modules_use_target_path_decompose_layout() -> None:
    migrated = (
        "path_decompose",
        "path_decomposed_solver",
    )
    for stem in migrated:
        header = PUBLIC_ROOT / "path" / "decompose" / f"{stem}.hpp"
        implementation = SRC_ROOT / "path" / "decompose" / f"{stem}.cpp"
        legacy_header = SRC_ROOT / f"{stem}.hpp"
        legacy_implementation = SRC_ROOT / f"{stem}.cpp"

        assert header.is_file(), (
            f"{stem}.hpp must live under solver/include/bbsolver/path/decompose/"
        )
        assert implementation.is_file(), (
            f"{stem}.cpp must live under solver/src/path/decompose/"
        )
        assert not legacy_header.exists(), (
            f"legacy solver/src/{stem}.hpp must not return after the "
            "path/decompose layout migration"
        )
        assert not legacy_implementation.exists(), (
            f"legacy solver/src/{stem}.cpp must not return after the "
            "path/decompose layout migration"
        )
        assert f'#include "bbsolver/path/decompose/{stem}.hpp"' in _read(implementation), (
            f"{stem}.cpp must include its own header via the public "
            "bbsolver/path/decompose path"
        )


def test_path_geometry_modules_use_target_path_geometry_layout() -> None:
    # path/geometry owns path-frame-fit leaf geometry helpers. The family has
    # paired modules, one header-only fraction helper, and source-only files
    # whose public declarations intentionally remain on path/frame_fit.
    paired_stems = (
        "path_feature_anchor",
        "path_feature_cluster",
        "path_geometry_refinement",
        "path_sharp_feature",
        "path_visible_outline_prepass",
    )
    header_only_stems = (
        "path_fraction_helpers",
    )
    source_only_stems = (
        "path_frame_geometry_refine",
        "path_outline_error",
        "path_outline_fraction_expand",
        "path_visible_outline_extract",
    )

    for stem in paired_stems:
        header = PUBLIC_ROOT / "path" / "geometry" / f"{stem}.hpp"
        implementation = SRC_ROOT / "path" / "geometry" / f"{stem}.cpp"
        legacy_header = SRC_ROOT / f"{stem}.hpp"
        legacy_implementation = SRC_ROOT / f"{stem}.cpp"

        assert header.is_file(), (
            f"{stem}.hpp must live under solver/include/bbsolver/path/geometry/"
        )
        assert implementation.is_file(), (
            f"{stem}.cpp must live under solver/src/path/geometry/"
        )
        assert not legacy_header.exists(), (
            f"legacy solver/src/{stem}.hpp must not return after the "
            "path/geometry layout migration"
        )
        assert not legacy_implementation.exists(), (
            f"legacy solver/src/{stem}.cpp must not return after the "
            "path/geometry layout migration"
        )
        assert f'#include "bbsolver/path/geometry/{stem}.hpp"' in _read(
            implementation
        ), (
            f"{stem}.cpp must include its own header via the public "
            "bbsolver/path/geometry path"
        )

    for stem in header_only_stems:
        header = PUBLIC_ROOT / "path" / "geometry" / f"{stem}.hpp"
        legacy_header = SRC_ROOT / f"{stem}.hpp"
        implementation = SRC_ROOT / "path" / "geometry" / f"{stem}.cpp"
        legacy_implementation = SRC_ROOT / f"{stem}.cpp"

        assert header.is_file(), (
            f"{stem}.hpp must live under solver/include/bbsolver/path/geometry/"
        )
        assert not legacy_header.exists(), (
            f"legacy solver/src/{stem}.hpp must not return after the "
            "path/geometry layout migration"
        )
        assert not implementation.exists(), (
            f"{stem}.cpp must not appear for the header-only path/geometry helper"
        )
        assert not legacy_implementation.exists(), (
            f"legacy solver/src/{stem}.cpp must not appear for the header-only "
            "path/geometry helper"
        )

    for stem in source_only_stems:
        implementation = SRC_ROOT / "path" / "geometry" / f"{stem}.cpp"
        legacy_header = SRC_ROOT / f"{stem}.hpp"
        legacy_implementation = SRC_ROOT / f"{stem}.cpp"

        assert implementation.is_file(), (
            f"{stem}.cpp must live under solver/src/path/geometry/"
        )
        assert not legacy_header.exists(), (
            f"legacy solver/src/{stem}.hpp must not appear for the source-only "
            "path/geometry helper"
        )
        assert not legacy_implementation.exists(), (
            f"legacy solver/src/{stem}.cpp must not return after the "
            "path/geometry layout migration"
        )


def test_path_fit_modules_use_target_path_fit_layout() -> None:
    # path/fit owns the canonical path-fit public surface, its geometry support,
    # the path-fit pipeline helpers, one header-only fraction-layout evaluator,
    # and a source-only feature-layout implementation whose declaration remains
    # on path/frame_fit.
    paired_stems = (
        "path_fit",
        "path_fit_geometry",
        "path_fit_pipeline",
    )
    for stem in paired_stems:
        header = PUBLIC_ROOT / "path" / "fit" / f"{stem}.hpp"
        implementation = SRC_ROOT / "path" / "fit" / f"{stem}.cpp"
        legacy_header = SRC_ROOT / f"{stem}.hpp"
        legacy_implementation = SRC_ROOT / f"{stem}.cpp"

        assert header.is_file(), (
            f"{stem}.hpp must live under solver/include/bbsolver/path/fit/"
        )
        assert implementation.is_file(), (
            f"{stem}.cpp must live under solver/src/path/fit/"
        )
        assert not legacy_header.exists(), (
            f"legacy solver/src/{stem}.hpp must not return after the "
            "path/fit layout migration"
        )
        assert not legacy_implementation.exists(), (
            f"legacy solver/src/{stem}.cpp must not return after the "
            "path/fit layout migration"
        )
        assert f'#include "bbsolver/path/fit/{stem}.hpp"' in _read(
            implementation
        ), (
            f"{stem}.cpp must include its own header via the public "
            "bbsolver/path/fit path"
        )

    header_only = PUBLIC_ROOT / "path" / "fit" / "path_fraction_layout_eval.hpp"
    assert header_only.is_file(), (
        "path_fraction_layout_eval.hpp must live under "
        "solver/include/bbsolver/path/fit/"
    )
    assert not (SRC_ROOT / "path_fraction_layout_eval.hpp").exists(), (
        "legacy solver/src/path_fraction_layout_eval.hpp must not return "
        "after the path/fit layout migration"
    )
    assert not (SRC_ROOT / "path" / "fit" / "path_fraction_layout_eval.cpp").exists(), (
        "path_fraction_layout_eval.cpp must not appear for the header-only "
        "path/fit helper"
    )

    source_only = SRC_ROOT / "path" / "fit" / "path_feature_layout.cpp"
    assert source_only.is_file(), (
        "path_feature_layout.cpp must live under solver/src/path/fit/"
    )
    assert not (SRC_ROOT / "path_feature_layout.cpp").exists(), (
        "legacy solver/src/path_feature_layout.cpp must not return after the "
        "path/fit layout migration"
    )
    assert '#include "bbsolver/path/fit/path_fraction_layout_eval.hpp"' in _read(
        source_only
    ), (
        "path_feature_layout.cpp must include the fraction-layout evaluator "
        "through the public bbsolver/path/fit path"
    )


def test_shape_modules_use_target_shape_layout() -> None:
    migrated = (
        "shape_flat_topology",
        "sharp_corner_policy",
    )
    for stem in migrated:
        header = PUBLIC_ROOT / "shape" / f"{stem}.hpp"
        implementation = SRC_ROOT / "shape" / f"{stem}.cpp"
        legacy_header = SRC_ROOT / f"{stem}.hpp"
        legacy_area_header = SRC_ROOT / "shape" / f"{stem}.hpp"
        legacy_implementation = SRC_ROOT / f"{stem}.cpp"

        assert header.is_file(), (
            f"{stem}.hpp must live under solver/include/bbsolver/shape/"
        )
        assert implementation.is_file(), (
            f"{stem}.cpp must live under solver/src/shape/"
        )
        assert not legacy_header.exists(), (
            f"legacy solver/src/{stem}.hpp must not return after the shape "
            "layout migration"
        )
        assert not legacy_area_header.exists(), (
            f"legacy solver/src/shape/{stem}.hpp must not return after the "
            "Rive-style header/source split"
        )
        assert not legacy_implementation.exists(), (
            f"legacy solver/src/{stem}.cpp must not return after the shape "
            "layout migration"
        )
        assert f'#include "bbsolver/shape/{stem}.hpp"' in _read(implementation), (
            f"{stem}.cpp must include its own shape header via the public "
            "bbsolver/shape path"
        )


def test_replacement_temporal_modules_use_target_replacement_temporal_layout() -> None:
    # The replacement-temporal subsystem migrates as its own top-level
    # area under `solver/include/bbsolver/replacement_temporal/`. It is
    # architecturally distinct from path/temporal (which is the path-
    # level validation/progress surface) and the fit area, so
    # it stays a peer rather than a nested folder.
    migrated = (
        "replacement_temporal_anchor_prune",
        "replacement_temporal_anchor_prune_fit",
        "replacement_temporal_forward_span",
        "replacement_temporal_keys",
        "replacement_temporal_options",
        "replacement_temporal_relaxed_fit",
        "replacement_temporal_segment_fit",
        "replacement_temporal_solve_options",
        "replacement_temporal_solver",
    )
    for stem in migrated:
        header = PUBLIC_ROOT / "replacement_temporal" / f"{stem}.hpp"
        implementation = SRC_ROOT / "replacement_temporal" / f"{stem}.cpp"
        legacy_header = SRC_ROOT / f"{stem}.hpp"
        legacy_implementation = SRC_ROOT / f"{stem}.cpp"

        assert header.is_file(), (
            f"{stem}.hpp must live under solver/include/bbsolver/replacement_temporal/"
        )
        assert implementation.is_file(), (
            f"{stem}.cpp must live under solver/src/replacement_temporal/"
        )
        assert not legacy_header.exists(), (
            f"legacy solver/src/{stem}.hpp must not return after the "
            "replacement_temporal layout migration"
        )
        assert not legacy_implementation.exists(), (
            f"legacy solver/src/{stem}.cpp must not return after the "
            "replacement_temporal layout migration"
        )
        assert f'#include "bbsolver/replacement_temporal/{stem}.hpp"' in _read(implementation), (
            f"{stem}.cpp must include its own header via the public "
            "bbsolver/replacement_temporal path"
        )


def test_path_frame_fit_modules_use_target_path_frame_fit_layout() -> None:
    # The path frame-fit family lives as a nested subarea
    # `solver/include/bbsolver/path/frame_fit/` alongside `path/temporal/`.
    # The family has a mix of paired modules (hpp+cpp), header-only
    # declarations (candidate / geometry / types) and source-only files
    # (at_fractions / main) that compose into the path_frame_fit
    # translation unit set.
    paired_stems = (
        "path_frame_fit",
        "path_frame_fit_cubic_span",
        "path_frame_fit_decimate",
    )
    header_only_stems = (
        "path_frame_fit_candidate",
        "path_frame_fit_geometry",
        "path_frame_fit_types",
    )
    source_only_stems = (
        "path_frame_fit_at_fractions",
        "path_frame_fit_main",
    )

    # Paired stems: header at public root, source at private root, both
    # the legacy flat header and source must be gone, the source must
    # include its own public header via the canonical path.
    for stem in paired_stems:
        header = PUBLIC_ROOT / "path" / "frame_fit" / f"{stem}.hpp"
        implementation = SRC_ROOT / "path" / "frame_fit" / f"{stem}.cpp"
        legacy_header = SRC_ROOT / f"{stem}.hpp"
        legacy_implementation = SRC_ROOT / f"{stem}.cpp"

        assert header.is_file(), (
            f"{stem}.hpp must live under solver/include/bbsolver/path/frame_fit/"
        )
        assert implementation.is_file(), (
            f"{stem}.cpp must live under solver/src/path/frame_fit/"
        )
        assert not legacy_header.exists(), (
            f"legacy solver/src/{stem}.hpp must not return after the "
            "path/frame_fit layout migration"
        )
        assert not legacy_implementation.exists(), (
            f"legacy solver/src/{stem}.cpp must not return after the "
            "path/frame_fit layout migration"
        )
        assert f'#include "bbsolver/path/frame_fit/{stem}.hpp"' in _read(implementation), (
            f"{stem}.cpp must include its own header via the public "
            "bbsolver/path/frame_fit path"
        )

    # Header-only stems: public header exists, no source pair, legacy
    # flat header gone.
    for stem in header_only_stems:
        header = PUBLIC_ROOT / "path" / "frame_fit" / f"{stem}.hpp"
        legacy_header = SRC_ROOT / f"{stem}.hpp"
        assert header.is_file(), (
            f"{stem}.hpp must live under solver/include/bbsolver/path/frame_fit/"
        )
        assert not legacy_header.exists(), (
            f"legacy solver/src/{stem}.hpp must not return after the "
            "path/frame_fit layout migration"
        )

    # Source-only stems: source at private root, legacy flat source
    # gone. No public header expected — these contribute TUs whose
    # declarations live in companion frame_fit headers.
    for stem in source_only_stems:
        implementation = SRC_ROOT / "path" / "frame_fit" / f"{stem}.cpp"
        legacy_implementation = SRC_ROOT / f"{stem}.cpp"
        assert implementation.is_file(), (
            f"{stem}.cpp must live under solver/src/path/frame_fit/"
        )
        assert not legacy_implementation.exists(), (
            f"legacy solver/src/{stem}.cpp must not return after the "
            "path/frame_fit layout migration"
        )


def test_path_multimode_modules_use_target_path_multimode_layout() -> None:
    # The path multimode family lives as a nested subarea
    # `solver/include/bbsolver/path/multimode/` alongside `path/temporal/`
    # and `path/frame_fit/`. All 19 modules are paired hpp+cpp.
    migrated = (
        "path_multimode_geometry",
        "path_multimode_input_validation",
        "path_multimode_landmark_diagnostics",
        "path_multimode_landmark_emission",
        "path_multimode_landmark_options",
        "path_multimode_landmark_output",
        "path_multimode_landmark_partition",
        "path_multimode_landmark_partition_alternatives",
        "path_multimode_landmark_segment_fit",
        "path_multimode_landmark_temporal_solve",
        "path_multimode_mask_channel_diagnostic",
        "path_multimode_notes",
        "path_multimode_recombined_temporal",
        "path_multimode_reconstruction",
        "path_multimode_region_candidate",
        "path_multimode_solver",
        "path_multimode_solver_notes",
        "path_multimode_temporal",
        "path_multimode_visible_probe",
    )
    for stem in migrated:
        header = PUBLIC_ROOT / "path" / "multimode" / f"{stem}.hpp"
        implementation = SRC_ROOT / "path" / "multimode" / f"{stem}.cpp"
        legacy_header = SRC_ROOT / f"{stem}.hpp"
        legacy_implementation = SRC_ROOT / f"{stem}.cpp"

        assert header.is_file(), (
            f"{stem}.hpp must live under solver/include/bbsolver/path/multimode/"
        )
        assert implementation.is_file(), (
            f"{stem}.cpp must live under solver/src/path/multimode/"
        )
        assert not legacy_header.exists(), (
            f"legacy solver/src/{stem}.hpp must not return after the "
            "path/multimode layout migration"
        )
        assert not legacy_implementation.exists(), (
            f"legacy solver/src/{stem}.cpp must not return after the "
            "path/multimode layout migration"
        )
        assert f'#include "bbsolver/path/multimode/{stem}.hpp"' in _read(implementation), (
            f"{stem}.cpp must include its own header via the public "
            "bbsolver/path/multimode path"
        )


def test_path_dense_modules_use_target_path_dense_layout() -> None:
    # path/dense subarea peer of path/frame_fit, path/multimode, path/temporal.
    # Two paired modules (hpp+cpp): polyline (subdivision + chord arithmetic)
    # and landmarks (sampling at fractions).
    migrated = (
        "path_dense_polyline",
        "path_dense_landmarks",
    )
    for stem in migrated:
        header = PUBLIC_ROOT / "path" / "dense" / f"{stem}.hpp"
        implementation = SRC_ROOT / "path" / "dense" / f"{stem}.cpp"
        legacy_header = SRC_ROOT / f"{stem}.hpp"
        legacy_implementation = SRC_ROOT / f"{stem}.cpp"

        assert header.is_file(), (
            f"{stem}.hpp must live under solver/include/bbsolver/path/dense/"
        )
        assert implementation.is_file(), (
            f"{stem}.cpp must live under solver/src/path/dense/"
        )
        assert not legacy_header.exists(), (
            f"legacy solver/src/{stem}.hpp must not return after the "
            "path/dense layout migration"
        )
        assert not legacy_implementation.exists(), (
            f"legacy solver/src/{stem}.cpp must not return after the "
            "path/dense layout migration"
        )
        assert f'#include "bbsolver/path/dense/{stem}.hpp"' in _read(implementation), (
            f"{stem}.cpp must include its own header via the public "
            "bbsolver/path/dense path"
        )


def test_path_bridge_prune_modules_use_target_path_bridge_prune_layout() -> None:
    # path/bridge_prune subarea peer of path/temporal, path/frame_fit,
    # path/multimode, path/dense, path/decompose. All 10 modules paired
    # hpp+cpp.
    migrated = (
        "path_bridge_prune",
        "path_bridge_prune_batch",
        "path_bridge_prune_batch_attempt",
        "path_bridge_prune_candidate",
        "path_bridge_prune_notes",
        "path_bridge_prune_plan",
        "path_bridge_prune_progress",
        "path_bridge_prune_result",
        "path_bridge_prune_round",
        "path_bridge_prune_selection",
    )
    for stem in migrated:
        header = PUBLIC_ROOT / "path" / "bridge_prune" / f"{stem}.hpp"
        implementation = SRC_ROOT / "path" / "bridge_prune" / f"{stem}.cpp"
        legacy_header = SRC_ROOT / f"{stem}.hpp"
        legacy_implementation = SRC_ROOT / f"{stem}.cpp"

        assert header.is_file(), (
            f"{stem}.hpp must live under solver/include/bbsolver/path/bridge_prune/"
        )
        assert implementation.is_file(), (
            f"{stem}.cpp must live under solver/src/path/bridge_prune/"
        )
        assert not legacy_header.exists(), (
            f"legacy solver/src/{stem}.hpp must not return after the "
            "path/bridge_prune layout migration"
        )
        assert not legacy_implementation.exists(), (
            f"legacy solver/src/{stem}.cpp must not return after the "
            "path/bridge_prune layout migration"
        )
        assert f'#include "bbsolver/path/bridge_prune/{stem}.hpp"' in _read(implementation), (
            f"{stem}.cpp must include its own header via the public "
            "bbsolver/path/bridge_prune path"
        )


def test_path_config_modules_use_target_path_config_layout() -> None:
    migrated = (
        "path_gap_policy",
        "path_solver_config",
    )
    for stem in migrated:
        header = PUBLIC_ROOT / "path" / "config" / f"{stem}.hpp"
        implementation = SRC_ROOT / "path" / "config" / f"{stem}.cpp"
        legacy_header = SRC_ROOT / f"{stem}.hpp"
        legacy_implementation = SRC_ROOT / f"{stem}.cpp"

        assert header.is_file(), (
            f"{stem}.hpp must live under solver/include/bbsolver/path/config/"
        )
        assert implementation.is_file(), (
            f"{stem}.cpp must live under solver/src/path/config/"
        )
        assert not legacy_header.exists(), (
            f"legacy solver/src/{stem}.hpp must not return after the "
            "path/config layout migration"
        )
        assert not legacy_implementation.exists(), (
            f"legacy solver/src/{stem}.cpp must not return after the "
            "path/config layout migration"
        )
        assert f'#include "bbsolver/path/config/{stem}.hpp"' in _read(implementation), (
            f"{stem}.cpp must include its own header via the public "
            "bbsolver/path/config path"
        )


def test_path_reduction_modules_use_target_path_reduction_layout() -> None:
    migrated = (
        "path_bridge_refit",
        "path_post_solve_reduction",
        "path_vertex_reduction",
    )
    for stem in migrated:
        header = PUBLIC_ROOT / "path" / "reduction" / f"{stem}.hpp"
        implementation = SRC_ROOT / "path" / "reduction" / f"{stem}.cpp"
        legacy_header = SRC_ROOT / f"{stem}.hpp"
        legacy_implementation = SRC_ROOT / f"{stem}.cpp"

        assert header.is_file(), (
            f"{stem}.hpp must live under solver/include/bbsolver/path/reduction/"
        )
        assert implementation.is_file(), (
            f"{stem}.cpp must live under solver/src/path/reduction/"
        )
        assert not legacy_header.exists(), (
            f"legacy solver/src/{stem}.hpp must not return after the "
            "path/reduction layout migration"
        )
        assert not legacy_implementation.exists(), (
            f"legacy solver/src/{stem}.cpp must not return after the "
            "path/reduction layout migration"
        )
        assert f'#include "bbsolver/path/reduction/{stem}.hpp"' in _read(implementation), (
            f"{stem}.cpp must include its own header via the public "
            "bbsolver/path/reduction path"
        )


def test_dp_modules_use_target_dp_layout() -> None:
    # dp family: 5 paired modules covering DP placement core (placer),
    # forward sweep, key assembly, placement limits, and progress emission.
    # All paired hpp+cpp.
    migrated = (
        "dp_forward_placement",
        "dp_key_assembly",
        "dp_placement_limits",
        "dp_placement_progress",
        "dp_placer",
    )
    for stem in migrated:
        header = PUBLIC_ROOT / "dp" / f"{stem}.hpp"
        implementation = SRC_ROOT / "dp" / f"{stem}.cpp"
        legacy_header = SRC_ROOT / f"{stem}.hpp"
        legacy_implementation = SRC_ROOT / f"{stem}.cpp"

        assert header.is_file(), (
            f"{stem}.hpp must live under solver/include/bbsolver/dp/"
        )
        assert implementation.is_file(), (
            f"{stem}.cpp must live under solver/src/dp/"
        )
        assert not legacy_header.exists(), (
            f"legacy solver/src/{stem}.hpp must not return after the "
            "dp layout migration"
        )
        assert not legacy_implementation.exists(), (
            f"legacy solver/src/{stem}.cpp must not return after the "
            "dp layout migration"
        )
        assert f'#include "bbsolver/dp/{stem}.hpp"' in _read(implementation), (
            f"{stem}.cpp must include its own header via the public "
            "bbsolver/dp path"
        )


def test_path_replacement_modules_use_target_path_replacement_layout() -> None:
    # path/replacement subarea peer of path/{fit,temporal,frame_fit,multimode,
    # dense,bridge_prune,decompose,config,reduction,geometry}. 18 paired
    # modules plus 1 source-only (target_ladder, whose declarations live in
    # bbsolver/path/frame_fit/path_frame_fit_types.hpp).
    paired_stems = (
        "path_replacement_acceptance",
        "path_replacement_adaptive_expansion",
        "path_replacement_baseline_solve",
        "path_replacement_candidate_validation",
        "path_replacement_decision_apply",
        "path_replacement_fast_vertex_acceptance",
        "path_replacement_feature_layout_trial",
        "path_replacement_fraction_layout",
        "path_replacement_fraction_trial",
        "path_replacement_initial_scan",
        "path_replacement_notes",
        "path_replacement_phase2_fit",
        "path_replacement_post_temporal",
        "path_replacement_preference",
        "path_replacement_progress",
        "path_replacement_retry_loop",
        "path_replacement_seed_selection",
        "path_replacement_solver",
    )
    source_only_stems = (
        "path_replacement_target_ladder",
    )
    for stem in paired_stems:
        header = PUBLIC_ROOT / "path" / "replacement" / f"{stem}.hpp"
        implementation = SRC_ROOT / "path" / "replacement" / f"{stem}.cpp"
        legacy_header = SRC_ROOT / f"{stem}.hpp"
        legacy_implementation = SRC_ROOT / f"{stem}.cpp"

        assert header.is_file(), (
            f"{stem}.hpp must live under solver/include/bbsolver/path/replacement/"
        )
        assert implementation.is_file(), (
            f"{stem}.cpp must live under solver/src/path/replacement/"
        )
        assert not legacy_header.exists(), (
            f"legacy solver/src/{stem}.hpp must not return after the "
            "path/replacement layout migration"
        )
        assert not legacy_implementation.exists(), (
            f"legacy solver/src/{stem}.cpp must not return after the "
            "path/replacement layout migration"
        )
        assert f'#include "bbsolver/path/replacement/{stem}.hpp"' in _read(implementation), (
            f"{stem}.cpp must include its own header via the public "
            "bbsolver/path/replacement path"
        )

    for stem in source_only_stems:
        implementation = SRC_ROOT / "path" / "replacement" / f"{stem}.cpp"
        legacy_implementation = SRC_ROOT / f"{stem}.cpp"
        assert implementation.is_file(), (
            f"{stem}.cpp must live under solver/src/path/replacement/"
        )
        assert not legacy_implementation.exists(), (
            f"legacy solver/src/{stem}.cpp must not return after the "
            "path/replacement layout migration"
        )


def main() -> int:
    tests = [
        test_cmake_recurses_implementation_sources,
        test_cmake_exports_public_include_root,
        test_cmake_uses_solver_local_protocol_schemas,
        test_flatbuffers_schemas_are_not_linted_as_cpp,
        test_solver_package_carries_source_control_hygiene_files,
        test_solver_package_carries_standalone_ci_and_sync_docs,
        test_cmake_presets_lock_fast_validation_workflows,
        test_cmake_installs_and_exports_standalone_package,
        test_solver_tree_contains_no_finder_metadata,
        test_solver_ctest_exports_source_dir_for_fixtures,
        test_cmake_dependency_lookup_ignores_user_package_registry,
        test_after_effects_harness_support_folder_is_packaged_and_used,
        test_package_config_template_is_dependency_aware,
        test_package_smoke_project_locks_exported_targets,
        test_solver_unit_tests_live_under_solver_tree,
        test_solver_fixtures_are_solver_local,
        test_third_party_backup_archives_are_hash_locked,
        test_public_headers_live_under_bbsolver_namespace_root,
        test_public_root_only_exposes_domain_header_and_area_dirs,
        test_no_legacy_h_headers_in_solver_layout,
        test_no_root_cxx_files_remain_in_solver_src,
        test_cmake_dp_feature_probe_uses_target_dp_layout,
        test_cli_options_uses_target_app_layout,
        test_main_entrypoint_uses_target_app_layout,
        test_routing_modules_use_target_routing_layout,
        test_io_modules_use_target_io_header_layout,
        test_runtime_modules_use_target_runtime_layout,
        test_diagnostics_modules_use_target_diagnostics_layout,
        test_progress_modules_use_target_progress_layout,
        test_samples_modules_use_target_samples_layout,
        test_motion_smooth_modules_use_target_motion_smooth_layout,
        test_segment_fit_modules_use_target_fit_layout,
        test_metrics_modules_use_target_metrics_layout,
        test_verify_modules_use_target_verify_layout,
        test_solve_modules_use_target_solve_layout,
        test_temporal_refit_modules_use_target_temporal_refit_layout,
        test_path_temporal_modules_use_target_path_temporal_layout,
        test_path_decompose_modules_use_target_path_decompose_layout,
        test_path_geometry_modules_use_target_path_geometry_layout,
        test_path_fit_modules_use_target_path_fit_layout,
        test_shape_modules_use_target_shape_layout,
        test_replacement_temporal_modules_use_target_replacement_temporal_layout,
        test_path_frame_fit_modules_use_target_path_frame_fit_layout,
        test_path_multimode_modules_use_target_path_multimode_layout,
        test_path_dense_modules_use_target_path_dense_layout,
        test_path_bridge_prune_modules_use_target_path_bridge_prune_layout,
        test_path_config_modules_use_target_path_config_layout,
        test_path_reduction_modules_use_target_path_reduction_layout,
        test_dp_modules_use_target_dp_layout,
        test_path_replacement_modules_use_target_path_replacement_layout,
    ]
    for test in tests:
        test()
        print(f"[PASS] {test.__name__}")
    print(f"summary: {len(tests)} passed, 0 failed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
