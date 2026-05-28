// ============================================================================
// blob_variable_topology_organic.js
//
// AE Property Expression — variable-topology animated blob (v4: geometry-
// driven vertex inclusion, modeled after the noodle rig's organic behavior).
//
// Place on Shape Layer > Contents > [Group] > Path 1 > Path.
//
// Design principle (from how the noodle handles topology change):
//   Vertex count varies because vertices are added/removed AT THE EXACT
//   MOMENT THEY BECOME (NON-)REDUNDANT. The noodle's repairSelfIntersections
//   routine prunes a vertex when it coincides with the line through its
//   neighbors; addition happens when geometry starts differentiating.
//
//   v4 applies the same principle directly: at every frame, generate a
//   dense candidate ring of M_MAX positions on the wave-modulated outline,
//   then keep only the candidates whose perpendicular distance to the chord
//   between their fixed-index neighbors exceeds SIMPLIFY_TOL. As the wave
//   morphs, each candidate's chord-distance is a continuous function of
//   time. At the moment a candidate crosses the threshold from below
//   (becoming "meaningful"), it is by definition very close to the chord,
//   so introducing it doesn't visibly change the path. Same for removal.
//
//   Topology change is therefore geometric: a vertex exists exactly when
//   it contributes meaningfully to the shape, and is removed exactly when
//   it becomes redundant. No timed event schedule; no on-/off-curve jumps.
//
// Tangents: for vertices that are far from the chord (deeply "meaningful"),
// use natural perpendicular-to-radius bezier handles. For vertices near
// the chord, blend toward chord-aligned handles. This ensures the bezier
// curve through the new vertex matches the chord at the moment of
// appearance.
//
// ES5-only syntax. Mostly cheap math; runs in roughly O(M_MAX) per frame.
// ============================================================================

var t = time;

// ---- Tunable ----
var M_MAX = 32;              // dense candidate count
var MIN_KEEP = 6;            // floor on output vertex count (createPath sanity)
var SIMPLIFY_TOL_HI = 0.6;   // px — keep vertex if chord-distance >= TOL_HI
var SIMPLIFY_TOL_LO = 0.3;   // px — drop vertex once chord-distance <= TOL_LO
                             //      (hysteresis prevents single-frame flicker)
var BASE_RADIUS = 60;
var WAVE_AMP_MIN = 4;
var WAVE_AMP_MAX = 22;
var DRIFT_AMP_X = 40;
var DRIFT_AMP_Y = 30;
var WAVE_FREQ_1 = 3;
var WAVE_FREQ_2 = 5;
var WAVE_FREQ_3 = 7;
var WAVE_PHASE_RATE = 0.30;
var DRIFT_RATE = 0.20;
var AMP_CYCLE_SEC = 5.0;

// ---- Drift + wave amplitude envelope ----
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

// ---- Step 1: dense candidate ring at M_MAX fixed angular slots ----
var cand = [];
for (var m = 0; m < M_MAX; m++) {
    var th = (m / M_MAX) * 2 * Math.PI;
    var r = radiusAt(th);
    cand.push({
        m: m,
        theta: th,
        x: driftX + r * Math.cos(th),
        y: driftY + r * Math.sin(th)
    });
}

// ---- Step 2: compute each candidate's perpendicular distance to the chord
// formed by its angular neighbors (one slot away on either side). This is
// the "redundancy metric" — small d means the chord through neighbors
// already approximates this point. ----
function perpDist(p, a, b) {
    var dx = b.x - a.x;
    var dy = b.y - a.y;
    var len = Math.sqrt(dx * dx + dy * dy);
    if (len < 1e-9) return Math.sqrt((p.x - a.x) * (p.x - a.x) + (p.y - a.y) * (p.y - a.y));
    return Math.abs(dy * p.x - dx * p.y + b.x * a.y - b.y * a.x) / len;
}

var dist = [];
for (var i = 0; i < M_MAX; i++) {
    var prev = cand[(i - 1 + M_MAX) % M_MAX];
    var curr = cand[i];
    var next = cand[(i + 1) % M_MAX];
    dist.push(perpDist(curr, prev, next));
}

// ---- Step 3: keep candidates with chord-distance above threshold.
// Use hysteresis bands to prevent single-frame oscillation: a vertex with
// d in [TOL_LO, TOL_HI] keeps its previous-frame state. Since AE expressions
// have no inter-frame state, we approximate by using the geometric mean
// threshold (TOL_HI * TOL_LO)^0.5 as the decision boundary — vertices near
// it produce smooth transitions because their position is near the chord
// at the threshold crossing. ----
var TOL_DEC = Math.sqrt(SIMPLIFY_TOL_HI * SIMPLIFY_TOL_LO);

var keep = [];
for (var k = 0; k < M_MAX; k++) {
    if (dist[k] >= TOL_DEC) keep.push(k);
}

// Enforce minimum vertex count for createPath sanity. If too few survive,
// re-add the largest-d candidates.
if (keep.length < MIN_KEEP) {
    var ranked = [];
    for (var z = 0; z < M_MAX; z++) ranked.push({i: z, d: dist[z]});
    ranked.sort(function (a, b) { return b.d - a.d; });
    keep = [];
    for (var q = 0; q < MIN_KEEP; q++) keep.push(ranked[q].i);
    keep.sort(function (a, b) { return a - b; });
}

var N = keep.length;

// ---- Step 4: build path. Bezier handles blend from chord-aligned (vertex
// near threshold = mostly on the chord, handles align with chord) toward
// natural perpendicular-to-radius as the vertex's chord-distance grows. ----
var vertices = [];
var inTangents = [];
var outTangents = [];

function tangentBlend(d) {
    // smoothstep from chord-aligned (d ~ TOL_DEC) to natural (d >= 2 * TOL_DEC)
    var lo = TOL_DEC;
    var hi = 2.0 * TOL_DEC;
    if (d <= lo) return 0.0;
    if (d >= hi) return 1.0;
    var f = (d - lo) / (hi - lo);
    return f * f * (3 - 2 * f);
}

for (var j = 0; j < N; j++) {
    var slotM = keep[j];
    var c = cand[slotM];
    vertices.push([c.x, c.y]);

    var prevS = keep[(j - 1 + N) % N];
    var nextS = keep[(j + 1) % N];
    var cp = cand[prevS];
    var cn = cand[nextS];

    // On-chord tangents (1/3 of displacement to each neighbor)
    var inOnX = (cp.x - c.x) / 3;
    var inOnY = (cp.y - c.y) / 3;
    var outOnX = (cn.x - c.x) / 3;
    var outOnY = (cn.y - c.y) / 3;

    // Natural tangents (perpendicular to radius, scaled by angular gap)
    var dirX = -Math.sin(c.theta);
    var dirY = Math.cos(c.theta);
    var prevGap = slotM - prevS;  if (prevGap <= 0) prevGap += M_MAX;
    var nextGap = nextS - slotM;  if (nextGap <= 0) nextGap += M_MAX;
    var rNow = Math.sqrt((c.x - driftX) * (c.x - driftX) + (c.y - driftY) * (c.y - driftY));
    var inLen = (prevGap * 2 * Math.PI / M_MAX) * rNow / 3;
    var outLen = (nextGap * 2 * Math.PI / M_MAX) * rNow / 3;
    var inNatX = -inLen * dirX;
    var inNatY = -inLen * dirY;
    var outNatX = outLen * dirX;
    var outNatY = outLen * dirY;

    // Blend by chord-distance: near threshold => chord-aligned, large => natural
    var b = tangentBlend(dist[slotM]);
    var ix = inOnX * (1 - b) + inNatX * b;
    var iy = inOnY * (1 - b) + inNatY * b;
    var ox = outOnX * (1 - b) + outNatX * b;
    var oy = outOnY * (1 - b) + outNatY * b;

    inTangents.push([ix, iy]);
    outTangents.push([ox, oy]);
}

createPath(vertices, inTangents, outTangents, true);
