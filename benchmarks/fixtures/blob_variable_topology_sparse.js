// ============================================================================
// blob_variable_topology_sparse.js
//
// AE Property Expression — variable-topology animated blob (v2: sparse
// scheduled activations, fixed angular slots).
//
// Designed to address the topology-transition-density saturation that
// limited the v1 continuous variant near 2.4-2.9× compression: instead of
// continuous phase pressure on 24 toggleable slots, this variant uses an
// explicit deterministic event schedule on a smaller pool of toggleable
// slots, with each slot held in its active state for a substantial
// fraction of the schedule duration.
//
// Place on Shape Layer > Contents > [Group] > Path 1 > Path.
//
// Expected behavior:
//   - 28 fixed angular slots; 20 baseline always-on + 8 toggleable.
//   - 16 total events (8 slots * 2 events each) spread evenly across
//     SCHEDULE_DUR. Density ~ 2.7 transitions/sec.
//   - Slow underlying geometry morph (WAVE_PHASE_RATE = 0.15).
//   - Slow drift translation (DRIFT_RATE = 0.20).
//   - Each toggleable slot's hold-on duration ≈ 40% of SCHEDULE_DUR, giving
//     long flat plateaus the temporal solver can compress through.
//
// Uses var/function only — ES5-compatible for any AE expression engine.
// ============================================================================

var t = time;

// ---- Tunable ----
var M = 28;
var ALWAYS_ON = 20;
var SCHEDULE_DUR = 6.0;
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

// ---- Build event schedule ----
var PHI = 0.61803398874989485;
var events = [];
for (var m = ALWAYS_ON; m < M; m++) {
    var onT = ((m * PHI) % 1) * SCHEDULE_DUR;
    var offT = (((m * PHI) + 0.40) % 1) * SCHEDULE_DUR;
    events.push({ t: onT,  slot: m, on: true });
    events.push({ t: offT, slot: m, on: false });
}
events.sort(function (a, b) { return a.t - b.t; });

// ---- Determine active slots at current t ----
var active = [];
for (var k = 0; k < ALWAYS_ON; k++) active.push(k);
for (var i = 0; i < events.length; i++) {
    var ev = events[i];
    if (ev.t > t) break;
    var idx = active.indexOf(ev.slot);
    if (ev.on) {
        if (idx < 0) active.push(ev.slot);
    } else {
        if (idx >= 0) active.splice(idx, 1);
    }
}
active.sort(function (a, b) { return a - b; });
var activeSlots = active;
var N = activeSlots.length;

// ---- Drift ----
var driftX = DRIFT_AMP_X * Math.sin(t * DRIFT_RATE) +
             DRIFT_AMP_X * 0.4 * Math.cos(t * DRIFT_RATE * 2.3);
var driftY = DRIFT_AMP_Y * Math.cos(t * DRIFT_RATE * 1.3) +
             DRIFT_AMP_Y * 0.5 * Math.sin(t * DRIFT_RATE * 2.0);

// ---- Wave amplitude oscillates slowly ----
var ampNorm = 0.5 + 0.5 * Math.sin(t * 2 * Math.PI / 8.0);
var amp = WAVE_AMP_MIN + (WAVE_AMP_MAX - WAVE_AMP_MIN) * ampNorm;
var wavePhase = t * WAVE_PHASE_RATE;

// ---- Build vertices ----
var vertices = [];
var inTangents = [];
var outTangents = [];

for (var j = 0; j < N; j++) {
    var slotM = activeSlots[j];
    var theta = (slotM / M) * 2 * Math.PI;
    var r = BASE_RADIUS
          + amp * Math.sin(theta * WAVE_FREQ_1 + wavePhase)
          + amp * 0.60 * Math.sin(theta * WAVE_FREQ_2 - wavePhase * 1.3)
          + amp * 0.30 * Math.cos(theta * WAVE_FREQ_3 + wavePhase * 0.7);

    vertices.push([driftX + r * Math.cos(theta), driftY + r * Math.sin(theta)]);

    var dirX = -Math.sin(theta);
    var dirY = Math.cos(theta);
    var prevM = activeSlots[(j - 1 + N) % N];
    var nextM = activeSlots[(j + 1) % N];
    var prevGap = slotM - prevM;
    if (prevGap <= 0) prevGap += M;
    var nextGap = nextM - slotM;
    if (nextGap <= 0) nextGap += M;
    var inLen = (prevGap * 2 * Math.PI / M) * r / 3;
    var outLen = (nextGap * 2 * Math.PI / M) * r / 3;

    inTangents.push([-inLen * dirX, -inLen * dirY]);
    outTangents.push([outLen * dirX, outLen * dirY]);
}

createPath(vertices, inTangents, outTangents, true);
