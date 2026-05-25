# bbsolver quickstart

A five-minute end-to-end path: build the CLI, run a packaged example, and
verify the result. No host application required.

If anything in this guide does not match what the CLI prints, the CLI is
the source of truth — please open an issue.

## Requirements

| | Minimum |
|---|---|
| CMake | `3.20` |
| C++ standard | `C++17` |
| macOS | 12+ (Intel and Apple Silicon) |
| Linux | x86_64 with GCC ≥ 11 (or Clang equivalent) |
| Windows | Windows 10+ x64 with Visual Studio 2022 / MSVC 19.36+ |
| Network access | optional — see "offline / reproducible build" below |

The build vendors Ceres, Eigen, oneTBB, FlatBuffers, and nlohmann/json.
Expect a few minutes for the first build because dependencies build from
source.

## 1. Build the CLI

From the package root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/bbsolver --version    # bbsolver 1.0.0
./build/bbsolver --help
```

Windows (PowerShell, MSVC):

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
.\build\Release\bbsolver.exe --version
.\build\Release\bbsolver.exe --help
```

For an offline / reproducible build that skips remote dependency downloads
and uses the hash-locked archives under
[`third_party/archive`](../third_party/archive):

```sh
cmake -S . -B build -DBBSOLVER_FORCE_THIRD_PARTY_ARCHIVES=ON
cmake --build build -j
```

The same flag works on Windows: `cmake --build build --config Release -j`.

## 2. Solve a packaged example

The package ships three minimal SampleBundles under
[`examples/json/`](../examples/json/). Solve the simplest one (a scalar
opacity ramp from 0 to 100 over 1 second):

```sh
./build/bbsolver solve \
  examples/json/minimal_scalar.bbsm.json \
  /tmp/minimal_scalar.bbky.json \
  --jobs 1
```

Windows (PowerShell):

```powershell
.\build\Release\bbsolver.exe solve `
  examples\json\minimal_scalar.bbsm.json `
  $env:TEMP\minimal_scalar.bbky.json `
  --jobs 1
```

This reduces 25 dense samples to two endpoint keys within the tolerance
declared in the SampleBundle.

## 3. Verify the result

```sh
./build/bbsolver verify \
  /tmp/minimal_scalar.bbky.json \
  examples/json/minimal_scalar.bbsm.json
```

Windows (PowerShell):

```powershell
.\build\Release\bbsolver.exe verify `
  $env:TEMP\minimal_scalar.bbky.json `
  examples\json\minimal_scalar.bbsm.json
```

You should see `"ok": true` in the printed JSON report and exit code `0`.
The verifier independently re-evaluates the produced keys against every
input sample and reports per-property error.

## 4. Solve the other two examples

```sh
for name in minimal_position minimal_shape_flat; do
  ./build/bbsolver solve \
    examples/json/${name}.bbsm.json \
    /tmp/${name}.bbky.json \
    --jobs 1
  ./build/bbsolver verify \
    /tmp/${name}.bbky.json \
    examples/json/${name}.bbsm.json
done
```

Windows (PowerShell):

```powershell
foreach ($name in @("minimal_position", "minimal_shape_flat")) {
  .\build\Release\bbsolver.exe solve `
    "examples\json\$name.bbsm.json" `
    "$env:TEMP\$name.bbky.json" `
    --jobs 1
  .\build\Release\bbsolver.exe verify `
    "$env:TEMP\$name.bbky.json" `
    "examples\json\$name.bbsm.json"
}
```

The expected outcomes are all `"ok": true` with `max_err` well under the
tolerance declared in each bundle. The full per-example contract is in
[`../examples/json/README.md`](../examples/json/README.md).

## 5. What's next

- **Understand the bundle format**: see
  [`API_REFERENCE.md`](API_REFERENCE.md) for SampleBundle and KeyBundle
  field definitions, and [`SOLVER_CLI.md`](SOLVER_CLI.md) for the
  CLI/process boundary contract (commands, flags, exit codes, progress,
  cancellation, diagnostics).
- **Build a SampleBundle from your own data**: any host that can write
  JSON can drive `bbsolver`. The shape is defined by
  [`../include/bbsolver/domain.hpp`](../include/bbsolver/domain.hpp).
- **Tune for quality vs. key count**: see [`TUNING_GUIDE.md`](TUNING_GUIDE.md)
  for tolerance, modes, and per-property tradeoffs.
- **Drive from After Effects**: see
  [`AE_SCRIPTUI_HARNESS.md`](AE_SCRIPTUI_HARNESS.md) and
  [`../examples/after-effects/`](../examples/after-effects/) for a working
  ScriptUI harness that samples AE properties, runs `bbsolver`, and writes
  results back to the timeline.

## Where the binary lives

Host integrations resolve `bbsolver` from (in order):

1. An explicit host-stored path.
2. `BBSOLVER_BIN` environment variable.
3. Per-user install:
   - macOS: `~/.bbsolver/bin/bbsolver`
   - Windows: `%APPDATA%\bbsolver\bin\bbsolver.exe`
4. System install:
   - macOS: `/usr/local/bin/bbsolver`, `/opt/homebrew/bin/bbsolver`
   - Windows: `%ProgramFiles%\bbsolver\bin\bbsolver.exe`,
     `%ProgramFiles(x86)%\bbsolver\bin\bbsolver.exe`
5. `bbsolver` on `PATH` (last resort — non-interactive shells inherit a
   minimal `PATH` and may not see user-only entries).

`cmake --install build --prefix /path/to/bbsolver-install` lays out the
binary at `<prefix>/bin/bbsolver` (or `bbsolver.exe`) — set
`BBSOLVER_BIN=<prefix>/bin/bbsolver` to point hosts at the installed
copy. See [`SOLVER_CLI.md`](SOLVER_CLI.md) for the full resolution rule
and [`AE_SCRIPTUI_HARNESS.md`](AE_SCRIPTUI_HARNESS.md) for the
JSX-specific implementation.

## Supported integration surface

The supported integration is the CLI process boundary plus the JSON
SampleBundle/KeyBundle schemas defined in
[`API_REFERENCE.md`](API_REFERENCE.md). The CMake package additionally
exports `bbsolver::bbsolver` (the CLI) and `bbsolver::core` (a static
library for in-tree embedding and tests) — see
[`PACKAGING.md`](PACKAGING.md).

The C++ symbol surface of `bbsolver::core` beyond the three command
entry points (`RunSolve`, `RunVerifyCommand`, `RunDumpCommand`) is
source-visible but is not part of the SDK contract. Some headers inside
`include/bbsolver/` are explicitly internal even though they live in the
public include tree; see [`DEVELOPER_GUIDE.md`](DEVELOPER_GUIDE.md) §11
for the public/private boundary discussion and the rules for extending
the solver in-tree. FlatBuffers schemas under [`../protocol/`](../protocol/)
are design references; the CLI exchanges JSON only.
