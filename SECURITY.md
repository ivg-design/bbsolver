# Security Policy

This document describes how to report a security issue in `bbsolver` and what
to expect after you do.

## Supported versions

| Version | Supported |
| --- | --- |
| `1.0.x` | yes |
| earlier development snapshots | no |

The `bbsolver --version` string identifies the binary you are running. The CMake
package version is exposed through `find_package(bbsolver CONFIG)`.

## Reporting a vulnerability

Please report suspected vulnerabilities **privately** — do not file a public
issue with vulnerability details.

The preferred channel is this repository's private security advisory mechanism
(on GitHub-hosted forks: *Security → Advisories → Report a vulnerability*).
Private advisories let maintainers coordinate a fix and a release without
exposing affected users in the meantime.

If you cannot use the repository's private advisory channel, reach out via a
direct message to a repository maintainer rather than the public issue tracker.

A useful report includes:

- the `bbsolver --version` string,
- the host platform (macOS / Linux / Windows + arch + compiler),
- the smallest reproducer you can share — usually a `SampleBundle` JSON, a
  `KeyBundle` JSON, or a CLI invocation,
- the observed behavior and the expected behavior,
- whether you believe the issue is exploitable (for example: arbitrary read
  through a malformed bundle, denial of service through a pathological input,
  crash on untrusted input) or a hardening opportunity (assertion failure,
  unchecked integer arithmetic, etc.).

## Scope

In scope for this policy:

- The `bbsolver` CLI binary and its JSON SampleBundle / KeyBundle parsers.
- The `bbsolver::core` C++ library when invoked through its documented
  command entry points (`RunSolve`, `RunVerifyCommand`, `RunDumpCommand`)
  with bundles parsed by the shipped IO layer.
- The shipped CMake package config (`bbsolverConfig.cmake`, exported targets)
  and the install tree layout.
- The shipped AE ScriptUI harness under
  [`examples/after-effects/`](examples/after-effects/) — in their published
  form (modified copies are out of scope).

Out of scope for this policy:

- Source-visible `bbsolver::core` symbols beyond the three documented `Run*`
  entry points. These are not part of the SDK contract — see
  [`docs/DEVELOPER_GUIDE.md`](docs/DEVELOPER_GUIDE.md) §11.
- Third-party dependencies (Ceres, Eigen, oneTBB, FlatBuffers, nlohmann/json).
  Their upstream security channels are listed in
  [`third_party/THIRD_PARTY_NOTICES.md`](third_party/THIRD_PARTY_NOTICES.md);
  please report dependency vulnerabilities upstream as well as to us so
  pinned versions can be advanced.
- Issues that require the attacker to already have local write access to the
  package's install tree or to substitute a different `bbsolver` binary.

## What to expect

Once a private report is filed, maintainers will:

1. Acknowledge receipt and confirm the channel is appropriate.
2. Reproduce the issue against the supported version.
3. Discuss a remediation plan and a coordinated-disclosure timeline with the
   reporter.
4. Land the fix on the development branch, publish a patch release, and
   credit the reporter in the changelog if they wish.

There is no formal acknowledgment SLA for v1.0.x; maintainers respond as
quickly as project capacity allows. Reporters who need a faster response
should say so in the initial report.

## What this policy does not promise

- A bug bounty.
- Coverage for issues in third-party dependencies — those are reported to
  the upstream projects directly.
- Coverage for issues in modified copies of the package.

Thank you for helping keep `bbsolver` and its users safe.
