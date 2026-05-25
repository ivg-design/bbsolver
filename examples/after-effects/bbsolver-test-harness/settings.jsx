// settings.jsx -- persistent settings across AE sessions.
// Requires _polyfill.jsx to be #included before this file (provides JSON).
//
// Storage path:
//   macOS:   ~/Library/Preferences/com.bbsolver-test-harness/settings.json
//            (Folder.userData on mac = ~/Library/Application Support;
//             parent = ~/Library; then /Preferences/com.bbsolver-test-harness)
//   Windows: %APPDATA%\com.bbsolver-test-harness\settings.json
//            (Folder.userData on Windows = %APPDATA%)
//
// Verified: Folder.userData (js-tools-guide folder-object.rst p.93):
//   macOS   -> ~/Library/Application Support
//   Windows -> %APPDATA%
// Verified: Folder.create() (js-tools-guide folder-object.rst p.254).
// Verified: File encoding, open/read/write/close (js-tools-guide file-object).

var BB_SETTINGS_DEFAULTS = {
    tolerance:           0.5,
    toleranceScreenPx:   1.0,
    sampleMode:          'auto',  // 'auto'|'frame'|'sub3'|'sub5'
    useWorkArea:         true,
    useSegmentMarkers:   false, // when enabled, layer markers define bake ranges
    startSec:            0,
    endSec:              0,
    disableExpression:   true,
    archiveExpression:   false,
    archiveAsGuideLayer: false,
    verifyRoundTrip:     false,
    verifyRigGaps:       false,
    rigGapTolerance:     0.5,
    showPreview:         true,
    autoSeparateForBake: false, // optional: enable Separate Dimensions on unkeyed spatial props
    flattenParentedPosition: false, // optional: sample parented Position on 2D layers in comp space, then unparent
    preserveSelectedParenting: false, // optional: keep parent links between selected layers; unparent only from unselected parents
    flattenParentedTolerance: 0.05, // stricter default for parent-flattened rig transforms
    rigRotationTolerance: 0.01, // degrees; small angular errors are visible at limb/joint lengths
    bbsolverPath:          '',   // empty = auto-detect
    solverJobs:          0,    // 0 = auto; positive values cap bbsolver/TBB worker count
    lastOutputDir:       '',   // last used output directory
    exportLogs:          true, // write the panel log to a text file
    logOutputDir:        '',   // empty = Desktop/bbsolver-test-harness-logs
    artifactRoot:        '',   // empty = repo corpus; optional live-run artifact root
    solveOptimizationMode: 'full', // 'full'|'temporal_only'|'vertex_only'|'motion_smooth'|'motion_path_smooth'
    motionSmoothUseEase: false, // motion_smooth: ease/rove spatial motion between smoothed anchors
    motionSmoothSourceFidelity: false, // motion_smooth: use sampled source motion as soft fidelity observations
    motionSmoothTolerance: 3.0, // motion_smooth: trajectory smoothing strength in pixels/comp units
    motionSmoothTemporalCleanupTolerance: 2.0, // motion_smooth cleanup: max error when reducing smoothed keys
    motionSmoothBezierX1: 0.33, // motion_smooth cubic Bezier timing x1
    motionSmoothBezierY1: 0.0, // motion_smooth cubic Bezier timing y1
    motionSmoothBezierX2: 0.67, // motion_smooth cubic Bezier timing x2
    motionSmoothBezierY2: 1.0, // motion_smooth cubic Bezier timing y2
    motionPathSmoothingTolerance: 3.0, // motion_path_smooth: dimensionless strength, clamped 1.0..32.0
    motionPathAccuracyTolerance: 1.5, // motion_path_smooth: max error to the smoothed path, clamped 0.1..200
    motionPathPreserveBounds: false, // motion_path_smooth: preserve global sampled path bounds
    motionPathBoundsTolerance: 0.0, // motion_path_smooth: allowed bounds side deviation in comp px, clamped 0..500
    motionPathPreserveSharpPoints: true, // motion_path_smooth: keep cusps/direction reversals anchored
    motionPathSharpAngleDeg: 75.0, // motion_path_smooth: turn angle treated as sharp
    motionPathRespectKeyedFrames: false, // motion_path_smooth: keep keyed source frames anchored
    emitLandmarkSubpaths: false,
    shapeTemporalFullGapFit: false,
    shapeReplacementFit: false,
    shapeReplacementPreferVertices: false, // run guarded post-temporal vertex prune after key reduction
    cleanupPassMode:     'prompt', // 'prompt'|'auto'|'off'; auto runs cleanup without confirmation
    promptSecondCleanupPass: true, // prompt to re-solve first-pass path keys before writeback
    preserveSharpPathCorners: true // keep high-angle path corners locked under loose tolerances
};

// Returns the absolute path to the settings file (creates parent dir if needed).
function _settingsFilePath() {
    var isWin = ($.os && $.os.indexOf('Windows') >= 0);
    var dir;
    if (isWin) {
        dir = Folder.userData.fsName + '/com.bbsolver-test-harness';
    } else {
        // Folder.userData on macOS = ~/Library/Application Support
        // parent = ~/Library  ->  /Preferences/com.bbsolver-test-harness
        dir = Folder.userData.parent.fsName + '/Preferences/com.bbsolver-test-harness';
    }
    var f = new Folder(dir);
    if (!f.exists) { f.create(); }
    return dir + '/settings.json';
}

// saveSettings(obj): serialise obj to JSON and write to disk.
// Returns true on success, false on failure (never throws).
function saveSettings(obj) {
    var path = _settingsFilePath();
    var f = new File(path);
    f.encoding = 'UTF-8';
    if (!f.open('w')) { return false; }
    var ok = false;
    try {
        f.write(JSON.stringify(obj, null, 2));
        ok = true;
    } catch (e) {}
    f.close();
    return ok;
}

// loadSettings(): read settings file and return a merged object
// (stored values merged over defaults so new keys always have a value).
// Returns BB_SETTINGS_DEFAULTS if file is missing or corrupt.
function loadSettings() {
    var result = {};
    var key;
    for (key in BB_SETTINGS_DEFAULTS) {
        if (Object.prototype.hasOwnProperty.call(BB_SETTINGS_DEFAULTS, key)) {
            result[key] = BB_SETTINGS_DEFAULTS[key];
        }
    }

    var path = _settingsFilePath();
    var f = new File(path);
    if (!f.exists) { return result; }

    f.encoding = 'UTF-8';
    if (!f.open('r')) { return result; }
    var text = '';
    try { text = f.read(); } catch (e) {}
    f.close();

    if (!text) { return result; }

    var stored;
    try { stored = JSON.parse(text); } catch (e) { return result; }
    var hasCleanupPassMode = !!(stored &&
        Object.prototype.hasOwnProperty.call(stored, 'cleanupPassMode'));

    // Merge stored values over defaults (forward-compatible: new keys keep defaults).
    for (key in stored) {
        if (Object.prototype.hasOwnProperty.call(stored, key) &&
            Object.prototype.hasOwnProperty.call(result, key)) {
            result[key] = stored[key];
        }
    }
    if (!hasCleanupPassMode) {
        result.cleanupPassMode =
            (result.promptSecondCleanupPass === true) ? 'prompt' : 'off';
    }
    if (stored &&
            Object.prototype.hasOwnProperty.call(
                stored, 'motionSmoothSourceKeyFidelity') &&
            !Object.prototype.hasOwnProperty.call(
                stored, 'motionSmoothSourceFidelity')) {
        result.motionSmoothSourceFidelity =
            stored.motionSmoothSourceKeyFidelity === true;
    }
    return result;
}
