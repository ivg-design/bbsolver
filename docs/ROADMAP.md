# Roadmap

This roadmap describes public `bbsolver` work after the v1 package boundary:
CLI integration, JSON bundles, CMake packaging, the AE ScriptUI harness, and
release artifacts. It is intentionally product-neutral; host applications can
embed the CLI workflow without depending on any private integration project.

## Now

- Keep the v1 CLI, JSON bundle, progress, diagnostics, and CMake package
  contracts stable.
- Publish JSON Schema files and a bundle-validation command for host-side
  integration checks.
- Keep examples runnable from a clean checkout and from an installed package.
- Publish local-release packaging instructions for macOS, Linux, and Windows
  binaries while hosted CI minutes are constrained.
- Keep the AE ScriptUI harness useful as a reference implementation for
  sampling, solving, logging, and writeback.

## Next

- Integrate the benchmark and case-study corpus when it is complete, including
  runtime, key-count reduction, max-error, memory, and job-count determinism
  reporting across representative animation and path cases.
- Add license-clean real-world SampleBundle and KeyBundle fixtures alongside
  the minimal synthetic examples.
- Expand schema examples to cover multi-property solves, variable-topology
  paths, parent-flattened Position solves, and motion-path smoothing.
- Publish platform release archives with checksums through GitHub Releases.

## Later

- Harden the public C++ embedding surface beyond the current command entry
  points if downstream integrators need a library-first API.
- Re-enable hosted CI gates when runner capacity is available, including
  sanitizer and lint visibility where practical.
- Add additional host-integration examples after the JSON process contract has
  more downstream users.

## Maintenance Principle

Stable public contracts should evolve additively. New CLI flags, diagnostics
fields, progress events, and optional JSON metadata are expected in minor
versions. Incompatible schema, exit-code, target-name, or solve-semantic
changes require a major version bump.
