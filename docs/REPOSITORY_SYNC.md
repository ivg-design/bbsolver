# Repository Sync

This maintainer document describes how an integration repository can publish
its tracked `solver/` tree into a standalone public `bbsolver` repository. In
the public repository, the contents of `solver/` become the repository root.

When this workflow is used, the `solver/` copy in the integration repository
remains the source of truth. Do not move or delete it during public-repo
publishing.

## Local Export

From the integration repository root:

```sh
python3 tools/sync_bbsolver_monorepo.py \
  --destination ../bbsolver \
  --branch main \
  --commit
```

The export uses `git archive HEAD solver`, so it copies only tracked files from
the committed solver tree and strips the leading `solver/` directory. Build
trees, caches, ignored files, and integration-repo-only files are not exported.
Sync commits include `Source-Repository:`, `Source-Ref:`, `Source-Commit:`,
`Source-Path:`, and `Source-Tree:` footers so each public snapshot can be traced
back to the exact source tree that produced it.

To push to a configured public remote:

```sh
python3 tools/sync_bbsolver_monorepo.py \
  --destination ../bbsolver \
  --remote-url https://github.com/ivg-design/bbsolver.git \
  --branch main \
  --commit \
  --push
```

Use `--dry-run` to print the selected source, destination, ref, and branch
without writing the destination.

The remote repository must already exist unless you create it separately with
your hosting provider or `gh repo create`. The sync tool refuses to mirror into
a non-empty non-git directory or a git repository that does not look like a
`bbsolver` checkout.

## GitHub Publication

The integration-repo workflow `.github/workflows/bbsolver-public-sync.yml`
publishes `solver/` into the standalone repository. It requires:

- public repository: `ivg-design/bbsolver` or the repository named by the
  workflow input / `BBSOLVER_PUBLIC_REPO` variable
- secret: `BBSOLVER_PUBLIC_REPO_TOKEN`, a token with permission to push to that
  public repository. For GitHub fine-grained personal access tokens, use the
  narrowest scope that can push repository contents: `Contents: Read and write`
  on `ivg-design/bbsolver`.
- target branch: `main`, unless overridden by workflow input /
  `BBSOLVER_PUBLIC_BRANCH`

The workflow validates the source package with the incremental standalone
validator, exports into a temporary standalone repository, validates that
exported repository, and only then pushes.

Automatic publication depends on the integration repository's configured
branch and path filters. Commits outside the tracked solver payload, such as
progress reports or repository-local audit notes, should not publish a new
standalone snapshot unless they also change `solver/`, the sync tool, or the
sync workflow.

For protected public repositories, publish to a staging branch and open a pull
request, or run this workflow with a token that is explicitly allowed by the
branch protection rules.

Rotate the publication token when maintainers change, after suspected exposure,
or on the same cadence as the project's other release credentials.

## Standalone CI

The exported repository carries
[`.github/workflows/ci.yml`](../.github/workflows/ci.yml). It runs:

- Linux fast validation with archive-backed dependency resolution
- CTest fast unit coverage (`unit` label, excluding `slow`)
- solver source-policy checks
- install and package-smoke validation
- incremental standalone validation
- macOS fast unit and package-smoke validation
- Windows fast unit and package-smoke validation
- manual/tag-triggered clean release validation

Run the full release gate locally before publishing a v1 release:

```sh
python3 scripts/validate_standalone_package.py --jobs 8 --clangd-jobs 8
```

## See also

- [`VALIDATION_WORKFLOWS.md`](VALIDATION_WORKFLOWS.md) — CMake-preset workflows
  (`dev`, `focused-test`, `package-smoke`, `release-validation`) and the
  incremental validator flags referenced above.
- [`../scripts/validate_standalone_package.py`](../scripts/validate_standalone_package.py)
  — the release-gate validator the workflow invokes.
- [`PACKAGING.md`](PACKAGING.md) — install tree, exported CMake targets, and
  the consumer-side `find_package(bbsolver CONFIG)` smoke project.
- [`RELEASE_PROCESS.md`](RELEASE_PROCESS.md) — maintainer checklist for version
  bumps, public CI, release-validation, tags, and GitHub Releases.
