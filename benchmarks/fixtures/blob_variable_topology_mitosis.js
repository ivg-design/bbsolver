// ============================================================================
// blob_variable_topology_mitosis.js
//
// AE Property Expression — variable-topology animated blob (v5: vertex
// "mitosis" — new vertex spawns at parent's exact position with zero
// tangents, then continuously migrates outward and grows its tangents).
//
// Place on Shape Layer > Contents > [Group] > Path 1 > Path.
//
// User-specified constraints addressed by this version:
//   1. No flat-to-bezier tangent flip on any vertex. Tangents are
//      continuous functions of time on every vertex: either constantly
//      natural (always-on baseline) or growing/shrinking via smoothstep
//      between 0 and natural (toggleable detail vertices).
//   2. New vertex appears at the EXACT POSITION of its parent (an always-on
//      vertex). At the moment of appearance, the toggleable vertex is
//      coincident with its parent and has tangents [0, 0]. Over GROW_DUR
//      seconds, the toggleable vertex migrates from parent's position to
//      its natural angular slot position, and its tangents grow from zero
//      to their natural perpendicular-to-radius values.
//   3. No frame-to-frame jumps. Position and tangents both blend through
//      smoothstep so velocity is zero at the spawn/death instant —
//      derivative-continuous transitions.
//
// Structural layout:
//   - 32 fixed angular slots evenly distributed around the loop.
//   - Even slots (0, 2, 4, ..., 30) are ALWAYS-ON baseline vertices.
//     There are 16 of them; their positions and tangents are determined
//     entirely by the wave-modulated outline and never depend on
//     whether toggleable detail vertices are present.
//   - Odd slots (1, 3, 5, ..., 31) are toggleable DETAIL vertices. Each
//     odd slot m has parent slot m-1 (the always-on neighbor immediately
//     counterclockwise). On spawn, slot m is born at slot m-1's current
//     position with zero tangents.
//
// Activation schedule:
//   Each toggleable slot has a deterministic spawn-time and death-time
//   derived from a golden-ratio phase. Holds for HOLD_FRACTION of the
//   schedule duration. Spawn and death are bracketed by GROW_DUR-second
//   ease-in / ease-out windows during which position and tangents blend.
//
// ES5-only syntax.
// ============================================================================

var t = time;

// ---- Tunable ----
var M = 32;
var SCHEDULE_DUR = 6.0;
var HOLD_FRACTION = 0.55;     // fraction of schedule each toggleable stays active
var GROW_DUR = 0.50;          // seconds for grow / shrink phase
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

// ---- Always-on iff even slot ----
function isAlwaysOn(m) { return (m % 2) === 0; }

// ---- Parent of a toggleable slot is the previous angular slot (always-on) ----
function parentOf(m) { return (m - 1 + M) % M; }

// ---- Lifecycle for slot m at time t ----
// Returns:
//   { alive: false }                              — slot is dormant
//   { alive: true, blend: 1.0 }                   — always-on or fully active
//   { alive: true, blend: 0..1 }                  — growing or shrinking
function lifecycle(m, tNow) {
    if (isAlwaysOn(m)) return { alive: true, blend: 1.0 };

    // Use the toggleable index (0..M/2-1) for phase so phases distribute
    // across the actual toggleable slots, not across all M.
    var toggleIdx = (m - 1) / 2;
    var phase = (toggleIdx * PHI) % 1;
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

    if (!inWindow) return { alive: false };

    var growEnd = Math.min(sinceSpawn / GROW_DUR, 1.0);
    var shrinkEnd = Math.min(untilDeath / GROW_DUR, 1.0);
    var raw = Math.min(growEnd, shrinkEnd);
    if (raw <= 0) raw = 0;
    var smoothed = raw * raw * (3 - 2 * raw);
    return { alive: true, blend: smoothed };
}

// ---- Drift + wave envelope ----
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

// ---- Per-slot natural position (where vertex m would be at full activity) ----
function naturalPos(m) {
    var th = (m / M) * 2 * Math.PI;
    var r = radiusAt(th);
    return { x: driftX + r * Math.cos(th), y: driftY + r * Math.sin(th), theta: th, r: r };
}

// ---- Per-slot natural tangents (uniform, based on full M-slot ring spacing).
// IMPORTANT: tangent length is calculated using full M-slot angular spacing —
// it does NOT depend on which other toggleable slots happen to be active.
// This keeps always-on vertex tangents stable regardless of toggle events. ----
function naturalTangents(m) {
    var p = naturalPos(m);
    var dirX = -Math.sin(p.theta);
    var dirY = Math.cos(p.theta);
    var arc = 2 * Math.PI / M;
    var len = arc * p.r / 3;
    return {
        inX: -len * dirX,  inY: -len * dirY,
        outX: len * dirX,  outY: len * dirY
    };
}

// ---- Build active slot list ----
var activeSlots = [];
for (var m = 0; m < M; m++) {
    var lc = lifecycle(m, t);
    if (lc.alive) activeSlots.push({ m: m, blend: lc.blend });
}
activeSlots.sort(function (a, b) { return a.m - b.m; });
var N = activeSlots.length;

// ---- Build vertices ----
var vertices = [];
var inTangents = [];
var outTangents = [];

for (var i = 0; i < N; i++) {
    var slot = activeSlots[i];
    var m = slot.m;
    var b = slot.blend;
    var nat = naturalPos(m);
    var natT = naturalTangents(m);

    if (isAlwaysOn(m)) {
        // Always-on: natural position, natural tangents, no dependence on toggleables.
        vertices.push([nat.x, nat.y]);
        inTangents.push([natT.inX, natT.inY]);
        outTangents.push([natT.outX, natT.outY]);
    } else {
        // Toggleable: at b=0, coincident with parent + zero tangents.
        // At b=1, natural position + natural tangents.
        var parentNat = naturalPos(parentOf(m));
        var x = parentNat.x * (1 - b) + nat.x * b;
        var y = parentNat.y * (1 - b) + nat.y * b;
        vertices.push([x, y]);
        inTangents.push([natT.inX * b, natT.inY * b]);
        outTangents.push([natT.outX * b, natT.outY * b]);
    }
}

createPath(vertices, inTangents, outTangents, true);
