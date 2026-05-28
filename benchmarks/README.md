# bbsolver benchmarks

Data, scripts, and reference baselines for the headline `bbsolver`
benchmarks. The top-level [`BENCHMARKS.md`](../BENCHMARKS.md) is a
light report summarising the results — this directory holds the raw
material needed to reproduce every number in it.

## Layout

| Path | Contents |
|---|---|
| [`corpus/`](corpus/) | Raw `bbsm` / `bbky` / `verify` / `progress.log` bundles for every benchmark-cited solve (~62 MB total). Indexed by [`corpus_manifest.csv`](corpus/corpus_manifest.csv) and [`THRESHOLD_NOTE.md`](corpus/THRESHOLD_NOTE.md). |
| [`supplementary/`](supplementary/) | One CSV per quantitative table or figure. Manifest at [`manifest.csv`](supplementary/manifest.csv). Includes determinism, production-corpus aggregate, FBX 4-way comparison, walk-cycle Pareto, noodle/blob lineage, SVG decimation, CS1/CS2. |
| [`case_studies/`](case_studies/) | CS1 (constant-topology animated path) and CS2 (variable-topology stress) source JSONs. |
| [`figures/`](figures/) | The 10 PNGs referenced by `BENCHMARKS.md` and the README highlights. |
| [`scripts/`](scripts/) | Deterministic Python pipeline. Includes `smoke_reproduce_one_row.py` (re-solves one corpus row against the published bundle), `_paths.py` (portability helper), and the figure / CSV generators. |
| [`external_runners/`](external_runners/) | Standalone-Python ports of the two Maya-plugin reducers used in the FBX 4-way comparison: `joosten_reducer/` (Apache 2.0 Paper.js Schneider port) and `toolchefs_reducer/` (LGPL iterative RDP). Upstream provenance preserved for diff. |
| [`fixtures/`](fixtures/) | 8 SVG decimation fixtures + the v1–v6 blob expression lineage. |
| [`after_effects_benchmark_project/`](after_effects_benchmark_project/) | After Effects 2024+ project containing every fixture used by the AE round-trip rows. Reproducing the round-trip requires an AE licence; the `bbsm` corpus is the AE-independent solver-only path. |
| [`fbx_mocap_retarget_full_size/`](fbx_mocap_retarget_full_size/) | FBX mocap source + the sampled Blender action used by the FBX 4-way comparison. |

## Reproducing a benchmark row

Each `bbsm` / `bbky` pair under `corpus/req-<id>/` can be re-solved or
re-verified from a fresh clone:

```sh
# Build bbsolver (one-time, from repo root)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j

# Re-solve a benchmark row from its bbsm input
./build/bbsolver solve \
    benchmarks/corpus/req-1779727765498/req-1779727765498_g1.bbsm.json \
    /tmp/reproduce_g1.bbky.json \
    --tolerance 0.05 --screen-px 1 --jobs 0

# Compare against the shipped bbky
diff -q /tmp/reproduce_g1.bbky.json \
        benchmarks/corpus/req-1779727765498/req-1779727765498_g1.bbky.json

# Re-verify against source samples
./build/bbsolver verify \
    benchmarks/corpus/req-1779727765498/req-1779727765498_g1.bbky.json \
    benchmarks/corpus/req-1779727765498/req-1779727765498_g1.bbsm.json
```

The shipped `solve_time_ms` is hardware-dependent; the per-property
`max_err` and total-key values are deterministic and should match the
shipped `verify.json` bit-for-bit on any platform.

For a 5-second one-row end-to-end check:

```sh
python3 benchmarks/scripts/smoke_reproduce_one_row.py
# expected last line: "PASS: reproduction matches published bundle within tolerance."
```

## Solver / verifier versions

The keys in `corpus/` were produced by `bbsolver 1.0.0`. The canonical
`verify.json` files were regenerated with `bbsolver 1.0.1` (a
verifier-side fix for variable-topology `shape_flat` bundles — the
solver itself is byte-identical between v1.0.0 and v1.0.1). Both
binaries are available on the GitHub release page; v1.0.1 is the
recommended download for reproducing `verify.json` from scratch.
See [`corpus/THRESHOLD_NOTE.md`](corpus/THRESHOLD_NOTE.md) for the
diagnosis.

## Three error metrics

Path benchmarks split the achieved error into three columns where they
differ:

- **`solver_max_err`** — recorded by the solver during in-loop
  validation against the sampled `bbsm` input.
- **`cli_verify_max_err`** — recomputed by the canonical
  `bbsolver verify` CLI against the shipped `bbky` + `bbsm` pair.
  Lives inside each `verify.json` under `corpus/`.
- **`ae_roundtrip_max_err`** — measured by the After Effects host
  after writing the keys back and re-sampling AE playback. Only
  recorded in the supplementary CSVs (the AE-side verify artifacts
  live in the original development corpora, which are private).

For fixed-topology transform rigs the three agree to rounding. For
variable-topology `shape_flat` (noodle, blob) the AE round-trip can
be looser than the solver-internal verify because AE densifies path
evaluation differently than the solver's polyline projection.
