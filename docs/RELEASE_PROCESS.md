# Release Process

This maintainer checklist covers the public `bbsolver` release flow after a
source repository has synced `solver/` into the standalone repository root.

## Preconditions

- The standalone repository exists and its `main` branch points at the intended
  public commit.
- `bbsolver --version`, `CMakeLists.txt`, `README.md`, and `CHANGELOG.md` agree
  on the release version.
- The source-to-public sync commit includes traceability footers:
  `Source-Repository`, `Source-Ref`, `Source-Commit`, `Source-Path`, and
  `Source-Tree`.
- Public CI is green on `main`.
- A manual `workflow_dispatch` run has completed the `release-validation` job
  on the same commit that will be tagged.

## Version Bump

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

Before tagging, confirm both public checks are green at the same commit:

```sh
gh run list --repo ivg-design/bbsolver --branch main --limit 5
gh workflow run "bbsolver CI" --repo ivg-design/bbsolver --ref main
```

Wait for the manual run to finish and confirm that `release-validation` passed.

## Tag

Tag the already-validated public commit:

```sh
git fetch origin main
git checkout main
git pull --ff-only
git tag -a v1.0.0 -m "bbsolver v1.0.0"
git push origin v1.0.0
```

The standalone CI workflow runs on tags matching `v*`; wait for the tag
workflow to finish and confirm that `release-validation` passed for the tag.

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
- the validation evidence for the tagged commit;
- source sync metadata from the public sync commit.

## Source-to-Public Sync

The integration repository can publish `solver/` through its
`.github/workflows/bbsolver-public-sync.yml` workflow or through the local
`tools/sync_bbsolver_monorepo.py` command documented in
[`REPOSITORY_SYNC.md`](REPOSITORY_SYNC.md).

The GitHub workflow requires Actions to be enabled on the integration
repository and a `BBSOLVER_PUBLIC_REPO_TOKEN` secret with permission to push to
the standalone repository. If the integration repository cannot run Actions,
use the local sync command and then rely on the standalone repository CI before
tagging.
