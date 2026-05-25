# Tests

The solver-owned test tree for the standalone `bbsolver` package:

- `solver_unit/` — C++ unit and focused integration tests, built by CMake
  when `-DBBSOLVER_BUILD_TESTS=ON`. Each test is a small executable
  registered with CTest under the `unit` label; the long-running
  replacement-temporal solver test is additionally tagged `slow`.
- `policies/` — solver-owned Python source-text policy checks. They are
  source-level guards for layout, diagnostics ownership, progress and
  diagnostics event contracts, and refactor boundaries. They have no
  runtime dependency on the built solver.
- `package_smoke/` — a minimal external-consumer project that exercises
  `find_package(bbsolver CONFIG)` against an installed package.
- `fixtures/` — bundle fixtures consumed by `solver_unit/` tests.

## Where to look

- For test families, per-family run commands, and valid/invalid output
  expectations: see [`../docs/TEST_CATALOG.md`](../docs/TEST_CATALOG.md).
- For CMake-preset workflows (fast edit loop, developer sweep, package
  smoke, release-style local sweep): see
  [`../docs/VALIDATION_WORKFLOWS.md`](../docs/VALIDATION_WORKFLOWS.md).
- For the full release gate (clean build, full CTest, all policies, install
  smoke, JSON examples, negative-bundle checks, install-hygiene scan, and
  optional clangd sweep): run
  [`../scripts/validate_standalone_package.py`](../scripts/validate_standalone_package.py).

## Common commands

```sh
# Configure with tests, build, run the fast unit sweep:
cmake -S . -B build -DBBSOLVER_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build -L unit -LE slow --output-on-failure -j 8

# Include the slow replacement-temporal coverage:
BBSOLVER_INCLUDE_SLOW=1 tests/run_unit_tests.sh build

# Run a single policy:
python3 tests/policies/solver_layout_policy.py

# Sweep all solver-owned policies:
for policy in tests/policies/*_policy.py; do
  python3 "$policy" || { echo "FAILED: $policy" >&2; exit 1; }
done
```
