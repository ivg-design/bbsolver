# bbsolver third-party backup archives

This directory contains exact backup archives for the solver dependencies
used by [`../CMakeLists.txt`](../CMakeLists.txt). They are versioned,
hash-locked tarballs of upstream releases — not forks, not patches.

For per-dependency upstream URLs, SPDX license identifiers, archive
hashes, and redistribution roles, see
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

## Dependency resolution order

CMake resolves each dependency in this order:

1. **Installed package** — `find_package` looks for a system-installed
   copy. Used when present.
2. **Pinned upstream download** — `FetchContent` downloads the version
   pinned in `CMakeLists.txt` (URL + SHA-256). Used when no installed
   package is found.
3. **Local hash-locked archive** — when
   `BBSOLVER_THIRD_PARTY_ARCHIVE_FALLBACK=ON` (default) and step 2 cannot
   reach the network, CMake falls back to the matching tarball under
   `archive/` with the identical `URL_HASH`.

Set `BBSOLVER_FORCE_THIRD_PARTY_ARCHIVES=ON` to skip steps 1–2 and use the
local mirror unconditionally. This is the recommended mode for offline or
reproducibility-sensitive builds.

## Current archive mirror

| Dependency | Version | Archive |
| --- | --- | --- |
| nlohmann_json | 3.11.3 | `archive/nlohmann-json-3.11.3.tar.xz` |
| Eigen | 3.4.0 | `archive/eigen-3.4.0.tar.gz` |
| Ceres Solver | 2.2.0 | `archive/ceres-solver-2.2.0.tar.gz` |
| oneTBB | 2021.13.0 | `archive/onetbb-2021.13.0.tar.gz` |
| FlatBuffers | 24.3.25 | `archive/flatbuffers-24.3.25.tar.gz` |

Each archive is bit-identical to the upstream release and shipped solely
as a build-time fallback. Hashes are checked by CMake on every configure.

## Update policy

Dependencies are pinned by version and SHA-256. Updates are deliberate,
not automatic. The full update procedure (including hash recomputation,
URL_HASH update, archive replacement, and smoke validation) is documented
in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) under "Update process".

Security-driven updates should reference the advisory ID and affected
version range in the commit message and bump the relevant
`URL_HASH`/`archive/` entry in the same commit.
