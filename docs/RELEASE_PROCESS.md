# Release Process

This maintainer checklist covers the public `bbsolver` release flow.

## Preconditions

- The repository's `main` branch points at the intended release commit.
- `bbsolver --version`, `CMakeLists.txt`, `README.md`, and `CHANGELOG.md`
  agree on the release version.
- Public CI is green on `main`, or hosted Actions are intentionally
  disabled and the local release gate plus local binary packaging steps
  below have been recorded in the release notes.
- If hosted Actions are enabled, a manual `workflow_dispatch` run has
  completed the `release-validation` job on the same commit that will be
  tagged.

## Version Bump

`bbsolver` uses semantic versioning for the public CLI, JSON schema, and CMake
package surface:

- Patch: bug fixes, documentation, test coverage, and behavior-preserving
  packaging changes.
- Minor: backward-compatible CLI flags, solve modes, metadata, examples, or
  C++ helper additions.
- Major: incompatible JSON schema changes, changed exit-code meaning, removed
  CLI flags or modes, renamed CMake targets, or intentionally different solve
  semantics for an existing mode.

For a normal release, update the version in these source locations before
syncing:

- `CMakeLists.txt`: the `project(bbsolver VERSION ...)` value.
- `include/bbsolver/app/cli_options.hpp`: `kBbsolverVersion`.

Then update user-facing release text:

- `README.md`: current version callout.
- `docs/QUICKSTART.md`: version examples if present.
- `CHANGELOG.md`: top release entry.

Run a focused version check after rebuilding:

```sh
./build/bbsolver --version
```

## Local Gate

From the standalone repository root:

```sh
python3 scripts/validate_standalone_package.py --jobs 8 --clangd-jobs 8
```

This creates a clean package copy, builds with shipped dependency archives,
runs the full C++ test suite and solver policies, installs the package, runs
the package-smoke consumer, exercises JSON examples, checks negative bundle
cases, and runs clangd when available.

## Public CI Gate

When hosted Actions are enabled, confirm both public checks are green at the
same commit before tagging:

```sh
gh run list --repo ivg-design/bbsolver --branch main --limit 5
gh workflow run "bbsolver CI" --repo ivg-design/bbsolver --ref main
```

Wait for the manual run to finish and confirm that `release-validation` passed.
If any job is red, open the failed job log first. For packaging failures, start
with the `Package smoke` or `Clean standalone release validator` step. For
platform-only failures, compare the failing command against the matching local
command in [`VALIDATION_WORKFLOWS.md`](VALIDATION_WORKFLOWS.md), then rerun the
smallest focused target locally before changing release docs or tags.

When hosted Actions are disabled or runner minutes are exhausted, do not run
manual workflows. Treat the local gate above as the release gate, and include
the exact local commands and platform evidence in the release notes.

## Tag

Tag the already-validated public commit:

```sh
git fetch origin main
git checkout main
git pull --ff-only
git tag -a v1.0.0 -m "bbsolver v1.0.0"
git push origin v1.0.0
```

If hosted Actions are enabled, the standalone CI workflow runs on tags matching
`v*`; wait for the tag workflow to finish and confirm that `release-validation`
passed for the tag. If hosted Actions are disabled, confirm no tag workflow was
started and rely on the recorded local gate.

## Local Binary Assets

When release binaries are built locally, build one binary per target platform
and package each archive with a SHA-256 checksum. Examples:

```sh
# macOS arm64 from the standalone repository root
cmake -S . -B build-macos-arm64 -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-macos-arm64 -j
python3 scripts/package_local_release.py \
  --binary build-macos-arm64/bbsolver \
  --version 1.0.0 \
  --platform macos-arm64 \
  --out-dir dist

# Linux x64 on a Linux x64 host
cmake -S . -B build-linux-x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux-x64 -j
python3 scripts/package_local_release.py \
  --binary build-linux-x64/bbsolver \
  --version 1.0.0 \
  --platform linux-x64 \
  --out-dir dist

# Windows x64 in a Visual Studio developer PowerShell
cmake -S . -B build-windows-x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build-windows-x64 --config Release -j
python scripts/package_local_release.py `
  --binary build-windows-x64/Release/bbsolver.exe `
  --version 1.0.0 `
  --platform windows-x64 `
  --out-dir dist
```

For macOS x64, run the macOS command with
`-DCMAKE_OSX_ARCHITECTURES=x86_64` and `--platform macos-x64`, either on an
Intel Mac or with a toolchain that can produce and validate x86_64 output.
Attach every archive and matching `.sha256` file to the GitHub Release:

```sh
gh release upload v1.0.0 dist/* --repo ivg-design/bbsolver
```

## GitHub Release

After the tag CI is green, create the GitHub Release from the same tag:

```sh
gh release create v1.0.0 --repo ivg-design/bbsolver \
  --title "bbsolver v1.0.0" \
  --notes-file /tmp/bbsolver-v1.0.0-notes.md
```

Release notes should include:

- a short capability summary;
- supported platforms and build requirements;
- the stable CLI/JSON/CMake integration surfaces;
- the validation evidence for the tagged commit.
