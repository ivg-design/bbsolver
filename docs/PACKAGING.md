# bbsolver Packaging

This document describes the standalone CMake package boundary for `bbsolver`.
It is intentionally limited to build, install, and package-consumer behavior;
CLI command semantics remain documented in `SOLVER_CLI.md`.

## Build and install

From the package root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cmake --install build --prefix /path/to/bbsolver-install
```

The install tree contains:

- `bin/bbsolver`: the command-line solver.
- `include/bbsolver/...`: public C++ headers.
- `include/bbsolver/*_generated.h`: generated FlatBuffers protocol headers.
- `share/bbsolver/protocol/*.fbs`: source FlatBuffers schemas.
- `share/bbsolver/examples/...`: JSON fixtures and host-integration examples,
  including the AE ScriptUI harness and JSON shim.
- `share/bbsolver/third_party/archive/...`: hash-locked dependency archives
  used by installed package fallback resolution.
- `share/doc/bbsolver/...`: docs, changelog, contributing guide, and license.
- `lib/cmake/bbsolver/`: package config, version, and exported targets.

## CMake targets

Consumers can load the package with:

```cmake
find_package(bbsolver CONFIG REQUIRED)
```

The package exports two namespaced targets:

- `bbsolver::bbsolver`: imported executable target for the CLI.
- `bbsolver::core`: static library target for tests or tightly controlled
  embedding.

The stable integration boundary is still the `bbsolver` process plus
`SampleBundle`/`KeyBundle` schemas. The `bbsolver::core` target is useful for
in-repo tests and trusted integrations, but its C++ symbol surface is
source-visible only and is not part of the SDK contract.

By default the package config resolves public dependencies in two stages:

1. It first uses dependency package configs already available to the consumer:
   `nlohmann_json`, `Eigen3`, `Ceres`, `TBB`, and `flatbuffers`.
2. If a package config is not available, it falls back to the installed
   hash-locked archives under `share/bbsolver/third_party/archive`.

Set `BBSOLVER_USE_BUNDLED_DEPS=OFF` before `find_package(bbsolver CONFIG)` to
require system/package-manager dependencies only. Set
`BBSOLVER_FORCE_BUNDLED_DEPS=ON` to skip CMake package-registry/system lookup
and prove that the installed archives are sufficient. Set
`BBSOLVER_FIND_DEPENDENCIES=OFF` only for target-name inspection tests that do
not link `bbsolver::core`.

## Protocol ownership

Standalone builds generate protocol headers from `protocol/`, not from any
parent repository directory. The package root owns the package boundary:

- configure-time generation uses `protocol/samples.fbs` and
  `protocol/keys.fbs`;
- generated headers install under `include/bbsolver/`;
- source schemas install under `share/bbsolver/protocol/`.

This keeps source distributions and installed packages self-contained.

## Package smoke test

The repository includes a minimal package-consumer project:

```sh
cmake -S tests/package_smoke -B build-package-smoke \
  -DCMAKE_PREFIX_PATH=/path/to/bbsolver-install \
  -DBBSOLVER_FORCE_BUNDLED_DEPS=ON
```

That default configuration validates that `find_package(bbsolver CONFIG)`
loads the exported targets and compiles/links a minimal executable against
`bbsolver::core`. It works with either system dependency package configs or the
installed dependency archives.

To inspect package target names without resolving/linking dependencies:

```sh
cmake -S tests/package_smoke -B build-package-smoke \
  -DCMAKE_PREFIX_PATH=/path/to/bbsolver-install \
  -DBBSOLVER_FIND_DEPENDENCIES=OFF \
  -DBBSOLVER_PACKAGE_SMOKE_LINK_CORE=OFF
```

## Release package validation

Before cutting a standalone release, run the package validator from the solver
root:

```sh
python3 scripts/validate_standalone_package.py
```

The validator creates a temporary standalone copy of the solver tree and then
checks the package as a fresh consumer would:

- configure and build with `BBSOLVER_BUILD_TESTS=ON`;
- force the shipped `third_party/archive` dependency mirror;
- run the full CTest suite;
- run every solver-owned policy under `tests/policies/`;
- install the package into a temporary prefix;
- configure, build, and run `tests/package_smoke` against the install tree;
- solve, verify, and dump every JSON example under `examples/json/`;
- confirm malformed or swapped bundles are rejected;
- scan installed text files for monorepo/product-name leaks;
- run a clangd `--check` sweep when `clangd` is available.

Use `--use-remote-deps` to validate the normal remote dependency path instead
of the forced local archive path, and `--keep-temp` when you need to inspect a
failed validation workspace.
