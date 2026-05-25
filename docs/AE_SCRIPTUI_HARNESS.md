# AE ScriptUI Test Harness

`bbsolver-test-harness.jsx` is an After Effects ScriptUI panel that shows a
complete host integration path for `bbsolver`:

1. Sample selected AE properties into a JSON `SampleBundle`.
2. Run `bbsolver solve`.
3. Read the JSON `KeyBundle`.
4. Write solved keys back to the selected AE properties.

The harness is intentionally standalone. It does not require CEP, Node, or a
commercial host panel. It is a working reference for sampling, bundle IO, solver
process invocation, logging, and writeback.

## Files

- Root panel:
  [`examples/after-effects/bbsolver-test-harness.jsx`](../examples/after-effects/bbsolver-test-harness.jsx)
- Required support folder:
  [`examples/after-effects/bbsolver-test-harness/`](../examples/after-effects/bbsolver-test-harness/)
- Harness version: `bbsolver-test-harness v1.1.0`
- Solver input: `*.bbsm.json` JSON `SampleBundle`
- Solver output: `*.bbky.json` JSON `KeyBundle`

The root JSX file uses `//@include` statements that expect the support folder to
sit next to the root file:

```text
ScriptUI Panels/
  bbsolver-test-harness.jsx
  bbsolver-test-harness/
    _polyfill.jsx
    settings.jsx
    _lookup.jsx
    sampler.jsx
    serialize_json.jsx
    parse_keys.jsx
    writeback.jsx
    apply.jsx
    verify.jsx
```

The v1.1.0 panel does not require a separate root-level JSON helper. JSON guard,
SampleBundle writing, and KeyBundle parsing live in the support folder.

## Install

For a dockable panel, copy both the root script and the support folder into the
After Effects `ScriptUI Panels` folder, then restart After Effects.

**macOS, app-level After Effects 2026 install:**

```sh
cp examples/after-effects/bbsolver-test-harness.jsx \
  "/Applications/Adobe After Effects 2026/Scripts/ScriptUI Panels/"
cp -R examples/after-effects/bbsolver-test-harness \
  "/Applications/Adobe After Effects 2026/Scripts/ScriptUI Panels/"
```

**macOS, user-level install:**

```sh
mkdir -p "$HOME/Library/Preferences/Adobe/After Effects/26.2/Scripts/ScriptUI Panels"
cp examples/after-effects/bbsolver-test-harness.jsx \
  "$HOME/Library/Preferences/Adobe/After Effects/26.2/Scripts/ScriptUI Panels/"
cp -R examples/after-effects/bbsolver-test-harness \
  "$HOME/Library/Preferences/Adobe/After Effects/26.2/Scripts/ScriptUI Panels/"
```

**Windows, user-level install:**

```powershell
New-Item -ItemType Directory -Force `
  "$env:APPDATA\Adobe\After Effects 2026\Scripts\ScriptUI Panels"
Copy-Item examples\after-effects\bbsolver-test-harness.jsx `
  "$env:APPDATA\Adobe\After Effects 2026\Scripts\ScriptUI Panels\"
Copy-Item -Recurse examples\after-effects\bbsolver-test-harness `
  "$env:APPDATA\Adobe\After Effects 2026\Scripts\ScriptUI Panels\"
```

**Windows, app-level install:**

```powershell
Copy-Item examples\after-effects\bbsolver-test-harness.jsx `
  "$env:ProgramFiles\Adobe\Adobe After Effects 2026\Support Files\Scripts\ScriptUI Panels\"
Copy-Item -Recurse examples\after-effects\bbsolver-test-harness `
  "$env:ProgramFiles\Adobe\Adobe After Effects 2026\Support Files\Scripts\ScriptUI Panels\"
```

Open the panel from:

```text
Window > bbsolver-test-harness.jsx
```

For one-off testing without installing, use `File > Scripts > Run Script
File...` and choose `bbsolver-test-harness.jsx`. The support folder must still
be adjacent to the root script.

## Solver Binary Paths

The harness resolves `bbsolver` in this order:

| Order | Source | macOS | Windows |
|---|---|---|---|
| 1 | Settings dialog path | as typed | as typed |
| 2 | `BBSOLVER_BIN` environment variable | absolute path | absolute path |
| 3 | Per-user install | `~/.bbsolver/bin/bbsolver` | `%APPDATA%\bbsolver\bin\bbsolver.exe` |
| 4 | System install | `/usr/local/bin/bbsolver`, `/opt/homebrew/bin/bbsolver` | `%ProgramFiles%\bbsolver\bin\bbsolver.exe`, `%ProgramFiles(x86)%\bbsolver\bin\bbsolver.exe` |
| 5 | Sibling of the panel | `<panel-dir>/bbsolver`, `<panel-dir>/bin/bbsolver` | `<panel-dir>\bbsolver.exe`, `<panel-dir>\bin\bbsolver.exe` |
| 6 | Bare command | `bbsolver` | `bbsolver.exe` |

Preferred standalone locations:

- macOS per-user: `~/.bbsolver/bin/bbsolver`
- macOS system: `/usr/local/bin/bbsolver` or `/opt/homebrew/bin/bbsolver`
- Windows per-user: `%APPDATA%\bbsolver\bin\bbsolver.exe`
- Windows system: `%ProgramFiles%\bbsolver\bin\bbsolver.exe`

Build-tree paths are also valid, for example `build/bbsolver` on macOS or
`build\Release\bbsolver.exe` on Windows. Before solving, the harness runs
`bbsolver --version` and requires `bbsolver 1.0.0` or newer.

## Main Workflow

1. Build or install `bbsolver`.
2. Install the root panel and support folder.
3. Open a comp in After Effects.
4. Select one or more animatable timeline properties.
5. Open `Window > bbsolver-test-harness.jsx`.
6. Use `Settings` to confirm the solver path, log folder, and time range.
7. Choose a solve mode and tolerances.
8. Click `Sample` to inspect the SampleBundle or `Solve and Bake` to sample,
   solve, preview, and apply in one operation.

The harness preserves the v103-style selected-property workflow: it does not
include a separate "batch bake every animated property on selected layers"
button, but it does support selecting multiple explicit timeline properties and
solving them together with one click.

## Included Features

### Selected Multi-Property Solving

The panel reads `comp.selectedProperties`, filters to animatable leaf
properties, samples all selected properties into one `SampleBundle`, and writes
all returned `property_results` back in one apply operation.

### Post-Expression Sampling

Samples use:

```jsx
property.valueAtTime(t, false)
```

`false` means "post-expression" in AE scripting terms. Expression-driven motion
is therefore baked into the samples. If `Disable expression after apply` is
enabled, writeback turns the expression off after the solved keys are written
while keeping the expression text on the property.

### Parented 2D Position Flattening

`Flatten parented 2D Position and unparent on apply` samples a parented 2D
layer Position in comp space, then unparents before applying solved Position
keys. AE may report 2D layer Position as a three-component vector with `z=0`;
the sampler accepts both two- and three-component Position values for this path.

### Motion Path Controls

The `Motion Path` section is used by `motion_path_smooth` solves:

| Control | Units | Range | Effect |
|---|---:|---:|---|
| `Smoothing` | dimensionless strength | `1..32` | Larger values smooth the source trajectory harder. |
| `Fit` | comp pixels | `0.1..200` | Larger values allow more deviation from the smoothed path and usually reduce keys. |
| `Preserve global path bounds` | checkbox | off/on | Replaces `Fit` with `Bounds`. |
| `Bounds` | comp pixels | `0..500` | Maximum allowed deviation of the solved path bounding box from the sampled path bounds. `0` requires matching bounds. |
| `Preserve sharp motion-path reversals` | checkbox | off/on | Keeps direction reversals above the angle threshold anchored. |
| `Sharp angle` | degrees | `1..179` | Direction changes larger than this threshold are preserved. |
| `Respect keyed source frames` | checkbox | off/on | Keeps existing keyed source frames anchored during smoothing. |

### Variable-Topology Shape Paths

For Shape Path properties, the sampler detects whether vertex topology changes
over the sampled range.

- Stable topology is exported as normal `shape_flat`.
- Variable topology is exported as raw per-frame `shape_flat` values with
  `shape_variable_topology: true`.
- When variable topology is detected, the harness automatically enables guarded
  replacement fitting in the SampleBundle configuration.

There is no public experimental checkbox for this path. The harness keeps the
UI focused while still demonstrating how a host should route variable topology.

### Logs, Progress, And Timer

The panel has a progress bar, an elapsed solve timer, a step log, a `Clear Log`
button, and an `Export logs` checkbox. Exported logs are written to the log
folder selected in `Settings`. Each log records:

- resolved solver path and version,
- sampled property count,
- parent-flatten and variable-topology decisions,
- exact `bbsolver solve` command,
- solver stdout/progress output,
- KeyBundle path and key count,
- writeback result and any skipped properties.

On macOS the harness launches `bbsolver` through a small detached shell runner
and polls the progress log with `app.scheduleTask()`. The elapsed timer should
continue moving while the solve is active; if it stops, the ScriptUI thread is
blocked. On Windows the harness falls back to synchronous `system.callSystem()`,
so the UI can freeze until the solver returns.

### Jobs

`Solver jobs` maps directly to:

```text
--jobs N
```

Units: solver worker threads. Range in the harness UI: `0..64`.
`0` lets `bbsolver` choose automatically. `1` forces single-threaded execution
for determinism checks or profiling.

## Deliberately Excluded From The Public Harness

The v1.1.0 standalone harness keeps the reliable integration surface and removes
internal/debug-only controls that would confuse a public reference panel:

- No "batch bake all animated properties on selected layers" button.
- No pairwise rig-gap verification UI.
- No landmark sub-path emission UI.
- No Bezier/fitted replacement raw experimental toggles.
- No expression-delete/archive marker flow.
- No guide-layer archive flow.

Selected multi-property solve/apply remains supported.

## JSON Contract

The harness writes JSON `SampleBundle` files and reads JSON `KeyBundle` files.
For the schema and CLI process contract, see:

- [`API_REFERENCE.md`](API_REFERENCE.md)
- [`SOLVER_CLI.md`](SOLVER_CLI.md)
- [`../examples/json/README.md`](../examples/json/README.md)
