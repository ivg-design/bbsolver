# Third-party notices

`bbsolver` itself is licensed under the MIT License (see
[`../LICENSE`](../LICENSE)). The solver depends on the libraries listed
below. Each dependency is fetched at build time from its upstream release
archive (or, optionally, from a hash-locked mirror under
[`archive/`](archive/) — see [`README.md`](README.md) for the fallback
policy).

This file documents each dependency's identity, version, upstream URL,
SPDX license identifier, archive filename, SHA-256 hash, and the role the
archive plays.

## Dependency table

| Dependency | Version | Upstream | License (SPDX) | Archive | SHA-256 | Role |
| --- | --- | --- | --- | --- | --- | --- |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.11.3 | <https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz> | `MIT` | `archive/nlohmann-json-3.11.3.tar.xz` | `d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d` | JSON SampleBundle / KeyBundle IO. |
| [Eigen](https://gitlab.com/libeigen/eigen) | 3.4.0 | <https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz> | `MPL-2.0` | `archive/eigen-3.4.0.tar.gz` | `8586084f71f9bde545ee7fa6d00288b264a2b7ac3607b974e54d13e7162c1c72` | Linear algebra used by Ceres and by the solver's spatial fitters. |
| [Ceres Solver](https://github.com/ceres-solver/ceres-solver) | 2.2.0 | <https://github.com/ceres-solver/ceres-solver/archive/refs/tags/2.2.0.tar.gz> | `BSD-3-Clause` | `archive/ceres-solver-2.2.0.tar.gz` | `12efacfadbfdc1bbfa203c236e96f4d3c210bed96994288b3ff0c8e7c6f350d4` | Non-linear segment fitter for high-dimensional and spatial properties. |
| [oneTBB](https://github.com/oneapi-src/oneTBB) | 2021.13.0 | <https://github.com/oneapi-src/oneTBB/archive/refs/tags/v2021.13.0.tar.gz> | `Apache-2.0` | `archive/onetbb-2021.13.0.tar.gz` | `3ad5dd08954b39d113dc5b3f8a8dc6dc1fd5250032b7c491eb07aed5c94133e1` | Parallel runtime for per-property solves. |
| [FlatBuffers](https://github.com/google/flatbuffers) | 24.3.25 | <https://github.com/google/flatbuffers/archive/refs/tags/v24.3.25.tar.gz> | `Apache-2.0` | `archive/flatbuffers-24.3.25.tar.gz` | `4157c5cacdb59737c5d627e47ac26b140e9ee28b1102f812b36068aab728c1ed` | Schema compiler and binary IO format definitions. The CLI exchanges JSON only today; the FlatBuffers schemas are design references. See "Distribution role" below. |

The SHA-256 hashes shown above match the `URL_HASH` values declared in
[`../CMakeLists.txt`](../CMakeLists.txt). Hashes can be recomputed with:

```sh
shasum -a 256 third_party/archive/*.tar.*
```

## Distribution role of `archive/`

The files under [`archive/`](archive/) are **build-time fallback mirrors**.
They are not redistributed as release artifacts of `bbsolver` and they are
not modified relative to the upstream tarballs they shadow. When a remote
download succeeds, CMake uses the upstream archive directly; the local
archive is only consulted when `BBSOLVER_THIRD_PARTY_ARCHIVE_FALLBACK=ON`
and the remote URL is unreachable (or when
`BBSOLVER_FORCE_THIRD_PARTY_ARCHIVES=ON` is set for offline builds).

Including the archives in the source tree is intended as a reproducibility
and offline-build aid, not a substitute for the upstream distributions.
Anyone redistributing `bbsolver` should preserve the upstream license text
shipped inside each archive.

## License obligations

Each dependency's license text is included inside its archive (typically as
`LICENSE`, `COPYING`, or `LICENSE.txt`). Redistribution must preserve:

- **MIT** (`nlohmann/json`): preserve the copyright notice and license
  text.
- **MPL-2.0** (`Eigen`): preserve the license file; modifications to MPL
  files must be released under MPL-2.0; unmodified use is unrestricted.
- **BSD-3-Clause** (`Ceres Solver`): preserve the copyright notice, license
  text, and disclaimer.
- **Apache-2.0** (`oneTBB`, `FlatBuffers`): preserve the license text and
  any `NOTICE` files, and document any modifications.

If your distribution form is the upstream archive itself (the default when
fallback archives are enabled), the in-archive license texts already satisfy
these obligations. If your distribution form is a built artifact that
statically links any of the above libraries, ship the corresponding license
text alongside the artifact.

## Update process

Dependencies are pinned by version and SHA-256. To update a dependency:

1. Verify the new release on the upstream project.
2. Compute the new archive's SHA-256 (`shasum -a 256 <archive>`).
3. Update the version, URL, and `URL_HASH` in
   [`../CMakeLists.txt`](../CMakeLists.txt).
4. Update the corresponding rows in this file and in
   [`README.md`](README.md).
5. Replace the file under [`archive/`](archive/) with the new tarball,
   re-running step 2 to confirm.
6. Build with `BBSOLVER_FORCE_THIRD_PARTY_ARCHIVES=ON` to confirm the
   local mirror is consistent.
7. Run the package-local policy + CLI smoke (see
   [`../docs/QUICKSTART.md`](../docs/QUICKSTART.md) once it ships, or
   the smoke loop in [`../examples/json/README.md`](../examples/json/README.md)).
8. Commit the version bump as a discrete change with a brief rationale.

Security-driven updates should additionally cite the advisory ID and the
affected version range in the commit message.

## Security contact

Report a security issue with `bbsolver` or with a dependency pin in this
file by opening a private security advisory on the upstream repository.
Do not file vulnerability details in public issues until they are fixed.

For dependency-side vulnerabilities, please also notify the upstream
project: each dependency's GitHub/GitLab project linked above has its own
security reporting channel.

## License of this notice

This document is part of `bbsolver` and is distributed under the same MIT
License (see [`../LICENSE`](../LICENSE)).
