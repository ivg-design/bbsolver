# Verify-card provenance

Every `<req>.verify.json` and `last_verify_card.txt` in this corpus was
produced by re-running the canonical `bbsolver verify` CLI directly against
the shipped `*_g{N}.bbsm.json` + `*_g{N}.bbky.json` pair. Each `verify.json`
carries a top-level `regenerator: "bbsolver verify (canonical CLI)"` field so
the provenance is explicit. The regenerator script is
`scripts/repatch_verify_cards.py` and is re-runnable from any clone:

```sh
python3 benchmarks/scripts/repatch_verify_cards.py
```

## Required bbsolver version

Use **bbsolver 1.0.1 or newer** for reproduction. v1.0.0 has a known
verifier-side limitation: its canonical `bbsolver verify` CLI rejects
variable-topology shape_flat bundles (noodle and blob fixtures in this
corpus) with `key_value_dimension_mismatch` because it enforces strict
`expected_dimensions == key_dimensions` equality. That check is correct for
fixed-topology properties but wrong for canonicalized variable-topology
shape_flat: the bbsm-declared `dimensions` reflects
`2 + 6 × shape_max_vertex_count` while each emitted key carries a v[] array
of `2 + 6 × vertex_count` where `vertex_count` is encoded per-key at `v[1]`
(blob v1 in particular exhibits per-key v[] lengths ranging from 50 to 194
inside a single bundle). v1.0.1 relaxes the verifier-scoped gate to
`shape_variable_topology=true && units_label=="shape_flat"` while keeping the
strict invariant everywhere else (solve, apply, dump, the main bbky parser).
**This is a verifier bug fix, not a solver numeric change.** The same
bbsolver 1.0.0 binary that produced the keys would now have its output
accepted by the v1.0.1 verifier; running the v1.0.1 binary against the
same bbsm produces bit-identical bbky output (same `max_err` to six
decimal places, same key counts, same per-key vertex counts).
