# bbsolver source-level policies

This directory contains solver-owned Python policy checks. They are
source-text guards for layout, diagnostics ownership, progress/event
contracts, and refactor boundaries. They are not runtime dependencies.

All checks under this directory are intended to travel with the `bbsolver`
package — they only inspect files inside the package root and do not depend
on anything outside the package.

Run a policy directly from the package root:

```sh
python3 tests/policies/solver_layout_policy.py
```

Or sweep all package-local policies:

```sh
for policy in tests/policies/*_policy.py; do
  python3 "$policy" || { echo "FAILED: $policy" >&2; exit 1; }
done
```

A subset of host-integration policies (for example AE panel checks,
installer/release workflow checks, and progress-report checks) live outside
this directory and are run only in the upstream development environment.
They are not required for solver-only contributors.
