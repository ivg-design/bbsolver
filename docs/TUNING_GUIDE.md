# Tuning bbsolver

How to pick `tolerance` and friends for a given property.

## Quick recipe

| Property type                       | Suggested `tolerance` | Suggested `tolerance_screen_px` |
|-------------------------------------|------------------------|----------------------------------|
| Opacity / scalar in `[0, 100]`     | `0.5`                  | 0 (off)                          |
| Rotation / angle (degrees)          | `0.5`                  | 0 (off)                          |
| Position 2D/3D                      | `2.0` (px)             | `1.0` (preferred — perceptual)    |
| Color RGBA                          | `0.005`                | 0                                |
| Slider effects                      | depends on slider range — start at 1% of range |  |
| Path (flat vertex)                  | `1.0` (px per vertex) | 0 (path can't be screen-projected v1) |

The unit is **property units** (px for spatial, degrees for rotation, 0–100 for
opacity, 0–1 for color components).

## How `tolerance` is enforced

The solver applies a hard **L∞ acceptance** test on every segment:

```
max_t in [t_i, t_{i+1}] | original(t) - reconstructed(t) | <= tolerance
```

`tolerance_screen_px` is enforced AS A SECOND, INDEPENDENT bound when > 0 — both
must pass. The screen-space metric uses `layer_xform_at_start` from the sampled
bundle to approximate the comp-space projection with an orthographic transform.

## What happens at the limits

- **Too loose tolerance:** K shrinks; reconstruction starts to drift visibly.
- **Too tight tolerance:** K grows toward N (sample count). At the limit you
  get one key per sample — `bbsolver` reports `converged=true` but with a
  saturation note.
- **Infeasible tolerance under max-gap:** solver falls back to
  all-samples-as-anchors and reports `converged=false` so the AE panel can
  warn you.

## Current noodle path acceptance cache

Use the latest noodle path cache as the current practical target for path bake
testing at `tol=0.5`:

| replacement target | expected result |
|---|---|
| 22 vertices | raw fallback |
| 24 vertices | raw fallback |
| 26 vertices | accepted, 94 keys, `max_err≈0.289` with residual-budget temporal solve |
| 27 vertices | accepted in non-budgeted probe, 84 keys, `max_err≈0.471` |

The 22/24 failures are expected near-miss behavior, not by themselves a bake
regression. The smoke signal is that the external solver retry reaches an
accepted 26- or 27-vertex candidate with `max_err <= 0.5`. The current default
uses the remaining outline budget for the temporal solve, so target 26 trades a
small key increase for much lower final outline error.

Keep this work in `bbsolver`: target search, replacement fitting, validation, and
fallback selection are external-solver responsibilities. AE must remain
read/write only: sample the live expression, hand the bundle to `bbsolver`, write
the returned final keys, and verify.

## Reading key-count comparisons

When comparing `bbsolver` output against a simple hold/linear reference, use
these verdict labels consistently in reports:

- `K_better` — Bezier saved keys (the common case).
- `K_eq` — same as Hold+Linear; usually means the curve is locally linear or
  hold-dominated.
- `K_REGRESSION` — the C++ solver produced MORE keys than Hold+Linear. This
  should never happen; investigate.
- `err_ok` — `max_err <= tolerance`.
- `ERR_EXCEEDS_TOL` — failed L∞ acceptance; a solver bug.

## When to crank the weights

`SolverConfig.weight_*` change which residuals matter to Ceres during the
nonlinear fit. They DO NOT loosen the L∞ gate. Defaults are tuned for
position/scalar motion. Adjust:

- `weight_vel` — increase if velocity overshoot is unacceptable (e.g. camera
  motion where the eye notices speed changes more than position).
- `weight_acc` — increase to penalize "jerky" motion in the fit.
- `weight_screen` — set > 0 alongside `tolerance_screen_px > 0` to bias the fit
  toward screen-space accuracy on distant 3D objects.

## Per-property override (v2)

Today: one `SolverConfig` per request. v2 plan: per-property override pulled
from the AE panel's settings panel (e.g. tighter tolerance on a hero camera).
