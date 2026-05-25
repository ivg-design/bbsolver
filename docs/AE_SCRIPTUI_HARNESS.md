# AE ScriptUI Test Harness

`bbsolver-test-harness.jsx` is a self-contained After Effects ScriptUI
example that demonstrates the full end-to-end integration loop for
`bbsolver`:

1. Sample one selected AE property into a `SampleBundle`.
2. Optionally unparent/bake parented 2D Position into comp space.
3. Optionally bake and disable expressions on the selected property.
4. Run `bbsolver solve` and capture stdout/exit code.
5. Verify the `KeyBundle` (via `bbsolver verify`, optional).
6. Write the solved keys back to the selected property.

The harness is part of the standalone `bbsolver` package and is meant to
be read, adapted, and dropped into a host integration. It is not a CEP
panel; it does not implement async progress pipes, cancellation files, or
multi-property batching. The deliberate limits are listed at the end of
this document.

## Files

- Script: [`examples/after-effects/bbsolver-test-harness.jsx`](../examples/after-effects/bbsolver-test-harness.jsx)
- JSON shim: [`examples/after-effects/bbsolver-json-shim.jsx`](../examples/after-effects/bbsolver-json-shim.jsx)
- Harness version: `bbsolver-test-harness v1.0.0`
- Solver input: `*.bbsm.json` (JSON SampleBundle)
- Solver output: `*.bbky.json` (JSON KeyBundle)

For the canonical JSON contract see
[`API_REFERENCE.md`](API_REFERENCE.md) and
[`../examples/json/README.md`](../examples/json/README.md). For the CLI
process contract see [`SOLVER_CLI.md`](SOLVER_CLI.md).

## Supported After Effects versions

The harness uses only stable AE scripting APIs that have been available
since AE CC 2014 and have not changed materially since:

- ScriptUI `Panel` / `Window` UI primitives.
- `system.callSystem` for spawning the `bbsolver` CLI.
- `CompItem.workArea*`, `Layer.property(...).valueAtTime(t, false)`,
  `Property.expression`/`expressionEnabled`, `Property.setValueAtTime`,
  `Property.setInterpolationTypeAtKey`, `Property.setTemporalEaseAtKey`,
  `Property.setSpatialTangentsAtKey`, `Property.setRovingAtKey`, and
  `Layer.sourcePointToComp` (parented-position case).
- AE `Shape` vertex / in-tangent / out-tangent arrays.

The example paths and version numbers below target **After Effects 2026
(version 26.2)**. The harness has no `app.version` check and should work
unchanged on earlier modern AE releases that ship ScriptUI panels and
the APIs listed above. If your AE install lives at a different path,
substitute the version-specific app folder and user-preferences folder
in the install commands.

| Folder | macOS | Windows |
|---|---|---|
| App-level ScriptUI Panels | `/Applications/Adobe After Effects <YEAR>/Scripts/ScriptUI Panels/` | `%ProgramFiles%\Adobe\Adobe After Effects <YEAR>\Support Files\Scripts\ScriptUI Panels\` |
| User-level ScriptUI Panels | `$HOME/Library/Preferences/Adobe/After Effects/<MAJOR.MINOR>/Scripts/ScriptUI Panels/` | `%APPDATA%\Adobe\After Effects <YEAR>\Scripts\ScriptUI Panels\` |

`<YEAR>` is the AE marketing year (e.g. `2024`, `2025`, `2026`).
`<MAJOR.MINOR>` is the macOS preferences-folder version (e.g. `24.0`,
`25.0`, `26.2`).

## Install

For a dockable panel, copy the JSX file and the adjacent JSON shim into the
ScriptUI Panels folder and restart After Effects. The harness includes the shim
with `//@include "bbsolver-json-shim.jsx"`, so both files must stay in the same
folder unless you edit the include path.

**macOS** (app-level install):

```sh
cp examples/after-effects/bbsolver-test-harness.jsx \
  "/Applications/Adobe After Effects 2026/Scripts/ScriptUI Panels/"
cp examples/after-effects/bbsolver-json-shim.jsx \
  "/Applications/Adobe After Effects 2026/Scripts/ScriptUI Panels/"
```

If the app-level folder is owned by `root`, use the user-level AE folder
instead:

```sh
cp examples/after-effects/bbsolver-test-harness.jsx \
  "$HOME/Library/Preferences/Adobe/After Effects/26.2/Scripts/ScriptUI Panels/"
cp examples/after-effects/bbsolver-json-shim.jsx \
  "$HOME/Library/Preferences/Adobe/After Effects/26.2/Scripts/ScriptUI Panels/"
```

**Windows** (user-level install):

```powershell
Copy-Item examples\after-effects\bbsolver-test-harness.jsx `
  "$env:APPDATA\Adobe\After Effects 2026\Scripts\ScriptUI Panels\"
Copy-Item examples\after-effects\bbsolver-json-shim.jsx `
  "$env:APPDATA\Adobe\After Effects 2026\Scripts\ScriptUI Panels\"
```

Then open the panel via:

```text
Window > bbsolver-test-harness.jsx
```

For one-off testing without installing, use:

```text
File > Scripts > Run Script File...
```

## Cross-platform `bbsolver` binary resolution

The harness resolves the solver binary in this order, picking the first
hit that exists on disk. It never relies on AE inheriting an interactive
shell `PATH` — `system.callSystem` on macOS in particular is invoked
through a non-interactive `sh` and will silently fail to find binaries on
user-only `PATH` entries.

| Order | Source | macOS | Windows |
|---|---|---|---|
| 1 | UI `bbsolver` field | as-typed | as-typed |
| 2 | `$.getenv("BBSOLVER_BIN")` | absolute path | absolute path |
| 3 | Per-user install | `~/.bbsolver/bin/bbsolver` | `%APPDATA%\bbsolver\bin\bbsolver.exe` |
| 4 | System install | `/usr/local/bin/bbsolver`, `/opt/homebrew/bin/bbsolver` | `%ProgramFiles%\bbsolver\bin\bbsolver.exe`, `%ProgramFiles(x86)%\bbsolver\bin\bbsolver.exe` |
| 5 | Sibling of the JSX file | `<jsx-dir>/bbsolver`, `<jsx-dir>/bin/bbsolver` | `<jsx-dir>\bbsolver.exe`, `<jsx-dir>\bin\bbsolver.exe` |
| 6 | Bare command (PATH lookup) | `bbsolver` | `bbsolver.exe` |

The preferred standalone install locations for hosts shipping `bbsolver`
to integrators are:

- macOS: `~/.bbsolver/bin/bbsolver` (per-user), or
  `/usr/local/bin/bbsolver` (system).
- Windows: `%APPDATA%\bbsolver\bin\bbsolver.exe` (per-user), or
  `%ProgramFiles%\bbsolver\bin\bbsolver.exe` (system).

The path may also point directly into a build tree, for example
`build/bbsolver` on macOS or `build\Release\bbsolver.exe` on Windows.
The harness quotes any path that contains spaces before
invoking `system.callSystem`. On macOS make sure the binary has the
executable bit set (`chmod +x bbsolver`); fresh `cmake --build` outputs
already do.

## Use

1. Build or install `bbsolver` so the harness can find it (see above).
2. Open an After Effects comp and set its work area.
3. Select one animatable property in the timeline.
4. Open `bbsolver-test-harness`.
5. Confirm the resolved `bbsolver` path, then pick an output folder, a
   solve mode, a tolerance, an optional screen-px budget, and a job
   count.
6. Click `Sample`, `Solve`, `Apply`, or `Sample + Solve + Apply` for the
   full one-click loop.

Each major step is logged to the on-panel log and to a persistent
`*.log.txt` file written alongside the `.bbsm.json` and `.bbky.json`
artifacts. The log records: resolved solver path, samples written,
expression detection result, parent-flatten decisions, the full
`bbsolver` command line, raw `bbsolver` stdout, and the writeback
result.

## What gets sampled

The harness samples the active comp work area at one sample per frame
using:

```jsx
property.valueAtTime(t, /*preExpression*/ false)
```

`preExpression: false` returns the AE-displayed value — that is, the
post-expression value. Any expression on the selected property is
therefore **baked into the samples**. The writeback step then disables
that expression (see "Expressions" below), so the baked motion replaces
the expression-driven motion without ever discarding the expression
text.

Supported property kinds and their bundle mapping:

| AE property type | SampleBundle `kind` | Notes |
|---|---|---|
| `OneD` | `Scalar` | 1-D values such as rotation, opacity. |
| `TwoD` | `TwoD` | Non-spatial 2-D values such as Scale, Anchor Point. |
| `ThreeD` | `ThreeD` | Non-spatial 3-D values such as separated Position. |
| `TwoD_SPATIAL` | `TwoD_Spatial` | 2-D Position-style motion paths. |
| `ThreeD_SPATIAL` | `ThreeD_Spatial` | 3-D Position-style motion paths. |
| `COLOR` | `Color` | RGBA. |
| `SHAPE` | `Custom` + `units_label="shape_flat"` | Shape Path vertices and tangents. |

For each sampled frame the harness also captures the AE source-key
metadata where it exists at that frame: temporal interpolation type,
temporal ease, spatial tangents, roving flag, and continuity flags. The
harness embeds those into the per-sample `key_timing` block, and the
solver uses them on writeback to faithfully restore AE's per-key
behavior.

## Shape Path encoding

Shape samples are flattened into the solver's `shape_flat` encoding:

```text
v = [closed_flag, n_vertices,
     vx_0, vy_0, in_x_0, in_y_0, out_x_0, out_y_0,
     vx_1, vy_1, in_x_1, in_y_1, out_x_1, out_y_1,
     ... ]
```

`v.length == 2 + 6 * n_vertices`.

**Stable-topology Shape paths** keep the same vertex count and closed
flag on every sampled frame. The harness copies AE's `Shape` vertex and
tangent arrays directly and the solver fits them through its normal
shape-flat temporal pipeline.

**Variable-topology Shape paths** change vertex count across frames
(closed flag must still be stable). The harness detects this case,
exports the raw per-frame `shape_flat` vectors, sets

```json
{
  "shape_canonicalized": false,
  "shape_variable_topology": true,
  "shape_canonical_method": "shape_flat_raw_variable",
  "shape_max_vertex_count": 52,
  "shape_source_topologies": ["closed:45", "closed:52"]
}
```

on the property, and — when the `fit variable-topology shape paths`
checkbox is on — appends `--fit-replacement-paths` to the solve command
and sets `allow_path_replacement_fit: true` in the bundle config.

These metadata fields are advisory: `bbsolver` derives the actual
topology from the encoded `v` arrays and enters replacement fitting via
the config/CLI enablement. Including the fields makes solver `notes` and
host logs more useful.

If the fitted replacement is accepted, `bbsolver` returns fixed-topology
Shape keys. If it is not accepted, the solver falls back to raw frame
keys or safe flat behavior and records the reason in
`PropertyKeys.notes`.

## Expressions: bake-then-disable

If the selected property has an enabled expression, the harness:

1. Detects it (`property.expressionEnabled === true`) and records the
   expression length in the bundle as `source_expression_enabled` and
   `source_expression_length` on the property info.
2. Samples through `valueAtTime(t, false)`, so the expression output is
   baked into the SampleBundle.
3. On writeback (when `disable expression after bake` is on, default
   on), sets `property.expressionEnabled = false`. The expression text
   is preserved on the property — only its evaluation is turned off.
4. Verifies the expression is actually disabled after the property
   write; if AE rejects the disable, the harness throws and the
   undo group is unwound.

This pattern lets a host rebuild the displayed motion as a static set of
keys without ever losing the original expression. Re-enabling the
expression at any time restores the original behavior; the baked keys
will simply be overridden again by the expression.

## Parent-flattened 2-D Position

The checkbox `flatten parented 2D Position and unparent on apply` bakes
the motion that comes from a parent layer into the selected child layer
itself. The harness restricts this to one well-defined case — a 2-D
layer with a 2-D parent and a `Position` property in the AE
`ADBE Transform Group` — because it is the simplest case where the
comp-space baking and unparent step are both unambiguous.

When enabled and eligible the harness:

1. Confirms the layer has a parent and is 2-D.
2. For every sample frame, calls
   `layer.sourcePointToComp(layer.anchorPoint.valueAtTime(t, false))`
   so that each sample is the layer anchor point projected into comp
   space.
3. Writes those comp-space samples into the bundle and tags the
   property with:

   ```json
   {
     "flatten_parented_position": true,
     "sample_space": "comp",
     "writeback_mode": "unparent_position"
   }
   ```

4. On Apply, calls `layer.parent = null` (falling back to
   `setParentWithJump(null)`) **before** writing the solved keys, so
   the baked comp-space motion can land on the now-unparented layer
   without being re-transformed by the parent chain.

If the property is not eligible for flatten and the checkbox is still
on, the harness raises a clear error rather than silently bypassing the
mode. The blocker reason (`selected layer is 3D`, `selected layer has
no parent`, `selected property is not a 2D Position`, etc.) is also
logged.

A host that needs to extend this to more cases — group parenting, 3-D
parented chains, parented cameras, parented rotation — should treat the
harness function `parentFlattenBlocker(rec, kind)` as a working
template, not a complete policy.

## Building the SampleBundle

The bundle the harness writes is exactly the format documented in
[`API_REFERENCE.md`](API_REFERENCE.md). The write path goes through
`writeSampleBundleJson()` in `bbsolver-json-shim.jsx`, which validates
`schema_version`, top-level bundle shape, property blocks, and per-sample value
arrays before writing the file. Concretely:

```json
{
  "_schema": "samples",
  "schema_version": 1,
  "request_id": "bbsolver-test-harness-<ts>-<safeName>",
  "comp": {
    "fps": 24.0,
    "duration_sec": 2.0,
    "width": 1920,
    "height": 1080,
    "pixel_aspect": 1.0,
    "shutter_angle_deg": 180.0,
    "shutter_phase_deg": 0.0,
    "motion_blur_enabled": false,
    "work_area_start_sec": 0.0,
    "work_area_end_sec": 2.0
  },
  "properties": [
    {
      "property": {
        "id": "<layer-index>::<match-name-path>",
        "match_name": "ADBE Position",
        "display_name": "Position",
        "layer_path": "<CompName>/<LayerName>/Transform/Position",
        "kind": "TwoD_Spatial",
        "dimensions": 2,
        "is_spatial": true,
        "is_separated": false,
        "units_label": "",
        "source_key_times": [0.0, 1.0, 2.0],
        "flatten_parented_position": false,
        "source_expression_enabled": false,
        "source_expression_length": 0
      },
      "t_start_sec": 0.0,
      "t_end_sec": 2.0,
      "samples_per_frame": 1,
      "samples": [
        { "t_sec": 0.0,    "v": [960.0, 540.0] },
        { "t_sec": 0.0417, "v": [964.2, 538.1] }
        /* … one entry per frame in the work area … */
      ],
      "layer_xform_at_start": { "position": [...], "scale": [...], "rotation": [...] }
    }
  ],
  "config": {
    "tolerance": 0.5,
    "tolerance_screen_px": 0.0,
    "solve_optimization_mode": "full",
    "parallel_jobs": 0,
    "motion_path_smoothing_tolerance": 3.0,
    "motion_path_accuracy_tolerance": 1.5,
    "motion_path_preserve_bounds": false,
    "motion_path_bounds_tolerance": 0.0,
    "motion_path_preserve_sharp_points": true,
    "motion_path_respect_keyed_frames": false,
    "allow_path_replacement_fit": false,
    "path_replacement_prefer_vertices": false
  }
}
```

The `property.id` is generated from the selected layer index and AE
match-name path so that the same property can be looked up in the
returned `KeyBundle`.

## JSON shim API

`bbsolver-json-shim.jsx` is a small, dependency-free ScriptUI helper
module the harness `//@include`s. It is published alongside the
harness and is intended to be reusable in any host AE script that
exchanges JSON bundles with `bbsolver`. The shim adds no external
dependencies beyond AE's built-in `JSON` and `File` objects.

Module constants:

| Name | Value | Purpose |
|---|---|---|
| `BBSOLVER_JSON_SHIM_VERSION` | `"1.0.0"` | Shim version string, independent of the solver version. |
| `BBSOLVER_SCHEMA_VERSION` | `1` | The `schema_version` value the shim accepts for both SampleBundle and KeyBundle. |

Public functions:

| Function | Returns / throws | Purpose |
|---|---|---|
| `bbsolverHasJson()` | `bool` | Returns `true` iff AE's runtime exposes a working native `JSON` object. |
| `bbsolverRequireJson()` | throws on missing JSON | Guard used at host startup to fail fast on AE builds without native JSON. |
| `bbsolverReadTextFile(path)` | `string` of file contents | UTF-8 text read with explicit AE `File` encoding. |
| `bbsolverWriteTextFile(path, text)` | `void` | UTF-8 text write with parent-folder creation if missing. |
| `bbsolverStringifyJson(value)` | `string` | `JSON.stringify` with the shim's standard indent. |
| `bbsolverParseJson(text, label)` | parsed `object` / throws | `JSON.parse` with a labelled error message for diagnostics. |
| `bbsolverRequireArray(value, label)` | throws if not array-like | Defensive validator. |
| `bbsolverRequireSchemaVersion(bundle, label)` | throws on mismatch | Validates `bundle.schema_version === BBSOLVER_SCHEMA_VERSION`. |
| `validateSampleBundleJson(bundle)` | `true` / throws | Full structural validation of a SampleBundle (`_schema: "samples"`, `schema_version`, `request_id`, `comp`, non-empty `properties[]`, `properties[].property.id/dimensions`, non-empty `samples[]`, `samples[].t_sec/v`, and `v.length == dimensions * samples_per_frame`). |
| `validateKeyBundleJson(bundle)` | `true` / throws | Full structural validation of a KeyBundle (`_schema: "keys"`, `schema_version`, non-empty `property_results[]`, `property_results[].property_id/dimensions`, `keys[]`, `keys[].t_sec/v`, and `keys[].v.length == dimensions`; `keys[]` may be empty only when `converged` is `false`). |
| `writeSampleBundleJson(bundle, filepath)` | `void` / throws | Validate-then-stringify-then-write. Recommended writer for hosts producing SampleBundles. |
| `readSampleBundleJson(filepath)` | parsed bundle / throws | Read-then-parse-then-validate. |
| `readKeyBundleJson(filepath)` | parsed bundle / throws | Read-then-parse-then-validate. Recommended reader for hosts consuming KeyBundles. |

Compact aliases (provided for brevity in inline examples; equivalent to
the matching `bbsolver*` function):

`hasJson`, `stringifyJson`, `parseJson`, `readTextFile`, `writeTextFile`.

Typical host integration pattern:

```jsx
//@include "bbsolver-json-shim.jsx"

// 1. Fail fast on AE builds without native JSON.
bbsolverRequireJson();

// 2. Build and emit a SampleBundle.
var bundle = buildHostSampleBundle();          // host-specific
writeSampleBundleJson(bundle, samplePath);     // validates + writes

// 3. Spawn bbsolver and capture stdout/exit.
var output = system.callSystem(spawnCommand);  // see "Running bbsolver from JSX"

// 4. Read and validate the result.
var keys = readKeyBundleJson(keyPath);         // throws on schema mismatch
applyKeysToHost(keys);                         // host-specific
```

The shim throws plain `Error` objects with descriptive messages; the
harness catches and routes them through its on-panel log. A host that
prefers structured error reporting can wrap the shim calls in `try/catch`
and emit the messages on its own log channel.

## Running `bbsolver` from JSX

The harness builds the command line, quotes each path that contains
spaces, runs it through `system.callSystem`, and captures the combined
stdout/stderr returned by AE.

The actual call has this shape:

```jsx
var cmd =
  quoteShell(solverPath) +
  " solve " + quoteShell(samplePath) +
  " " + quoteShell(keyPath) +
  " --tolerance " + options.tolerance +
  " --screen-px "  + options.screenPx +
  " --jobs "       + options.jobs +
  " --solve-mode " + options.solveMode;

if (needsReplacementFit) {
  cmd += " --fit-replacement-paths";
}

var output = system.callSystem(cmd);          // synchronous; blocks AE
if (!(new File(keyPath)).exists) {
  throw new Error("bbsolver did not write a KeyBundle.\n\n" + output);
}
```

`system.callSystem` is synchronous and blocks the AE main thread. The
example does this on purpose to keep the integration short — a
production host should run the solve in a background task and poll
progress via `--progress-fd`. The full process contract (commands,
flags, exit codes, progress events, cancellation, diagnostics) is in
[`SOLVER_CLI.md`](SOLVER_CLI.md).

## Verifying a `KeyBundle`

The harness does not run `bbsolver verify` automatically — it trusts the
solve's `converged` flag and refuses to apply non-converged results. An
integrator who wants an independent re-check can call:

```jsx
var verifyCmd =
  quoteShell(solverPath) +
  " verify "  + quoteShell(keyPath) +
  " "         + quoteShell(samplePath);
var verifyOut = system.callSystem(verifyCmd);
// verifyOut is the JSON report; exit code 0 = ok, 3 = at least one mismatch.
```

The same loop is documented end-to-end in
[`QUICKSTART.md`](QUICKSTART.md) and exercised against the packaged JSON
fixtures in [`../examples/json/`](../examples/json/).

## Writeback

When applying a `KeyBundle`, the harness first reads it through
`readKeyBundleJson()` in `bbsolver-json-shim.jsx`, which validates
`schema_version`, `property_results[]`, key times, and key value arrays. It then:

1. Matches the selected property to `property_results[].property_id`.
2. Refuses to apply non-converged results (`converged === false`).
3. If `flatten_parented_position` was set on the matching property
   info, unparents the layer first.
4. If `disable expression after bake` is on, disables the selected
   property's expression and verifies the disable took effect. The
   expression text remains on the property.
5. Wraps the writeback in an undo group named
   `bbsolver-test-harness apply`.
6. Removes existing keys from the selected property.
7. Calls `prop.setValueAtTime(keys[k].t_sec, keys[k].v)` for each key.
8. Reapplies AE per-key timing: interpolation type, temporal ease,
   temporal continuous, temporal auto-Bezier; for spatial properties
   also spatial continuous/auto-Bezier, spatial tangents, and roving
   flag where AE accepts those settings.

For Shape Path output, `shape_flat` vectors are converted back into AE
`Shape` objects before writeback.

## Reading the on-panel log file

Each run writes a `<bundle-name>.log.txt` alongside the `.bbsm.json` and
`.bbky.json`. The log captures:

- the resolved `bbsolver` path
- absolute paths of the SampleBundle, KeyBundle, and log file
- number of samples, samples-per-frame, expression state, shape
  topology decision, parent-flatten decision
- the full `bbsolver` command line that was run
- raw `bbsolver` stdout
- per-key writeback summary

Inspect this file when a result is unexpected: most surprises (wrong
binary path, wrong solve mode, expression baked the wrong way, parent
flatten silently disabled) show up clearly in the log.

## End-to-end recipe (no UI clicks)

For automation, the same loop can be driven from any host that can:

1. Build a JSON SampleBundle with the shape above.
2. Invoke `bbsolver solve <bundle>.bbsm.json <out>.bbky.json` with the
   flags it needs.
3. Read the returned KeyBundle JSON.
4. Apply the keys to whatever target representation the host owns.

The runnable JSON fixtures in
[`../examples/json/`](../examples/json/) do exactly this without AE.
Treat the JSX as a host-specific adapter layer that knows AE's
`Property` and `Shape` APIs; the JSON contract above is what makes the
adapter portable.

## Deliberate limits

`bbsolver-test-harness.jsx` is meant to be read and adapted by
integrators. It deliberately does **not** include:

- CEP panel plumbing.
- Async progress pipes (`--progress-fd`), cancellation files, or
  background-task lifecycle.
- Multi-property batching across many selected properties at once.
- A production installer or version manager.
- Exhaustive parent-space flattening for every AE layer configuration
  (groups, 3-D parents, cameras, mixed-dimension chains, etc.).
- Advanced path-replacement controls beyond the variable-topology
  checkbox.

Use it as the reference for bundle IO, expression handling, parent
flattening, and key writeback, then add host-specific progress,
cancellation, validation, and UI around it.

## Cross-references

- [`SOLVER_CLI.md`](SOLVER_CLI.md): the CLI process contract spawned by
  this harness (commands, flags, exit codes, progress events).
- [`API_REFERENCE.md`](API_REFERENCE.md): SampleBundle / KeyBundle JSON
  shape.
- [`USER_GUIDE.md`](USER_GUIDE.md): solver modes, tolerance, and
  per-property tuning.
- [`PATH_HANDLING.md`](PATH_HANDLING.md): Shape Path strategy and
  variable-topology handling.
- [`QUICKSTART.md`](QUICKSTART.md): non-AE smoke loop on the packaged
  JSON fixtures.
- [`../examples/after-effects/bbsolver-test-harness.jsx`](../examples/after-effects/bbsolver-test-harness.jsx):
  the script itself — the source of truth for harness behavior.
