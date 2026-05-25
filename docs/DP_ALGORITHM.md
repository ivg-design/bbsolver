# DP Key Placement — Algorithm Notes

## Problem statement

Given N samples `s_0..s_{N-1}` of a property at known times, find the **fewest**
anchor sample indices `0 = a_0 < a_1 < ... < a_{K-1} = N-1` such that for every
consecutive pair `(a_s, a_{s+1})` the AE-representable segment between them
satisfies the L∞ tolerance.

This is the **minimum-cardinality piecewise approximation** problem. Greedy
"split-at-worst-error" is not optimal — it over-keys when a smarter cut point
elsewhere would have allowed a longer segment. We solve it exactly with DP.

## State

```
dp[j] = minimum number of segments to cover samples [0 .. j]
        (equivalently: K-1 where K is the number of anchors up to and including j)
prev[j] = i such that dp[j] = dp[i] + 1 and fit(i, j) feasible
```

## Recurrence

```
dp[0] = 0
dp[j] = min over i in [max(0, j - G), j-1] of (dp[i] + 1) such that fit(i, j) feasible
```

where `G` is the max-gap cap (in samples), default `round(2 * comp.fps)`.

## fit(i, j) predicate

Returns feasible iff a single AE segment of type {Hold, Linear, Bezier} (configured
by `SolverConfig.allow_*`) joining `s_i` and `s_j` can reproduce
`s_i .. s_j` within `tolerance` (L∞ in property units) and optionally
`tolerance_screen_px` (L∞ in projected screen pixels).

Implementation order, fastest first (short-circuit on the cheapest feasible kind):
1. **Hold**: only feasible if `|v_k - v_i| <= tol` for all `k in [i, j]`.
2. **Linear**: linear interpolation v_i -> v_j, check L∞.
3. **Bezier**: per-dimension cubic temporal Bezier with handles fit by NLLS
   (Ceres). For spatial properties, also fit the spatial cubic Bezier tangents.

The fit function returns a `SegmentFitResult` carrying all the decision variables
needed for AE writeback (interp_in/out, eases, tangents).

## Complexity

- Without cap: `O(N^2)` fits, each fit is `O(N * iter)` worst-case.
- With cap `G`: `O(N * G)` fits.
- Per-property at `N ~ 250` (10 sec at ~25 fps), `G ~ 50`: ~12.5K fits.
- Per-fit NLLS Ceres run on a 50-point segment is sub-millisecond on
  modern hardware. Total: tens of ms per property, comfortably parallel.

## Parallelization

The DP recurrence is sequential in `j`, but the **fits themselves are
independent across distinct `(i, j)` pairs**. Two viable strategies:

### Option A — prefill feasibility matrix in parallel, then sequential DP
```
parallel_for (i, j) in pairs(N, G):
    feas[i][j] = fit(i, j)   // thread-safe
serial:
    for j in 1..N-1:
        dp[j] = min over i in window where feas[i][j] is feasible
```
Pros: simple, predictable, perfect for TBB `parallel_for`.
Cons: computes fits we may not need (rare with a sensible cap).

### Option B — lazy memo with parallel work-stealing
Use `tbb::concurrent_hash_map<(i,j), SegmentFitResult>` and a recursive
solver that schedules fit computations on the TBB task arena. More efficient
when feasibility is sparse, but more complex.

**v1 chooses Option A**, sized by `max_gap_samples`.

## Reconstruction & key assembly

After DP picks the anchor set, we assemble keys:

```
Key 0:
  v = v_{a_0}
  interp_in  = (matches segment[-1]'s interp choice but for the start key it's irrelevant)
  interp_out = segments[0].interp
  temporal_ease_out = segments[0].ease_out_at_i
  spatial_out       = segments[0].spatial_out_at_i

Key k in [1..K-2]:
  v = v_{a_k}
  interp_in  = segments[k-1].interp
  interp_out = segments[k].interp
  temporal_ease_in  = segments[k-1].ease_in_at_j
  temporal_ease_out = segments[k].ease_out_at_i
  spatial_in        = segments[k-1].spatial_in_at_j
  spatial_out       = segments[k].spatial_out_at_i

Key K-1:
  v = v_{a_{K-1}}
  interp_in  = segments[K-2].interp
  temporal_ease_in  = segments[K-2].ease_in_at_j
  spatial_in        = segments[K-2].spatial_in_at_j

All keys: temporal_continuous=false, spatial_continuous=false,
          temporal_auto_bezier=false, spatial_auto_bezier=false, roving=false.
```

## Verification

After DP + assembly we run the **internal verifier** (`verifier.cpp`) that
replays the AE-style Bezier curve at every original sample time and checks
L∞ against the original values. Any failing segment is marked
`converged=false` and the SegmentReport carries the failing sample index +
error.

Host-side round-trip verification (for example the AE ScriptUI harness in
`examples/after-effects/`) re-evaluates the produced keys after writeback as
an end-to-end check.

## Pathological cases

- **Discontinuities** (e.g. step functions). The fitter must emit `Hold`
  interpolation flanked by anchor keys at the discontinuity samples on both
  sides. The DP will naturally place anchors there because `fit(i, j)`
  becomes infeasible across a jump.
- **Long flats**. `Hold` is short-circuited as a single segment regardless of
  length, keeping `K` low.
- **High-frequency wiggle**. `K` will approach `N`. We report this and the
  solver returns `converged=true` but with `notes` indicating saturation.
  Caller can tighten `max_keys_hint` to force concession.
- **Spatial roving**. v1 does not generate roving keys. v2 may.
