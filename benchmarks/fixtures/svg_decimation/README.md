# SVG vertex-decimation fixtures

Eight closed-polyline SVGs (single `<path>` each, M+L commands only, viewBox
1000×1000) designed for head-to-head vertex-decimation comparison between
bbsolver and Adobe Illustrator's `Object > Path > Simplify`.

| Filename | Vertices | Character |
|---|---:|---|
| `noisy_circle_120.svg` | 120 | Smooth circle with low-amplitude high-frequency noise; simulates an auto-traced smooth shape |
| `signature_curve_180.svg` | 180 | Long meandering s-curve; simulates a handwriting/signature trace |
| `organic_blob_150.svg` | 150 | Irregular asymmetric blob with mixed sharp + smooth regions |
| `dense_silhouette_240.svg` | 239 | Auto-traced character silhouette analogue, highest vertex count |
| `angular_path_100.svg` | 100 | Alternating sharp corners and smooth arcs — tests sharp-feature preservation |
| `star_curved_160.svg` | 160 | Five-point star with bowed-out curved sides |
| `spiral_200.svg` | 153 | Equiangular spiral; vertex density decreases with radius |
| `heart_140.svg` | 140 | Heart outline; sharp top cleft + smooth bottom |

All fixtures are deterministic; regenerate with:

```sh
python3 generate_test_svgs.py
```

## Illustrator comparison protocol

For each SVG, in Adobe Illustrator:

1. **File > Open** the `.svg`.
2. Select the path with the Selection tool.
3. **Object > Path > Simplify…** (or **Object > Path > Simplify Path** depending on version).
4. Use the dialog's percentage slider OR the explicit "Curve Precision" / "Angle Threshold" controls. Try a few different settings (e.g., 95%, 90%, 80%, 50% curve precision).
5. **Window > Document Info > Objects** (or **Window > Properties**) to read the resulting anchor count.
6. Optionally export the simplified path back to SVG (**File > Export As > SVG…**) for visual diffing.
7. Record (input %, output anchor count) per setting.

## bbsolver comparison protocol

`run_svg_decimation_comparison.py` reads each SVG, converts to bbsm, runs
`bbsolver solve --solve-mode vertex_only --tolerance <ε>` at multiple
tolerances, decodes the resulting bbky's kept-vertex set, computes the
true L∞ pixel-space residual against original vertices, and emits a
simplified SVG plus a CSV row. Output paths:

- `work/svg_decimation/<svg_name>/bbsolver_eps_<eps>.svg`
- `data/supplementary/svg_decimation_results.csv`
