// ============================================================================
// blob_variable_topology_continuous.js
//
// AE Property Expression — variable-topology animated blob (v1: continuous
// activation, fixed angular slots).
//
// This is the expression that produced the §6.7 supplementary blob bake at
// req-1779762426464 (ε = 0.5 / 1.5 / 3.0). Preserved as the verifiable
// fixture for the bake artifacts of record.
//
// Place on Shape Layer > Contents > [Group] > Path 1 > Path.
//
// Behavior:
//   - 32 fixed angular slots around a closed loop (theta = m / 32 * 2*pi).
//   - 8 baseline slots always active (m = 0..7).
//   - Remaining 24 toggleable slots each have a golden-ratio activation
//     phase. As the global activation level L(t) sweeps 0..1 via a sin wave
//     with period TOPO_CYCLE_SEC, each toggleable slot turns ON when L
//     crosses its phase from below, OFF when L crosses from above.
//   - Per-vertex angular positions are FIXED — when a new slot activates,
//     existing vertices do not move; only the new vertex appears on the
//     existing curve.
//   - Underlying radius is wave-modulated (3 superposed frequencies) and
//     evolves slowly via wavePhase.
//   - Layer drift moves the whole blob smoothly via two non-commensurate
//     frequency pairs.
//
// Known property of this configuration: high topology-transition density.
// One 6-second bake at 60 fps produced 364 source samples spanning 25
// distinct vertex counts {8..32} with ~74 topology transitions, which
// saturates the achievable temporal compression near 2.4-2.9× because the
// solver must plant a key near every transition regardless of ε. See the
// blob_variable_topology_sparse.js variant for a low-transition-density
// counterpart.
// ============================================================================

var t = time;

// ---- Tunable ----
var M = 32;
var ALWAYS_ON = 8;
var TOPO_CYCLE_SEC = 4.0;
var BASE_RADIUS = 60;
var WAVE_AMP_MIN = 4;
var WAVE_AMP_MAX = 22;
var DRIFT_AMP_X = 60;
var DRIFT_AMP_Y = 45;
var WAVE_FREQ_1 = 3;
var WAVE_FREQ_2 = 5;
var WAVE_FREQ_3 = 7;
var WAVE_PHASE_RATE = 0.5;

// ---- Slot activation phases (golden-ratio sequence, well-distributed) ----
var PHI = 0.61803398874989485;
function slotPhase(m) { return (m * PHI) % 1; }

// ---- Activation level varies smoothly with time ----
var ampPhase = t * 2 * Math.PI / TOPO_CYCLE_SEC;
var L = 0.5 + 0.5 * Math.sin(ampPhase);

// ---- Choose active slots ----
var activeSlots = [];
for (var m = 0; m < M; m++) {
    if (m < ALWAYS_ON || slotPhase(m) < L) activeSlots.push(m);
}
var N = activeSlots.length;

// ---- Layer drift ----
var driftX = DRIFT_AMP_X * Math.sin(t * 0.40) + DRIFT_AMP_X * 0.4 * Math.cos(t * 1.43);
var driftY = DRIFT_AMP_Y * Math.cos(t * 0.55) + DRIFT_AMP_Y * 0.5 * Math.sin(t * 1.13);

// ---- Wave amplitude correlates with L ----
var amp = WAVE_AMP_MIN + (WAVE_AMP_MAX - WAVE_AMP_MIN) * L;
var wavePhase = t * WAVE_PHASE_RATE;

function radiusAt(theta) {
    return BASE_RADIUS
         + amp * Math.sin(theta * WAVE_FREQ_1 + wavePhase)
         + amp * 0.60 * Math.sin(theta * WAVE_FREQ_2 - wavePhase * 1.3)
         + amp * 0.30 * Math.cos(theta * WAVE_FREQ_3 + wavePhase * 0.7);
}

// ---- Build vertices ----
var vertices = [];
var inTangents = [];
var outTangents = [];

for (var i = 0; i < N; i++) {
    var slotM = activeSlots[i];
    var theta = (slotM / M) * 2 * Math.PI;
    var r = radiusAt(theta);

    vertices.push([driftX + r * Math.cos(theta), driftY + r * Math.sin(theta)]);

    var dirX = -Math.sin(theta);
    var dirY = Math.cos(theta);
    var prevM = activeSlots[(i - 1 + N) % N];
    var nextM = activeSlots[(i + 1) % N];
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
