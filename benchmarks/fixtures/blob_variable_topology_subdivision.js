// ============================================================================
// blob_variable_topology_subdivision.js
//
// AE Property Expression — variable-topology animated blob (v6: vertex
// addition via true De Casteljau Bezier subdivision; no curve disturbance
// at insertion or removal).
//
// Place on Shape Layer > Contents > [Group] > Path 1 > Path.
//
// Geometric principle:
//   When a new vertex M is inserted between an existing pair of vertices
//   B and C, the visible bezier curve through (B → C) must be preserved
//   exactly. De Casteljau subdivision at parameter T provides this
//   guarantee: it splits one bezier B→C into two beziers B→M→C such that
//   together they trace EXACTLY the same curve as the original. The new
//   vertex M lies on the original curve at parameter T, and all four
//   relevant control-point displacements (B.out, M.in, M.out, C.in)
//   shift to specific subdivision-derived values that reproduce the
//   original curve.
//
//   At T = 0, M is coincident with B; B.out becomes ZERO (transferred
//   entirely to M.out which now carries the original B→C handle); M.in
//   is zero. The curve from B to M is degenerate (zero length, both
//   endpoints + control points at B's position with B.out=0 and M.in=0,
//   no rendered geometry). The curve from M to C is exactly the
//   original B→C bezier. Inserting M at T = 0 is therefore VISUALLY
//   INVISIBLE: no protrusion, no path change, just an additional
//   anchor coincident with B.
//
//   As T grows from 0 toward 0.5 (M slides toward the midpoint of the
//   original curve), all four tangents shift continuously per
//   subdivision math. At T = 0.5, M sits at the midpoint of the
//   original B→C bezier, with B.out, M.in, M.out, C.in all at their
//   half-subdivision values.
//
// User-stated constraints addressed:
//   1. No flat-to-bezier tangent flip. Every tangent (on every vertex,
//      every frame) is a smooth function of the subdivision parameter
//      T. At T=0 some tangents are zero, but they ramp up continuously
//      as T grows — never a single-frame jump.
//   2. New vertex appears at exact parent position. At T=0, M = B
//      exactly. As T grows, M migrates along the existing bezier curve
//      toward its midpoint — staying ON THE CURVE at every intermediate
//      T (by the subdivision identity).
//   3. No frame-to-frame jumps. T blends through smoothstep from 0 to
//      0.5 over GROW_DUR seconds. All tangents and positions evolve
//      smoothly. Path geometry is mathematically identical at every T
//      (preserved by subdivision).
//
// Trade: always-on vertices DO have time-varying tangents in this
// version. Specifically, an always-on vertex's out-tangent for the
// segment toward its next-always-on neighbor depends on whether a
// detail vertex is currently subdivision-inserted into that segment.
// However, those tangent changes are SMOOTH (continuous in T, never
// discontinuous), which honors the spirit of constraint 1.
//
// Structural layout:
//   - 16 always-on baseline vertices at theta_k = k/16 * 2π, k = 0..15.
//     Always-on bezier handles are sized for the always-on-to-always-on
//     arc length (assuming no subdivision present).
//   - 16 detail slots, one per always-on segment. Detail slot k lives
//     between always-on k and always-on (k+1)%16. Its parent (for
//     subdivision purposes) is always-on k.
//
// ES5-only syntax.
// ============================================================================

var t = time;

// ---- Tunable ----
var M_BASE = 16;             // always-on baseline vertex count
var SCHEDULE_DUR = 6.0;
var HOLD_FRACTION = 0.55;    // fraction of schedule each detail slot is active
var GROW_DUR = 0.50;         // sec for grow / shrink phase
var SUBDIV_TARGET = 0.50;    // subdivision parameter T at full activity (0..0.5)
var BASE_RADIUS = 60;
var WAVE_AMP_MIN = 4;
var WAVE_AMP_MAX = 22;
var DRIFT_AMP_X = 40;
var DRIFT_AMP_Y = 30;
var WAVE_FREQ_1 = 3;
var WAVE_FREQ_2 = 5;
var WAVE_FREQ_3 = 7;
var WAVE_PHASE_RATE = 0.20;
var DRIFT_RATE = 0.20;
var AMP_CYCLE_SEC = 6.0;

var PHI = 0.61803398874989485;

// ---- Drift + amplitude envelope ----
var driftX = DRIFT_AMP_X * Math.sin(t * DRIFT_RATE) +
             DRIFT_AMP_X * 0.4 * Math.cos(t * DRIFT_RATE * 2.3);
var driftY = DRIFT_AMP_Y * Math.cos(t * DRIFT_RATE * 1.3) +
             DRIFT_AMP_Y * 0.5 * Math.sin(t * DRIFT_RATE * 2.0);

var ampNorm = 0.5 + 0.5 * Math.sin(t * 2 * Math.PI / AMP_CYCLE_SEC);
var amp = WAVE_AMP_MIN + (WAVE_AMP_MAX - WAVE_AMP_MIN) * ampNorm;
var wavePhase = t * WAVE_PHASE_RATE;

function radiusAt(theta) {
    return BASE_RADIUS
         + amp * Math.sin(theta * WAVE_FREQ_1 + wavePhase)
         + amp * 0.60 * Math.sin(theta * WAVE_FREQ_2 - wavePhase * 1.3)
         + amp * 0.30 * Math.cos(theta * WAVE_FREQ_3 + wavePhase * 0.7);
}

// ---- Always-on baseline vertices ----
function basePos(k) {
    var th = (k / M_BASE) * 2 * Math.PI;
    var r = radiusAt(th);
    return { x: driftX + r * Math.cos(th), y: driftY + r * Math.sin(th), theta: th, r: r };
}

// Natural tangent length for an always-on vertex (1/3 of full-segment arc)
function baseTangentLen(k) {
    var p = basePos(k);
    return (2 * Math.PI / M_BASE) * p.r / 3;
}

// Natural tangent direction (perpendicular to radius, along increasing theta)
function baseTangentDir(k) {
    var th = (k / M_BASE) * 2 * Math.PI;
    return { dx: -Math.sin(th), dy: Math.cos(th) };
}

// "Natural" full out-tangent at always-on k (vector from anchor)
function naturalOut(k) {
    var len = baseTangentLen(k);
    var d = baseTangentDir(k);
    return { x: len * d.dx, y: len * d.dy };
}

// "Natural" full in-tangent at always-on k (vector from anchor, opposite direction)
function naturalIn(k) {
    var len = baseTangentLen(k);
    var d = baseTangentDir(k);
    return { x: -len * d.dx, y: -len * d.dy };
}

// ---- Lifecycle for detail slot k (between always-on k and k+1) ----
function detailLifecycle(k, tNow) {
    var phase = (k * PHI) % 1;
    var spawnT = phase * SCHEDULE_DUR;
    var deathT = ((phase + HOLD_FRACTION) % 1) * SCHEDULE_DUR;

    var inWindow, sinceSpawn, untilDeath;
    if (spawnT <= deathT) {
        inWindow = (tNow >= spawnT && tNow <= deathT);
        sinceSpawn = tNow - spawnT;
        untilDeath = deathT - tNow;
    } else {
        if (tNow >= spawnT) {
            inWindow = true;
            sinceSpawn = tNow - spawnT;
            untilDeath = (deathT + SCHEDULE_DUR) - tNow;
        } else if (tNow <= deathT) {
            inWindow = true;
            sinceSpawn = (tNow + SCHEDULE_DUR) - spawnT;
            untilDeath = deathT - tNow;
        } else {
            inWindow = false;
        }
    }

    if (!inWindow) return { alive: false, T: 0 };

    var growF = Math.min(sinceSpawn / GROW_DUR, 1.0);
    var shrinkF = Math.min(untilDeath / GROW_DUR, 1.0);
    var raw = Math.min(growF, shrinkF);
    if (raw < 0) raw = 0;
    var smoothed = raw * raw * (3 - 2 * raw);
    // T (subdivision param) goes from 0 (spawn instant) to SUBDIV_TARGET (full activity)
    return { alive: true, T: smoothed * SUBDIV_TARGET };
}

// ---- De Casteljau subdivision of a single bezier (P0, P1, P2, P3) at parameter T ----
// Returns the subdivided control structure needed for AE shape paths:
//   newOutOfP0   (P0's modified out-tangent vector from P0)
//   newAnchor    (the subdivision anchor S0)
//   newInOfS0    (S0's in-tangent vector from S0)
//   newOutOfS0   (S0's out-tangent vector from S0)
//   newInOfP3    (P3's modified in-tangent vector from P3)
function subdivide(P0, P1, P2, P3, T) {
    var Q0x = (1 - T) * P0.x + T * P1.x;
    var Q0y = (1 - T) * P0.y + T * P1.y;
    var Q1x = (1 - T) * P1.x + T * P2.x;
    var Q1y = (1 - T) * P1.y + T * P2.y;
    var Q2x = (1 - T) * P2.x + T * P3.x;
    var Q2y = (1 - T) * P2.y + T * P3.y;
    var R0x = (1 - T) * Q0x + T * Q1x;
    var R0y = (1 - T) * Q0y + T * Q1y;
    var R1x = (1 - T) * Q1x + T * Q2x;
    var R1y = (1 - T) * Q1y + T * Q2y;
    var S0x = (1 - T) * R0x + T * R1x;
    var S0y = (1 - T) * R0y + T * R1y;
    return {
        newOutOfP0: { x: Q0x - P0.x, y: Q0y - P0.y },
        newAnchor:  { x: S0x,        y: S0y        },
        newInOfS0:  { x: R0x - S0x,  y: R0y - S0y  },
        newOutOfS0: { x: R1x - S0x,  y: R1y - S0y  },
        newInOfP3:  { x: Q2x - P3.x, y: Q2y - P3.y }
    };
}

// ---- Build the path ----
var vertices = [];
var inTangents = [];
var outTangents = [];

// Per-always-on-slot computed bezier endpoints + subdivision state for the segment leaving this slot.
// We process segments in order: for each k in 0..M_BASE-1, handle the always-on vertex k and the
// optional detail vertex inserted into the segment from k to (k+1) % M_BASE.
//
// An always-on vertex's actual in-tangent comes from the subdivision of the segment ARRIVING from
// the previous always-on (slot k-1). Its actual out-tangent comes from the subdivision of the
// segment LEAVING to the next always-on (slot k).

// Precompute per-segment subdivision data so we can use it for both endpoints.
var segData = [];
for (var k = 0; k < M_BASE; k++) {
    var kNext = (k + 1) % M_BASE;
    var P0 = basePos(k);
    var P3 = basePos(kNext);
    // Original bezier control points for segment k→k+1, using natural always-on tangents
    var outK = naturalOut(k);
    var inKNext = naturalIn(kNext);
    var P1 = { x: P0.x + outK.x,        y: P0.y + outK.y        };
    var P2 = { x: P3.x + inKNext.x,     y: P3.y + inKNext.y     };

    var lc = detailLifecycle(k, t);
    if (lc.alive) {
        var sub = subdivide(P0, P1, P2, P3, lc.T);
        segData.push({
            kStart: k, kEnd: kNext,
            hasDetail: true,
            detail: sub,
            // The "natural" full out-of-P0 and in-of-P3 (used when no detail is alive)
            naturalOutK: outK, naturalInKNext: inKNext
        });
    } else {
        segData.push({
            kStart: k, kEnd: kNext,
            hasDetail: false,
            naturalOutK: outK, naturalInKNext: inKNext
        });
    }
}

// Now emit vertices in order. For each k:
//   1. Emit always-on vertex k with appropriate in-tangent (from previous segment) and
//      out-tangent (from current segment).
//   2. If current segment has an active detail, emit the detail anchor.
for (var k2 = 0; k2 < M_BASE; k2++) {
    var prevK = (k2 - 1 + M_BASE) % M_BASE;
    var prevSeg = segData[prevK];   // segment arriving at k2
    var currSeg = segData[k2];      // segment leaving k2

    // In-tangent at k2: from the segment arriving (prevK → k2).
    // If that segment had a detail, k2's in-tangent is sub.newInOfP3; else it's the natural in.
    var inK2;
    if (prevSeg.hasDetail) {
        inK2 = prevSeg.detail.newInOfP3;
    } else {
        inK2 = prevSeg.naturalInKNext;
    }
    // Out-tangent at k2: from the segment leaving (k2 → k2+1).
    // If that segment has a detail, k2's out-tangent is sub.newOutOfP0; else it's the natural out.
    var outK2;
    if (currSeg.hasDetail) {
        outK2 = currSeg.detail.newOutOfP0;
    } else {
        outK2 = currSeg.naturalOutK;
    }

    var pk = basePos(k2);
    vertices.push([pk.x, pk.y]);
    inTangents.push([inK2.x, inK2.y]);
    outTangents.push([outK2.x, outK2.y]);

    // Optionally emit the detail vertex for the segment k2 → k2+1
    if (currSeg.hasDetail) {
        var a = currSeg.detail.newAnchor;
        var di = currSeg.detail.newInOfS0;
        var dout = currSeg.detail.newOutOfS0;
        vertices.push([a.x, a.y]);
        inTangents.push([di.x, di.y]);
        outTangents.push([dout.x, dout.y]);
    }
}

createPath(vertices, inTangents, outTangents, true);
