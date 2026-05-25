# Validation Workflows

This guide documents the repeatable CMake preset loops for day-to-day
`bbsolver` validation. The presets are intentionally solver-local and write to
`out/` build trees so they do not disturb the conventional `build/` directory
or the full standalone package validator.

For what each test suite covers and how to interpret pass/fail output, see
[`TEST_CATALOG.md`](TEST_CATALOG.md). For the install/`find_package` smoke
project that the `package-smoke` preset wraps, see
[`PACKAGING.md`](PACKAGING.md). For the final release gate, see
[`../scripts/validate_standalone_package.py`](../scripts/validate_standalone_package.py).

## Presets

List the available presets from the solver root:

```sh
cmake --list-presets
cmake --build --list-presets
ctest --list-presets
```

The presets are:

- `dev`: Debug build with `BBSOLVER_BUILD_TESTS=ON`.
- `focused-test`: RelWithDebInfo build for fast incremental target/test loops.
- `package-smoke`: Release build for `bbsolver` plus
  `test_package_smoke_source`.
- `release-validation`: Release build with tests enabled and dependency
  resolution forced through the shipped third-party archives.

All presets use Ninja for fast incremental builds and emit
`compile_commands.json` for editor and clangd tooling.
`tests/policies/solver_layout_policy.py` locks the standard preset
names, matching configure/build/test surfaces, `out/` build directories, and
the `/out/` ignore rule so local validation workflows do not drift silently.

## Fast Edit Loop

Use this when changing one focused module or a narrow policy surface:

```sh
cmake --preset focused-test
cmake --build --preset focused-test --target test_temporal_refit
ctest --preset focused-test -R '^test_temporal_refit$'
```

Replace `test_temporal_refit` with the CMake test target that covers the code
you touched. Add `bbsolver` to the build command when the CLI binary is part of
the validation:

```sh
cmake --build --preset focused-test --target bbsolver test_verify_dump_commands
ctest --preset focused-test -R '^test_verify_dump_commands$'
```

For package-facing work that still needs the standalone validator's CLI,
policy, example, or optional install checks, use incremental mode against an
existing build tree:

```sh
python3 scripts/validate_standalone_package.py --mode incremental \
  --skip-configure --build-dir build --ctest-label unit
```

Incremental mode reuses the configured build directory, checks only changed
files with `clangd` by default, uses normal dependency resolution by default,
and excludes tests labeled `slow` unless `--include-slow` is passed. Release
mode remains the clean full gate.

## Developer Sweep

Use this before handing off a normal implementation task:

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

This is still an incremental build. It is not a replacement for a clean
standalone package validation pass.

## Package Smoke

Use this when package or install-facing changes need a quick in-tree smoke
before the full standalone validator:

```sh
cmake --preset package-smoke
cmake --build --preset package-smoke
ctest --preset package-smoke
```

For a real installed-package consumer check, install the build tree into a
temporary prefix and configure the package-smoke project against that prefix:

```sh
cmake --install out/package-smoke --prefix /tmp/bbsolver-install
cmake -S tests/package_smoke -B out/package-smoke-consumer \
  -DCMAKE_PREFIX_PATH=/tmp/bbsolver-install \
  -DBBSOLVER_FORCE_BUNDLED_DEPS=ON
cmake --build out/package-smoke-consumer
```

## Release-Style Local Sweep

Use this when dependency archive resolution or release packaging is part of the
risk area:

```sh
cmake --preset release-validation
cmake --build --preset release-validation
ctest --preset release-validation
```

The release-style preset is intentionally local and incremental. The final
standalone release gate remains
[`scripts/validate_standalone_package.py`](../scripts/validate_standalone_package.py):

```sh
python3 scripts/validate_standalone_package.py
```

Run that validator before declaring a standalone package ready. It performs a
fresh package copy, forced archive dependency resolution, the full C++ test
suite including `slow` tests, policies, install/package smoke, packaged JSON
examples, install hygiene checks, and the full `clangd` sweep when available.
The clangd sweep runs in parallel; use `--clangd-jobs N` to cap it separately
from build/test parallelism when the machine is under load.
