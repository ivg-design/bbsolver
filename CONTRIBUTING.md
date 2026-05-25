# Contributing to bbsolver

`bbsolver` is a standalone open-source solver. This policy covers contributions
to the solver core, CLI, tests, examples, and documentation.

## License of contributions

By contributing to `bbsolver`, you agree that your contribution is licensed
under the MIT License in `LICENSE`.

If your employer or client owns rights to your work, get their approval before
submitting a contribution.

## Reporting issues

Functional bugs, feature requests, and documentation issues belong on this
repository's issue tracker. A useful report names the `bbsolver --version`
string, the host platform (macOS / Linux / Windows), the smallest reproducer
you can share (usually a `SampleBundle` JSON or a CLI invocation), the
observed behavior, and the expected behavior.

For suspected **security** issues, use the private channel described in
[`SECURITY.md`](SECURITY.md) instead — do not file public issues for
vulnerability details.

## Development setup

```sh
cmake -S . -B build -DBBSOLVER_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure -j 8
```

## Required checks

Run the smallest focused checks needed for your change, then the broader guard
before a release or integration commit.

Recommended focused loop:

```sh
cmake --build build --target bbsolver test_verify_dump_commands -j 8
ctest --test-dir build -R '^test_verify_dump_commands$' --output-on-failure
```

Use [`docs/VALIDATION_WORKFLOWS.md`](docs/VALIDATION_WORKFLOWS.md) for
presets and the incremental package validator. The quick loops exclude tests
labeled `slow`; run with `BBSOLVER_INCLUDE_SLOW=1` or
`scripts/validate_standalone_package.py --mode incremental --include-slow`
when your change touches slow-path coverage.

For documentation-only changes, at least run:

```sh
git diff --check
```

Before a standalone release, run the full package gate:

```sh
python3 scripts/validate_standalone_package.py
```

It validates the solver from a temporary standalone copy, using the shipped
third-party archive mirror, the full C++ test suite, solver-owned source
policies, package install/smoke tests, JSON examples, negative bundle checks,
install hygiene checks, and clangd when available.

Maintainers should also follow [`docs/RELEASE_PROCESS.md`](docs/RELEASE_PROCESS.md)
for version bumps, public CI confirmation, release-validation, tagging, and
GitHub Release creation.

## Code organization rules

- Public headers belong under `include/bbsolver/<area>/`.
- Implementation files belong under `src/<area>/`.
- C++ solver tests belong under `tests/solver_unit/`.
- Solver-owned source policies belong under `tests/policies/`.
- Do not add new `.h` solver headers. Use `.hpp`.
- Do not place tracked C++ sources directly under `src/`.
- Keep include paths canonical, for example
  `#include "bbsolver/solve/solve_command.hpp"`.

## API and architecture rules

- Preserve the CLI, SampleBundle, and KeyBundle contracts unless the schema
  version and migration path are explicitly updated.
- Keep hot solver kernels, temporal fitting loops, geometry processing, and
  error metric evaluation free of PImpl indirection.
- Use PImpl only for cold, optional, or host-facing integration surfaces where
  ABI insulation matters more than cache locality and compiler optimization.
- Keep progress, cancellation, and diagnostics owned by orchestration/lifecycle
  boundaries. Helper modules should report through passed-in policy or writer
  objects rather than opening their own global channels.
- Preserve deterministic results across serial and parallel execution. If a
  parallel change is introduced, include a jobs-parity test or explicit
  deterministic reduction.
- Do not mix behavior changes into structural refactor patches unless the
  behavior change is the stated scope of the patch.

## Testing expectations

Add or update tests when a change affects:

- public CLI behavior
- SampleBundle or KeyBundle parsing/writing
- solve-mode routing
- tolerance or acceptance behavior
- diagnostics, progress, or cancellation
- path topology, path replacement, or motion smoothing
- multicore behavior or reduction ordering

Prefer focused unit tests for small rules and fixture-backed integration tests
for full solve behavior.

## Dependency policy

The solver uses pinned third-party dependencies. CMake may use installed
packages, pinned upstream archives, or the hash-checked local fallback archives
under `third_party/archive`.

Do not update or add dependencies without:

- naming the dependency and version
- pinning the source URL
- pinning the SHA256 hash
- updating `third_party/README.md`
- validating both normal fetch and forced local archive builds when practical

Do not commit build outputs or generated dependency trees.

## Documentation policy

User-facing solver docs live under `docs/`.

Update docs in the same change when behavior, settings, modes, CLI flags,
diagnostic notes, build steps, or integration contracts change. Keep the
changelog current for solver-facing behavior and packaging changes.

## Pull request expectations

A useful change description includes:

- what changed and why
- which behavior is intentionally unchanged
- validation commands and results
- any remaining risks or follow-up work
- notes about determinism, diagnostics, or public API impact when relevant

Small, reviewable patches are preferred. Refactors should state their boundary
and keep behavior-neutral validation separate from feature work.
