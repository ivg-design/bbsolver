# `bbsolver` CLI process contract

`bbsolver` is a command-line application: hosts integrate by spawning it
and exchanging JSON bundles on the file system. This document is the
authoritative contract for that process boundary.

For the concise integration reference (CLI plus IO schema in one place)
see [`API_REFERENCE.md`](API_REFERENCE.md). For the AE example that
spawns this CLI from JSX, see
[`AE_SCRIPTUI_HARNESS.md`](AE_SCRIPTUI_HARNESS.md).

## Binary location resolution

Host integrations resolve the `bbsolver` binary in this order, picking
the first hit that exists on disk. The pattern is the same regardless of
host language; the AE harness uses exactly this order.

1. Host-stored override (an explicit path in host settings).
2. `BBSOLVER_BIN` environment variable.
3. Per-user install:
   - macOS: `~/.bbsolver/bin/bbsolver`
   - Windows: `%APPDATA%\bbsolver\bin\bbsolver.exe`
4. System install:
   - macOS: `/usr/local/bin/bbsolver`, `/opt/homebrew/bin/bbsolver`
   - Windows: `%ProgramFiles%\bbsolver\bin\bbsolver.exe`,
     `%ProgramFiles(x86)%\bbsolver\bin\bbsolver.exe`
5. Binary next to the host script, or in a sibling `bin/` folder.
6. Bare `bbsolver` / `bbsolver.exe` looked up on `PATH`.

Hosts that spawn through a non-interactive shell — including AE's
`system.callSystem` on macOS — should not rely on step 6 alone, because
the inherited `PATH` will not include user-only entries. Prefer steps 2
or 3.

## Subcommands

`bbsolver` accepts JSON bundle files only. `*.bbsm.json` files carry
`SampleBundle` data and `*.bbky.json` files carry `KeyBundle` data. The
FlatBuffers schemas in [`protocol/`](../protocol/) are design
references for a future binary IO surface and are not used by the CLI
today.

JSON Schema files live in [`schemas/`](../schemas/). To validate a host
bundle before invoking the CLI:

```sh
python3 scripts/validate_json_bundle.py in.bbsm.json
python3 scripts/validate_json_bundle.py out.bbky.json
```

### `bbsolver solve <input> <output> [opts]`

```
bbsolver solve in.bbsm.json out.bbky.json
  [--tolerance T]            # L∞ in property units; overrides bundle config
  [--screen-px P]            # L∞ in projected px; 0 disables
  [--jobs N]                 # 0 = auto via TBB
  [--verbose]
  [--progress-fd FD]         # write JSON progress events to FD
  [--diagnostics PATH]       # write JSONL diagnostics events
  [--cancel-file PATH]       # poll this path; if it exists, abort cleanly
  [--decompose-paths]        # advanced: per-channel path decomposition
  [--fit-canonical-paths]    # advanced: canonical Shape Path fitting
  [--fit-replacement-paths]  # advanced: fitted Shape Path replacement
  [--emit-landmark-subpaths] # advanced (default-off): multi-path output
  [--solve-mode MODE]        # full|temporal-only|vertex-only|motion-smooth|motion-path-smooth
```

Reads SampleBundle, writes KeyBundle.

`--emit-landmark-subpaths` is an advanced, default-off output. When
enabled for a `shape_flat` Path property, the solver first writes the
normal source `PropertyKeys`, then appends extra `PropertyKeys` entries
with the same `property_id` and notes containing
`landmark_subpath; subpath_index=N`. Hosts that opt in are expected to
route those entries through a multi-path applier such as
`applyKeyBundleMultiPath`; hosts that have not adopted the multi-path
representation should leave this flag off. The same mode can be enabled
with the `BBSOLVER_EMIT_LANDMARK_SUBPATHS=1` environment variable.

**Exit codes**:

| Code | Meaning |
| --- | --- |
| `0` | Command completed. For `solve`, inspect per-property `converged` flags before applying results. |
| `1` | Runtime, IO, format, or unsupported-`schema_version` error. The user-visible message is the last stderr line. |
| `2` | Usage error such as missing or unparseable arguments. |
| `3` | `verify` completed and at least one property failed validation. |
| `5` | Cancelled via `--cancel-file`. The KeyBundle written contains all properties solved before cancellation, with `notes` reflecting the cancel state. |

### `bbsolver verify <bundle.bbky.json> <samples.bbsm.json>`

Replays a `KeyBundle` as AE-style Bezier and re-checks every sample
against the source `SampleBundle`. Writes a JSON verification report to
stdout and exits with:

- `0` — within tolerance for all properties.
- `3` — at least one property failed.

The first argument must be a KeyBundle with `_schema: "keys"` and the
second argument must be a SampleBundle with `_schema: "samples"`. Swapped
or malformed bundle arguments are treated as format errors and exit `1`.

### `bbsolver dump <bundle.bbsm.json|bundle.bbky.json>`

Pretty-prints a JSON `SampleBundle` or `KeyBundle` to stdout. `dump`
accepts JSON only, requires `_schema: "samples"` or `_schema: "keys"`,
and rejects arbitrary JSON; there is no binary auto-detection.

### `bbsolver --version`

Prints the solver version on stdout (one line). Host integrations
should compare this against an embedded expectation on startup and
warn the user on mismatch.

### `bbsolver --help`

Prints the standard usage block to stdout.

## JSX spawn pattern (illustrative)

```javascript
function spawnBbsolver(args) {
    var bin = resolveBbsolverBin();                  // see "Binary location resolution"
    var cmdline = '"' + bin + '" ' + args.join(' ');
    var output  = system.callSystem(cmdline);        // synchronous, blocks AE
    return output;                                   // combined stdout/stderr
}
```

A production host should run the solve in a background task and poll
progress via `--progress-fd`. The synchronous `system.callSystem` form
above is what the AE ScriptUI harness uses today because it keeps the
example short and self-contained.

## Stdout / stderr conventions

- `bbsolver solve` is mostly silent unless `--verbose` or
  `--progress-fd` is set.
- Progress events are one JSON object per line written to the file
  descriptor supplied via `--progress-fd`.
- Errors always go to stderr; the user-visible error is the last
  stderr line.
- `bbsolver verify` and `bbsolver dump` write JSON to stdout. `verify`
  prints a structured report; `dump` pretty-prints a validated bundle.

## Stability guarantees

- Subcommand names and argument **names** are stable.
- Argument **defaults** may change between minor versions; hosts SHOULD
  pass explicit values rather than rely on defaults.
- Exit codes are stable.
- Output JSON shape is governed by the bundle `schema_version`.

## Versioning the protocol

`bbsolver solve` and `bbsolver verify` require the supported
`schema_version` declared in `include/bbsolver/io/schema_contract.hpp`.
If the version is unsupported, the command exits with code `1` and a
stderr message of the form:

```
bbsolver: Unsupported SampleBundle schema_version=<N>; bbsolver supports schema_version=<supported>
```

Hosts SHOULD compare `bbsolver --version` against their embedded
expectation on startup and warn the user on mismatch.
