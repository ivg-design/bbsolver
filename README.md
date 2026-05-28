# bbsolver

`bbsolver` is a standalone C++ spatiotemporal optimization engine for
animation data. It reads dense sampled motion or path data, fits sparse
keyframes within an explicit error budget, optimizes animated paths through
spatial and temporal fitting plus vertex decimation, bakes parented transforms,
works with fixed and variable-topology paths, and can smooth keyframed layer
motion paths or animated shape paths while preserving sharp points and
keyed-frame constraints.

The package also ships a small ExtendScript/ScriptUI After Effects test
harness as an integration example: it samples AE properties into
solver-ready bundles, invokes the `bbsolver` CLI, receives solved bundles,
and writes the resulting keys back into AE.

## Use Cases

`bbsolver` is useful when animation data is dense, expression-driven,
parented, or path-heavy, and the result still needs to come back as editable
keyframes. These public examples demonstrate the kinds of inputs and outputs
the solver is designed around.

### Expression Bake + Vertex Optimization

Expression-driven stable-topology path animation is optimized while preserving
the path shape within tolerance.

https://github.com/user-attachments/assets/e62d6b2d-596d-49f2-9ad0-23fe7d8ec362

### Rig Bake + Unparent

Parented/rigged Position motion is sampled in context, solved, written back,
and unparented.

https://github.com/user-attachments/assets/7e1d18fc-d231-4255-a7a0-697895b3fda1

### Motion Sketch Cleanup

Dense hand-drawn Position samples become a cleaner keyed motion path.

https://github.com/user-attachments/assets/4ab8c257-d922-46af-b74b-67a6179a8747

### What to Look For

These clips show the integration problems `bbsolver` is built to solve:
dense samples become sparse editable keys, expression and rig output can be
baked into ordinary host keyframes, and path-heavy animations can be reduced
without changing the intended shape beyond the configured tolerance.

## Benchmark whitepaper and reproducibility corpus

The full technical report on `bbsolver`, with head-to-head accuracy and key-count
comparisons against three widely-used open-source baselines, ships in this
repository at
[`benchmarks/01_arxiv_full_technical_report/`](benchmarks/01_arxiv_full_technical_report/).
It includes:

- the report itself
  ([`bbsolver_arxiv_full_technical_report_v3.md`](benchmarks/01_arxiv_full_technical_report/bbsolver_arxiv_full_technical_report_v3.md))
  and every figure it cites under `figures/`;
- the **paper-figure reproducibility corpus** at
  [`data/paper_corpus/`](benchmarks/01_arxiv_full_technical_report/data/paper_corpus/) —
  raw `bbsm` / `bbky` / `verify` / `progress.log` bundles for all 11 cited
  solves (~62 MB), indexed by [`corpus_manifest.csv`](benchmarks/01_arxiv_full_technical_report/data/paper_corpus/corpus_manifest.csv);
- the **supplementary CSVs** at
  [`data/supplementary/`](benchmarks/01_arxiv_full_technical_report/data/supplementary/) —
  one CSV per quantitative table or figure in the Results section;
- the **After Effects benchmark project** at
  [`benchmarks/after_effects_benchmark_project/bbSolver_benchmarking.aep`](benchmarks/after_effects_benchmark_project/bbSolver_benchmarking.aep)
  containing every rig and procedural setup cited in the paper (DUIK
  humanoid, ant hexapod, FK noodle, v1-v6 blob lineage, CS1/CS2 path
  fixtures). *Opening or re-running this .aep requires a valid Adobe After
  Effects 2024+ license — Adobe AE is commercial software and we cannot
  redistribute the application itself, only the project file the harness
  drives. The serialized SampleBundles in `data/paper_corpus/` (next item)
  are the AE-independent path: they capture the sampled comp state and can
  be re-solved with just `bbsolver` on any platform.*
- the **standalone-Python ports** of two open-source baselines under
  [`external_runners/`](benchmarks/01_arxiv_full_technical_report/external_runners/)
  (`joosten_reducer/` and `toolchefs_reducer/`), each with upstream
  provenance preserved for diffability;
- the deterministic figure-generation pipeline at
  [`scripts/`](benchmarks/01_arxiv_full_technical_report/scripts/).

The solver build that produced every `bbky.json` in the corpus is `bbsolver
1.0.0`, tag `v1.0.0` of this repository. Each `bbky.json` records its own
`solver_version` and `solver_build` for cross-check against any future build.
The canonical `verify.json` files shipped under `data/paper_corpus/` were
regenerated with `bbsolver 1.0.1` (a verifier-side bug fix for variable-topology
`shape_flat` bundles; same solver, no numeric change). v1.0.1 is the recommended
binary for reproducing `verify.json` from scratch — see
[`THRESHOLD_NOTE.md`](benchmarks/01_arxiv_full_technical_report/data/paper_corpus/THRESHOLD_NOTE.md)
for the diagnosis and §8.1 of the
[benchmark report](benchmarks/01_arxiv_full_technical_report/bbsolver_arxiv_full_technical_report_v3.md)
for the SHA-256 manifest of both releases.

Per-row exact runtime, key-count reduction, max-error, memory, and
job-count determinism numbers are in the supplementary CSVs and the paper
itself; nothing is "TBD". Every numerical claim is **auditable from
public artifacts** in this repository, and **regenerable where the
required inputs are available**: solver-only rows (in-loop max_err, key
counts, canonical CLI verify) can be re-derived from the public
`bbsm` / `bbky` corpus; full AE / Illustrator host round trips require
the corresponding licensed applications; full raw regeneration of the
203-run production corpus requires the private original `live_runs`
folders, while the shipped per-run and summary CSVs make the aggregate
publicly auditable.

> **Integration surface.** The supported integration is the CLI process
> boundary plus the JSON SampleBundle/KeyBundle schemas. The CMake
> package additionally exports `bbsolver::bbsolver` (the CLI) and
> `bbsolver::core` (a static library for in-tree embedding and tests) —
> see [`docs/PACKAGING.md`](docs/PACKAGING.md). FlatBuffers schemas under
> [`protocol/`](protocol/) are design references; today the CLI exchanges
> JSON only. See [`docs/DEVELOPER_GUIDE.md`](docs/DEVELOPER_GUIDE.md) §11
> for the full public/private boundary discussion.

**Current version:** `bbsolver 1.0.1` (solver behaviour unchanged from `v1.0.0`; verifier-side bug fix for variable-topology `shape_flat`). License: [MIT](LICENSE).

## Repository model

`ivg-design/bbsolver` is the public standalone release/export repository.
During the current transition, active solver development is staged in the
`solver/` subtree of the integration repository and exported here as the
repository root. Public issues and pull requests are welcome in the standalone
repository; maintainers may route core solver patches back through the
integration subtree before the next export so the two trees stay identical.

See [`docs/REPOSITORY_SYNC.md`](docs/REPOSITORY_SYNC.md) for the export
workflow, [`docs/ROADMAP.md`](docs/ROADMAP.md) for near-term priorities, and
[`docs/MAINTAINERS.md`](docs/MAINTAINERS.md) for maintainer policy.

## Pre-built binaries

Both `v1.0.0` and `v1.0.1` ship pre-built binaries for **macOS arm64**, **macOS
x86_64**, **Windows arm64**, and **Windows x64** as release assets, each with
its own `SHA256SUMS.txt`. `v1.0.1` is the recommended download for reproducing
the paper's canonical CLI verify pass (it fixes a verifier-side strict-dim
check that had rejected the six variable-topology `shape_flat` rows of the
corpus). `v1.0.0` remains available unchanged for byte-level reproduction of
the original solver outputs. Binaries are **not** Apple-notarized /
Authenticode-signed, so Gatekeeper and SmartScreen may warn on first run —
see §8.1 of the
[benchmark report](benchmarks/01_arxiv_full_technical_report/bbsolver_arxiv_full_technical_report_v3.md)
for the full chain-of-custody discussion and verify commands.

```sh
# macOS / Linux: download + integrity-check (v1.0.1 canonical)
gh release download v1.0.1 --repo ivg-design/bbsolver \
  --pattern 'bbsolver-v1.0.1-macos-arm64.tar.gz' --pattern 'SHA256SUMS.txt'
shasum -a 256 -c SHA256SUMS.txt --ignore-missing
```

## Quickstart

A five-minute build + solve + verify path is in
[`docs/QUICKSTART.md`](docs/QUICKSTART.md). The condensed form (macOS /
Linux):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

./build/bbsolver solve \
  examples/json/minimal_scalar.bbsm.json \
  /tmp/minimal_scalar.bbky.json
./build/bbsolver verify \
  /tmp/minimal_scalar.bbky.json \
  examples/json/minimal_scalar.bbsm.json
```

Windows (PowerShell, MSVC):

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j

.\build\Release\bbsolver.exe solve `
  examples\json\minimal_scalar.bbsm.json `
  $env:TEMP\minimal_scalar.bbky.json
.\build\Release\bbsolver.exe verify `
  $env:TEMP\minimal_scalar.bbky.json `
  examples\json\minimal_scalar.bbsm.json
```

Three runnable JSON examples live under
[`examples/json/`](examples/json/) — see
[`examples/json/README.md`](examples/json/README.md) for the full smoke
loop and per-bundle expectations.

## What it does

- Reduces sampled animation to fewer keys within an explicit error
  tolerance.
- Preserves AE interpolation semantics: hold, linear, Bezier, temporal
  ease, spatial tangents, and roving flags where applicable.
- Solves ordinary scalar/vector properties, separated Position streams,
  Shape Path data, and advanced path-replacement flows.
- Provides a dedicated `motion_path_smooth` mode for smoothing
  Position-style motion paths while optionally preserving sharp turns and
  keyed frames.
- Emits progress, cancellation, diagnostics, and notes for host
  applications.

`bbsolver` is not a sample decimator. Its normal solve path accepts output
only when reconstructed values stay inside the configured accuracy budget.

## Repository layout

```text
bbsolver/
|-- CMakeLists.txt
|-- CMakePresets.json
|-- README.md
|-- LICENSE
|-- CONTRIBUTING.md
|-- CHANGELOG.md
|-- .github/workflows/     # Standalone CI
|-- protocol/              # FlatBuffers schemas used to generate headers
|-- schemas/               # JSON Schema files for SampleBundle/KeyBundle
|-- examples/
|   |-- after-effects/      # Minimal AE ScriptUI integration harness
|   `-- json/               # Runnable non-AE JSON SampleBundle examples
|-- include/bbsolver/       # Source-visible C++ headers and SDK entry points
|-- src/                    # Private implementation, mirroring public areas
|-- tests/solver_unit/      # C++ unit tests built by CMake
|-- tests/policies/         # Solver-owned source-level policy checks
|-- docs/                   # User, developer, and integration documentation
|-- cmake/                  # bbsolverConfig.cmake.in template
|-- scripts/                # Standalone validation tooling
|-- SECURITY.md             # Security policy
`-- third_party/            # Hash-locked dependency archives + notices
```

## Requirements

| | Minimum |
|---|---|
| CMake | `3.20` |
| C++ standard | `C++17` |
| macOS | 12+ (Intel and Apple Silicon, tested on `macos-14`) |
| Linux | any glibc-based x86_64 distro with the toolchain below (tested on `ubuntu-latest`) |
| Windows | Windows 10+ x64 with Visual Studio 2022 / MSVC (tested on `windows-latest`) |
| Compilers tested | AppleClang on macOS, GCC ≥ 11 on Linux, MSVC 19.36+ on Windows |
| Network access | optional — only used by `FetchContent` for pinned dependency downloads. Offline builds work via `-DBBSOLVER_FORCE_THIRD_PARTY_ARCHIVES=ON`. |

The build vendors Ceres, Eigen, oneTBB, FlatBuffers, and nlohmann/json
via `FetchContent` with hash-locked fallback archives shipped under
`third_party/archive/` (see
[`third_party/THIRD_PARTY_NOTICES.md`](third_party/THIRD_PARTY_NOTICES.md)).
Plan for several minutes of first-time build because the dependencies
build from source.

## Build

From the package root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/bbsolver --version
./build/bbsolver --help
```

On Windows (PowerShell, MSVC):

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
.\build\Release\bbsolver.exe --help
```

To install into a prefix (CLI, headers, schemas, docs, and CMake
package config):

```sh
cmake --install build --prefix /path/to/bbsolver-install
```

Consumers can then `find_package(bbsolver CONFIG)` and link
`bbsolver::core` or invoke `bbsolver::bbsolver`. The installed package
first uses dependency package configs already available to the consumer, then
falls back to the installed hash-locked archives under
`share/bbsolver/third_party/archive`. See
[`docs/PACKAGING.md`](docs/PACKAGING.md) for the install tree layout,
exported targets, and package smoke test.

To build tests:

```sh
cmake -S . -B build -DBBSOLVER_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure -j 8
```

For fast edit/test loops, use the solver-local presets and incremental
validator documented in [`docs/VALIDATION_WORKFLOWS.md`](docs/VALIDATION_WORKFLOWS.md).

To run the standalone release gate from a clean package copy:

```sh
python3 scripts/validate_standalone_package.py
```

That command copies the package to a temporary standalone root, builds with
the shipped third-party archive mirror, runs all C++ tests and solver-owned
policies, installs the package, validates `find_package(bbsolver CONFIG)`
from the external package-smoke project, exercises every packaged JSON
example, and runs a clangd sweep when `clangd` is available.

Useful dependency flags:

```sh
cmake -S . -B build -DBBSOLVER_FORCE_THIRD_PARTY_ARCHIVES=ON
cmake -S . -B build -DBBSOLVER_THIRD_PARTY_ARCHIVE_FALLBACK=OFF
cmake -S tests/package_smoke -B build-package-smoke \
  -DCMAKE_PREFIX_PATH=/path/to/bbsolver-install \
  -DBBSOLVER_FORCE_BUNDLED_DEPS=ON
```

To validate JSON bundles before invoking the solver:

```sh
python3 scripts/validate_json_bundle.py examples/json/minimal_scalar.bbsm.json
```

The JSON Schema files live in [`schemas/`](schemas/) and are installed under
`share/bbsolver/schemas`.

By default CMake first uses installed packages, then pinned upstream
downloads, then the hash-checked archives under `third_party/archive` if
fallback is enabled and a remote download is unavailable. The full
dependency policy, license obligations, and update process are documented
in
[`third_party/THIRD_PARTY_NOTICES.md`](third_party/THIRD_PARTY_NOTICES.md).

## CLI overview

```sh
bbsolver solve input.bbsm.json output.bbky.json \
  --tolerance 0.5 \
  --screen-px 1.0 \
  --jobs 0

bbsolver verify output.bbky.json input.bbsm.json
bbsolver dump input.bbsm.json
bbsolver --version
```

`solve`, `verify`, and `dump` accept JSON bundles only and reject
non-`.json` inputs with a clear error.

Common solve modes:

- `full`: normal temporal and eligible path optimization.
- `temporal_only`: key reduction without path topology changes.
- `vertex_only`: eligible path vertex reduction without temporal fitting.
- `motion_smooth`: smooth source key timing/trajectory using the
  motion-smooth path.
- `motion_path_smooth`: smooth Position-style motion paths with separate
  motion-path tolerance controls.

See the docs below for option-level behavior. The full CLI contract,
exit codes, and process-boundary semantics are in
[`docs/SOLVER_CLI.md`](docs/SOLVER_CLI.md).

## Public/private API surface

| Surface | Status |
| --- | --- |
| CLI subcommands (`solve`, `verify`, `dump`, `--version`) | **stable** |
| Exit codes documented in `docs/SOLVER_CLI.md` | **stable** |
| JSON SampleBundle (`*.bbsm.json`) shape | **stable** (`schema_version=1`) |
| JSON KeyBundle (`*.bbky.json`) shape | **stable** (`schema_version=1`) |
| Progress JSON events (`--progress-fd`) | **stable** |
| Diagnostics JSON events (`--diagnostics`) | **stable** |
| CMake package (`bbsolver::bbsolver`, `bbsolver::core`, `install`/`find_package`) | **stable** — see [`docs/PACKAGING.md`](docs/PACKAGING.md) |
| `bbsolver::core` C++ symbol surface beyond the three `Run*` entry points | source-visible; not part of the SDK contract |
| `--decompose-paths`, `--fit-canonical-paths`, `--fit-replacement-paths`, `--emit-landmark-subpaths` | advanced, off by default — see [`docs/PATH_HANDLING.md`](docs/PATH_HANDLING.md) |
| FlatBuffers binary IO | design reference under [`protocol/`](protocol/); CLI is JSON-only |

Some headers under `include/bbsolver/` are explicitly internal even though
they live in the public include tree — see
[`docs/DEVELOPER_GUIDE.md`](docs/DEVELOPER_GUIDE.md) §11 for the
public/private boundary discussion. The three command entry points
(`RunSolve`, `RunVerifyCommand`, `RunDumpCommand`) and the schema-pinned
bundle structs in `domain.hpp` are the supported C++ surface for in-tree
embedding.

## Documentation

- [Quickstart](docs/QUICKSTART.md): build + solve + verify in five
  minutes.
- [Packaging](docs/PACKAGING.md): CMake install tree, exported targets,
  and `find_package(bbsolver CONFIG)` consumption.
- [User Guide](docs/USER_GUIDE.md): modes, settings, workflows, and
  expected behavior.
- [API Reference](docs/API_REFERENCE.md): concise CLI, IO schema, exit
  code, progress, diagnostics, and integration reference.
- [CLI Contract](docs/SOLVER_CLI.md): process boundary, commands, flags,
  and exit codes.
- [Developer Guide](docs/DEVELOPER_GUIDE.md): API surface, architecture,
  extension points, diagnostics, and integration notes.
- [AE ScriptUI Test Harness](docs/AE_SCRIPTUI_HARNESS.md): After Effects
  sampling, bundle IO, CLI solve, logging, and writeback example.
- [Tuning Guide](docs/TUNING_GUIDE.md): tolerance and mode guidance.
- [DP Algorithm](docs/DP_ALGORITHM.md): key-placement algorithm notes.
- [Path Handling](docs/PATH_HANDLING.md): Shape Path strategy and notes.
- [Test Catalog](docs/TEST_CATALOG.md): solver test families, run
  commands, and valid/invalid output expectations.
- [Validation Workflows](docs/VALIDATION_WORKFLOWS.md): fast focused,
  incremental package, package-smoke, and clean release validation loops.
- [Release Process](docs/RELEASE_PROCESS.md): maintainer checklist for version
  bumps, public CI, release-validation, tags, and GitHub Releases.
- [Repository Sync](docs/REPOSITORY_SYNC.md): maintainer workflow for
  exporting `solver/` into the public standalone repository.
- [Roadmap](docs/ROADMAP.md): public near-term and longer-term work.
- [Maintainers](docs/MAINTAINERS.md): triage, support, release, and security
  ownership policy.
- [JSON examples](examples/json/README.md): runnable, non-AE JSON
  SampleBundles + smoke loop.
- [Third-party archives](third_party/README.md) and
  [Third-party notices](third_party/THIRD_PARTY_NOTICES.md): dependency
  fallback policy and provenance/license obligations.
- [Changelog](CHANGELOG.md): solver-facing changes.
- [Security policy](SECURITY.md): supported versions, vulnerability
  reporting channel, and scope.

See [LICENSE](LICENSE) for the license text.

## Project

Issues, feature requests, and documentation bugs belong on this repository's
issue tracker — see [`CONTRIBUTING.md`](CONTRIBUTING.md) for bug-report and
pull-request guidance. For suspected security issues, use the private channel
in [`SECURITY.md`](SECURITY.md); do not file public issues for vulnerability
details.

## Contributing

Contributions are welcome. Start with [CONTRIBUTING.md](CONTRIBUTING.md).
In short:

- Keep public headers under `include/bbsolver/`.
- Keep implementation code under `src/<area>/`.
- Add or update focused tests for behavior changes.
- Preserve deterministic output across `--jobs 1` and parallel jobs.
- Keep diagnostics and progress ownership at the orchestration boundary.
- Do not vendor or update dependencies without pinning versions and
  hashes — see
  [`third_party/THIRD_PARTY_NOTICES.md`](third_party/THIRD_PARTY_NOTICES.md)
  for the full update process.
