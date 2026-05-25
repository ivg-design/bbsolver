# Maintainers

This document defines how `bbsolver` is maintained as a public standalone
package.

## Project Model

The standalone repository is the public release and collaboration surface.
During the transition period, the implementation is also maintained in an
integration repository under a `solver/` subtree. Maintainers export that
subtree into the standalone repository so public releases, tags, examples, and
documentation remain usable without the integration repository.

Public contributors should open issues and pull requests against the
standalone repository. Maintainers may apply accepted core solver changes to
the integration subtree first, then export them back into the standalone
repository with traceability footers.

## Triage

- Bugs that affect the CLI contract, JSON parsing, verification, package
  installation, or the AE harness are release blockers when reproducible.
- Documentation gaps that prevent a developer from building, validating, or
  integrating the solver are treated as package bugs.
- Feature requests should identify the host integration, bundle shape, expected
  output, and tolerance semantics.
- Performance reports should include the input bundle, solve flags, platform,
  CPU, build type, job count, runtime, key count, and verify result.

## Pull Requests

Pull requests should include:

- a clear description of the behavior or documentation change;
- focused tests or example updates when behavior changes;
- validation commands run locally;
- no generated build products, caches, or private host assets;
- license-clean fixtures and media.

For solver behavior changes, maintain deterministic output across `--jobs 1`
and parallel jobs unless the change explicitly documents a new deterministic
contract.

## Releases

Releases follow semantic versioning for the public CLI, JSON schema, progress
events, diagnostics, and CMake package targets. The maintainer checklist lives
in [`RELEASE_PROCESS.md`](RELEASE_PROCESS.md). When hosted CI is unavailable,
maintainers must run the local release gate and attach locally built archives
with SHA-256 checksums to the GitHub Release.

## Security

Report suspected vulnerabilities through the private channel in
[`../SECURITY.md`](../SECURITY.md). Do not file public issues with exploit
details.
