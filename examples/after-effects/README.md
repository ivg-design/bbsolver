# AE ScriptUI test harness

Two files for an end-to-end After Effects integration example:

- `bbsolver-test-harness.jsx` — the ScriptUI panel. It samples one selected AE
  property, optionally unparents/bakes a parented 2-D Position, optionally
  bakes-and-disables expressions, runs `bbsolver solve`, optionally
  re-validates with `bbsolver verify`, and writes the solved keys back to the
  selected property.
- `bbsolver-json-shim.jsx` — small, dependency-free JSON helpers the harness
  `//@include`s. Provides `bbsolverHasJson()`, `validateSampleBundleJson()`,
  `writeSampleBundleJson()`, `readKeyBundleJson()`, and a handful of related
  helpers. Both files must stay in the same folder unless you edit the
  include path.

See [`../../docs/AE_SCRIPTUI_HARNESS.md`](../../docs/AE_SCRIPTUI_HARNESS.md) for:

- supported AE versions and macOS/Windows install paths,
- the `bbsolver` binary-resolution order the harness uses,
- the per-`ValueKind` sampling and `shape_flat` encoding,
- the expression bake-and-disable behavior,
- the parented-2D-Position flatten/unparent recipe,
- per-key writeback semantics, and
- the full JSON shim API surface.

For a non-AE smoke path that exercises the same JSON contract from the
command line, see [`../json/README.md`](../json/README.md).
