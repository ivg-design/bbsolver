// ============================================================================
// blob_variable_topology_smooth.js
//
// AE Property Expression — variable-topology animated blob (v3: vertex
// introductions are "on the existing path" so no visual jump on toggle).
//
// Place on Shape Layer > Contents > [Group] > Path 1 > Path.
//
// Design fix vs v1 / v2:
//   v1 (continuous) and v2 (sparse) both placed the new vertex at its
//   "natural" radius (the wave-modulated value at theta_m) the moment it
//   activated. That puts the vertex OFF the existing N-vertex path, so the
//   path geometry jumps at the activation frame.
//
//   v3 introduces the vertex AT THE NEIGHBOR-INTERPOLATED RADIUS — the
//   point where the existing path already passes through theta_m. At that
//   moment, the geometry is unchanged: a new control point lies exactly on
//   the previous path. Over the next TRANSITION_DUR seconds, the vertex's
//   radius and bezier handles smoothly migrate from the on-path values to
//   the natural wave-modulated values. Removal runs the same logic in
//   reverse — vertex first eases back onto the line between its remaining
//   neighbors, then is removed.
//
// The blend uses smoothstep so the transition has zero velocity at both
// endpoints (no derivative discontinuity at activation or deactivation).
//
// ES5-only syntax (var, function declarations) for any AE version.
// ============================================================================

var t = time;

// ---- Tunable ----
var M = 28;
var ALWAYS_ON = 20;
var SCHEDULE_DUR = 6.0;
var TRANSITION_DUR = 0.40;   // seconds for vertex to ease in / out
var HOLD_FRACTION = 0.40;    // fraction of SCHEDULE_DUR each slot stays active

var BASE_RADIUS = 60;
var WAVE_AMP_MIN = 4;
var WAVE_AMP_MAX = 18;
var DRIFT_AMP_X = 40;
var DRIFT_AMP_Y = 30;
var WAVE_FREQ_1 = 3;
var WAVE_FREQ_2 = 5;
var WAVE_FREQ_3 = 7;
var WAVE_PHASE_RATE = 0.15;
var DRIFT_RATE = 0.20;
var AMP_CYCLE_SEC = 8.0;

var PHI = 0.61803398874989485;

// ---- Per-slot activation interval [tOn, tOff] (or null = always on) ----
function slotInterval(m) {
    if (m < ALWAYS_ON) return null;
    var onT = ((m * PHI) % 1) * SCHEDULE_DUR;
    var offT = ((m * PHI + HOLD_FRACTION) % 1) * SCHEDULE_DUR;
    return [onT, offT];
}

// ---- Is the slot active at time t? ----
function isActive(m, tNow) {
    var iv = slotInterval(m);
    if (iv === null) return true;
    var onT = iv[0], offT = iv[1];
    if (onT <= offT) return (tNow >= onT && tNow <= offT);
    return (tNow <= offT || tNow >= onT);  // wrapped interval
}

// ---- Blend factor: 0 at transition boundary, 1 fully active. Smoothstep. ----
function activeBlend(m, tNow) {
    var iv = slotInterval(m);
    if (iv === null) return 1.0;
    var onT = iv[0], offT = iv[1];

    var sinceOn, untilOff;
    if (onT <= offT) {
        sinceOn = tNow - onT;
        untilOff = offT - tNow;
    } else {
        if (tNow >= onT) { sinceOn = tNow - onT; untilOff = (offT + SCHEDULE_DUR) - tNow; }
        else             { sinceOn = (tNow + SCHEDULE_DUR) - onT; untilOff = offT - tNow; }
    }
    var d = Math.min(sinceOn, untilOff);
    if (d <= 0) return 0;
    if (d >= TRANSITION_DUR) return 1.0;
    var f = d / TRANSITION_DUR;
    return f * f * (3 - 2 * f);
}

// ---- Build active-slot list ----
var activeSlots = [];
for (var m = 0; m < M; m++) {
    if (isActive(m, t)) activeSlots.push(m);
}
var N = activeSlots.length;

// ---- Layer drift + amplitude envelope ----
var driftX = DRIFT_AMP_X * Math.sin(t * DRIFT_RATE) +
             DRIFT_AMP_X * 0.4 * Math.cos(t * DRIFT_RATE * 2.3);
var driftY = DRIFT_AMP_Y * Math.cos(t * DRIFT_RATE * 1.3) +
             DRIFT_AMP_Y * 0.5 * Math.sin(t * DRIFT_RATE * 2.0);

var ampNorm = 0.5 + 0.5 * Math.sin(t * 2 * Math.PI / AMP_CYCLE_SEC);
var amp = WAVE_AMP_MIN + (WAVE_AMP_MAX - WAVE_AMP_MIN) * ampNorm;
var wavePhase = t * WAVE_PHASE_RATE;

function naturalRadius(theta) {
    return BASE_RADIUS
         + amp * Math.sin(theta * WAVE_FREQ_1 + wavePhase)
         + amp * 0.60 * Math.sin(theta * WAVE_FREQ_2 - wavePhase * 1.3)
         + amp * 0.30 * Math.cos(theta * WAVE_FREQ_3 + wavePhase * 0.7);
}

// ---- Pass 1: natural radii at every active slot ----
var thetaOf = [];
var natR = [];
for (var i = 0; i < N; i++) {
    var th = (activeSlots[i] / M) * 2 * Math.PI;
    thetaOf.push(th);
    natR.push(naturalRadius(th));
}

// ---- Pass 2: compute final radius per vertex (blend against neighbor-interpolation) ----
var finalR = [];
var finalP = [];   // [x, y] positions
for (var k = 0; k < N; k++) {
    var b = activeBlend(activeSlots[k], t);
    var r;
    if (b >= 1.0) {
        r = natR[k];
    } else {
        // Linear-radial interpolation between flanking active slots at theta_k.
        // This is the radius the path would pass through at theta_k if slot k
        // weren't present. Introducing the vertex at this radius keeps the
        // existing path geometry unchanged at the moment of activation.
        var prev = (k - 1 + N) % N;
        var next = (k + 1) % N;
        var dPrev = thetaOf[k] - thetaOf[prev];   if (dPrev <= 0) dPrev += 2 * Math.PI;
        var dNext = thetaOf[next] - thetaOf[k];   if (dNext <= 0) dNext += 2 * Math.PI;
        var f = dPrev / (dPrev + dNext);
        var interpR = natR[prev] * (1 - f) + natR[next] * f;
        r = interpR * (1 - b) + natR[k] * b;
    }
    finalR.push(r);
    finalP.push([driftX + r * Math.cos(thetaOf[k]), driftY + r * Math.sin(thetaOf[k])]);
}

// ---- Build vertices + bezier handles (handles also blend on-path -> natural) ----
var vertices = [];
var inTangents = [];
var outTangents = [];

for (var j = 0; j < N; j++) {
    var b2 = activeBlend(activeSlots[j], t);
    var px = finalP[j][0];
    var py = finalP[j][1];
    vertices.push([px, py]);

    var prev = (j - 1 + N) % N;
    var next = (j + 1) % N;
    var prevX = finalP[prev][0], prevY = finalP[prev][1];
    var nextX = finalP[next][0], nextY = finalP[next][1];

    // "On-path" tangents: 1/3 of the displacement toward each neighbor.
    // These produce a bezier that visually follows a smooth curve through
    // prev -> curr -> next, with no sharp corner at curr.
    var inOnX = (prevX - px) / 3;
    var inOnY = (prevY - py) / 3;
    var outOnX = (nextX - px) / 3;
    var outOnY = (nextY - py) / 3;

    // "Natural" tangents: perpendicular to radius (along increasing theta),
    // length proportional to angular gap to each neighbor.
    var theta = thetaOf[j];
    var dirX = -Math.sin(theta);
    var dirY = Math.cos(theta);
    var slotM = activeSlots[j];
    var prevSlot = activeSlots[prev];
    var nextSlot = activeSlots[next];
    var prevGap = slotM - prevSlot;  if (prevGap <= 0) prevGap += M;
    var nextGap = nextSlot - slotM;  if (nextGap <= 0) nextGap += M;
    var r = finalR[j];
    var inLen = (prevGap * 2 * Math.PI / M) * r / 3;
    var outLen = (nextGap * 2 * Math.PI / M) * r / 3;
    var inNatX = -inLen * dirX;
    var inNatY = -inLen * dirY;
    var outNatX = outLen * dirX;
    var outNatY = outLen * dirY;

    // Blend on-path tangents (at activation) toward natural tangents (fully active).
    var ix = inOnX  * (1 - b2) + inNatX  * b2;
    var iy = inOnY  * (1 - b2) + inNatY  * b2;
    var ox = outOnX * (1 - b2) + outNatX * b2;
    var oy = outOnY * (1 - b2) + outNatY * b2;

    inTangents.push([ix, iy]);
    outTangents.push([ox, oy]);
}

createPath(vertices, inTangents, outTangents, true);
