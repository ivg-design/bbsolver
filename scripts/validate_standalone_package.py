#!/usr/bin/env python3
"""End-to-end standalone package validation for bbsolver.

The script copies the solver tree to a temporary package root, builds it
from that copy, runs CTest and source policies, installs it, validates the
installed CMake package from an external consumer project, and exercises the
packaged JSON examples through solve/verify/dump.
"""

from __future__ import annotations

import argparse
import fnmatch
import json
import os
import platform
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path


EXCLUDE_PATTERNS = (
    ".git",
    ".DS_Store",
    "__pycache__",
    ".pytest_cache",
    "build",
    "build-*",
    "cmake-build-*",
    "out",
    "CMakeCache.txt",
    "CMakeFiles",
    "CTestTestfile.cmake",
    "DartConfiguration.tcl",
    "Testing",
    "cmake_install.cmake",
    "compile_commands.json",
    "install_manifest.txt",
    "*.a",
    "*.app",
    "*.d",
    "*.dSYM",
    "*.dylib",
    "*.exe",
    "*.lib",
    "*.o",
    "*.obj",
    "*.out",
    "*.so",
    "*.so.*",
    "*.pyc",
)

TEXT_INSTALL_EXTENSIONS = {
    ".cmake",
    ".fbs",
    ".h",
    ".hpp",
    ".json",
    ".jsx",
    ".md",
    ".txt",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Validate bbsolver as a standalone build/install/package."
    )
    parser.add_argument(
        "--mode",
        choices=("release", "incremental"),
        default="release",
        help=(
            "release=clean copy/build/install/package gate; "
            "incremental=validate the source tree using an existing build dir."
        ),
    )
    parser.add_argument(
        "--source",
        type=Path,
        default=None,
        help="Solver source root. Defaults to the parent of this script directory.",
    )
    parser.add_argument(
        "--work-dir",
        type=Path,
        default=None,
        help=(
            "Temporary work/output directory. Defaults to a fresh system temp "
            "directory for release mode and a reusable temp directory for "
            "incremental mode."
        ),
    )
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=None,
        help=(
            "Build directory for incremental mode. Defaults to <source>/build. "
            "Release mode always builds inside the copied package tree."
        ),
    )
    parser.add_argument(
        "--build-target",
        action="append",
        default=[],
        help=(
            "Build only this target, repeatable. The bbsolver CLI target is "
            "always added because policies/examples need it."
        ),
    )
    parser.add_argument(
        "--ctest-regex",
        default=None,
        help="Optional CTest -R regex for focused incremental test runs.",
    )
    parser.add_argument(
        "--ctest-label",
        action="append",
        default=[],
        help="Optional CTest -L label filter, repeatable.",
    )
    parser.add_argument(
        "--ctest-exclude-label",
        action="append",
        default=[],
        help="Optional CTest -LE label filter, repeatable.",
    )
    parser.add_argument(
        "--include-slow",
        action="store_true",
        help=(
            "Incremental mode only: include tests labeled slow by disabling "
            "the default incremental slow-test exclusion. Release mode does "
            "not auto-exclude slow tests."
        ),
    )
    parser.add_argument(
        "--jobs",
        type=str,
        default=str(os.cpu_count() or 8),
        help="Parallel jobs for cmake --build and ctest.",
    )
    parser.add_argument(
        "--build-type",
        default="Release",
        help="CMake build type/configuration to validate.",
    )
    parser.add_argument(
        "--generator",
        default=None,
        help="Optional CMake generator, for example Ninja or Visual Studio.",
    )
    parser.add_argument(
        "--use-remote-deps",
        action="store_true",
        help=(
            "Allow normal dependency resolution in release mode instead of "
            "forcing the shipped third_party/archive mirror. Incremental mode "
            "uses normal dependency resolution by default."
        ),
    )
    parser.add_argument(
        "--force-local-deps",
        action="store_true",
        help=(
            "Force the shipped third_party/archive mirror. Release mode does "
            "this by default; incremental mode does not unless this flag is set."
        ),
    )
    parser.add_argument(
        "--skip-configure",
        action="store_true",
        help=(
            "Incremental mode only: reuse an existing configured build "
            "directory and go straight to build/test commands."
        ),
    )
    parser.add_argument(
        "--skip-clangd",
        action="store_true",
        help="Skip the optional clangd diagnostic sweep.",
    )
    parser.add_argument(
        "--clangd-jobs",
        type=int,
        default=None,
        help=(
            "Parallel clangd --check jobs. Defaults to --jobs when that value "
            "is numeric, otherwise the local CPU count."
        ),
    )
    parser.add_argument(
        "--clangd-scope",
        choices=("auto", "full", "changed", "off"),
        default="auto",
        help=(
            "clangd coverage. auto means full in release mode and changed "
            "files only in incremental mode."
        ),
    )
    parser.add_argument("--skip-ctest", action="store_true", help="Skip CTest.")
    parser.add_argument(
        "--skip-policies", action="store_true", help="Skip solver policy scripts."
    )
    parser.add_argument(
        "--skip-examples", action="store_true", help="Skip packaged JSON example probes."
    )
    parser.add_argument(
        "--with-install",
        action="store_true",
        help=(
            "In incremental mode, also run install, package-smoke, and install "
            "hygiene checks. Release mode always runs them."
        ),
    )
    parser.add_argument(
        "--keep-temp",
        action="store_true",
        help="Keep the temporary validation directory after the run.",
    )
    return parser.parse_args()


def find_solver_root(anchor: Path) -> Path:
    for candidate in (anchor, *anchor.parents):
        if (
            (candidate / "CMakeLists.txt").is_file()
            and (candidate / "include" / "bbsolver").is_dir()
            and (candidate / "src").is_dir()
        ):
            return candidate.resolve()
    raise RuntimeError(f"Could not locate solver root from {anchor}")


def should_exclude(name: str) -> bool:
    return any(fnmatch.fnmatch(name, pattern) for pattern in EXCLUDE_PATTERNS)


def copy_solver_tree(source: Path, destination: Path) -> None:
    def ignore(_directory: str, names: list[str]) -> set[str]:
        return {name for name in names if should_exclude(name)}

    shutil.copytree(source, destination, ignore=ignore)


def run(
    command: list[str],
    *,
    cwd: Path | None = None,
    env: dict[str, str] | None = None,
    allow_failure: bool = False,
    capture: bool = False,
) -> subprocess.CompletedProcess[str]:
    printable = " ".join(str(part) for part in command)
    if cwd:
        print(f"$ ({cwd}) {printable}", flush=True)
    else:
        print(f"$ {printable}", flush=True)
    completed = subprocess.run(
        [str(part) for part in command],
        cwd=str(cwd) if cwd else None,
        env=env,
        text=True,
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.PIPE if capture else None,
        check=False,
    )
    if completed.returncode != 0 and not allow_failure:
        if capture:
            if completed.stdout:
                print(completed.stdout, end="")
            if completed.stderr:
                print(completed.stderr, end="", file=sys.stderr)
        raise RuntimeError(f"Command failed with exit {completed.returncode}: {printable}")
    return completed


def build_solver(
    package_root: Path,
    build_dir: Path,
    build_type: str,
    jobs: str,
    generator: str | None,
    use_remote_deps: bool,
    build_targets: list[str] | None = None,
    skip_configure: bool = False,
) -> Path:
    configure = [
        "cmake",
        "-S",
        package_root,
        "-B",
        build_dir,
        f"-DCMAKE_BUILD_TYPE={build_type}",
        "-DBBSOLVER_BUILD_TESTS=ON",
    ]
    if not use_remote_deps:
        configure.extend(
            [
                "-DBBSOLVER_FORCE_THIRD_PARTY_ARCHIVES=ON",
                "-DBBSOLVER_ENABLE_REMOTE_PROBES=OFF",
            ]
        )
    if generator:
        configure.extend(["-G", generator])
    if skip_configure:
        if not (build_dir / "CMakeCache.txt").is_file():
            raise RuntimeError(
                f"--skip-configure requires an existing CMake build dir: {build_dir}"
            )
        print(f"[INFO] reusing configured build directory: {build_dir}")
    else:
        run(configure)
    if build_targets:
        targets = list(dict.fromkeys(["bbsolver", *build_targets]))
        for target in targets:
            run(
                [
                    "cmake",
                    "--build",
                    build_dir,
                    "--config",
                    build_type,
                    "--target",
                    target,
                    "--parallel",
                    jobs,
                ]
            )
    else:
        run(["cmake", "--build", build_dir, "--config", build_type, "--parallel", jobs])
    binary = resolve_built_bbsolver(build_dir, build_type)
    run([binary, "--version"])
    return binary


def resolve_built_bbsolver(build_dir: Path, build_type: str) -> Path:
    names = ["bbsolver.exe", "bbsolver"] if platform.system() == "Windows" else ["bbsolver", "bbsolver.exe"]
    candidates = [
        build_dir / name for name in names
    ] + [
        build_dir / build_type / name for name in names
    ]
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    matches = [
        path
        for path in build_dir.rglob("bbsolver*")
        if path.is_file() and path.name in names
    ]
    if matches:
        return matches[0]
    raise RuntimeError(f"Could not find built bbsolver binary under {build_dir}")


def resolve_package_smoke_binary(build_dir: Path, build_type: str) -> Path:
    names = ["bbsolver_package_smoke.exe", "bbsolver_package_smoke"]
    candidates = [
        build_dir / name for name in names
    ] + [
        build_dir / build_type / name for name in names
    ]
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    matches = [
        path
        for path in build_dir.rglob("bbsolver_package_smoke*")
        if path.is_file() and path.name in names
    ]
    if matches:
        return matches[0]
    raise RuntimeError(f"Could not find package smoke binary under {build_dir}")


def run_ctest(
    build_dir: Path,
    build_type: str,
    jobs: str,
    regex: str | None = None,
    labels: list[str] | None = None,
    exclude_labels: list[str] | None = None,
) -> None:
    command = [
        "ctest",
        "--test-dir",
        build_dir,
        "-C",
        build_type,
        "--output-on-failure",
        "-j",
        jobs,
    ]
    if regex:
        command.extend(["-R", regex])
    for label in labels or []:
        command.extend(["-L", label])
    for label in exclude_labels or []:
        command.extend(["-LE", label])
    run(command)


def run_policies(package_root: Path, binary: Path) -> None:
    env = os.environ.copy()
    env["BBSOLVER_TEST_BINARY"] = str(binary)
    for policy in sorted((package_root / "tests" / "policies").glob("*_policy.py")):
        run([sys.executable, policy], cwd=package_root, env=env)


def install_solver(build_dir: Path, install_dir: Path, build_type: str) -> None:
    run(["cmake", "--install", build_dir, "--config", build_type, "--prefix", install_dir])


def seed_stale_generated_namespace(build_dir: Path) -> None:
    stale_dir = build_dir / "generated" / "(alternative)"
    stale_dir.mkdir(parents=True, exist_ok=True)
    (stale_dir / "stale_generated.h").write_text(
        "// This sentinel must not be installed by the bbsolver package.\n",
        encoding="utf-8",
    )


def validate_package_smoke(
    package_root: Path,
    install_dir: Path,
    smoke_build_dir: Path,
    build_type: str,
    jobs: str,
    generator: str | None,
) -> None:
    if smoke_build_dir.exists():
        shutil.rmtree(smoke_build_dir)
    configure = [
        "cmake",
        "-S",
        package_root / "tests" / "package_smoke",
        "-B",
        smoke_build_dir,
        f"-DCMAKE_PREFIX_PATH={install_dir}",
        "-DBBSOLVER_FORCE_BUNDLED_DEPS=ON",
    ]
    if generator:
        configure.extend(["-G", generator])
    run(configure)
    run(["cmake", "--build", smoke_build_dir, "--config", build_type, "--parallel", jobs])
    run([resolve_package_smoke_binary(smoke_build_dir, build_type)])


def validate_examples(package_root: Path, binary: Path, output_dir: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    sample_paths = sorted((package_root / "examples" / "json").glob("*.bbsm.json"))
    if not sample_paths:
        raise RuntimeError("No packaged JSON examples found")

    first_key: Path | None = None
    first_sample: Path | None = None
    for sample in sample_paths:
        stem = sample.name.removesuffix(".bbsm.json")
        key_path = output_dir / f"{stem}.bbky.json"
        run([binary, "solve", sample, key_path, "--jobs", "1"])
        run([binary, "verify", key_path, sample])
        run([binary, "dump", sample], capture=True)
        run([binary, "dump", key_path], capture=True)
        if first_key is None:
            first_key = key_path
            first_sample = sample

    assert first_key is not None
    assert first_sample is not None
    expect_failure(
        [binary, "verify", first_sample, first_key],
        "swapped verify arguments should be rejected as a format error",
        expected_codes={1},
    )
    arbitrary_json = output_dir / "not_a_bundle.json"
    arbitrary_json.write_text(
        json.dumps({"_schema": "samples", "schema_version": 1, "properties": "not a list"}),
        encoding="utf-8",
    )
    expect_failure(
        [binary, "dump", arbitrary_json],
        "dump should reject arbitrary JSON that is not a valid bundle",
        expected_codes={1},
    )
    missing_schema = output_dir / "missing_schema_version.json"
    missing_schema.write_text(json.dumps({"_schema": "samples"}), encoding="utf-8")
    expect_failure(
        [binary, "dump", missing_schema],
        "dump should reject bundles without integer schema_version",
        expected_codes={1},
    )


def expect_failure(command: list[object], reason: str, expected_codes: set[int]) -> None:
    completed = run(command, allow_failure=True, capture=True)
    if completed.returncode not in expected_codes:
        if completed.stdout:
            print(completed.stdout, end="")
        if completed.stderr:
            print(completed.stderr, end="", file=sys.stderr)
        raise RuntimeError(
            f"{reason}; expected exit {sorted(expected_codes)}, got {completed.returncode}"
        )
    print(f"[PASS] {reason} (exit {completed.returncode})")


def validate_install_tree(install_dir: Path, source_root: Path) -> None:
    forbidden_tokens = {
        "baker" + "boy",
        "baker" + "Boy",
        "Baker" + "Boy",
        "BAKER" + "BOY",
        str(source_root),
    }
    historical_text_allowlist = {
        "share/doc/bbsolver/CHANGELOG.md",
    }
    findings: list[str] = []
    for path in install_dir.rglob("*"):
        rel = path.relative_to(install_dir).as_posix()
        if path.name == ".DS_Store":
            findings.append(f"{rel}: .DS_Store should not be installed")
        for token in forbidden_tokens:
            if token and token in rel:
                findings.append(f"{rel}: path contains forbidden token {token!r}")
        if not path.is_file() or path.suffix not in TEXT_INSTALL_EXTENSIONS:
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        if rel in historical_text_allowlist:
            continue
        for token in forbidden_tokens:
            if token and token in text:
                findings.append(f"{rel}: text contains forbidden token {token!r}")
    if findings:
        raise RuntimeError("Install tree hygiene failures:\n" + "\n".join(findings))


def resolve_worker_count(raw_jobs: str, override: int | None) -> int:
    if override is not None:
        return max(1, override)
    try:
        return max(1, int(raw_jobs))
    except ValueError:
        return max(1, os.cpu_count() or 8)


def run_clangd_paths(
    package_root: Path,
    paths: list[Path],
    label: str,
    jobs: int,
) -> None:
    clangd = shutil.which("clangd")
    if not clangd:
        print("[SKIP] clangd not found on PATH")
        return
    paths = sorted(path for path in paths if path.is_file())
    if not paths:
        print(f"[SKIP] clangd {label}: no matching files")
        return

    worker_count = min(max(1, jobs), len(paths))
    print(
        f"[INFO] clangd {label}: checking {len(paths)} files "
        f"with {worker_count} jobs"
    )
    failures: list[str] = []

    def check_one(path: Path) -> tuple[Path, int, str]:
        completed = subprocess.run(
            [clangd, f"--check={path}", "--tweaks=0"],
            cwd=package_root,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        return path, completed.returncode, (
            (completed.stdout or "") + (completed.stderr or "")
        )

    with ThreadPoolExecutor(max_workers=worker_count) as executor:
        futures = [executor.submit(check_one, path) for path in paths]
        for future in as_completed(futures):
            path, returncode, output = future.result()
            if returncode == 0:
                continue
            tail = "\n".join(output.splitlines()[-8:])
            failures.append(
                path.relative_to(package_root).as_posix() +
                (f"\n{tail}" if tail else "")
            )
    if failures:
        failures.sort()
        raise RuntimeError(f"clangd {label} failed for:\n" + "\n".join(failures))
    print(f"[PASS] clangd {label} checked {len(paths)} files")


def run_clangd_sweep(package_root: Path, jobs: int) -> None:
    public_sources = list((package_root / "include" / "bbsolver").rglob("*.hpp"))
    cpp_sources = list((package_root / "src").rglob("*.cpp"))
    test_sources = list((package_root / "tests" / "solver_unit").rglob("*.cpp"))
    smoke_source = package_root / "tests" / "package_smoke" / "main.cpp"
    run_clangd_paths(
        package_root,
        public_sources + cpp_sources + test_sources + [smoke_source],
        "sweep",
        jobs,
    )


def git_root_for(path: Path) -> Path | None:
    completed = subprocess.run(
        ["git", "rev-parse", "--show-toplevel"],
        cwd=path,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    if completed.returncode != 0:
        return None
    return Path(completed.stdout.strip()).resolve()


def git_changed_files_for(source_root: Path) -> list[Path]:
    git_root = git_root_for(source_root)
    if git_root is None:
        return []
    commands = [
        ["git", "diff", "--name-only", "--diff-filter=ACMR"],
        ["git", "diff", "--cached", "--name-only", "--diff-filter=ACMR"],
        ["git", "ls-files", "--others", "--exclude-standard"],
    ]
    changed: set[Path] = set()
    for command in commands:
        completed = subprocess.run(
            command,
            cwd=git_root,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            check=False,
        )
        if completed.returncode != 0:
            continue
        for line in completed.stdout.splitlines():
            path = (git_root / line).resolve()
            try:
                path.relative_to(source_root)
            except ValueError:
                continue
            if path.suffix in {".c", ".cc", ".cpp", ".cxx", ".h", ".hpp"}:
                changed.add(path)
    return sorted(changed)


def main() -> int:
    args = parse_args()
    if args.use_remote_deps and args.force_local_deps:
        raise RuntimeError("--use-remote-deps and --force-local-deps are mutually exclusive")
    if args.skip_configure and args.mode != "incremental":
        raise RuntimeError("--skip-configure is only valid in incremental mode")

    source_root = find_solver_root(
        args.source.resolve() if args.source else Path(__file__).resolve().parent.parent
    )
    if args.skip_clangd:
        args.clangd_scope = "off"

    created_temp = False
    if args.work_dir:
        work_dir = args.work_dir.resolve()
        work_dir.mkdir(parents=True, exist_ok=True)
    elif args.mode == "incremental":
        work_dir = Path(tempfile.gettempdir()) / "bbsolver-standalone-incremental"
        work_dir.mkdir(parents=True, exist_ok=True)
    else:
        work_dir = Path(tempfile.mkdtemp(prefix="bbsolver-standalone-validate-"))
        created_temp = True

    package_root = work_dir / "bbsolver" if args.mode == "release" else source_root
    build_dir = (
        package_root / "build"
        if args.mode == "release"
        else (args.build_dir.resolve() if args.build_dir else package_root / "build")
    )
    install_dir = work_dir / "install"
    smoke_build_dir = work_dir / "package-smoke-build"
    output_dir = work_dir / "example-output"

    try:
        print(f"[INFO] source:  {source_root}")
        print(f"[INFO] work:    {work_dir}")
        print(f"[INFO] mode:    {args.mode}")
        use_remote_deps = args.use_remote_deps
        if args.mode == "incremental" and not args.force_local_deps:
            use_remote_deps = True
        if args.force_local_deps:
            use_remote_deps = False
        print(
            "[INFO] deps:    " +
            ("normal resolution" if use_remote_deps else "local archives")
        )
        if args.mode == "release":
            copy_solver_tree(source_root, package_root)
        binary = build_solver(
            package_root,
            build_dir,
            args.build_type,
            args.jobs,
            args.generator,
            use_remote_deps,
            args.build_target,
            args.skip_configure,
        )
        if not args.skip_ctest:
            ctest_exclude_labels = list(args.ctest_exclude_label)
            if (
                args.mode == "incremental"
                and not args.include_slow
                and "slow" not in args.ctest_label
                and "slow" not in ctest_exclude_labels
            ):
                ctest_exclude_labels.append("slow")
            if ctest_exclude_labels:
                print(
                    "[INFO] ctest exclude labels: " +
                    ", ".join(ctest_exclude_labels)
                )
            run_ctest(
                build_dir,
                args.build_type,
                args.jobs,
                args.ctest_regex,
                args.ctest_label,
                ctest_exclude_labels,
            )
        if not args.skip_policies:
            run_policies(package_root, binary)
        run_install_checks = args.mode == "release" or args.with_install
        if run_install_checks:
            seed_stale_generated_namespace(build_dir)
            install_solver(build_dir, install_dir, args.build_type)
            validate_package_smoke(
                package_root,
                install_dir,
                smoke_build_dir,
                args.build_type,
                args.jobs,
                args.generator,
            )
            validate_install_tree(install_dir, source_root)
        if not args.skip_examples:
            validate_examples(package_root, binary, output_dir)
        clangd_scope = args.clangd_scope
        if clangd_scope == "auto":
            clangd_scope = "full" if args.mode == "release" else "changed"
        clangd_jobs = resolve_worker_count(args.jobs, args.clangd_jobs)
        if clangd_scope == "full":
            run_clangd_sweep(package_root, clangd_jobs)
        elif clangd_scope == "changed":
            run_clangd_paths(
                package_root,
                git_changed_files_for(package_root),
                "changed-file sweep",
                clangd_jobs,
            )
        print("[PASS] bbsolver standalone package validation completed")
        return 0
    finally:
        if args.keep_temp:
            print(f"[INFO] kept work directory: {work_dir}")
        elif created_temp:
            shutil.rmtree(work_dir, ignore_errors=True)


if __name__ == "__main__":
    raise SystemExit(main())
