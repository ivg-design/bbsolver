// bbsolver-test-harness.jsx -- entry point + ScriptUI palette.
// Installed layout: root script in ScriptUI Panels, support files in
// ./bbsolver-test-harness/.

//@include "bbsolver-test-harness/_polyfill.jsx"
//@include "bbsolver-test-harness/settings.jsx"
//@include "bbsolver-test-harness/_lookup.jsx"
//@include "bbsolver-test-harness/sampler.jsx"
//@include "bbsolver-test-harness/serialize_json.jsx"
//@include "bbsolver-test-harness/parse_keys.jsx"
//@include "bbsolver-test-harness/writeback.jsx"
//@include "bbsolver-test-harness/apply.jsx"
//@include "bbsolver-test-harness/verify.jsx"

(function bbsolverTestHarnessPanel(thisObj) {

    var PANEL_VERSION   = '1.1.0';
    var TITLE           = 'bbsolver-test-harness';
    var BBSOLVER_EXPECTED = '1.0.0';
    var DEFAULT_TOL     = 0.5;
    var DEFAULT_TOLPX   = 1.0;
    var MOTION_PATH_SMOOTHING_DEFAULT = 3.0;
    var MOTION_PATH_SMOOTHING_MIN = 1.0;
    var MOTION_PATH_SMOOTHING_MAX = 32.0;
    var MOTION_PATH_FIT_DEFAULT = 1.5;
    var MOTION_PATH_FIT_MIN = 0.1;
    var MOTION_PATH_FIT_MAX = 200.0;
    var MOTION_PATH_BOUNDS_DEFAULT = 0.0;
    var MOTION_PATH_BOUNDS_MIN = 0.0;
    var MOTION_PATH_BOUNDS_MAX = 500.0;
    var MOTION_PATH_SHARP_DEFAULT = 75.0;
    var MOTION_PATH_SHARP_MIN = 1.0;
    var MOTION_PATH_SHARP_MAX = 179.0;

    function _trim(s) {
        return String(s || '').replace(/^\s+|\s+$/g, '');
    }

    // ---- Settings (persistent) -------------------------------------------

    var settings = loadSettings();
    function _saveCurrentSettings() { saveSettings(settings); }

    // ---- Per-property override maps (session-only) -----------------------

    var _propTolMap = {};
    var _propTolPxMap = {};
    var _propSolveModeMap = {};
    var _propCleanupModeMap = {};
    var _propMotionSmoothTolMap = {};
    var _propMotionSmoothBezierMap = {};
    var _solveModeValues = ['full', 'temporal_only', 'vertex_only',
        'motion_smooth', 'motion_path_smooth'];
    var _solveModeLabels = [
        'Full (keys + vertices)',
        'Temporal keys only',
        'Vertex decimation only',
        'Motion smooth',
        'Motion path smooth'
    ];
    var _cleanupModeValues = ['prompt', 'auto', 'off'];
    var _cleanupModeLabels = ['Prompt', 'Auto', 'Off'];

    function _propOverrideValue(map, propId) {
        if (typeof map[propId] === 'number') {
            return map[propId];
        }
        // For separated-dimension ids '<parentId>/sep/<d>', inherit the parent override.
        var sepMark = '/sep/';
        var sepAt = propId.lastIndexOf(sepMark);
        if (sepAt >= 0) {
            var sepParentId = propId.substring(0, sepAt);
            if (typeof map[sepParentId] === 'number') {
                return map[sepParentId];
            }
        }
        return null;
    }

    function _propOverrideString(map, propId) {
        if (typeof map[propId] === 'string' && map[propId] !== '') {
            return map[propId];
        }
        var sepMark = '/sep/';
        var sepAt = propId.lastIndexOf(sepMark);
        if (sepAt >= 0) {
            var sepParentId = propId.substring(0, sepAt);
            if (typeof map[sepParentId] === 'string' &&
                    map[sepParentId] !== '') {
                return map[sepParentId];
            }
        }
        return null;
    }

    function _propOverrideObject(map, propId) {
        if (map[propId] && typeof map[propId] === 'object') {
            return map[propId];
        }
        var sepMark = '/sep/';
        var sepAt = propId.lastIndexOf(sepMark);
        if (sepAt >= 0) {
            var sepParentId = propId.substring(0, sepAt);
            if (map[sepParentId] && typeof map[sepParentId] === 'object') {
                return map[sepParentId];
            }
        }
        return null;
    }

    function _effectiveTol(propId) {
        var value = _propOverrideValue(_propTolMap, propId);
        if (typeof value === 'number') { return value; }
        return settings.tolerance;
    }

    function _effectiveTolPx(propId) {
        var value = _propOverrideValue(_propTolPxMap, propId);
        if (typeof value === 'number') { return value; }
        return settings.toleranceScreenPx;
    }

    function _effectiveMotionSmoothTolerance(propId) {
        var value = _propOverrideValue(_propMotionSmoothTolMap, propId);
        if (typeof value === 'number') { return value; }
        return (typeof settings.motionSmoothTolerance === 'number' &&
                settings.motionSmoothTolerance > 0)
            ? settings.motionSmoothTolerance : 3.0;
    }

    function _motionSmoothTemporalCleanupTolerance() {
        return (typeof settings.motionSmoothTemporalCleanupTolerance === 'number' &&
                settings.motionSmoothTemporalCleanupTolerance > 0)
            ? settings.motionSmoothTemporalCleanupTolerance : 2.0;
    }

    function _clampMotionPathSmoothingTolerance(value) {
        var parsed = parseFloat(value);
        if (!isFinite(parsed) || parsed <= 0) {
            return MOTION_PATH_SMOOTHING_DEFAULT;
        }
        if (parsed < MOTION_PATH_SMOOTHING_MIN) {
            return MOTION_PATH_SMOOTHING_MIN;
        }
        if (parsed > MOTION_PATH_SMOOTHING_MAX) {
            return MOTION_PATH_SMOOTHING_MAX;
        }
        return parsed;
    }

    function _motionPathSmoothingTolerance() {
        return _clampMotionPathSmoothingTolerance(
            settings.motionPathSmoothingTolerance);
    }

    function _clampMotionPathFitTolerance(value) {
        var parsed = parseFloat(value);
        if (!isFinite(parsed) || parsed <= 0) {
            return MOTION_PATH_FIT_DEFAULT;
        }
        if (parsed < MOTION_PATH_FIT_MIN) {
            return MOTION_PATH_FIT_MIN;
        }
        if (parsed > MOTION_PATH_FIT_MAX) {
            return MOTION_PATH_FIT_MAX;
        }
        return parsed;
    }

    function _motionPathAccuracyTolerance() {
        return _clampMotionPathFitTolerance(settings.motionPathAccuracyTolerance);
    }

    function _clampMotionPathBoundsTolerance(value) {
        var parsed = parseFloat(value);
        if (!isFinite(parsed) || parsed < 0) {
            return MOTION_PATH_BOUNDS_DEFAULT;
        }
        if (parsed < MOTION_PATH_BOUNDS_MIN) {
            return MOTION_PATH_BOUNDS_MIN;
        }
        if (parsed > MOTION_PATH_BOUNDS_MAX) {
            return MOTION_PATH_BOUNDS_MAX;
        }
        return parsed;
    }

    function _motionPathBoundsTolerance() {
        return _clampMotionPathBoundsTolerance(
            settings.motionPathBoundsTolerance);
    }

    function _clampMotionPathSharpAngleDeg(value) {
        var parsed = parseFloat(value);
        if (!isFinite(parsed) || parsed <= 0) {
            return MOTION_PATH_SHARP_DEFAULT;
        }
        if (parsed < MOTION_PATH_SHARP_MIN) {
            return MOTION_PATH_SHARP_MIN;
        }
        if (parsed > MOTION_PATH_SHARP_MAX) {
            return MOTION_PATH_SHARP_MAX;
        }
        return parsed;
    }

    function _motionPathSharpAngleDeg() {
        return _clampMotionPathSharpAngleDeg(settings.motionPathSharpAngleDeg);
    }

    function _motionSmoothBezierFromSettings() {
        return {
            x1: (typeof settings.motionSmoothBezierX1 === 'number') ? settings.motionSmoothBezierX1 : 0.33,
            y1: (typeof settings.motionSmoothBezierY1 === 'number') ? settings.motionSmoothBezierY1 : 0.0,
            x2: (typeof settings.motionSmoothBezierX2 === 'number') ? settings.motionSmoothBezierX2 : 0.67,
            y2: (typeof settings.motionSmoothBezierY2 === 'number') ? settings.motionSmoothBezierY2 : 1.0
        };
    }

    function _sanitizeMotionSmoothBezier(curve) {
        curve = curve || {};
        var x1 = parseFloat(curve.x1);
        var y1 = parseFloat(curve.y1);
        var x2 = parseFloat(curve.x2);
        var y2 = parseFloat(curve.y2);
        if (!isFinite(x1)) { x1 = 0.33; }
        if (!isFinite(y1)) { y1 = 0.0; }
        if (!isFinite(x2)) { x2 = 0.67; }
        if (!isFinite(y2)) { y2 = 1.0; }
        x1 = Math.min(Math.max(x1, 0.001), 0.999);
        x2 = Math.min(Math.max(x2, 0.001), 0.999);
        y1 = Math.min(Math.max(y1, 0.0), 1.0);
        y2 = Math.min(Math.max(y2, 0.0), 1.0);
        return { x1: x1, y1: y1, x2: x2, y2: y2 };
    }

    function _effectiveMotionSmoothBezier(propId) {
        var value = _propOverrideObject(_propMotionSmoothBezierMap, propId);
        if (value) { return _sanitizeMotionSmoothBezier(value); }
        return _sanitizeMotionSmoothBezier(_motionSmoothBezierFromSettings());
    }

    function _motionSmoothBezierLabel(curve) {
        curve = _sanitizeMotionSmoothBezier(curve);
        return curve.x1 + ',' + curve.y1 + ',' + curve.x2 + ',' + curve.y2;
    }

    function _hasPropOverride(propId) {
        return typeof _propOverrideValue(_propTolMap, propId) === 'number' ||
            typeof _propOverrideValue(_propTolPxMap, propId) === 'number' ||
            typeof _propOverrideValue(_propMotionSmoothTolMap, propId) === 'number' ||
            _propOverrideObject(_propMotionSmoothBezierMap, propId) !== null ||
            _propOverrideString(_propSolveModeMap, propId) !== null ||
            _propOverrideString(_propCleanupModeMap, propId) !== null;
    }

    function _isRotationPropertyInfo(pinfo) {
        var mn = '';
        try { mn = pinfo.match_name || pinfo.matchName || ''; } catch (e) {}
        return mn === 'ADBE Rotate X' || mn === 'ADBE Rotate Y' || mn === 'ADBE Rotate Z';
    }

    // ---- Cancel state ----------------------------------------------------
    // v1 cancel mechanics: system.callSystem is synchronous, so clicking Cancel
    // during a bbsolver invocation is not possible (UI is frozen). Cancel takes
    // effect between tolerance-group iterations and is also passed to bbsolver
    // via --cancel-file. If bbsolver polls the file before finishing, it exits 5.
    // v2 will use CEP child_process for true async cancellation.

    var _cancelFile = '';       // absolute path to sentinel file for this bake
    var _cancelRequested = false;

    function _writeCancelFile() {
        if (!_cancelFile) { return; }
        var f = new File(_cancelFile);
        if (f.exists) { return; }
        // Verified: File.open('w') creates file (js-tools-guide p.331).
        if (f.open('w')) { f.close(); }
    }

    function _cleanCancelFile() {
        if (!_cancelFile) { return; }
        var f = new File(_cancelFile);
        // Verified: File.remove() deletes the file (js-tools-guide p.438).
        if (f.exists) { f.remove(); }
        _cancelFile = '';
        _cancelRequested = false;
        _setAbortEnabled(false);
        _stopSolveElapsedTimer();
    }

    function _isCancelled() {
        if (_cancelRequested) { return true; }
        if (_cancelFile) {
            var f = new File(_cancelFile);
            return f.exists;
        }
        return false;
    }

    function _setAbortEnabled(enabled) {
        try {
            if (ui && ui.abortBtn) {
                ui.abortBtn.enabled = enabled === true;
            }
        } catch (e) {}
    }

    function _requestSafeAbort() {
        if (!_cancelFile) {
            log('No active solve to abort.');
            return;
        }
        _cancelRequested = true;
        _writeCancelFile();
        if (_activeBbsolverState) {
            _abortCooperativeBbsolver(_activeBbsolverState, 'user requested abort');
        }
        _setAbortEnabled(false);
        setStatusText('Abort requested; stopping solver');
        log('Abort requested; sent stop signal to the active external solver.');
    }

    // ---- Separation confirmation -----------------------------------------

    // Count how many props will have dimensionsSeparated set for the first time.
    // A prop qualifies if: isSpatial, isSeparationLeader, dimensionsSeparated===false,
    // numKeys===0, and autoSeparateForBake is enabled.
    // Verified: prop.isSeparationLeader, prop.dimensionsSeparated (property.md pp.190,361).
    function _countPropsNeedingSeparation(props) {
        var count = 0;
        var i, prop;
        for (i = 0; i < props.length; i++) {
            prop = props[i];
            var isSpatial = false;
            var isLeader  = false;
            var separated = true;  // default to true so we don't warn spuriously
            var nk        = 0;
            try {
                var pvt = prop.propertyValueType;
                isSpatial = (pvt === PropertyValueType.TwoD_SPATIAL ||
                             pvt === PropertyValueType.ThreeD_SPATIAL);
            } catch (e) {}
            if (!isSpatial) { continue; }
            if (settings.flattenParentedPosition === true &&
                    _isParentedPositionFlattenCandidate(prop, _classifyProperty(prop))) {
                continue;
            }
            try { isLeader  = prop.isSeparationLeader;   } catch (e) {}
            try { separated = prop.dimensionsSeparated;  } catch (e) {}
            try { nk        = prop.numKeys;              } catch (e) {}
            if (isLeader && !separated && nk === 0) { count = count + 1; }
        }
        return count;
    }

    // Show the pre-bake confirmation when separation is needed.
    // Returns true if user confirmed, false if cancelled.
    function _confirmSeparation(count) {
        var dlg = new Window('dialog', 'bbsolver-test-harness: Separate Dimensions Required');
        dlg.orientation = 'column';
        dlg.alignChildren = ['fill', 'top'];
        dlg.spacing = 8;
        dlg.add('statictext', undefined,
            count + ' position propert' + (count === 1 ? 'y' : 'ies') +
            ' will have "Separate Dimensions" permanently enabled.');
        dlg.add('statictext', undefined,
            'This cannot be reversed via Ctrl+Z.');
        dlg.add('statictext', undefined,
            'Separate Dimensions lets bbsolver fit each axis independently,');
        dlg.add('statictext', undefined,
            'but it changes the Position property into X/Y/Z axes.');
        dlg.add('statictext', undefined,
            'To preserve the unified motion path, cancel and leave this setting off.');
        var btnGrp = dlg.add('group');
        btnGrp.alignment = 'right';
        var okBtn  = btnGrp.add('button', undefined, 'Continue', { name: 'ok' });
        var canBtn = btnGrp.add('button', undefined, 'Cancel',   { name: 'cancel' });
        var result = false;
        okBtn.onClick  = function () { result = true;  dlg.close(1); };
        canBtn.onClick = function () { result = false; dlg.close(0); };
        dlg.show();
        return result;
    }

    // ---- Session state ---------------------------------------------------

    var _lastBundle = null;     // most recently sampled SampleBundle
    var _lastBundleSignature = '';
    var _sampleBundleCache = {};
    var _sampleBundleCacheKeys = [];
    var SAMPLE_BUNDLE_CACHE_LIMIT = 24;

    function _sampleCacheKey(signature) {
        return 'sig_' + _hashString(signature || '');
    }

    function _cachedSampleBundle(signature) {
        var key = _sampleCacheKey(signature);
        return _sampleBundleCache[key] || null;
    }

    function _rememberSampleBundle(signature, bundle) {
        if (!signature || !bundle) { return; }
        var key = _sampleCacheKey(signature);
        if (!_sampleBundleCache[key]) {
            _sampleBundleCacheKeys.push(key);
        }
        _sampleBundleCache[key] = bundle;
        while (_sampleBundleCacheKeys.length > SAMPLE_BUNDLE_CACHE_LIMIT) {
            delete _sampleBundleCache[_sampleBundleCacheKeys.shift()];
        }
    }

    function _clearSampleBundleCache() {
        _sampleBundleCache = {};
        _sampleBundleCacheKeys = [];
        _lastBundleSignature = '';
    }

    // ---- OS / path helpers -----------------------------------------------

    function _ensureFolderPath(path) {
        var normalized = String(path || '').replace(/\\/g, '/');
        if (!normalized) { return false; }
        var parts = normalized.split('/');
        var cur = '';
        var i, part, f;
        if (normalized.charAt(0) === '/') {
            cur = '';
            i = 1;
        } else {
            cur = parts[0];
            i = 1;
        }
        for (; i < parts.length; i++) {
            part = parts[i];
            if (!part) { continue; }
            cur = cur ? (cur + '/' + part) : ('/' + part);
            f = new Folder(cur);
            if (!f.exists) {
                try {
                    if (!f.create()) { return false; }
                } catch (e) {
                    return false;
                }
            }
        }
        return (new Folder(normalized)).exists;
    }

    function _cacheScratchDir() {
        var base = _isWindowsPlatform()
            ? Folder.userData.fsName + '/bbsolver-test-harness'
            : Folder.userData.parent.fsName + '/Caches/bbsolver-test-harness';
        _ensureFolderPath(base);
        return base;
    }

    function _repoArtifactRootFromBbsolver(bbsolver) {
        var normalized = String(bbsolver || '').replace(/\\/g, '/');
        var marker = '/build/';
        var idx = normalized.indexOf(marker);
        if (idx < 0) { return ''; }
        return normalized.substring(0, idx) + '/artifacts/bbsolver/corpus';
    }

    function _repoArtifactRootFromHome() {
        if (_isWindowsPlatform()) { return ''; }
        var home = Folder.userData.parent.parent.fsName;
        return home + '/github/bbsolver/artifacts/bbsolver/corpus';
    }

    function _liveRunRoot(root) {
        root = String(root || '').replace(/\\/g, '/');
        if (!root) { return ''; }
        if (root.match(/\/live_runs$/)) { return root; }
        return root + '/live_runs';
    }

    function _candidateArtifactRoots(bbsolver) {
        var out = [];
        var envRoot = null;
        var repoRoot = _repoArtifactRootFromBbsolver(bbsolver);
        var homeRepoRoot = _repoArtifactRootFromHome();
        if (repoRoot) { out.push(_liveRunRoot(repoRoot)); }
        if (homeRepoRoot) { out.push(_liveRunRoot(homeRepoRoot)); }
        try { envRoot = $.getenv('BBSOLVER_ARTIFACT_ROOT'); } catch (e) {}
        if (settings.artifactRoot) { out.push(_liveRunRoot(settings.artifactRoot)); }
        if (envRoot) { out.push(_liveRunRoot(envRoot)); }
        return out;
    }

    function _scratchDir(bbsolver) {
        var roots = _candidateArtifactRoots(bbsolver);
        var i, root, f;
        for (i = 0; i < roots.length; i++) {
            root = String(roots[i] || '').replace(/\\/g, '/');
            if (!root) { continue; }
            if (_ensureFolderPath(root)) {
                f = new Folder(root);
                if (f.exists) { return f.fsName; }
            }
        }
        return _cacheScratchDir();
    }

    function _runScratchDir(bbsolver, requestId) {
        var root = _scratchDir(bbsolver);
        var dir = root + '/' + requestId;
        if (_ensureFolderPath(dir)) { return dir; }
        return root;
    }


    // VERIFY-IN-AE: $.getenv() on macOS reads system + launchctl vars only.
    function _findBbsolver() {
        if (settings.bbsolverPath) { return settings.bbsolverPath; }
        var envBin = null;
        try { envBin = $.getenv('BBSOLVER_BIN'); } catch (e) {}
        if (envBin) { return envBin; }
        var i, f, candidates;
        if (_isWindowsPlatform()) {
            // On Windows Folder.userData is typically
            // C:/Users/<name>/AppData/Roaming. Probe both per-user and
            // system-wide install locations for the .exe build.
            var userData = Folder.userData.fsName;
            candidates = [
                userData + '/bbsolver/bin/bbsolver.exe',
                'C:/Program Files/bbsolver/bin/bbsolver.exe',
                'C:/Program Files (x86)/bbsolver/bin/bbsolver.exe'
            ];
            for (i = 0; i < candidates.length; i++) {
                f = new File(candidates[i]);
                if (f.exists) { return candidates[i]; }
            }
            return 'bbsolver.exe';
        }
        var home = Folder.userData.parent.parent.fsName;
        candidates = [
            home + '/.bbsolver/bin/bbsolver',
            '/opt/homebrew/bin/bbsolver',
            '/usr/local/bin/bbsolver'
        ];
        for (i = 0; i < candidates.length; i++) {
            f = new File(candidates[i]);
            if (f.exists) { return candidates[i]; }
        }
        return 'bbsolver';
    }

    // ---- UI refs ---------------------------------------------------------

    var ui;
    var _logBuffer = '';
    var _logTail = '';
    var _logFlushDeadlineMs = 0;
    var _logFlushScheduled = false;
    var _fullLogText = '';
    var _exportLogPath = '';
    var _lastProgressPaintMs = 0;
    var _lastPaintedProgressValue = -1;
    var _solveElapsedStartMs = 0;
    var _solveElapsedActive = false;
    var _solveElapsedSchedulePending = false;
    var _solveElapsedToken = 0;
    var _solveElapsedBusyPaintMs = 0;

    function _timestampForLog() {
        var d = new Date();
        return d.getFullYear() +
            _twoDigits(d.getMonth() + 1) +
            _twoDigits(d.getDate()) + '-' +
            _twoDigits(d.getHours()) +
            _twoDigits(d.getMinutes()) +
            _twoDigits(d.getSeconds());
    }

    function _defaultLogDir() {
        try { return Folder.desktop.fsName + '/bbsolver-test-harness-logs'; }
        catch (eDesktop) { return _cacheScratchDir() + '/logs'; }
    }

    function _ensureExportLogPath() {
        if (settings.exportLogs !== true) { return ''; }
        if (_exportLogPath) { return _exportLogPath; }
        var dir = String(settings.logOutputDir || '');
        if (!dir) { dir = _defaultLogDir(); }
        if (!_ensureFolderPath(dir)) { return ''; }
        _exportLogPath = dir + '/bbsolver-test-harness-' +
            _timestampForLog() + '.log.txt';
        return _exportLogPath;
    }

    function _writeExportedLog() {
        var path = _ensureExportLogPath();
        if (!path) { return; }
        var f = new File(path);
        f.encoding = 'UTF-8';
        try {
            if (f.open('w')) {
                f.write(_fullLogText);
                f.close();
            }
        } catch (eWriteLog) {
            try { f.close(); } catch (eCloseLog) {}
        }
    }

    function _flushLog(force) {
        if (!_logBuffer) {
            _logFlushScheduled = false;
            return true;
        }
        var now = (new Date()).getTime();
        if (!force && _logFlushDeadlineMs > 0 && now < _logFlushDeadlineMs) {
            return false;
        }
        if (ui && ui.logBox) {
            var next = _logBuffer + _logTail;
            if (next.length > 60000) { next = next.substr(0, 60000); }
            try {
                ui.logBox.text = next;
                _logTail = next;
                _logBuffer = '';
                _logFlushDeadlineMs = now + 100;
                _logFlushScheduled = false;
                return true;
            } catch (eSet) {}
        }
        return false;
    }

    function _scheduleLogFlush(delayMs) {
        if (_logFlushScheduled) { return; }
        _logFlushScheduled = true;
        try {
            app.scheduleTask('$.global.bbsolverHarnessFlushLog()', delayMs, false);
        } catch (eSchedule) {
            _logFlushScheduled = false;
        }
    }

    function log(msg) {
        var line = _timestampForLog() + '  ' + msg;
        _fullLogText += line + '\n';
        _writeExportedLog();
        _logBuffer = line + '\n' + _logBuffer;
        if (_logBuffer.length > 12000) {
            _flushLog(true);
        } else if (!_flushLog(false)) {
            _scheduleLogFlush(100);
        }
    }
    function clearLog() {
        _logBuffer = '';
        _logTail = '';
        _fullLogText = '';
        _exportLogPath = '';
        _logFlushDeadlineMs = 0;
        _logFlushScheduled = false;
        if (ui && ui.logBox) { ui.logBox.text = ''; }
    }

    try {
        $.global.bbsolverHarnessFlushLog = function () { _flushLog(true); };
    } catch (eRegisterLogFlush) {}

    function _twoDigits(n) {
        return (n < 10 ? '0' : '') + n;
    }

    function _formatElapsed(ms) {
        var total = Math.floor(ms / 1000);
        if (total < 0) { total = 0; }
        var hours = Math.floor(total / 3600);
        var mins = Math.floor((total % 3600) / 60);
        var secs = total % 60;
        if (hours > 0) {
            return hours + ':' + _twoDigits(mins) + ':' + _twoDigits(secs);
        }
        return _twoDigits(mins) + ':' + _twoDigits(secs);
    }

    function _paintSolveElapsed() {
        if (!ui || !ui.elapsedLabel || !_solveElapsedStartMs) { return; }
        var elapsed = (new Date()).getTime() - _solveElapsedStartMs;
        try { ui.elapsedLabel.text = 'Elapsed ' + _formatElapsed(elapsed); }
        catch (eElapsedLabel) {}
        try { ui.update(); } catch (eElapsedUpdate) {}
    }

    function _paintSolveElapsedDuringBusyWork() {
        if (!_solveElapsedActive) { return; }
        var now = (new Date()).getTime();
        if (now - _solveElapsedBusyPaintMs < 500) { return; }
        _solveElapsedBusyPaintMs = now;
        _paintSolveElapsed();
    }

    function _scheduleSolveElapsedTick() {
        if (!_solveElapsedActive || _solveElapsedSchedulePending) { return; }
        _solveElapsedSchedulePending = true;
        try {
            app.scheduleTask(
                '$.global.bbsolverHarnessElapsedTick(' + _solveElapsedToken + ')',
                1000, false);
        } catch (eScheduleElapsed) {
            _solveElapsedSchedulePending = false;
        }
    }

    function _solveElapsedTick(token) {
        _solveElapsedSchedulePending = false;
        if (!_solveElapsedActive || token !== _solveElapsedToken) { return; }
        _paintSolveElapsed();
        _scheduleSolveElapsedTick();
    }

    function _startSolveElapsedTimer() {
        _solveElapsedToken++;
        _solveElapsedStartMs = (new Date()).getTime();
        _solveElapsedActive = true;
        _solveElapsedSchedulePending = false;
        _solveElapsedBusyPaintMs = 0;
        _paintSolveElapsed();
        _scheduleSolveElapsedTick();
    }

    function _stopSolveElapsedTimer() {
        if (!_solveElapsedActive) { return; }
        _paintSolveElapsed();
        _solveElapsedActive = false;
        _solveElapsedSchedulePending = false;
        _solveElapsedToken++;
        _solveElapsedBusyPaintMs = 0;
    }

    try {
        $.global.bbsolverHarnessElapsedTick = function (token) {
            _solveElapsedTick(token);
        };
    } catch (eRegisterElapsedTick) {}

    function _clampProgressPct(pct) {
        var v = (typeof pct === 'number') ? pct : 0;
        if (v < 0) { v = 0; }
        if (v > 100) { v = 100; }
        return Math.round(v);
    }

    function _updateProgressMeter(pct) {
        if (!ui || !ui.progressView) { return; }
        var value = _clampProgressPct(pct);
        var now = (new Date()).getTime();
        if (value === _lastPaintedProgressValue &&
                now - _lastProgressPaintMs < 100) {
            _flushLog(false);
            return;
        }
        _lastPaintedProgressValue = value;
        _lastProgressPaintMs = now;
        try { ui.progressView.value = value; } catch (eNativeVal) {}
        try { ui.progressView._value = value; } catch (eVal) {}
        try { ui.progressView.notify('onDraw'); } catch (eDraw) {}
        if (ui.progressPctLabel) {
            try { ui.progressPctLabel.text = value + '%'; } catch (eLbl) {}
        }
        _flushLog(false);
        try { ui.update(); } catch (eUpdate) {}
    }

    function setProgress(pct) {
        var value = _clampProgressPct(pct);
        _updateProgressMeter(value);
    }

    function _drawProgressView() {
        try {
            var g = this.graphics;
            var w = this.size ? this.size[0] : 390;
            var h = this.size ? this.size[1] : 12;
            if (w <= 0) { w = 390; }
            if (h <= 0) { h = 12; }
            var value = (typeof this._value === 'number') ? this._value : 0;
            if (value < 0) { value = 0; }
            if (value > 100) { value = 100; }
            var fillW = Math.round(w * value / 100);
            if (fillW < 1 && value > 0) { fillW = 1; }
            if (fillW > w) { fillW = w; }
            var bgBrush = g.newBrush(g.BrushType.SOLID_COLOR, [0.18, 0.18, 0.18, 1]);
            var fillBrush = g.newBrush(g.BrushType.SOLID_COLOR, [0.16, 0.52, 0.92, 1]);
            var borderPen = g.newPen(g.PenType.SOLID_COLOR, [0.05, 0.05, 0.05, 1], 1);
            g.newPath();
            g.rectPath(0, 0, w, h);
            g.fillPath(bgBrush);
            if (fillW > 0) {
                g.newPath();
                g.rectPath(0, 0, fillW, h);
                g.fillPath(fillBrush);
            }
            g.newPath();
            g.rectPath(0.5, 0.5, w - 1, h - 1);
            g.strokePath(borderPen);
        } catch (e) {}
    }

    // Status text shown above the progress bar. It pairs with setProgress so
    // the user can tell which phase the panel is in even when the AE UI
    // is between repaint cycles during AE work or solver progress polling.
    function setStatusText(text) {
        if (ui && ui.statusLabel) {
            try { ui.statusLabel.text = text || ''; } catch (e) {}
        }
    }

    // Combined helper: update the status label, the progress bar, and append
    // a 'phase: <text>' line to the log. Returns the millisecond timestamp at
    // which the phase started; pass the prior return value as prevStart to
    // get a 'phase done: Ns' line that prints the duration of the previous
    // phase. The elapsed line never replaces the new phase header -- both go
    // to the log so a user scrolling the log post-bake can see the full
    // timeline.
    function setStatus(text, pct, prevStart) {
        var now = (new Date()).getTime();
        if (prevStart && typeof prevStart === 'number') {
            var elapsed = now - prevStart;
            if (elapsed >= 100) {
                var secs = (elapsed / 1000).toFixed(1);
                log('  phase done: ' + secs + 's');
            }
        }
        if (typeof pct === 'number') { setProgress(pct); }
        if (text) {
            setStatusText(text + (typeof pct === 'number'
                ? ' (' + pct + '%)' : ''));
            log('phase: ' + text +
                (typeof pct === 'number' ? ' (' + pct + '%)' : ''));
        }
        return now;
    }

    function _controlSize(ctrl, axis) {
        try {
            if (ctrl.size && typeof ctrl.size[axis] === 'number') {
                return ctrl.size[axis];
            }
        } catch (e1) {}
        try {
            if (ctrl.bounds) {
                if (axis === 'width') {
                    return ctrl.bounds[2] - ctrl.bounds[0];
                }
                return ctrl.bounds[3] - ctrl.bounds[1];
            }
        } catch (e2) {}
        return 0;
    }

    function _resizeMainLog(panel) {
        if (!panel || !panel.logBox) { return; }
        var w = _controlSize(panel, 'width');
        var h = _controlSize(panel, 'height');
        if (w <= 0 || h <= 0) { return; }

        panel.logBox.preferredSize = [
            Math.max(320, w - 40),
            Math.max(90, h - 280)
        ];
    }

    // ---- Listbox label builder -------------------------------------------

    function _friendlyPropertyLabel(info) {
        if (!info) { return ''; }
        if (info.layer_path) { return info.layer_path; }
        if (info.display_name) { return info.display_name; }
        return info.id || '';
    }

    function _propertyLabelMap(bundle) {
        var map = {};
        if (!bundle || !bundle.properties) { return map; }
        var i, info;
        for (i = 0; i < bundle.properties.length; i++) {
            info = bundle.properties[i].property || {};
            if (info.id) { map[info.id] = _friendlyPropertyLabel(info); }
        }
        return map;
    }

    function _propertyLabelForId(propId, labelMap) {
        if (labelMap && labelMap[propId]) { return labelMap[propId]; }
        return propId;
    }

    function _copyPlainObject(obj) {
        var out = {};
        var k;
        if (!obj) { return out; }
        for (k in obj) {
            if (obj.hasOwnProperty(k)) { out[k] = obj[k]; }
        }
        return out;
    }

    function _propertyInfoMap(bundle) {
        var map = {};
        if (!bundle || !bundle.properties) { return map; }
        var i, info;
        for (i = 0; i < bundle.properties.length; i++) {
            info = bundle.properties[i].property || {};
            if (info.id) { map[info.id] = info; }
        }
        return map;
    }

    function _splitLayerPath(path) {
        var raw = String(path || '').split('/');
        var out = [];
        var i;
        for (i = 0; i < raw.length; i++) {
            if (raw[i] !== '') { out.push(raw[i]); }
        }
        return out;
    }

    function _layerNameFromInfo(info, comp) {
        var parts = _splitLayerPath(info ? info.layer_path : '');
        var offset = 0;
        if (parts.length > 1) {
            try {
                if (comp && parts[0] === comp.name) { offset = 1; }
            } catch (eCompName) {}
        }
        return parts.length > offset ? parts[offset] : '';
    }

    function _resolvedPropLayerName(prop) {
        try {
            var layer = _containingLayer(prop);
            return layer ? String(layer.name || '') : '';
        } catch (eLayerName) {}
        return '';
    }

    function _findLayerByName(comp, layerName) {
        if (!comp || !layerName) { return null; }
        var layer = null;
        try { layer = comp.layer(layerName); } catch (eDirectLayer) {}
        if (layer) { return layer; }
        var i, n, candidate;
        try { n = comp.numLayers || 0; } catch (eNumLayers) { n = 0; }
        for (i = 1; i <= n; i++) {
            try { candidate = comp.layer(i); } catch (eLayer) { continue; }
            try {
                if (candidate && candidate.name === layerName) { return candidate; }
            } catch (eLayerCmp) {}
        }
        return null;
    }

    function _findLayerByStableId(comp, layerId) {
        layerId = parseInt(layerId, 10);
        if (!comp || !isFinite(layerId) || layerId <= 0) { return null; }
        var i, n, candidate, candidateId;
        try { n = comp.numLayers || 0; } catch (eNumLayers) { n = 0; }
        for (i = 1; i <= n; i++) {
            try { candidate = comp.layer(i); } catch (eLayer) { continue; }
            candidateId = _layerStableIdFromLayer(candidate);
            if (candidateId === layerId) { return candidate; }
        }
        return null;
    }

    function _childByNameOrMatch(parent, name) {
        if (!parent || !name) { return null; }
        var child = null;
        try { child = parent.property(name); } catch (eDirect) {}
        if (child) { return child; }

        var n = 0, i, candidate;
        try { n = parent.numProperties || 0; } catch (eCount) { n = 0; }
        for (i = 1; i <= n; i++) {
            try { candidate = parent.property(i); } catch (eChild) { continue; }
            try {
                if (candidate &&
                        (candidate.name === name || candidate.matchName === name)) {
                    return candidate;
                }
            } catch (eCmp) {}
        }
        return null;
    }

    function _contentsChild(parent) {
        return _childByNameOrMatch(parent, 'Contents') ||
               _childByNameOrMatch(parent, 'ADBE Root Vectors Group') ||
               _childByNameOrMatch(parent, 'ADBE Vectors Group') ||
               _childByNameOrMatch(parent, 'ADBE Mask Parade');
    }

    function _resolvePropertyByPathPartsFromLayer(info, comp, layer) {
        if (!info || !info.layer_path) { return null; }
        var parts = _splitLayerPath(info.layer_path);
        if (parts.length < 2) { return null; }

        var offset = 0;
        try {
            if (comp && parts[0] === comp.name) { offset = 1; }
        } catch (eCompName) {}

        var cur = layer;
        if (!cur) { return null; }

        var i, seg, next;
        for (i = offset + 1; i < parts.length; i++) {
            seg = parts[i];
            if (seg === 'Contents') {
                next = _contentsChild(cur);
            } else if (i === parts.length - 1 && info.match_name) {
                next = _childByNameOrMatch(cur, info.match_name) ||
                       _childByNameOrMatch(cur, seg);
            } else {
                next = _childByNameOrMatch(cur, seg);
            }
            if (!next) { return null; }
            cur = next;
        }

        try {
            if (cur.propertyType !== PropertyType.PROPERTY) { return null; }
        } catch (eType) { return null; }
        return cur;
    }

    function _resolvePropertyByLayerPath(info, comp) {
        if (!info || !info.layer_path) { return null; }
        var parts = _splitLayerPath(info.layer_path);
        if (parts.length < 2) { return null; }

        var offset = 0;
        try {
            if (comp && parts[0] === comp.name) { offset = 1; }
        } catch (eCompName) {}

        var layerName = parts[offset];
        var layer = _findLayerByName(comp, layerName);
        return _resolvePropertyByPathPartsFromLayer(info, comp, layer);
    }

    function _layerStableIdFromLayer(layer) {
        try {
            if (typeof layer.id === 'number' && layer.id > 0) {
                return layer.id;
            }
        } catch (eLayerId) {}
        return 0;
    }

    function _sampledInfoLayerId(info) {
        var id = info ? parseInt(info.layer_id, 10) : 0;
        return isFinite(id) && id > 0 ? id : 0;
    }

    function _sampledInfoLayerIndex(info) {
        var idx = info ? parseInt(info.layer_index, 10) : 0;
        return isFinite(idx) && idx > 0 ? idx : 0;
    }

    function _resolvedPropertyMatchesSampledInfo(info, prop) {
        if (!info || !prop) { return false; }
        var layer = null;
        try { layer = _containingLayer(prop); } catch (eLayer) {}
        if (!layer) { return false; }

        var expectedLayerId = _sampledInfoLayerId(info);
        var actualLayerId = _layerStableIdFromLayer(layer);
        if (expectedLayerId > 0 && actualLayerId > 0) {
            return expectedLayerId === actualLayerId;
        }

        var expectedLayerIndex = _sampledInfoLayerIndex(info);
        if (expectedLayerIndex > 0) {
            try {
                if (layer.index !== expectedLayerIndex) { return false; }
            } catch (eIndex) { return false; }
        }
        return true;
    }

    function _resolvePropertyByLayerStableId(info, comp) {
        if (!info || !comp) { return null; }
        var layer = _findLayerByStableId(comp, _sampledInfoLayerId(info));
        if (!layer) { return null; }

        var prop = null;
        var id = String(info.id || '');
        var slash = id.indexOf('/');
        if (slash >= 0) {
            try {
                prop = parseSepId('L' + layer.index + id.substring(slash), comp);
            } catch (eStableIdPath) { prop = null; }
            if (prop) { return prop; }
        }

        return _resolvePropertyByPathPartsFromLayer(info, comp, layer);
    }

    function _resolveSampledProperty(info, comp) {
        if (!info || !comp) { return null; }
        var prop = null;
        try { prop = _resolvePropertyByLayerStableId(info, comp); } catch (eStableLayer) {}
        if (prop && _resolvedPropertyMatchesSampledInfo(info, prop)) {
            return prop;
        }
        try { prop = _resolvePropertyByLayerPath(info, comp); } catch (ePath) {}
        if (prop && _resolvedPropertyMatchesSampledInfo(info, prop)) {
            return prop;
        }
        try { prop = parseSepId(info.id, comp); } catch (eId) { prop = null; }
        if (prop && _resolvedPropertyMatchesSampledInfo(info, prop)) {
            return prop;
        }
        return null;
    }

    function _refreshedPropertyInfo(info, prop, comp) {
        var out = _copyPlainObject(info);
        var layer = null;
        try { layer = _containingLayer(prop); } catch (eLayer) {}
        if (layer) {
            try { out.layer_id = _layerStableIdFromLayer(layer); } catch (eId) {}
            try { out.layer_index = layer.index; } catch (eIdx) {}
            try {
                if (typeof _buildLayerPath === 'function') {
                    out.layer_path = _buildLayerPath(
                        prop, layer.name || '', comp ? comp.name || '' : '');
                }
            } catch (ePath) {}
        }
        try { out.match_name = prop.matchName || out.match_name; } catch (eMatch) {}
        try { out.display_name = prop.name || out.display_name; } catch (eName) {}
        return out;
    }

    function _logPathReplacementNotes(keyBundle, labelMap) {
        if (!keyBundle || !keyBundle.property_results) { return; }
        var i, pk, notes, shortNotes, gateMsg, vertexMsg, vertexSummary;
        var advisoryMsg, smoothMsg, label;
        for (i = 0; i < keyBundle.property_results.length; i++) {
            pk = keyBundle.property_results[i];
            notes = pk.notes || '';
            if (notes.indexOf('path_replacement') < 0 &&
                    notes.indexOf('replacement_temporal') < 0 &&
                    notes.indexOf('replacement_rejected') < 0 &&
                    notes.indexOf('post_solve_vertex_reduction') < 0 &&
                    notes.indexOf('temporal_fallback_used') < 0 &&
                    notes.indexOf('optimization_blocker=accuracy_gate') < 0 &&
                    notes.indexOf('no_practical_optimization_at_accuracy_gate') < 0 &&
                    notes.indexOf('marginal_optimization_at_accuracy_gate') < 0 &&
                    notes.indexOf('subpath_segment_lower_bound_top') < 0 &&
                    notes.indexOf('subpath_segment_rejection_top') < 0 &&
                    notes.indexOf('shape_flat_already_near_optimal') < 0 &&
                    notes.indexOf('motion_smooth_recommended') < 0 &&
                    notes.indexOf('motion_smooth_shape_rove_time') < 0 &&
                    notes.indexOf('motion_smooth_shape_trajectory_filter') < 0 &&
                    notes.indexOf('motion_smooth_spatial_trajectory_filter') < 0 &&
                    notes.indexOf('near_miss') < 0) {
                continue;
            }
            shortNotes = notes;
            if (shortNotes.length > 360) {
                shortNotes = shortNotes.substring(0, 357) + '...';
            }
            label = _propertyLabelForId(pk.property_id, labelMap);
            log('  [path] ' + label + ': ' + shortNotes);
            vertexSummary = _pathVertexSummaryMessage(notes);
            if (vertexSummary) { log('  [path] ' + label + ': ' + vertexSummary); }
            advisoryMsg = _pathOptimizationAdvisoryMessage(notes);
            if (advisoryMsg) { log('  [path] ' + label + ': ' + advisoryMsg); }
            smoothMsg = _motionSmoothResultMessage(notes);
            if (smoothMsg) { log('  [path] ' + label + ': ' + smoothMsg); }
            gateMsg = _optimizationAccuracyGateMessage(notes);
            if (gateMsg) { log('  [path] ' + label + ': ' + gateMsg); }
            vertexMsg = _vertexOptimizationGateMessage(notes);
            if (vertexMsg) { log('  [path] ' + label + ': ' + vertexMsg); }
        }
    }

    function _optimizationAccuracyGateMessage(notes) {
        notes = notes || '';
        if (notes.indexOf('no_practical_optimization_at_accuracy_gate=true') >= 0) {
            return 'Optimization: no practical key reduction at this accuracy gate; ' +
                'increase tolerance or screen-px tolerance to attempt more reduction.';
        }
        if (notes.indexOf('marginal_optimization_at_accuracy_gate=true') >= 0) {
            return 'Optimization: reduction is marginal at this accuracy gate; ' +
                'increase tolerance or screen-px tolerance to attempt more reduction.';
        }
        return '';
    }

    function _formatNoteNumber(value, decimals) {
        var n = parseFloat(value);
        if (isNaN(n) || !isFinite(n)) { return value || ''; }
        return n.toFixed(decimals);
    }

    function _noteFieldValue(notes, key, afterMarker) {
        notes = notes || '';
        var startAt = 0;
        if (afterMarker) {
            startAt = notes.indexOf(afterMarker);
            if (startAt < 0) { return ''; }
        }
        var needle = key + '=';
        var idx = notes.indexOf(needle, startAt);
        if (idx < 0) { return ''; }
        idx = idx + needle.length;
        var end = notes.indexOf(';', idx);
        if (end < 0) { end = notes.length; }
        return _trim(notes.substring(idx, end));
    }

    function _pathOptimizationAdvisoryMessage(notes) {
        notes = notes || '';
        if (notes.indexOf('shape_flat_already_near_optimal=true') < 0 &&
                notes.indexOf('motion_smooth_recommended=true') < 0) {
            return '';
        }
        var sourceKeys = _noteFieldValue(notes, 'source_key_count', '');
        var sourceVertices = _noteFieldValue(notes, 'source_vertices', '');
        var inputSamples = _noteFieldValue(notes, 'input_samples', '');
        var parts = [];
        if (sourceKeys) { parts.push(sourceKeys + ' source keys'); }
        if (sourceVertices) { parts.push(sourceVertices + ' vertices'); }
        if (inputSamples) { parts.push(inputSamples + ' samples'); }
        return 'Path already near optimal' +
            (parts.length ? ' (' + parts.join(', ') + ')' : '') +
            '; use Motion Smooth mode for trajectory smoothing instead of ' +
            'vertex/key reduction.';
    }

    function _motionSmoothResultMessage(notes) {
        notes = notes || '';
        var motionPath =
            notes.indexOf('motion_path_spatial_trajectory_filter=true') >= 0;
        var shapeRove = notes.indexOf('motion_smooth_shape_rove_time=true') >= 0;
        if (!shapeRove &&
                notes.indexOf('motion_smooth_shape_trajectory_filter=true') < 0 &&
                notes.indexOf('motion_smooth_spatial_trajectory_filter=true') < 0 &&
                !motionPath) {
            return '';
        }
        var sourceKeys = _noteFieldValue(notes, 'source_key_count', '');
        var outputKeys = _noteFieldValue(notes, 'output_keys', '');
        var strength = _noteFieldValue(notes, 'smoothing_strength', '');
        var maxDisp = _noteFieldValue(notes, 'max_smoothing_displacement', '');
        if (!maxDisp) {
            maxDisp = _noteFieldValue(notes, 'raw_source_max_displacement', '');
        }
        var smoothedPathErr =
            _noteFieldValue(notes, 'smoothed_path_max_err', '');
        var pathAccuracy =
            _noteFieldValue(notes, 'motion_path_accuracy_tolerance', '');
        var preserveBounds =
            _noteFieldValue(notes, 'motion_path_preserve_bounds', '');
        var boundsTolerance =
            _noteFieldValue(notes, 'motion_path_bounds_tolerance', '');
        var boundsDeviation =
            _noteFieldValue(notes, 'bounds_max_deviation', '');
        var sharpPoints =
            _noteFieldValue(notes, 'motion_path_sharp_points', '');
        var keyedPoints =
            _noteFieldValue(notes, 'motion_path_keyed_points', '');
        var maxControlDisp = _noteFieldValue(
            notes, 'max_control_smoothing_displacement', '');
        var passes = _noteFieldValue(notes, 'smoothing_passes', '');
        var schedule = _noteFieldValue(notes, 'key_schedule', '');
        var maxShift = _noteFieldValue(notes, 'rove_max_time_shift_sec', '');
        var totalTravel = _noteFieldValue(notes, 'rove_total_travel', '');
        var turnBefore = _noteFieldValue(notes, 'trajectory_turn_before_deg', '');
        var turnAfter = _noteFieldValue(notes, 'trajectory_turn_after_deg', '');
        var closedLoop = _noteFieldValue(notes, 'motion_smooth_closed_loop', '');
        var adaptiveLoop =
            _noteFieldValue(notes, 'adaptive_closed_loop_resample', '');
        var loopSubdivisions = _noteFieldValue(notes, 'loop_subdivisions', '');
        var lockPairs = _noteFieldValue(notes, 'shape_tangent_pairs_locked', '');
        var staticRemoved = _noteFieldValue(notes, 'static_source_keys_removed', '');
        var fidelity = _noteFieldValue(
            notes, 'motion_smooth_source_fidelity', '');
        var fidelityConstraints = _noteFieldValue(
            notes, 'source_pose_constraints', '') ||
            _noteFieldValue(notes, 'source_fidelity_samples', '');
        var ease = _noteFieldValue(notes, 'motion_smooth_ease', '');
        var shapeDefault =
            notes.indexOf('motion_smooth_shape_ease_default=true') >= 0;
        var parts = [];
        if (sourceKeys || outputKeys) {
            parts.push((sourceKeys || '?') + ' source keys -> ' +
                (outputKeys || '?') + ' output keys');
        }
        if (shapeRove) { parts.push('rove timing'); }
        if (closedLoop === 'true') { parts.push('closed loop'); }
        if (adaptiveLoop === 'true') { parts.push('adaptive samples'); }
        if (loopSubdivisions) {
            parts.push(loopSubdivisions + 'x loop samples');
        }
        if (lockPairs && parseInt(lockPairs, 10) > 0) {
            parts.push(lockPairs + ' tangent pairs locked');
        }
        if (schedule) { parts.push(schedule); }
        if (strength) { parts.push('strength ' + _formatNoteNumber(strength, 2)); }
        if (passes) { parts.push(passes + ' passes'); }
        if (maxDisp) {
            parts.push('max displacement ' + _formatNoteNumber(maxDisp, 3));
        }
        if (maxControlDisp) {
            parts.push('max control shift ' +
                _formatNoteNumber(maxControlDisp, 3));
        }
        if (smoothedPathErr) {
            parts.push('path error ' +
                _formatNoteNumber(smoothedPathErr, 3));
        }
        if (pathAccuracy) {
            parts.push('fit ' + _formatNoteNumber(pathAccuracy, 3));
        }
        if (preserveBounds === 'true') {
            parts.push('bounds +/-' +
                _formatNoteNumber(boundsTolerance || '0', 3) +
                ' px, drift ' + _formatNoteNumber(boundsDeviation || '0', 3));
        }
        if (sharpPoints && parseInt(sharpPoints, 10) > 0) {
            parts.push(sharpPoints + ' sharp points');
        }
        if (keyedPoints && parseInt(keyedPoints, 10) > 0) {
            parts.push(keyedPoints + ' keyed locks');
        }
        if (turnBefore || turnAfter) {
            parts.push('turn ' + _formatNoteNumber(turnBefore || '0', 1) +
                ' -> ' + _formatNoteNumber(turnAfter || '0', 1) + ' deg');
        }
        if (maxShift) {
            parts.push('max time shift ' + _formatNoteNumber(maxShift, 3) + 's');
        }
        if (totalTravel) {
            parts.push('travel ' + _formatNoteNumber(totalTravel, 3));
        }
        if (staticRemoved && parseInt(staticRemoved, 10) > 0) {
            parts.push(staticRemoved + ' static keys removed');
        }
        if (fidelity === 'true') {
            parts.push('source-key pose constraints' +
                (fidelityConstraints ? ' ' + fidelityConstraints : ''));
        }
        if (ease) {
            parts.push('easing ' + ease +
                (shapeDefault ? ' (shape default)' : ''));
        }
        return (motionPath ? 'Motion Path Smooth: ' : 'Motion Smooth: ') +
            parts.join(', ') + '.';
    }

    function _pathVertexSummaryMessage(notes) {
        notes = notes || '';
        var src, fit, mode, keys, candKeys, fallbackKeys, replFit, replTarget;
        if (notes.indexOf('auto_cleanup_pass_accepted') >= 0) {
            var firstPassKeys = _noteFieldValue(notes, 'first_pass_keys',
                'auto_cleanup_pass_accepted');
            var cleanupKeys = _noteFieldValue(notes, 'cleanup_keys',
                'auto_cleanup_pass_accepted');
            var firstPassVertices = _noteFieldValue(notes, 'first_pass_vertices',
                'auto_cleanup_pass_accepted');
            var cleanupVertices = _noteFieldValue(notes, 'cleanup_vertices',
                'auto_cleanup_pass_accepted');
            var originalExceeded =
                notes.indexOf('auto_cleanup_original_verify_exceeded=true') >= 0;
            return 'Path cleanup pass accepted (' +
                (firstPassKeys || '?') + ' -> ' + (cleanupKeys || '?') +
                ' keys, ' +
                (firstPassVertices || '?') + ' -> ' +
                (cleanupVertices || '?') + ' vertices; first-pass verified' +
                (originalExceeded ? ', original-source error logged' : '') +
                ').';
        }
        if (notes.indexOf('post_solve_vertex_reduction_accepted') >= 0) {
            src = _noteFieldValue(notes, 'source_vertices',
                'post_solve_vertex_reduction_accepted');
            fit = _noteFieldValue(notes, 'fitted_vertices',
                'post_solve_vertex_reduction_accepted');
            mode = _noteFieldValue(notes, 'mode',
                'post_solve_vertex_reduction_accepted');
            keys = _noteFieldValue(notes, 'keys',
                'post_solve_vertex_reduction_accepted');
            if (mode === 'duplicate_terminal_closure') {
                return 'Path vertices: duplicate-terminal cleanup accepted (' +
                    (src || '?') + ' -> ' + (fit || '?') +
                    (keys ? ', ' + keys + ' keys' : '') + ').';
            }
            if (mode === 'post_temporal_bridge_prune') {
                var prepassSrc = _noteFieldValue(notes, 'prepass_source_vertices',
                    'post_solve_vertex_reduction_accepted');
                if (prepassSrc) { src = prepassSrc; }
                return 'Path vertices: second pass accepted (' +
                    (src || '?') + ' -> ' + (fit || '?') +
                    (keys ? ', ' + keys + ' keys' : '') +
                    '; key timing preserved).';
            }
            return 'Path vertices: post-solve cleanup accepted (' +
                (src || '?') + ' -> ' + (fit || '?') +
                (mode ? ', ' + mode : '') +
                (keys ? ', ' + keys + ' keys' : '') + ').';
        }
        if (notes.indexOf('replacement_rejected') >= 0) {
            candKeys = _noteFieldValue(notes, 'path_replacement_candidate_keys', '');
            fallbackKeys = _noteFieldValue(notes, 'original_fallback_keys', '');
            replFit = _noteFieldValue(notes, 'fitted_vertices',
                'path_replacement_fit;');
            replTarget = _noteFieldValue(notes, 'target_vertices',
                'path_replacement_fit;');
            return 'Path vertices: replacement candidate attempted' +
                (replFit || replTarget
                    ? ' (' + (replFit || replTarget) + ' vertices)'
                    : '') +
                ' and rejected' +
                (candKeys || fallbackKeys
                    ? ' (' + (candKeys || '?') + ' keys vs ' +
                        (fallbackKeys || '?') + '-key fallback)'
                    : '') + '.';
        }
        if (notes.indexOf('post_solve_vertex_reduction_skipped: mixed_key_topology') >= 0) {
            return 'Path vertices: variable solved topology; second-pass vertex pruning did not find a safe removal.';
        }
        return '';
    }

    function _vertexOptimizationGateMessage(notes) {
        notes = notes || '';
        if (notes.indexOf('post_solve_vertex_reduction_accepted') >= 0 ||
                notes.indexOf('path_replacement_accepted') >= 0) {
            return '';
        }
        if (notes.indexOf('post_solve_vertex_reduction_skipped: mixed_key_topology') >= 0) {
            return 'Path vertex optimization: solved path has variable vertex counts, ' +
                'and the second-pass vertex prune did not find a safe removal at this ' +
                'accuracy gate; increase path tolerance or screen-px tolerance.';
        }
        if (notes.indexOf('post_solve_vertex_reduction_rejected') >= 0 ||
                notes.indexOf('post_solve_vertex_reduction_skipped: no_duplicate_terminal_closure') >= 0 ||
                notes.indexOf('replacement_rejected') >= 0 ||
                notes.indexOf('path_replacement_fit skipped') >= 0) {
            return 'Path vertex optimization: no safe vertex reduction met this ' +
                'accuracy gate; increase path tolerance or screen-px tolerance to ' +
                'attempt vertex reduction.';
        }
        return '';
    }

    // Log shape key endpoint deviation after writeback.
    // Calls verifyShapeKeyEndpoints (from verify.jsx) for each shape_flat property,
    // comparing AE-stored key values against the original source samples.
    // Reports whether endpoints are exact (key == source) or relaxed (solver-chosen).
    // No-op when verifyShapeKeyEndpoints is not loaded or bundle has no shape paths.
    function _logShapeEndpointDeviation(bundle, keyBundle, comp) {
        if (typeof verifyShapeKeyEndpoints !== 'function') { return; }
        if (!bundle || !bundle.properties || !keyBundle || !keyBundle.property_results) { return; }

        // Build source-sample map: propId -> samples, for shape_flat only.
        var srcMap = {};
        var bi, bps;
        for (bi = 0; bi < bundle.properties.length; bi++) {
            bps = bundle.properties[bi];
            if (bps.property && bps.property.units_label === 'shape_flat' &&
                    bps.property.id) {
                srcMap[bps.property.id] = bps.samples;
            }
        }

        var totalRelaxed = 0, totalExact = 0, maxErr = 0;
        var ri, pk, srcSamples, epProp, epResult;
        for (ri = 0; ri < keyBundle.property_results.length; ri++) {
            pk = keyBundle.property_results[ri];
            // Sub-path entries share the source property_id but their key values
            // belong to a newly created adjacent path, not the source property.
            // Skipping them prevents reading source property keyValues for sub-paths.
            if (_isSubPathEntry(pk)) { continue; }
            srcSamples = srcMap[pk.property_id];
            if (!srcSamples || !pk.keys || !pk.keys.length) { continue; }
            epProp = null;
            try { epProp = parseSepId(pk.property_id, comp); } catch (epE) {}
            if (!epProp) { continue; }
            try {
                epResult = verifyShapeKeyEndpoints(epProp, pk.keys, srcSamples);
                totalRelaxed = totalRelaxed + epResult.relaxed_key_count;
                totalExact   = totalExact   + epResult.exact_key_count;
                if (epResult.max_endpoint_err > maxErr) {
                    maxErr = epResult.max_endpoint_err;
                }
            } catch (epCallErr) {}
        }

        if (totalRelaxed + totalExact > 0) {
            if (totalRelaxed > 0) {
                log('  Shape endpoints: ' + totalRelaxed + ' relaxed (max_err=' +
                    maxErr.toFixed(4) + ') / ' + totalExact + ' exact');
            } else {
                log('  Shape endpoints: all exact (' + totalExact + ')');
            }
        }
    }

    function _shapeVertexLabel(pinfo) {
        pinfo = pinfo || {};
        var n = pinfo.shape_max_vertex_count ||
                pinfo.shape_canonical_vertex_count || 0;
        if (n <= 0 && pinfo.dimensions > 2) {
            n = Math.floor((pinfo.dimensions - 2) / 6);
        }
        if (n <= 0) { return ''; }
        return pinfo.shape_variable_topology
            ? 'vertices<=' + n
            : 'vertices=' + n;
    }

    function _propertyTypeLabel(pinfo) {
        pinfo = pinfo || {};
        if (pinfo.units_label === 'shape_flat') {
            return _shapeVertexLabel(pinfo);
        }
        if (pinfo.kind === 'Scalar') { return 'scalar'; }
        if (pinfo.kind === 'TwoD' || pinfo.kind === 'TwoD_Spatial') { return '2D'; }
        if (pinfo.kind === 'ThreeD' || pinfo.kind === 'ThreeD_Spatial') { return '3D'; }
        if (pinfo.kind === 'Color') { return 'color'; }
        return '';
    }

    // Builds the display string for a PropertySamples entry.
    // Format: '<layer/path/name>  N=<samples>  <type summary>  tol=<eff>  px=<eff>  mode=<eff>  cleanup=<eff>'
    // Properties whose per-prop override differs from global are flagged with '*'.
    function _propLabel(ps) {
        var pid       = ps.property.id;
        var name      = _friendlyPropertyLabel(ps.property) || pid;
        var n         = ps.samples ? ps.samples.length : 0;
        var eff       = _effectiveGroupTolerances(ps.property || {});
        var effTol    = eff.tol;
        var effTolPx  = eff.tolPx;
        var tolOverride = (typeof _propOverrideValue(_propTolMap, pid) === 'number' ||
            typeof _propOverrideValue(_propTolPxMap, pid) === 'number') ? '*' : '';
        var modeOverride = _hasSolveModeOverride(pid) ? '*' : '';
        var cleanupOverride = _hasCleanupModeOverride(pid) ? '*' : '';
        var smoothOverride =
            (typeof _propOverrideValue(_propMotionSmoothTolMap, pid) === 'number') ? '*' : '';
        var smoothBezierOverride =
            _propOverrideObject(_propMotionSmoothBezierMap, pid) !== null ? '*' : '';
        var smoothText = _effectiveSolveMode(pid) === 'motion_smooth'
            ? ('  smooth=' + smoothOverride + _effectiveMotionSmoothTolerance(pid) +
               '  ease=' + smoothBezierOverride +
               _motionSmoothBezierLabel(_effectiveMotionSmoothBezier(pid)))
            : '';
        var typeLabel = _propertyTypeLabel(ps.property);
        return name + '  N=' + n +
               (typeLabel ? '  ' + typeLabel : '') +
               '  tol=' + tolOverride + effTol +
               '  px=' + tolOverride + effTolPx +
               '  mode=' + modeOverride +
                    _solveOptimizationModeShortLabel(_effectiveSolveMode(pid)) +
               smoothText +
               '  cleanup=' + cleanupOverride +
                    _cleanupModeLabel(_effectiveCleanupMode(pid));
    }

    // Refresh the property listbox from _lastBundle.
    // Verified: listbox.removeAll() and listbox.add('item', text) (js-tools-guide p.1729, p.1674).
    function _refreshPropList() {
        if (!ui || !ui.propList) { return; }
        ui.propList.removeAll();
        if (!_lastBundle) { return; }
        var props = _lastBundle.properties;
        var i;
        for (i = 0; i < props.length; i++) {
            ui.propList.add('item', _propLabel(props[i]));
        }
    }

    function _refreshSampledPropertiesFromAE() {
        if (_cooperativeBakeActive) {
            alert('bbsolver-test-harness: sampled properties are locked while a bake is running.');
            return;
        }
        if (!_lastBundle || !_lastBundle.properties ||
                _lastBundle.properties.length === 0) {
            log('INFO [refresh]: no sampled properties to refresh.');
            _refreshPropList();
            return;
        }
        var comp = getActiveComp();
        if (!comp) {
            alert('bbsolver-test-harness: no active composition.');
            return;
        }

        var kept = [];
        var removed = 0;
        var relabeled = 0;
        var i, ps, info, before, prop, copy, refreshed;
        for (i = 0; i < _lastBundle.properties.length; i++) {
            ps = _lastBundle.properties[i];
            info = ps ? ps.property : null;
            before = _friendlyPropertyLabel(info);
            prop = _resolveSampledProperty(info, comp);
            if (!prop) {
                removed++;
                if (info && info.id) {
                    delete _propTolMap[info.id];
                    delete _propTolPxMap[info.id];
                    delete _propSolveModeMap[info.id];
                    delete _propCleanupModeMap[info.id];
                    delete _propMotionSmoothTolMap[info.id];
                }
                log('INFO [refresh]: removed stale sampled row: ' +
                    (before || (info ? info.id : 'unknown property')));
                continue;
            }
            copy = _copyPlainObject(ps);
            refreshed = _refreshedPropertyInfo(info, prop, comp);
            if (_friendlyPropertyLabel(refreshed) !== before) {
                relabeled++;
            }
            copy.property = refreshed;
            kept.push(copy);
        }

        _lastBundle.properties = kept;
        if (removed > 0 || relabeled > 0) {
            _clearSampleBundleCache();
        }
        _refreshPropList();
        log('INFO [refresh]: sampled list refreshed; kept ' + kept.length +
            ', removed ' + removed + ', relabeled ' + relabeled + '.');
        setStatusText('Sampled list refreshed');
    }

    // ---- bbsolver version check --------------------------------------------

    function _bbsolverVersion(bbsolver) {
        try {
            var out = system.callSystem(_platformQuote(bbsolver) + ' --version');
            return out ? out.replace(/[\r\n]+/g, '') : null;
        } catch (e) { return null; }
    }

    function _showSetupDialog(reason) {
        var dlg = new Window('dialog', 'bbsolver-test-harness: Solver Setup Required');
        dlg.orientation = 'column';
        dlg.alignChildren = ['fill', 'top'];
        dlg.spacing = 8;
        dlg.add('statictext', undefined, 'bbsolver-test-harness needs bbsolver.');
        dlg.add('statictext', undefined, reason);
        if (_isWindowsPlatform()) {
            dlg.add('statictext', undefined,
                'Build (Windows): cmake -B build, cmake --build build --config Release');
            dlg.add('statictext', undefined,
                'Default search paths: %APPDATA%/bbsolver/bin/bbsolver.exe, ' +
                'C:/Program Files/bbsolver/bin/bbsolver.exe');
            dlg.add('statictext', undefined,
                'Note: Windows uses a synchronous bbsolver runner; UI will freeze during bake.');
        } else {
            dlg.add('statictext', undefined,
                'Build: cmake -B build && cmake --build build');
            dlg.add('statictext', undefined,
                'Default search paths: ~/.bbsolver/bin/bbsolver, /opt/homebrew/bin/bbsolver, /usr/local/bin/bbsolver');
        }
        dlg.add('statictext', undefined, 'Or set the path in Settings.');
        dlg.add('button', undefined, 'OK', { name: 'ok' });
        dlg.show();
    }

    function _checkBbsolverOnStartup() {
        var bbsolver = _findBbsolver();
        var ver    = _bbsolverVersion(bbsolver);
        if (!ver) {
            _showSetupDialog('bbsolver not found. Expected version ' + BBSOLVER_EXPECTED + '.');
            return;
        }
        if (ver.indexOf(BBSOLVER_EXPECTED) < 0) {
            _showSetupDialog('Version mismatch: got "' + ver +
                '", expected ' + BBSOLVER_EXPECTED + '.');
        }
    }

    function _solverJobsSetting() {
        var jobs = parseInt(settings.solverJobs, 10);
        if (!isFinite(jobs) || jobs < 0) { return 0; }
        if (jobs > 64) { return 64; }
        return jobs;
    }

    // ---- Progress parsing ------------------------------------------------

    // Parse the solver's JSON progress stream (the lines emitted on
    // --progress-fd 1). system.callSystem is synchronous so we only see
    // these lines after bbsolver returns; we cannot stream them live. To
    // give the user a useful audit trail we therefore replay every
    // distinct phase row into the log here, with its progress percentage,
    // so that scrolling the log after a freeze tells the user exactly
    // what bbsolver was doing while the UI was unresponsive.
    //
    // The function also updates the progress bar inside the solve-stage
    // range and keeps the status label on the solver's last reported phase.
    function _mappedSolverPct(progress, solverStartPct, solverEndPct) {
        var p = (typeof progress === 'number') ? progress : 0;
        if (p < 0) { p = 0; }
        if (p > 1) { p = 1; }
        if (typeof solverStartPct === 'number' &&
                typeof solverEndPct === 'number' &&
                solverEndPct > solverStartPct) {
            return Math.round(solverStartPct +
                (solverEndPct - solverStartPct) * p);
        }
        return Math.round(p * 100);
    }

    function _heartbeatSolverPct(lastBarPct, solverStartPct, solverEndPct, elapsedMs) {
        var start = (typeof solverStartPct === 'number') ? solverStartPct : 0;
        var end = (typeof solverEndPct === 'number') ? solverEndPct : 100;
        if (end <= start) { end = start + 1; }
        var cap = end - 1;
        if (cap < start) { cap = start; }
        var base = (typeof lastBarPct === 'number' && lastBarPct >= 0)
            ? lastBarPct
            : start;
        var stepPct = Math.floor(elapsedMs / 2000);
        var nextPct = start + stepPct;
        if (nextPct < base) { nextPct = base; }
        if (nextPct > cap) { nextPct = cap; }
        return nextPct;
    }

    function _parseProgress(output, solverStartPct, solverEndPct) {
        if (!output) { return -1; }
        var lines = output.split('\n');
        var maxProg = -1;
        var lastPhase = '';
        var lastPct = -1;
        var emittedAny = false;
        var i, line, obj, pct, phase;
        for (i = 0; i < lines.length; i++) {
            line = lines[i].replace(/^\s+|\s+$/g, '');
            if (!line || line.charAt(0) !== '{') { continue; }
            try { obj = JSON.parse(line); } catch (e) { continue; }
            if (typeof obj.progress !== 'number') { continue; }
            pct = Math.round(obj.progress * 100);
            phase = obj.phase ? String(obj.phase) : '';
            // Only log a row when the phase name or percent advances --
            // bbsolver may emit many lines at the same step and we don't
            // want to flood the log.
            if (phase !== lastPhase || pct !== lastPct) {
                log('  solver: ' + (phase || 'progress') +
                    ' (' + pct + '%)');
                setProgress(_mappedSolverPct(
                    obj.progress, solverStartPct, solverEndPct));
                emittedAny = true;
                lastPhase = phase;
                lastPct = pct;
            }
            if (obj.progress > maxProg) { maxProg = obj.progress; }
        }
        if (maxProg >= 0) {
            // The outer phase progress in _runBake controls the panel-level
            // bar; here we only update the status label so the user sees the
            // solver's last reported phase without clobbering the panel's
            // own phase percentage.
            if (lastPhase) {
                setStatusText('solver: ' + lastPhase +
                    ' (' + Math.round(maxProg * 100) + '%)');
            }
            setProgress(_mappedSolverPct(maxProg, solverStartPct, solverEndPct));
            if (!emittedAny) {
                log('  solver: progress (' +
                    Math.round(maxProg * 100) + '%)');
            }
        }
        return maxProg;
    }

    // ---- Comp / property helpers -----------------------------------------

    function getActiveComp() {
        var item = app.project.activeItem;
        if (!item || !(item instanceof CompItem)) { return null; }
        return item;
    }

    function getSelectedProperties(comp) {
        var result = [];
        var sel = comp.selectedProperties;
        if (!sel) { return result; }
        var i, p;
        for (i = 0; i < sel.length; i++) {
            p = sel[i];
            if (p.propertyType === PropertyType.PROPERTY && p.canVaryOverTime) {
                result.push(p);
            }
        }
        return result;
    }

    function getAnimatedPropsFromSelectedLayers(comp) {
        var result = [];
        var layers = comp.selectedLayers;
        if (!layers) { return result; }
        var i, j, props;
        for (i = 0; i < layers.length; i++) {
            props = collectAnimatedProps(layers[i]);
            for (j = 0; j < props.length; j++) { result.push(props[j]); }
        }
        return result;
    }

    function _fullCompTimeRange(comp) {
        var start = 0.0;
        var end = 0.0;
        try { start = comp.displayStartTime || 0.0; } catch (eDisplayStart) {}
        try { end = start + (comp.duration || 0.0); } catch (eDuration) {}
        if (!(end > start)) {
            start = 0.0;
            try { end = comp.duration || 0.0; } catch (eDurationFallback) {}
        }
        return { tStart: start, tEnd: end };
    }

    function _timeRange(comp) {
        if (settings.useWorkArea) {
            return { tStart: comp.workAreaStart,
                     tEnd:   comp.workAreaStart + comp.workAreaDuration };
        }
        var manualStart = parseFloat(settings.startSec);
        var manualEnd = parseFloat(settings.endSec);
        if (!isFinite(manualStart)) { manualStart = 0.0; }
        if (!isFinite(manualEnd)) { manualEnd = 0.0; }
        if (manualEnd > manualStart + 0.000001) {
            return { tStart: manualStart, tEnd: manualEnd };
        }
        return _fullCompTimeRange(comp);
    }

    function _rangeKey(range) {
        return range.tStart.toFixed(6) + '|' + range.tEnd.toFixed(6);
    }

    function _formatTimeRange(range) {
        return range.tStart.toFixed(3) + '-' + range.tEnd.toFixed(3) + 's';
    }

    function _markerComment(markerValue) {
        var comment = '';
        try { comment = markerValue.comment || ''; } catch (eComment) {}
        return String(comment || '');
    }

    function _markerSearchText(markerValue) {
        var parts = [];
        function addPart(value) {
            if (value === null || typeof value === 'undefined') { return; }
            value = String(value || '');
            if (value.length > 0) { parts.push(value); }
        }

        addPart(_markerComment(markerValue));
        try { addPart(markerValue.chapter); } catch (eChapter) {}
        try { addPart(markerValue.cuePointName); } catch (eCue) {}
        try { addPart(markerValue.url); } catch (eUrl) {}
        try { addPart(markerValue.frameTarget); } catch (eFrameTarget) {}
        try {
            var params = markerValue.getParameters();
            var k;
            for (k in params) {
                if (params.hasOwnProperty && !params.hasOwnProperty(k)) { continue; }
                addPart(k);
                addPart(params[k]);
            }
        } catch (eParams) {}

        return parts.join(' ');
    }

    function _normalizedMarkerSearchText(markerValue) {
        var text = _markerSearchText(markerValue).toLowerCase();
        text = text.replace(/[^a-z0-9]+/g, ' ');
        text = text.replace(/\s+/g, ' ');
        return ' ' + text + ' ';
    }

    function _markerIsBbsolverSegment(markerValue) {
        var raw = _markerSearchText(markerValue).toLowerCase();
        var text = _normalizedMarkerSearchText(markerValue);
        if (raw.indexOf('bbsolver_segment') >= 0) { return true; }
        if (text.indexOf(' bbsolve ') >= 0) { return true; }
        if (text.indexOf(' bbsolver ') >= 0) { return true; }
        return false;
    }

    function _markerPropUsable(markerProp) {
        if (!markerProp) { return false; }
        try {
            var count = markerProp.numKeys;
            return typeof count === 'number' && count >= 0;
        } catch (eCount) {}
        return false;
    }

    function _layerMarkerProperty(layer) {
        var markerProp = null;
        try { markerProp = layer.marker; } catch (eLayerMarker) {}
        if (_markerPropUsable(markerProp)) { return markerProp; }

        // AE scripting docs show layer.marker and layer.property("Marker") as
        // the public marker APIs. Keep the match-name lookup as a fallback for
        // hosts that expose only the underlying AE property name.
        try { markerProp = layer.property('Marker'); } catch (eNamedMarker) {}
        if (_markerPropUsable(markerProp)) { return markerProp; }

        try { markerProp = layer.property('ADBE Marker'); } catch (eAdbeMarker) {}
        if (_markerPropUsable(markerProp)) { return markerProp; }

        return null;
    }

    function _layerSegmentMarkerSummary(layer, baseRange) {
        var layerName = '?';
        try { layerName = layer.name || '?'; } catch (eLayerName) {}
        var markerProp = _layerMarkerProperty(layer);
        if (!markerProp) {
            return 'layer "' + layerName + '" has no readable marker property';
        }
        var count = 0;
        try { count = markerProp.numKeys || 0; } catch (eCount) {}
        var matched = 0;
        var firstText = '';
        var i, mv, text;
        for (i = 1; i <= count; i++) {
            try { mv = markerProp.keyValue(i); } catch (eValue) { continue; }
            text = _trim(_markerSearchText(mv));
            if (!firstText && text) { firstText = text; }
            if (_markerIsBbsolverSegment(mv)) { matched++; }
        }
        return 'layer "' + layerName + '" marker keys=' + count +
            ', matching bbsolver range markers=' + matched +
            ', active range=' + _formatTimeRange(baseRange) +
            (firstText ? ', first marker text="' + firstText + '"' : '');
    }

    function _nextSegmentMarkerTime(markerProp, markerIndex) {
        var j, mv;
        for (j = markerIndex + 1; j <= markerProp.numKeys; j++) {
            try {
                mv = markerProp.keyValue(j);
                if (_markerIsBbsolverSegment(mv)) {
                    return markerProp.keyTime(j);
                }
            } catch (eNextSeg) {}
        }
        return null;
    }

    function _mergeRanges(ranges, frameDur) {
        if (!ranges || ranges.length <= 1) { return ranges || []; }
        ranges.sort(function (a, b) {
            if (a.tStart < b.tStart) { return -1; }
            if (a.tStart > b.tStart) { return 1; }
            return a.tEnd < b.tEnd ? -1 : (a.tEnd > b.tEnd ? 1 : 0);
        });
        var merged = [];
        var eps = Math.max(0.000001, (frameDur || 0.0) * 0.25);
        var i, cur, last;
        for (i = 0; i < ranges.length; i++) {
            cur = ranges[i];
            if (merged.length === 0) {
                merged.push(cur);
                continue;
            }
            last = merged[merged.length - 1];
            if (cur.tStart <= last.tEnd + eps) {
                if (cur.tEnd > last.tEnd) { last.tEnd = cur.tEnd; }
                last.label = last.label + ', ' + cur.label;
            } else {
                merged.push(cur);
            }
        }
        return merged;
    }

    function _layerSegmentRanges(layer, baseRange, comp) {
        var ranges = [];
        var markerProp = null;
        var frameDur = comp ? comp.frameDuration : 0.0;
        markerProp = _layerMarkerProperty(layer);
        if (!markerProp) { return ranges; }
        var count = 0;
        try { count = markerProp.numKeys || 0; } catch (eMarkerCount) {}
        if (count <= 0) { return ranges; }

        var i, mv, start, duration, end, nextTime, clippedStart, clippedEnd;
        for (i = 1; i <= count; i++) {
            try { mv = markerProp.keyValue(i); } catch (eMarkerValue) { continue; }
            if (!_markerIsBbsolverSegment(mv)) { continue; }

            try { start = markerProp.keyTime(i); } catch (eMarkerTime) { continue; }
            duration = 0.0;
            try { duration = mv.duration || 0.0; } catch (eDuration) {}
            if (duration > Math.max(0.000001, frameDur * 0.25)) {
                end = start + duration;
            } else {
                nextTime = _nextSegmentMarkerTime(markerProp, i);
                end = (nextTime !== null) ? nextTime : baseRange.tEnd;
            }
            clippedStart = Math.max(start, baseRange.tStart);
            clippedEnd   = Math.min(end, baseRange.tEnd);
            if (clippedEnd > clippedStart + Math.max(0.000001, frameDur * 0.25)) {
                ranges.push({
                    tStart: clippedStart,
                    tEnd:   clippedEnd,
                    label:  _trim(_markerComment(mv)) || ('bbsolver marker ' + i)
                });
            }
        }
        return _mergeRanges(ranges, frameDur);
    }

    function _layerKey(layer) {
        try { return String(layer.index); } catch (eLayerIndex) {}
        return '';
    }

    function _fallbackSelectedLayerSegmentRanges(comp, propLayer, baseRange) {
        var layers = null;
        try { layers = comp.selectedLayers; } catch (eSelectedLayers) {}
        if (!layers || layers.length === 0) { return null; }

        var propLayerKey = propLayer ? _layerKey(propLayer) : '';
        var matches = [];
        var i, layer, layerKey, ranges;
        for (i = 0; i < layers.length; i++) {
            layer = layers[i];
            layerKey = _layerKey(layer);
            if (propLayerKey && layerKey === propLayerKey) { continue; }
            ranges = _layerSegmentRanges(layer, baseRange, comp);
            if (ranges && ranges.length > 0) {
                matches.push({ layer: layer, ranges: ranges });
            }
        }

        if (matches.length === 1) {
            try {
                log('INFO [segment]: using bbsolver range markers from selected layer "' +
                    matches[0].layer.name + '" because the property layer had no marker ranges.');
            } catch (eFallbackLog) {}
            return matches[0].ranges;
        }

        if (matches.length > 1) {
            try {
                log('SKIP [segment]: multiple selected layers have bbsolver range markers; ' +
                    'select properties on the marker layer or leave only one marker layer selected.');
            } catch (eAmbiguousLog) {}
        }
        return null;
    }

    function _buildBakeBatches(comp, props) {
        var baseRange = _timeRange(comp);
        if (settings.useSegmentMarkers !== true) {
            return [{
                label: 'time range ' + _formatTimeRange(baseRange),
                tStart: baseRange.tStart,
                tEnd: baseRange.tEnd,
                props: props,
                segmentMode: false
            }];
        }

        var grouped = {};
        var order = [];
        var skipped = 0;
        var pi, prop, layer, ranges, ri, range, key, batch;
        for (pi = 0; pi < props.length; pi++) {
            prop = props[pi];
            layer = null;
            try { layer = _containingLayer(prop); } catch (eLayer) {}
            ranges = layer ? _layerSegmentRanges(layer, baseRange, comp) : [];
            if (!ranges || ranges.length === 0) {
                ranges = _fallbackSelectedLayerSegmentRanges(comp, layer, baseRange) || ranges;
            }
            if (!ranges || ranges.length === 0) {
                skipped++;
                try {
                    log('SKIP [segment]: ' + prop.name +
                        ' has no layer marker range containing a bbsolver marker (' +
                        (layer ? _layerSegmentMarkerSummary(layer, baseRange) :
                            'no containing layer') + ').');
                } catch (eSkipLog) {}
                continue;
            }
            for (ri = 0; ri < ranges.length; ri++) {
                range = ranges[ri];
                key = _rangeKey(range);
                if (!grouped[key]) {
                    grouped[key] = {
                        label: 'segment ' + _formatTimeRange(range),
                        markerLabel: range.label,
                        tStart: range.tStart,
                        tEnd: range.tEnd,
                        props: [],
                        segmentMode: true
                    };
                    order.push(key);
                }
                grouped[key].props.push(prop);
            }
        }

        var batches = [];
        for (pi = 0; pi < order.length; pi++) {
            batch = grouped[order[pi]];
            if (batch.props.length > 0) { batches.push(batch); }
        }
        batches.sort(function (a, b) {
            if (a.tStart < b.tStart) { return -1; }
            if (a.tStart > b.tStart) { return 1; }
            return a.tEnd < b.tEnd ? -1 : (a.tEnd > b.tEnd ? 1 : 0);
        });
        if (batches.length > 0) {
            log('INFO [segment]: using ' + batches.length +
                ' bbsolver marker range' +
                (batches.length === 1 ? '' : 's') +
                (skipped > 0 ? '; skipped ' + skipped +
                    ' selected prop(s) without segment markers.' : '.'));
        }
        return batches;
    }

    // ---- Per-property override dialog ------------------------------------

    function _setToleranceForProp(propId) {
        var current = (typeof _propTolMap[propId] === 'number')
            ? String(_propTolMap[propId]) : '';
        var label = _propertyLabelForId(propId, _propertyLabelMap(_lastBundle));
        var dlg = new Window('dialog', 'Property Solve Overrides');
        dlg.orientation = 'column';
        dlg.alignChildren = ['fill', 'top'];
        dlg.spacing = 6;
        dlg.add('statictext', undefined, 'Property: ' + label);
        dlg.add('statictext', undefined,
            'Set row-specific tolerance, solver mode, and cleanup behavior. ' +
            'Blank/default entries use the main UI settings.');

        var tolGrp = dlg.add('group');
        tolGrp.add('statictext', undefined, 'Tolerance / px:');
        var tolEdit = tolGrp.add('edittext', undefined, current);
        tolEdit.preferredSize.width = 80;
        tolEdit.helpTip = 'Per-property override applied to both bbsolver --tolerance and --screen-px for this property group.';
        tolGrp.add('statictext', undefined,
            '(empty = global ' + settings.tolerance + ' / ' +
            settings.toleranceScreenPx + ' px)');

        var smoothCurrent = (typeof _propOverrideValue(
                _propMotionSmoothTolMap, propId) === 'number')
            ? String(_propOverrideValue(_propMotionSmoothTolMap, propId)) : '';
        var smoothTolGrp = dlg.add('group');
        smoothTolGrp.add('statictext', undefined, 'Motion smooth tolerance:');
        var smoothTolEdit = smoothTolGrp.add('edittext', undefined, smoothCurrent);
        smoothTolEdit.preferredSize.width = 80;
        smoothTolEdit.helpTip = 'Motion Smooth only: larger values smooth/decimate the spatial motion path more aggressively. This is separate from solve accuracy tolerance.';
        smoothTolGrp.add('statictext', undefined,
            '(empty = global ' + _effectiveMotionSmoothTolerance(propId) + ')');

        var bezierOverride = _propOverrideObject(
            _propMotionSmoothBezierMap, propId);
        var bezierEffective = _effectiveMotionSmoothBezier(propId);
        var bezierGrp = dlg.add('group');
        bezierGrp.add('statictext', undefined, 'Motion smooth ease x1,y1,x2,y2:');
        var bezierX1Edit = bezierGrp.add('edittext', undefined,
            bezierOverride ? String(bezierOverride.x1) : '');
        var bezierY1Edit = bezierGrp.add('edittext', undefined,
            bezierOverride ? String(bezierOverride.y1) : '');
        var bezierX2Edit = bezierGrp.add('edittext', undefined,
            bezierOverride ? String(bezierOverride.x2) : '');
        var bezierY2Edit = bezierGrp.add('edittext', undefined,
            bezierOverride ? String(bezierOverride.y2) : '');
        bezierX1Edit.preferredSize.width = 46;
        bezierY1Edit.preferredSize.width = 46;
        bezierX2Edit.preferredSize.width = 46;
        bezierY2Edit.preferredSize.width = 46;
        bezierGrp.helpTip = 'Motion Smooth only: cubic Bezier timing curve for the existing source key schedule. Empty fields use global ' +
            _motionSmoothBezierLabel(bezierEffective) + '.';

        var modeGrp = dlg.add('group');
        modeGrp.add('statictext', undefined, 'Solve mode:');
        var modeItems = [
            'Global (' +
                _solveOptimizationModeShortLabel(settings.solveOptimizationMode) +
                ')'
        ];
        var mi;
        for (mi = 0; mi < _solveModeLabels.length; mi++) {
            modeItems.push(_solveModeLabels[mi]);
        }
        var modeDDL = modeGrp.add('dropdownlist', undefined, modeItems);
        modeDDL.preferredSize.width = 190;
        var modeOverride = _propOverrideString(_propSolveModeMap, propId);
        modeDDL.selection = modeOverride === null ? 0 :
            (_solveModeIndex(modeOverride) + 1);
        function selectedOverrideMode() {
            if (modeDDL.selection && modeDDL.selection.index > 0) {
                return _solveModeFromIndex(modeDDL.selection.index - 1);
            }
            return _normalizeSolveOptimizationMode(settings.solveOptimizationMode);
        }
        function syncMotionSmoothOverrideFields() {
            var isMotionSmooth = selectedOverrideMode() === 'motion_smooth';
            smoothTolEdit.enabled = isMotionSmooth;
            bezierX1Edit.enabled = isMotionSmooth;
            bezierY1Edit.enabled = isMotionSmooth;
            bezierX2Edit.enabled = isMotionSmooth;
            bezierY2Edit.enabled = isMotionSmooth;
        }
        syncMotionSmoothOverrideFields();
        modeDDL.onChange = syncMotionSmoothOverrideFields;

        var cleanupGrp = dlg.add('group');
        cleanupGrp.add('statictext', undefined, 'Cleanup pass:');
        var cleanupItems = [
            'Global (' + _cleanupModeLabel(_globalCleanupMode()) + ')'
        ];
        var ci;
        for (ci = 0; ci < _cleanupModeLabels.length; ci++) {
            cleanupItems.push(_cleanupModeLabels[ci]);
        }
        var cleanupDDL = cleanupGrp.add('dropdownlist', undefined, cleanupItems);
        cleanupDDL.preferredSize.width = 140;
        cleanupDDL.helpTip = 'Prompt asks before second pass, Auto runs eligible second-pass cleanup without asking, Off skips cleanup for this property.';
        var cleanupOverride = _propOverrideString(_propCleanupModeMap, propId);
        cleanupDDL.selection = cleanupOverride === null ? 0 :
            (_cleanupModeIndex(cleanupOverride) + 1);

        var btnGrp = dlg.add('group');
        btnGrp.alignment = 'right';
        var okBtn     = btnGrp.add('button', undefined, 'OK',     { name: 'ok' });
        var clearBtn  = btnGrp.add('button', undefined, 'Clear overrides');
        var cancelBtn = btnGrp.add('button', undefined, 'Cancel', { name: 'cancel' });
        okBtn.onClick = function () {
            if (tolEdit.text.replace(/^\s+|\s+$/g, '') === '') {
                delete _propTolMap[propId];
                delete _propTolPxMap[propId];
            } else {
                var val = parseFloat(tolEdit.text);
                if (!isNaN(val) && val > 0) {
                    _propTolMap[propId] = val;
                    _propTolPxMap[propId] = val;
                }
            }
            if (modeDDL.selection && modeDDL.selection.index > 0) {
                _propSolveModeMap[propId] =
                    _solveModeFromIndex(modeDDL.selection.index - 1);
            } else {
                delete _propSolveModeMap[propId];
            }
            if (smoothTolEdit.text.replace(/^\s+|\s+$/g, '') === '') {
                delete _propMotionSmoothTolMap[propId];
            } else {
                var smoothVal = parseFloat(smoothTolEdit.text);
                if (!isNaN(smoothVal) && smoothVal > 0) {
                    _propMotionSmoothTolMap[propId] = smoothVal;
                }
            }
            if (bezierX1Edit.text.replace(/^\s+|\s+$/g, '') === '' &&
                    bezierY1Edit.text.replace(/^\s+|\s+$/g, '') === '' &&
                    bezierX2Edit.text.replace(/^\s+|\s+$/g, '') === '' &&
                    bezierY2Edit.text.replace(/^\s+|\s+$/g, '') === '') {
                delete _propMotionSmoothBezierMap[propId];
            } else {
                _propMotionSmoothBezierMap[propId] =
                    _sanitizeMotionSmoothBezier({
                        x1: bezierX1Edit.text,
                        y1: bezierY1Edit.text,
                        x2: bezierX2Edit.text,
                        y2: bezierY2Edit.text
                    });
            }
            if (cleanupDDL.selection && cleanupDDL.selection.index > 0) {
                _propCleanupModeMap[propId] =
                    _cleanupModeFromIndex(cleanupDDL.selection.index - 1);
            } else {
                delete _propCleanupModeMap[propId];
            }
            dlg.close(1);
        };
        clearBtn.onClick = function () {
            delete _propTolMap[propId];
            delete _propTolPxMap[propId];
            delete _propSolveModeMap[propId];
            delete _propCleanupModeMap[propId];
            delete _propMotionSmoothTolMap[propId];
            delete _propMotionSmoothBezierMap[propId];
            dlg.close(1);
        };
        cancelBtn.onClick = function () { dlg.close(0); };
        dlg.show();
        _refreshPropList();
    }

    // ---- Sample helper ---------------------------------------------------

    function _sampleLayerKeyFromProp(prop) {
        try {
            var layer = _containingLayer(prop);
            return String(layer.index);
        } catch (eLayerKey) {}
        return '';
    }

    function _selectedLayerMapFromProps(props) {
        var map = {};
        var i, key;
        for (i = 0; i < props.length; i++) {
            key = _sampleLayerKeyFromProp(props[i]);
            if (key) { map[key] = true; }
        }
        return map;
    }

    function _propSignatureId(prop) {
        try {
            var layer = _containingLayer(prop);
            return _buildId(prop, layer.index);
        } catch (eId) {}
        try { return String(prop.matchName || prop.name || 'prop'); } catch (eName) {}
        return 'prop';
    }

    function _propExpressionSignature(prop) {
        var enabled = false;
        var expr = '';
        try { enabled = prop.expressionEnabled === true; } catch (eEnabled) {}
        if (enabled) {
            try { expr = prop.expression || ''; } catch (eExpr) {}
        }
        return (enabled ? 'expr:1:' : 'expr:0:') + _hashString(expr);
    }

    function _normalizeSolveOptimizationMode(mode) {
        mode = String(mode || '');
        if (mode === 'temporal_only' || mode === 'vertex_only' ||
                mode === 'motion_smooth' ||
                mode === 'motion_path_smooth') {
            return mode;
        }
        return 'full';
    }

    function _solveModeIndex(mode) {
        mode = _normalizeSolveOptimizationMode(mode);
        var i;
        for (i = 0; i < _solveModeValues.length; i++) {
            if (_solveModeValues[i] === mode) { return i; }
        }
        return 0;
    }

    function _solveModeFromIndex(index) {
        index = parseInt(index, 10);
        if (isNaN(index) || index < 0 ||
                index >= _solveModeValues.length) {
            return 'full';
        }
        return _solveModeValues[index];
    }

    function _solveOptimizationModeLabel(mode) {
        mode = _normalizeSolveOptimizationMode(mode);
        if (mode === 'temporal_only') { return 'temporal-key only'; }
        if (mode === 'vertex_only') { return 'vertex-decimation only'; }
        if (mode === 'motion_smooth') { return 'motion smooth'; }
        if (mode === 'motion_path_smooth') { return 'motion path smooth'; }
        return 'full';
    }

    function _solveOptimizationModeShortLabel(mode) {
        mode = _normalizeSolveOptimizationMode(mode);
        if (mode === 'temporal_only') { return 'temporal'; }
        if (mode === 'vertex_only') { return 'vertex'; }
        if (mode === 'motion_smooth') { return 'smooth'; }
        if (mode === 'motion_path_smooth') { return 'path smooth'; }
        return 'full';
    }

    function _normalizeCleanupMode(mode) {
        mode = String(mode || '');
        if (mode === 'auto' || mode === 'off') { return mode; }
        return 'prompt';
    }

    function _globalCleanupMode() {
        if (settings.cleanupPassMode) {
            return _normalizeCleanupMode(settings.cleanupPassMode);
        }
        return settings.promptSecondCleanupPass === true ? 'prompt' : 'off';
    }

    function _setGlobalCleanupMode(mode) {
        mode = _normalizeCleanupMode(mode);
        settings.cleanupPassMode = mode;
        settings.promptSecondCleanupPass = (mode !== 'off');
    }

    function _cleanupModeIndex(mode) {
        mode = _normalizeCleanupMode(mode);
        var i;
        for (i = 0; i < _cleanupModeValues.length; i++) {
            if (_cleanupModeValues[i] === mode) { return i; }
        }
        return 0;
    }

    function _cleanupModeFromIndex(index) {
        index = parseInt(index, 10);
        if (isNaN(index) || index < 0 ||
                index >= _cleanupModeValues.length) {
            return 'prompt';
        }
        return _cleanupModeValues[index];
    }

    function _cleanupModeLabel(mode) {
        mode = _normalizeCleanupMode(mode);
        if (mode === 'auto') { return 'auto'; }
        if (mode === 'off') { return 'off'; }
        return 'prompt';
    }

    function _effectiveSolveMode(propId) {
        var value = _propOverrideString(_propSolveModeMap, propId);
        if (value !== null) { return _normalizeSolveOptimizationMode(value); }
        return _normalizeSolveOptimizationMode(settings.solveOptimizationMode);
    }

    function _effectiveCleanupMode(propId) {
        var value = _propOverrideString(_propCleanupModeMap, propId);
        if (value !== null) { return _normalizeCleanupMode(value); }
        return _globalCleanupMode();
    }

    function _hasSolveModeOverride(propId) {
        return _propOverrideString(_propSolveModeMap, propId) !== null;
    }

    function _hasCleanupModeOverride(propId) {
        return _propOverrideString(_propCleanupModeMap, propId) !== null;
    }

    function _syncMainModeControls() {
        try {
            if (ui && ui.solveModeDDL) {
                ui.solveModeDDL.selection =
                    _solveModeIndex(settings.solveOptimizationMode);
            }
            if (ui && ui.cleanupModeDDL) {
                ui.cleanupModeDDL.selection =
                    _cleanupModeIndex(_globalCleanupMode());
            }
        } catch (e) {}
    }

    function _setMainSolveMode(mode) {
        settings.solveOptimizationMode =
            _normalizeSolveOptimizationMode(mode);
        _saveCurrentSettings();
        _refreshPropList();
    }

    function _setMainCleanupMode(mode) {
        _setGlobalCleanupMode(mode);
        _saveCurrentSettings();
        _refreshPropList();
    }

    function _applySolveOptimizationModeToConfig(
            config, mode, motionSmoothTolerance, motionSmoothBezier,
            motionSmoothUseEase, motionSmoothSourceFidelity) {
        if (!config) { config = {}; }
        mode = _normalizeSolveOptimizationMode(mode);
        config.solve_optimization_mode = mode;
        config.motion_smooth_use_ease = false;
        if (typeof motionSmoothTolerance !== 'number' ||
                !isFinite(motionSmoothTolerance) ||
                motionSmoothTolerance <= 0) {
            motionSmoothTolerance =
                (typeof settings.motionSmoothTolerance === 'number' &&
                 settings.motionSmoothTolerance > 0)
                    ? settings.motionSmoothTolerance : 3.0;
        }
        config.motion_smooth_tolerance = motionSmoothTolerance;
        motionSmoothBezier = _sanitizeMotionSmoothBezier(
            motionSmoothBezier || _motionSmoothBezierFromSettings());
        config.motion_smooth_bezier_x1 = motionSmoothBezier.x1;
        config.motion_smooth_bezier_y1 = motionSmoothBezier.y1;
        config.motion_smooth_bezier_x2 = motionSmoothBezier.x2;
        config.motion_smooth_bezier_y2 = motionSmoothBezier.y2;
        config.motion_smooth_source_fidelity = false;
        config.motion_path_smoothing_tolerance =
            _motionPathSmoothingTolerance();
        config.motion_path_accuracy_tolerance =
            _motionPathAccuracyTolerance();
        config.motion_path_preserve_bounds =
            settings.motionPathPreserveBounds === true;
        config.motion_path_bounds_tolerance =
            _motionPathBoundsTolerance();
        config.motion_path_preserve_sharp_points =
            settings.motionPathPreserveSharpPoints !== false;
        config.motion_path_sharp_angle_deg = _motionPathSharpAngleDeg();
        config.motion_path_respect_keyed_frames =
            settings.motionPathRespectKeyedFrames === true;
        if (mode === 'temporal_only') {
            config.allow_path_replacement_fit = false;
            config.path_replacement_prefer_vertices = false;
        } else if (mode === 'vertex_only') {
            config.allow_shape_temporal_bezier = false;
            config.allow_path_replacement_fit = false;
            config.path_replacement_prefer_vertices = true;
            config.path_specific_max_gap = 0;
            config.shape_temporal_bezier_attempt_threshold_ratio = -1.0;
        } else if (mode === 'motion_smooth') {
            config.allow_shape_temporal_bezier = false;
            config.allow_path_replacement_fit = false;
            config.path_replacement_prefer_vertices = false;
            config.path_specific_max_gap = 0;
            config.shape_temporal_bezier_attempt_threshold_ratio = -1.0;
            config.motion_smooth_use_ease =
                (typeof motionSmoothUseEase === 'boolean')
                    ? motionSmoothUseEase
                    : (settings.motionSmoothUseEase === true);
            config.motion_smooth_source_fidelity =
                (typeof motionSmoothSourceFidelity === 'boolean')
                    ? motionSmoothSourceFidelity
                    : (settings.motionSmoothSourceFidelity === true);
        } else if (mode === 'motion_path_smooth') {
            config.allow_shape_temporal_bezier = false;
            config.allow_path_replacement_fit = false;
            config.path_replacement_prefer_vertices = false;
            config.path_specific_max_gap = 0;
            config.shape_temporal_bezier_attempt_threshold_ratio = -1.0;
            config.motion_smooth_use_ease =
                (typeof motionSmoothUseEase === 'boolean')
                    ? motionSmoothUseEase
                    : (settings.motionSmoothUseEase === true);
            config.motion_smooth_source_fidelity = false;
        }
        return config;
    }

    function _cleanupSolveOptimizationMode(phase) {
        return phase === 'temporal' ? 'temporal_only' : 'vertex_only';
    }

    function _cleanupPhaseLabel(phase) {
        return phase === 'temporal' ? 'temporal cleanup' : 'vertex cleanup';
    }

    function _sampleCacheSignature(comp, props, autoSeparate, rangeOverride) {
        var range = rangeOverride || _timeRange(comp);
        var doSep = (typeof autoSeparate === 'boolean')
            ? autoSeparate
            : (settings.autoSeparateForBake === true);
        var parts = [
            'comp=' + (comp ? String(comp.id || comp.name || '') : ''),
            'fps=' + (comp ? String(comp.frameDuration) : ''),
            'start=' + range.tStart,
            'end=' + range.tEnd,
            'segmentMode=' + (settings.useSegmentMarkers === true ? '1' : '0'),
            'sampleMode=' + settings.sampleMode,
            'autoSep=' + (doSep === true ? '1' : '0'),
            'flattenParentedPosition=' +
                (settings.flattenParentedPosition === true ? '1' : '0'),
            'preserveSelectedParenting=' +
                (settings.preserveSelectedParenting === true ? '1' : '0'),
            'solveOptimizationMode=' +
                _normalizeSolveOptimizationMode(settings.solveOptimizationMode),
            'motionSmoothUseEase=' +
                (settings.motionSmoothUseEase === true ? '1' : '0'),
            'motionSmoothSourceFidelity=' +
                (settings.motionSmoothSourceFidelity === true ? '1' : '0'),
            'motionSmoothTemporalCleanupTolerance=' +
                _motionSmoothTemporalCleanupTolerance(),
            'motionPathSmoothingTolerance=' +
                _motionPathSmoothingTolerance(),
            'motionPathAccuracyTolerance=' +
                _motionPathAccuracyTolerance(),
            'motionPathPreserveBounds=' +
                (settings.motionPathPreserveBounds === true ? '1' : '0'),
            'motionPathBoundsTolerance=' +
                _motionPathBoundsTolerance(),
            'motionPathPreserveSharpPoints=' +
                (settings.motionPathPreserveSharpPoints !== false ? '1' : '0'),
            'motionPathSharpAngleDeg=' +
                _motionPathSharpAngleDeg(),
            'motionPathRespectKeyedFrames=' +
                (settings.motionPathRespectKeyedFrames === true ? '1' : '0'),
            'shapeTemporalFullGapFit=0',
            'shapeReplacementFit=auto_variable_topology',
            'shapeReplacementPreferVertices=' +
                (settings.shapeReplacementPreferVertices === true ? '1' : '0'),
            'preserveSharpPathCorners=' +
                (settings.preserveSharpPathCorners !== false ? '1' : '0')
        ];
        var i, prop, layerName, numKeys;
        for (i = 0; i < props.length; i++) {
            prop = props[i];
            layerName = '';
            numKeys = 0;
            try { layerName = _containingLayer(prop).name || ''; } catch (eLayerName) {}
            try { numKeys = prop.numKeys || 0; } catch (eNumKeys) {}
            parts.push(
                'prop=' + _propSignatureId(prop) +
                '|layer=' + layerName +
                '|keys=' + numKeys +
                '|' + _propExpressionSignature(prop));
        }
        return parts.join('\n');
    }

    function _sampleOrReuseForBake(comp, props, autoSeparate, rangeOverride) {
        var signature = _sampleCacheSignature(
            comp, props, autoSeparate, rangeOverride);
        var cachedBundle = _cachedSampleBundle(signature);
        if (cachedBundle &&
                cachedBundle.properties && cachedBundle.properties.length > 0) {
            log('INFO [sampling]: reusing cached sample bundle; tolerance changes do not require resampling. Click Sample again to refresh source values.');
            setProgress(10);
            setStatusText('Sampling reused from cache (10%)');
            _lastBundle = cachedBundle;
            _lastBundleSignature = signature;
            _refreshPropList();
            return cachedBundle;
        }
        return _sample(comp, props, autoSeparate, signature, rangeOverride);
    }

    function _layerParentIsSelected(layer, selectedLayerMap) {
        var parent = null;
        try { parent = layer.parent; } catch (eParent) {}
        if (!parent) { return false; }
        try { return selectedLayerMap[String(parent.index)] === true; } catch (eIdx) {}
        return false;
    }

    // _sample() calls collectSampleBundle with current settings.
    // autoSeparate: override for autoSeparateForBake (undefined = use settings).
    // Returns a SampleBundle, or null if every property was filtered out.
    function _sample(comp, props, autoSeparate, cacheSignature, rangeOverride) {
        var range     = rangeOverride || _timeRange(comp);
        var requestId = 'req-' + (new Date().getTime());
        var doSep = (typeof autoSeparate === 'boolean')
            ? autoSeparate
            : (settings.autoSeparateForBake === true);
        if (!cacheSignature) {
            cacheSignature = _sampleCacheSignature(comp, props, doSep, range);
        }

        // Pre-filter properties the panel must not hand to bbsolver. Shape/path
        // properties are bakeable when their topology is stable. Unseparated spatial
        // properties are valid bake targets; the solver emits one shared temporal
        // ease plus per-dimension spatial tangents for AE's Position model.
        // Verified: PropertyValueType.SHAPE (property.md p.495).
        // Verified: Shape.vertices is the vertex array (shape.md p.248).
        var sampleableProps = [];
        var hasUnifiedSpatial = false;
        var hasShapePath = false;
        var hasVariableTopologyShapePath = false;
        var si, sp, sc;
        var flattenLayerCandidateMap = {};
        var hasFlattenLayerCandidate = false;
        var selectedLayerMap = _selectedLayerMapFromProps(props);
        if (settings.flattenParentedPosition === true) {
            for (si = 0; si < props.length; si++) {
                try {
                    var flLayer = _containingLayer(props[si]);
                    if (_isParentedPositionFlattenCandidate(
                            props[si], _classifyProperty(props[si])) &&
                            !(settings.preserveSelectedParenting === true &&
                              _layerParentIsSelected(flLayer, selectedLayerMap))) {
                        flattenLayerCandidateMap[flLayer.index] = true;
                        hasFlattenLayerCandidate = true;
                    }
                } catch (efm) {}
            }
        }
        for (si = 0; si < props.length; si++) {
            sp = props[si];
            sc = _classifyProperty(sp);
            if (!sc.isShape) {
                var flattenCandidate = false;
                var preserveSelectedParent = false;
                var flattenThisPositionForWrite = false;
                var parentedPosition = false;
                var spLayer = null;
                try { spLayer = _containingLayer(sp); } catch (elc) {}
                if (settings.flattenParentedPosition === true && _isPositionProperty(sp)) {
                    parentedPosition = !!(spLayer && _layerHasParent(spLayer));
                    flattenCandidate = _isParentedPositionFlattenCandidate(sp, sc);
                    preserveSelectedParent =
                        settings.preserveSelectedParenting === true &&
                        spLayer &&
                        _layerParentIsSelected(spLayer, selectedLayerMap);
                    flattenThisPositionForWrite = flattenCandidate && !preserveSelectedParent;
                    if (parentedPosition && !flattenCandidate && !preserveSelectedParent) {
                        log('SKIP [parent-flatten]: ' + sp.name +
                            ' is parented but not eligible (' +
                            _parentedPositionFlattenDetails(sp, sc) + ').');
                        continue;
                    }
                    if (flattenThisPositionForWrite &&
                            typeof spLayer.sourcePointToComp !== 'function') {
                        log('SKIP [parent-flatten]: ' + sp.name +
                            ' cannot be sampled in comp space; sourcePointToComp is unavailable.');
                        continue;
                    }
                    if (flattenThisPositionForWrite) {
                        log('INFO [parent-flatten]: ' + sp.name +
                            ' on "' + spLayer.name +
                            '" will sample comp-space Position and unparent before writeback.');
                        log('WARN [parent-flatten]: selected Rotation on the same layer ' +
                            'will be sampled as comp-space angle before unparenting; Scale is not flattened yet.');
                    } else if (preserveSelectedParent) {
                        log('INFO [parent-flatten]: ' + sp.name +
                            ' on "' + spLayer.name +
                            '" keeps its selected parent; baking local Position only.');
                    }
                }
                if (settings.flattenParentedPosition === true &&
                        _isRotationZProperty(sp) &&
                        spLayer &&
                        flattenLayerCandidateMap[spLayer.index] === true) {
                    if (typeof spLayer.sourcePointToComp !== 'function') {
                        log('SKIP [parent-flatten]: ' + sp.name +
                            ' cannot be sampled as comp-space rotation; sourcePointToComp is unavailable.');
                        continue;
                    }
                    log('INFO [parent-flatten]: ' + sp.name +
                        ' on "' + spLayer.name +
                        '" will sample comp-space rotation before unparenting.');
                } else if (settings.flattenParentedPosition === true &&
                        hasFlattenLayerCandidate &&
                        _isRotationZProperty(sp) &&
                        spLayer &&
                        _is2DLayer(spLayer)) {
                    log('INFO [parent-flatten]: ' + sp.name +
                        ' on "' + spLayer.name +
                        '" will solve with rig rotation tolerance for accuracy.');
                }

                if (sc.is_spatial) {
                    var isLeader = false;
                    var separated = false;
                    var nk = 0;
                    try { isLeader  = sp.isSeparationLeader;  } catch (eisl) {}
                    try { separated = sp.dimensionsSeparated; } catch (eds) {}
                    try { nk        = sp.numKeys;             } catch (enk) {}

                    if (!isLeader && !separated) {
                        log('SKIP [spatial]: ' + sp.name +
                            ' is spatial but is not a separation leader or follower. ' +
                            'Select the Position leader or a separated X/Y/Z follower.');
                        continue;
                    }
                    if (isLeader && !separated) {
                        hasUnifiedSpatial = true;
                        if (doSep === true && nk > 0) {
                            log('INFO [spatial]: ' + sp.name +
                                ' has existing keyframes; preserving unified Position instead of auto-separating.');
                        } else if (doSep !== true) {
                            log('INFO [spatial]: ' + sp.name +
                                ' will bake as unified Position; motion path stays unseparated.');
                        }
                    }
                }
                sampleableProps.push(sp);
                continue;
            }

            // Probe topology at work-area boundaries for early user feedback.
            // The sampler scans every frame. Fixed topology stays exact
            // shape_flat; variable vertex counts are exported raw. The solver
            // attempts replacement topology automatically for variable-topology paths.
            var nv0 = -1, nv1 = -1, closed0 = -1, closed1 = -1;
            try {
                var sh0 = sp.valueAtTime(range.tStart, false);
                nv0 = sh0.vertices.length;
                closed0 = sh0.closed ? 1 : 0;
            } catch (ep0) {}
            try {
                var sh1 = sp.valueAtTime(range.tEnd, false);
                nv1 = sh1.vertices.length;
                closed1 = sh1.closed ? 1 : 0;
            } catch (ep1) {}

            if (closed0 >= 0 && closed1 >= 0 && closed0 !== closed1) {
                log('SKIP [path]: variable-closed shape at work-area boundaries. ' +
                    'Canonical path bake requires the path to stay open or closed consistently.');
            } else if (nv0 < 0 || nv1 < 0) {
                log('SKIP [path]: shape/path vertex count could not be inspected. ' +
                    'Use tools/ae_harness_export.jsx to export path fixtures for the solver.');
            } else if (nv0 !== nv1) {
                hasVariableTopologyShapePath = true;
                log('INFO [path]: ' + sp.name + ' has variable vertex count (' +
                    nv0 + ' at t=' + range.tStart.toFixed(2) + 's, ' +
                    nv1 + ' at t=' + range.tEnd.toFixed(2) +
                    's); bbsolver replacement topology fitting will be enabled for this variable-topology path.');
                hasShapePath = true;
                sampleableProps.push(sp);
            } else {
                log('INFO [path]: ' + sp.name + ' will bake as shape_flat path (' +
                    nv0 + ' vertices, dims=' + (2 + 6 * nv0) +
                    '; sampler will scan for mid-range topology changes).');
                hasShapePath = true;
                sampleableProps.push(sp);
            }
        }
        if (sampleableProps.length === 0) { return null; }

        var effectiveSampleMode = settings.sampleMode;
        if (hasShapePath && effectiveSampleMode !== 'frame') {
            log('INFO [path]: shape_flat Path uses frame-centre sampling in v1; ' +
                'sub-frame motion-blur sampling is disabled for Path bakes.');
            effectiveSampleMode = 'frame';
        }
        if (hasUnifiedSpatial && effectiveSampleMode !== 'frame') {
            log('INFO [spatial]: unified Position uses frame-centre sampling in v1; ' +
                'sub-frame motion-blur sampling is not optimized yet.');
            effectiveSampleMode = 'frame';
        }

        var opts = {
            sampleMode:          effectiveSampleMode,
            tStartSec:           range.tStart,
            tEndSec:             range.tEnd,
            toleranceUnits:      settings.tolerance,
            toleranceScreenPx:   settings.toleranceScreenPx,
            requestId:           requestId,
            autoSeparateForBake: doSep,
            flattenParentedPosition: settings.flattenParentedPosition === true,
            preserveSelectedParenting:
                settings.preserveSelectedParenting === true,
            shapeTemporalFullGapFit: false,
            shapeReplacementFit: hasVariableTopologyShapePath === true,
            shapeReplacementPreferVertices:
                settings.shapeReplacementPreferVertices === true,
            motionSmoothTolerance: _effectiveMotionSmoothTolerance(''),
            motionSmoothBezierX1: _effectiveMotionSmoothBezier('').x1,
            motionSmoothBezierY1: _effectiveMotionSmoothBezier('').y1,
            motionSmoothBezierX2: _effectiveMotionSmoothBezier('').x2,
            motionSmoothBezierY2: _effectiveMotionSmoothBezier('').y2,
            motionPathSmoothingTolerance: _motionPathSmoothingTolerance(),
            motionPathAccuracyTolerance: _motionPathAccuracyTolerance(),
            motionPathPreserveBounds:
                settings.motionPathPreserveBounds === true,
            motionPathBoundsTolerance: _motionPathBoundsTolerance(),
            motionPathPreserveSharpPoints:
                settings.motionPathPreserveSharpPoints !== false,
            motionPathSharpAngleDeg: _motionPathSharpAngleDeg(),
            motionPathRespectKeyedFrames:
                settings.motionPathRespectKeyedFrames === true,
            motionSmoothSourceFidelity:
                settings.motionSmoothSourceFidelity === true,
            solveOptimizationMode:
                _normalizeSolveOptimizationMode(settings.solveOptimizationMode),
            preserveSharpPathCorners:
                settings.preserveSharpPathCorners !== false
        };
        var sampleFrameCount = Math.max(
            1,
            Math.round((range.tEnd - range.tStart) / comp.frameDuration) + 1);
        var sampleSpf = 1;
        try { sampleSpf = resolveSamplesPerFrame(opts, comp); } catch (esp) {}
        var sampleReadTotal = 0;
        var samplePlanDims, samplePlanLeader, samplePlanSeparated, samplePlanSc;
        var samplePlanKeys, samplePlanFlatten, samplePlanAutoSep;
        for (var samplePlanIdx = 0; samplePlanIdx < sampleableProps.length; samplePlanIdx++) {
            samplePlanDims = 1;
            samplePlanLeader = false;
            samplePlanSeparated = false;
            samplePlanKeys = 0;
            samplePlanFlatten = false;
            try {
                samplePlanSc = _classifyProperty(sampleableProps[samplePlanIdx]);
                samplePlanDims = samplePlanSc.dimensions || 1;
                samplePlanLeader = sampleableProps[samplePlanIdx].isSeparationLeader;
                samplePlanSeparated = sampleableProps[samplePlanIdx].dimensionsSeparated;
                samplePlanKeys = sampleableProps[samplePlanIdx].numKeys || 0;
                samplePlanFlatten = settings.flattenParentedPosition === true &&
                    _isParentedPositionFlattenCandidate(
                        sampleableProps[samplePlanIdx], samplePlanSc);
            } catch (espd) {}
            samplePlanAutoSep = samplePlanLeader && !samplePlanSeparated &&
                doSep === true && samplePlanKeys === 0 && !samplePlanFlatten;
            if (samplePlanLeader && (samplePlanSeparated || samplePlanAutoSep)) {
                sampleReadTotal += sampleFrameCount * samplePlanDims * sampleSpf;
            } else {
                sampleReadTotal += sampleFrameCount * sampleSpf;
            }
        }
        opts.sampleReadTotal = sampleReadTotal;
        var sampleStartedMs = (new Date()).getTime();
        var sampleLastLogMs = 0;
        var sampleLastPct = -1;
        var sampleLastLabel = '';
        function sampleShortLabel(label) {
            var s = String(label || '');
            if (s.length > 76) { return '...' + s.substr(s.length - 73); }
            return s;
        }
        opts.progressCallback = function (info) {
            info = info || {};
            var total = Math.max(1, info.read_total || sampleReadTotal || 1);
            var read = Math.max(0, info.read_index || 0);
            var frac = Math.max(0, Math.min(1, read / total));
            var pct = 5 + Math.round(5 * frac);
            if (pct < 5) { pct = 5; }
            if (pct > 10) { pct = 10; }
            if (pct < sampleLastPct) { pct = sampleLastPct; }

            var propTotal = Math.max(1, info.property_total || sampleableProps.length || 1);
            var propIdx = Math.min(propTotal, (info.property_index || 0) + 1);
            var label = 'sampling: ' + (info.stage || 'sample') +
                ' ' + propIdx + '/' + propTotal;
            if (info.sample_total > 0) {
                label += ' frame ' + Math.min(info.sample_total, info.sample_index || 0) +
                    '/' + info.sample_total;
            }
            var shortProp = sampleShortLabel(info.property_label || '');
            if (shortProp) { label += ' ' + shortProp; }

            if (pct !== sampleLastPct || label !== sampleLastLabel) {
                _paintSolveElapsedDuringBusyWork();
                setProgress(pct);
                setStatusText(label + ' (' + pct + '%)');
                sampleLastPct = pct;
                sampleLastLabel = label;
            }

            var nowMs = (new Date()).getTime();
            if (info.stage === 'property_done') {
                log('  sample done ' + propIdx + '/' + propTotal + ': ' +
                    shortProp + ' (' +
                    ((info.elapsed_ms || 0) / 1000).toFixed(1) + 's)');
                sampleLastLogMs = nowMs;
            } else if (nowMs - sampleLastLogMs >= 2500) {
                log('  sample progress: ' + propIdx + '/' + propTotal +
                    ' frame ' + (info.sample_index || 0) + '/' +
                    (info.sample_total || sampleFrameCount) +
                    ' (' + pct + '%)');
                sampleLastLogMs = nowMs;
            }
        };
        log('INFO [sampling]: plan ' + sampleableProps.length + ' propert' +
            (sampleableProps.length === 1 ? 'y' : 'ies') +
            ' x ' + sampleFrameCount + ' frames x ' + sampleSpf +
            ' sample(s)/frame = ' + sampleReadTotal + ' AE value reads.');
        if (settings.flattenParentedPosition === true && hasFlattenLayerCandidate) {
            log('INFO [sampling]: parent-flatten sampling uses comp-space sourcePointToComp; ' +
                'this is AE-side work before bbsolver starts.');
        }
        var bundle = collectSampleBundle(sampleableProps, opts);
        if (bundle && bundle.config) {
            _applySolveOptimizationModeToConfig(
                bundle.config,
                settings.solveOptimizationMode,
                _effectiveMotionSmoothTolerance(''));
        }
        log('INFO [sampling]: collected sample bundle in ' +
            (((new Date()).getTime() - sampleStartedMs) / 1000).toFixed(1) + 's.');
        if (bundle && bundle.config && bundle.config.solve_optimization_mode) {
            log('INFO [path]: Solve optimization mode: ' +
                _solveOptimizationModeLabel(bundle.config.solve_optimization_mode) +
                ' (' + bundle.config.solve_optimization_mode + ').');
        }
        if (bundle && bundle.config && bundle.config.allow_shape_temporal_bezier === true) {
            log('INFO [path]: Shape temporal Bezier optimization enabled for path timing.');
        }
        if (bundle && bundle.config && bundle.config.path_specific_max_gap > 0) {
            log('INFO [path]: Experimental full-gap Shape temporal fitting enabled ' +
                '(max_gap=' + bundle.config.path_specific_max_gap +
                ', threshold_ratio=' +
                bundle.config.shape_temporal_bezier_attempt_threshold_ratio + ').');
        }
        if (bundle && bundle.config && bundle.config.allow_path_spatial_fit === true) {
            log('INFO [path]: Solver-side canonical path fitting enabled.');
        }
        if (bundle && bundle.config && bundle.config.allow_path_replacement_fit === true) {
            log('INFO [path]: Solver-side replacement topology fitting enabled; rejected candidates fall back to temporal keys.');
        }
        if (bundle && bundle.config &&
                bundle.config.path_replacement_prefer_vertices === true) {
            log('INFO [path]: Guarded second-pass vertex prune enabled; it runs after key reduction and preserves the accepted key timing.');
        }
        if (bundle && bundle.config &&
                (bundle.config.allow_path_replacement_fit === true ||
                 bundle.config.path_replacement_prefer_vertices === true) &&
                bundle.config.path_preserve_sharp_corners !== false) {
            log('INFO [path]: Persistent sharp path corners are guarded during vertex reduction; the corner lock scales with the active solve tolerance.');
        }
        var bi, bps, binfo;
        for (bi = 0; bi < bundle.properties.length; bi++) {
            bps = bundle.properties[bi];
            binfo = bps.property || {};
            if (binfo.shape_canonicalized === true) {
                log('INFO [path]: ' + (binfo.display_name || binfo.id || 'Path') +
                    ' canonicalized variable topology to ' +
                    binfo.shape_canonical_vertex_count + ' vertices (' +
                    binfo.shape_canonical_method + ').');
            } else if (binfo.shape_variable_topology === true) {
                log('INFO [path]: ' + (binfo.display_name || binfo.id || 'Path') +
                    ' exported raw variable topology (max ' +
                    binfo.shape_max_vertex_count +
                    ' vertices); geometry reduction will run in bbsolver.');
            }
        }
        _lastBundle = bundle;
        _lastBundleSignature = cacheSignature;
        _rememberSampleBundle(cacheSignature, bundle);
        _refreshPropList();
        return bundle;
    }

    function _flattenParentedPositionMapFromBundle(bundle) {
        var map = {};
        if (!bundle || !bundle.properties) { return map; }
        var i, ps, info;
        for (i = 0; i < bundle.properties.length; i++) {
            ps = bundle.properties[i];
            info = ps.property || {};
            if (info.flatten_parented_position === true && info.id) {
                map[info.id] = true;
            }
        }
        return map;
    }

    function _countMapKeys(map) {
        var n = 0;
        var k;
        for (k in map) {
            if (Object.prototype.hasOwnProperty.call(map, k)) { n = n + 1; }
        }
        return n;
    }

    function _unifiedSpatialSolveBlocker(bundle) {
        if (!bundle || !bundle.properties) { return ''; }
        var i;
        for (i = 0; i < bundle.properties.length; i++) {
            var ps = bundle.properties[i];
            var info = ps.property || {};
            if (info.is_spatial === true && info.is_separated !== true &&
                    ps.samples_per_frame && ps.samples_per_frame > 1) {
                return 'Unified spatial property "' + (info.display_name || info.name || info.id || 'Position') +
                    '" was sampled at ' + ps.samples_per_frame +
                    ' samples per frame. Unified Position optimization is frame-centre only in v1; ' +
                    'set Sample resolution to Frame centre and bake again.';
            }
        }
        return '';
    }

    // ---- Solver invocation -----------------------------------------------

    // ---- bbsolver runner (sync + async) -----------------------------------
    //
    // bbsolver is launched with --progress-fd 1, which makes it emit JSON
    // progress rows to stdout as it advances through solver phases. To
    // surface that information live in the UI we run bbsolver inside a
    // detached shell sub-process and tail its progress log from scheduled
    // short callbacks. Between polls we read any new JSON rows, update
    // the progress bar / status label / log, and return control to AE.
    //
    // Important caveats (called out so the user does not expect more
    // than the runtime can deliver):
    //
    // * The cancel button click handler does NOT fire during the poll
    //   legacy blocking loop -- ExtendScript runs single-threaded on AE's
    //   main thread. The cooperative macOS runner returns control to AE
    //   between polls. The cancel pathway still works because bbsolver
    //   itself polls the --cancel-file sentinel; if the user touches
    //   that file from a Terminal the solver will exit 5 and the poll
    //   loop will observe the done sentinel.
    // * Async launch is macOS-only (writes a small POSIX shell script
    //   to <outPath>.runner.sh and runs it via /bin/sh). On other
    //   platforms or when shell launch fails, the runner falls back to
    //   the original synchronous path (system.callSystem) and replays
    //   progress after bbsolver returns.

    function _shellQuote(s) {
        // Quote a value for /bin/sh inside double quotes. Inside a
        // double-quoted context the shell still interprets backslash,
        // dollar sign, BTICK (the command-substitution character whose
        // literal cannot appear in this comment because the panel-side
        // ES3 lint forbids it), and double quote itself. Everything
        // else (including apostrophes and spaces) is literal. AE
        // scratch paths almost never contain these characters, but
        // make the helper correct regardless. The BTICK regex and
        // replacement are built from String.fromCharCode(96) so the
        // ScriptUI source itself contains no template-literal token.
        var BTICK = String.fromCharCode(96);
        var BTICK_RE = new RegExp(BTICK, 'g');
        var str = String(s);
        str = str.replace(/\\/g, '\\\\');   // backslash first
        str = str.replace(/"/g,  '\\"');
        str = str.replace(/\$/g, '\\$');
        str = str.replace(BTICK_RE, '\\' + BTICK);
        return '"' + str + '"';
    }

    function _isMacOSPlatform() {
        var os = '';
        try { os = String($.os || ''); } catch (e) {}
        os = os.toLowerCase();
        return os.indexOf('mac') >= 0 || os.indexOf('darwin') >= 0;
    }

    function _isWindowsPlatform() {
        var os = '';
        try { os = String($.os || ''); } catch (e) {}
        return os.toLowerCase().indexOf('windows') >= 0;
    }

    // Quote a value for Windows cmd.exe inside system.callSystem. cmd.exe
    // joins arguments inside double quotes and treats backslashes literally
    // (unlike POSIX shells), so paths like C:\Users\name pass through
    // unchanged. The only character that needs escaping inside a
    // double-quoted region is the double quote itself, which we double per
    // the cmd.exe convention. AE scratch paths do not contain '%' or '!',
    // so we do not attempt to neutralize those.
    function _winQuote(s) {
        var str = String(s).replace(/"/g, '""');
        return '"' + str + '"';
    }

    // Pick the right quoting strategy for whichever shell system.callSystem
    // will hand the command to. POSIX async runner script bodies continue
    // to use _shellQuote directly because they always execute under /bin/sh.
    function _platformQuote(s) {
        if (_isWindowsPlatform()) { return _winQuote(s); }
        return _shellQuote(s);
    }

    function _safeUnlink(p) {
        var f = new File(p);
        if (f.exists) { try { f.remove(); } catch (e) {} }
    }

    function _readFileText(path) {
        var f = new File(path);
        if (!f.exists) { return null; }
        try {
            f.encoding = 'UTF-8';
            if (!f.open('r')) { return null; }
            var text = f.read();
            f.close();
            return text || '';
        } catch (e) { return null; }
    }

    function _touchFile(path) {
        if (!path) { return; }
        var f = new File(path);
        if (f.exists) { return; }
        try { if (f.open('w')) { f.close(); } } catch (e) {}
    }

    // Write a Unix shell script to the given path. Returns true on
    // success. The script is written with LF line endings (Mac shells
    // dislike CRLF) and a #!/bin/sh shebang so it is self-describing.
    function _writeShellScript(path, body) {
        var f = new File(path);
        try {
            f.encoding = 'UTF-8';
            f.lineFeed = 'Unix';
            if (!f.open('w')) { return false; }
            f.write('#!/bin/sh\n' + body);
            f.close();
            return true;
        } catch (e) {
            try { f.close(); } catch (e2) {}
            return false;
        }
    }

    // Take a freshly read tail of the progress log and return the
    // newline-bounded portion only. The trailing bytes (anything after
    // the last '\n') stay in the buffer for the next poll tick to pick
    // up once bbsolver has finished flushing the line. Without this guard
    // a half-written JSON row would be split across two polls and both
    // halves would fail JSON.parse.
    //
    // Returns { complete: <str>, consumed: <int> } where consumed is
    // the number of bytes from the start of tail that were
    // newline-terminated. complete is the newline-terminated prefix
    // (without the final newline).
    function _completedLines(tail) {
        if (!tail) { return { complete: '', consumed: 0 }; }
        var lastNewline = tail.lastIndexOf('\n');
        if (lastNewline < 0) { return { complete: '', consumed: 0 }; }
        return {
            complete: tail.substr(0, lastNewline),
            consumed: lastNewline + 1
        };
    }

    // Forward declaration so _runBbsolver can dispatch before the helpers
    // are defined below; ExtendScript hoists function declarations like
    // ES5, so this is safe.
    function _runBbsolverSync(
            bbsolver, inPath, outPath, tol, tolPx, cmdBase,
            solverStartPct, solverEndPct) {
        var bbsolverStartMs = (new Date()).getTime();
        var winFallback = _isWindowsPlatform();
        if (winFallback) {
            log('  solver: Windows sync fallback (no live async progress); ' +
                'UI will freeze until bbsolver returns.');
            setStatusText('solver: running (Windows sync fallback, UI frozen)');
        } else {
            log('  solver: launching bbsolver synchronously (UI will freeze ' +
                'until it returns).');
            setStatusText('solver: running (sync, UI frozen)');
        }

        var cmdOut;
        try { cmdOut = system.callSystem(cmdBase); } catch (e) {
            if (new File(outPath).exists) {
                log('  WARN: bbsolver non-zero exit, output exists: ' + e.message);
                cmdOut = '';
            } else {
                throw new Error('bbsolver failed: ' + e.message);
            }
        }
        var bbsolverMs = (new Date()).getTime() - bbsolverStartMs;
        log('  solver: bbsolver returned in ' + (bbsolverMs / 1000).toFixed(1) +
            's; replaying progress.');
        _parseProgress(cmdOut, solverStartPct, solverEndPct);
        if (cmdOut) {
            var lines = cmdOut.split('\n');
            var li, line;
            for (li = 0; li < lines.length; li++) {
                line = lines[li].replace(/^\s+|\s+$/g, '');
                if (line && line.charAt(0) !== '{') { log('  ' + line); }
            }
        }
        if (!new File(outPath).exists) {
            throw new Error('bbsolver produced no output at: ' + outPath);
        }
        return readKeyBundleJson(outPath);
    }

    var _asyncBbsolverRuns = {};
    var _asyncBbsolverSeq = 0;
    var _activeBbsolverState = null;

    function _scheduleBbsolverPoll(runId, delayMs) {
        var delay = (typeof delayMs === 'number' && delayMs >= 0) ? delayMs : 200;
        try {
            app.scheduleTask('$.global.bbsolverHarnessPollBbsolver("' + runId + '")',
                delay, false);
            return true;
        } catch (se) {
            log('  WARN: could not schedule bbsolver poll callback (' +
                se.message + ').');
            return false;
        }
    }

    function _consumeBbsolverProgress(state, text) {
        if (!text || text.length <= state.processedChars) { return 0; }
        var fresh = text.substr(state.processedChars);
        var trimmed = _completedLines(fresh);
        if (trimmed.consumed <= 0) { return 0; }
        state.processedChars = state.processedChars + trimmed.consumed;
        var freshLines = trimmed.complete.split('\n');
        var fli, fln, fobj, fpct, fphase;
        state.progressBytesConsumed = (state.progressBytesConsumed || 0) +
            trimmed.consumed;
        for (fli = 0; fli < freshLines.length; fli++) {
            fln = freshLines[fli].replace(/^\s+|\s+$/g, '');
            if (!fln) { continue; }
            if (fln.charAt(0) !== '{') {
                log('  ' + fln);
                continue;
            }
            try { fobj = JSON.parse(fln); } catch (eje) { continue; }
            if (typeof fobj.progress !== 'number') { continue; }
            state.progressEventCount = (state.progressEventCount || 0) + 1;
            fpct = Math.round(fobj.progress * 100);
            fphase = fobj.phase ? String(fobj.phase) : '';
            if (fphase !== state.lastPhase || fpct !== state.lastPct) {
                var mappedPct = _mappedSolverPct(
                    fobj.progress, state.solverStartPct, state.solverEndPct);
                if (state.lastBarPct >= 0 && mappedPct < state.lastBarPct) {
                    mappedPct = state.lastBarPct;
                }
                log('  solver: ' + (fphase || 'progress') +
                    ' (' + fpct + '%)');
                setStatusText('solver: ' + (fphase || '...') +
                    ' (' + fpct + '%)');
                setProgress(mappedPct);
                state.lastBarPct = mappedPct;
                state.lastPhase = fphase;
                state.lastPct = fpct;
            }
        }
        return trimmed.consumed;
    }

    function _readBbsolverPid(pidPath) {
        var txt = _readFileText(pidPath);
        if (!txt) { return ''; }
        txt = txt.replace(/^\s+|\s+$/g, '');
        if (!/^[0-9]+$/.test(txt)) { return ''; }
        var n = parseInt(txt, 10);
        if (!isFinite(n) || n <= 1) { return ''; }
        return String(n);
    }

    function _bbsolverPidCommand(pid) {
        if (!pid) { return ''; }
        // /bin/ps is POSIX-only. The cooperative async runner is gated to
        // macOS upstream, so this branch is defensive: never shell out to
        // /bin/ps on Windows even if a future caller mis-wires the path.
        if (!_isMacOSPlatform()) { return ''; }
        try {
            return system.callSystem('/bin/ps -p ' + pid + ' -o command=');
        } catch (pe) {
            return '';
        }
    }

    function _pidLooksLikeBbsolverSolve(pid) {
        var cmd = _bbsolverPidCommand(pid);
        if (!cmd) { return false; }
        return cmd.indexOf('bbsolver') >= 0 &&
            cmd.indexOf(' solve ') >= 0;
    }

    function _abortCooperativeBbsolver(state, reason) {
        if (!state) { return; }
        if (state.cancelPath) { _touchFile(state.cancelPath); }
        // /bin/kill is POSIX-only. The async runner is macOS-only by
        // construction, so this guard is defensive: on non-Mac we rely on
        // bbsolver observing the cancel-file sentinel and exiting on its
        // own, with no syscall to a POSIX signal.
        if (!_isMacOSPlatform()) { return; }

        var pid = _readBbsolverPid(state.pidPath);
        if (!pid) { return; }
        if (!_pidLooksLikeBbsolverSolve(pid)) {
            log('  WARN: refusing to kill pid ' + pid +
                ' because it does not look like current bbsolver solve.');
            return;
        }

        log('  solver: aborting bbsolver pid ' + pid +
            (reason ? ' (' + reason + ')' : '') + '.');
        try { system.callSystem('/bin/kill -TERM ' + pid); } catch (ke) {}
    }

    function _finishCooperativeBbsolver(state) {
        var tail = _readFileText(state.progressPath);
        if (tail && tail.length > state.processedChars) {
            _consumeBbsolverProgress(state, tail);
            var leftover = tail.substr(state.processedChars);
            if (leftover.length > 0) {
                _parseProgress(
                    leftover, state.solverStartPct, state.solverEndPct);
            }
        }

        var rc = -1;
        var rcText = _readFileText(state.rcPath);
        if (rcText) {
            rc = parseInt(rcText.replace(/^\s+|\s+$/g, ''), 10);
        }

        var totalMs = (new Date()).getTime() - state.bbsolverStartMs;
        log('  solver: done in ' + (totalMs / 1000).toFixed(1) +
            's (rc=' + (isFinite(rc) ? rc : '?') + ')');
        log('  solver: poll summary: polls=' + state.pollCount +
            ', bytes=' + state.progressBytesConsumed +
            ', events=' + state.progressEventCount);
        setStatusText('solver: done');

        _safeUnlink(state.donePath);
        _safeUnlink(state.rcPath);
        _safeUnlink(state.pidPath);
        _safeUnlink(state.scriptPath);
        if (_activeBbsolverState && _activeBbsolverState.id === state.id) {
            _activeBbsolverState = null;
        }
        delete _asyncBbsolverRuns[state.id];

        try {
            if (!new File(state.outPath).exists) {
                throw new Error('bbsolver produced no output at: ' + state.outPath +
                    ' (rc=' + rc +
                    '; progress log retained at ' + state.progressPath + ')');
            }
            state.onDone(readKeyBundleJson(state.outPath));
        } catch (doneErr) {
            state.onError(doneErr);
        }
    }

    $.global.bbsolverHarnessPollBbsolver = function (runId) {
        var state = _asyncBbsolverRuns[runId];
        if (!state) { return; }
        try {
            state.pollCount++;
            var elapsedMs = (new Date()).getTime() - state.bbsolverStartMs;
            _consumeBbsolverProgress(state, _readFileText(state.progressPath));

            if (!state.cancelObserved && _isCancelled()) {
                state.cancelObserved = true;
                state.cancelObservedMs = elapsedMs;
                log('  solver: cancel observed; waiting for bbsolver to exit.');
                setStatusText('solver: cancel observed; awaiting exit');
            }

            if (!state.cancelObserved &&
                    elapsedMs - state.lastHeartbeatProgressMs >=
                        state.heartbeatProgressMs) {
                state.lastHeartbeatProgressMs = elapsedMs;
                var heartbeatPct = _heartbeatSolverPct(
                    state.lastBarPct, state.solverStartPct,
                    state.solverEndPct, elapsedMs);
                if (heartbeatPct > state.lastBarPct) {
                    setProgress(heartbeatPct);
                    state.lastBarPct = heartbeatPct;
                }
            }

            if (elapsedMs - state.lastHeartbeatLogMs >= state.heartbeatLogMs) {
                state.lastHeartbeatLogMs = elapsedMs;
                var elapsedS = (elapsedMs / 1000).toFixed(0);
                log('  solver: poll alive (' + elapsedS + 's, polls=' +
                    state.pollCount + ', bytes=' +
                    state.progressBytesConsumed + ', events=' +
                    state.progressEventCount + ')');
                if (state.cancelObserved) {
                    setStatusText('solver: cancel observed; awaiting exit (' +
                        elapsedS + 's)');
                } else if (state.lastPct < 0) {
                    setStatusText('solver: starting (elapsed ' + elapsedS + 's)');
                    log('  solver: still starting (' + elapsedS + 's elapsed)');
                } else {
                    setStatusText('solver: ' + (state.lastPhase || 'progress') +
                        ' (' + state.lastPct + '%) elapsed ' + elapsedS + 's');
                }
            }

            if (new File(state.donePath).exists) {
                _finishCooperativeBbsolver(state);
                return;
            }

            if (state.processedChars <= 0 &&
                    elapsedMs > state.noOutputWaitMs) {
                log('  ERROR: bbsolver runner produced no progress output within ' +
                    (state.noOutputWaitMs / 1000) +
                    's. Progress log retained at: ' + state.progressPath);
                throw new Error('bbsolver runner produced no progress output within ' +
                    (state.noOutputWaitMs / 1000) + 's');
            }
            if (state.cancelObserved && state.cancelObservedMs >= 0 &&
                    elapsedMs - state.cancelObservedMs > state.cancelExitWaitMs) {
                log('  ERROR: bbsolver did not exit within ' +
                    (state.cancelExitWaitMs / 1000) +
                    's after cancel was observed. Progress log retained at: ' +
                    state.progressPath);
                throw new Error('bbsolver cancel watchdog exceeded ' +
                    (state.cancelExitWaitMs / 1000) + 's');
            }
            if (state.maxWaitMs > 0 && elapsedMs > state.maxWaitMs) {
                log('  ERROR: bbsolver watchdog exceeded ' +
                    (state.maxWaitMs / 1000) +
                    's; Progress log retained at: ' + state.progressPath);
                throw new Error('bbsolver poll watchdog exceeded ' +
                    (state.maxWaitMs / 1000) + 's');
            }
            if (!_scheduleBbsolverPoll(runId, state.pollMs)) {
                throw new Error('could not schedule bbsolver poll callback');
            }
        } catch (pollErr) {
            _abortCooperativeBbsolver(state, pollErr.message);
            _safeUnlink(state.donePath);
            _safeUnlink(state.rcPath);
            _safeUnlink(state.pidPath);
            _safeUnlink(state.scriptPath);
            if (_activeBbsolverState && _activeBbsolverState.id === runId) {
                _activeBbsolverState = null;
            }
            delete _asyncBbsolverRuns[runId];
            state.onError(pollErr);
        }
    };

    function _runBbsolverAsync(
            bbsolver, inPath, outPath, tol, tolPx, cmdBase,
            solverStartPct, solverEndPct, onDone, onError) {
        if (typeof onDone !== 'function' || typeof onError !== 'function') {
            log('  solver: cooperative async runner was called without a continuation; ' +
                'using synchronous fallback.');
            return _runBbsolverSync(
                bbsolver, inPath, outPath, tol, tolPx, cmdBase,
                solverStartPct, solverEndPct);
        }
        var progressPath = outPath + '.progress.log';
        var donePath     = outPath + '.done';
        var rcPath       = outPath + '.rc';
        var pidPath      = outPath + '.bbsolver.pid';
        var scriptPath   = outPath + '.runner.sh';
        _safeUnlink(progressPath);
        _safeUnlink(donePath);
        _safeUnlink(rcPath);
        _safeUnlink(pidPath);
        _safeUnlink(scriptPath);

        var scriptBody =
            '(\n' +
            "  printf '%s\\n' " +
                _shellQuote('{"phase":"runner_launch","progress":0.0}') +
                ' > ' + _shellQuote(progressPath) + '\n' +
            '  ' + cmdBase + ' >> ' + _shellQuote(progressPath) + ' 2>&1 &\n' +
            '  bb_pid=$!\n' +
            '  echo "$bb_pid" > ' + _shellQuote(pidPath) + '\n' +
            '  wait "$bb_pid"\n' +
            '  bb_rc=$?\n' +
            '  echo "$bb_rc" > ' + _shellQuote(rcPath) + '\n' +
            '  touch ' + _shellQuote(donePath) + '\n' +
            ') >/dev/null 2>&1 </dev/null &\n';
        if (!_writeShellScript(scriptPath, scriptBody)) {
            log('  WARN: could not write async runner script at ' +
                scriptPath + '.');
            return false;
        }

        var launchCmd = '/bin/sh ' + _shellQuote(scriptPath) +
            ' </dev/null >/dev/null 2>&1';
        var bbsolverStartMs = (new Date()).getTime();
        log('  solver: runner mode = async scheduleTask (macOS)');
        log('  solver: launching bbsolver async ' +
            '(progress log: ' + progressPath + ', ' +
            'runner: ' + scriptPath + ').');
        setStatusText('solver: starting');

        _asyncBbsolverSeq++;
        var runId = 'bb_run_' + (new Date()).getTime() + '_' + _asyncBbsolverSeq;
        var state = {
            id: runId,
            outPath: outPath,
            progressPath: progressPath,
            donePath: donePath,
            rcPath: rcPath,
            pidPath: pidPath,
            scriptPath: scriptPath,
            cancelPath: _cancelFile,
            solverStartPct: solverStartPct,
            solverEndPct: solverEndPct,
            bbsolverStartMs: bbsolverStartMs,
            processedChars: 0,
            lastPhase: '',
            lastPct: -1,
            lastBarPct: -1,
            pollCount: 0,
            progressBytesConsumed: 0,
            progressEventCount: 0,
            lastHeartbeatProgressMs: 0,
            lastHeartbeatLogMs: 0,
            cancelObserved: false,
            cancelObservedMs: -1,
            pollMs: 200,
            heartbeatProgressMs: 1000,
            heartbeatLogMs: 5000,
            noOutputWaitMs: 15 * 1000,
            // Do not impose a wall-clock limit on path solves. Long vertex
            // reduction can legitimately exceed 30 minutes while still
            // emitting progress. Startup silence and cancel exit timeout are
            // still guarded separately.
            maxWaitMs: 0,
            cancelExitWaitMs: 60 * 1000,
            onDone: onDone,
            onError: onError
        };
        _asyncBbsolverRuns[runId] = state;
        _activeBbsolverState = state;
        if (!_scheduleBbsolverPoll(runId, 1)) {
            delete _asyncBbsolverRuns[runId];
            if (_activeBbsolverState && _activeBbsolverState.id === runId) {
                _activeBbsolverState = null;
            }
            _safeUnlink(scriptPath);
            return false;
        }

        try { system.callSystem(launchCmd); }
        catch (le) {
            _safeUnlink(scriptPath);
            delete _asyncBbsolverRuns[runId];
            if (_activeBbsolverState && _activeBbsolverState.id === runId) {
                _activeBbsolverState = null;
            }
            log('  WARN: async shell launch failed (' + le.message + ').');
            return false;
        }
        log('  solver: async launch returned; scheduled polling is active.');
        return true;
    }

    // Run bbsolver for a given input/output path and tolerance.
    // Passes --cancel-file so bbsolver can abort on detecting the sentinel.
    // Returns the parsed KeyBundle object.
    //
    // On macOS the runner uses the async path that tails a progress log
    // while bbsolver executes, updating UI live. On other platforms (or if
    // async launch fails) it falls back to the synchronous path that
    // replays progress after bbsolver returns. Either way the panel logs
    // make clear which mode is in effect.
    function _runBbsolver(
            bbsolver, inPath, outPath, tol, tolPx,
            solverStartPct, solverEndPct, solverConfig) {
        var cancelArg = _cancelFile ? (' --cancel-file ' + _platformQuote(_cancelFile)) : '';
        var lmArg = '';
        solverConfig = solverConfig || {};
        var solverJobs = _solverJobsSetting();
        var jobsArg = ' --jobs ' + solverJobs;
        var solveMode = _normalizeSolveOptimizationMode(
            solverConfig.solve_optimization_mode);
        var cmdBase = _platformQuote(bbsolver) +
            ' solve ' + _platformQuote(inPath) + ' ' + _platformQuote(outPath) +
            ' --tolerance ' + tol +
            ' --screen-px ' + tolPx +
            jobsArg +
            ' --solve-mode ' + solveMode +
            ' --progress-fd 1' +
            cancelArg +
            lmArg;
        log('  bbsolver: tol=' + tol + ', px=' + tolPx +
            ', jobs=' + (solverJobs === 0 ? 'auto' : solverJobs) +
            ', solve mode=' + solveMode +
            ', second-pass vertex prune=' +
            (solverConfig.path_replacement_prefer_vertices === true ? 'on' : 'off') +
            ', replacement topology=' +
            (solverConfig.allow_path_replacement_fit === true ? 'on' : 'off') +
            ', preserve sharp corners=' +
            (solverConfig.path_preserve_sharp_corners !== false ? 'on' : 'off') +
            (cancelArg ? ' [cancel-file set]' : ''));
        if (_isMacOSPlatform()) {
            try {
                return _runBbsolverAsync(
                    bbsolver, inPath, outPath, tol, tolPx, cmdBase,
                    solverStartPct, solverEndPct);
            } catch (ae) {
                // Async run threw -- surface and fall through to sync so
                // a transient async failure doesn't lose the bake. The
                // sync path will re-run bbsolver from scratch.
                log('  WARN: async runner threw, retrying synchronously: ' +
                    ae.message);
            }
        } else if (_isWindowsPlatform()) {
            log('  solver: Windows sync fallback (async live-progress runner ' +
                'is macOS-only); /bin/sh, /bin/ps, and /bin/kill paths are ' +
                'skipped on Windows.');
        } else {
            log('  solver: async live-progress mode is macOS-only; ' +
                'running synchronously.');
        }
        return _runBbsolverSync(
            bbsolver, inPath, outPath, tol, tolPx, cmdBase,
            solverStartPct, solverEndPct);
    }

    // ---- Convergence error dialog ----------------------------------------

    function _showConvergenceErrorDialog(failedResults, allResults, labelMap) {
        var dlg = new Window('dialog', 'bbsolver-test-harness: Convergence Failures');
        dlg.orientation = 'column';
        dlg.alignChildren = ['fill', 'top'];
        dlg.spacing = 8;
        dlg.add('statictext', undefined,
            failedResults.length + ' of ' + allResults.length +
            ' properties did not converge.');
        var errTxt = dlg.add('edittext', undefined, '',
            { multiline: true, scrolling: true });
        errTxt.preferredSize = [440, 90];
        var lines = [];
        var i, pk;
        for (i = 0; i < failedResults.length; i++) {
            pk = failedResults[i];
            lines.push('FAIL  ' + _propertyLabelForId(pk.property_id, labelMap) +
                '  max_err=' + (typeof pk.max_err === 'number'
                    ? pk.max_err.toFixed(4) : '?') +
                (pk.notes ? '  (' + pk.notes + ')' : ''));
        }
        errTxt.text = lines.join('\n');
        dlg.add('statictext', undefined, 'What would you like to do?');
        var btnGrp = dlg.add('group');
        btnGrp.orientation = 'column';
        btnGrp.alignChildren = ['fill', 'top'];
        var convBtn   = btnGrp.add('button', undefined,
            'Apply converged only (' + (allResults.length - failedResults.length) + ')');
        var loosenBtn = btnGrp.add('button', undefined,
            'Retry with 2\xD7 tolerance (' + (settings.tolerance * 2).toFixed(3) + ')');
        var skipBtn   = btnGrp.add('button', undefined, 'Cancel -- apply nothing');
        var action = 'skipAll';
        convBtn.onClick   = function () { action = 'convergedOnly'; dlg.close(1); };
        loosenBtn.onClick = function () { action = 'loosen';        dlg.close(2); };
        skipBtn.onClick   = function () { action = 'skipAll';       dlg.close(0); };
        dlg.show();
        return action;
    }

    function _finishMergedBake(comp, bundle, scratchDir, requestId,
            mergedResults, phaseStart) {
        _cleanCancelFile();

        mergedResults = _mergeSegmentedPropertyResults(mergedResults);
        if (mergedResults.length === 0) {
            log('Nothing to apply.');
            setStatus('Idle', 0, phaseStart);
            return;
        }

        var mergedKeyBundle = {
            _schema:          'keys',
            schema_version:   1,
            request_id:       requestId,
            property_results: mergedResults,
            total_keys:       0
        };
        var mk;
        for (mk = 0; mk < mergedResults.length; mk++) {
            mergedKeyBundle.total_keys += mergedResults[mk].keys
                ? mergedResults[mk].keys.length : 0;
        }

        var flattenParentedPositionMap = _flattenParentedPositionMapFromBundle(bundle);
        var flattenCount = _countMapKeys(flattenParentedPositionMap);
        var propInfoById = _propertyInfoMap(bundle);
        var fallbackResolverLogged = {};
        if (flattenCount > 0) {
            log('INFO [parent-flatten]: unparenting ' + flattenCount +
                ' layer' + (flattenCount === 1 ? '' : 's') +
                ' before applying comp-space Position keys.');
        }
        var reportPath = scratchDir + '/' + requestId + '.verify.json';
        var cardPath   = scratchDir + '/last_verify_card.txt';
        var applyLastPct = -1;
        var applyLastLogPct = -1;
        var applyLastLabel = '';
        function applyProgress(info) {
            info = info || {};
            var total = Math.max(1, info.property_total || 1);
            var idx = info.property_index || 0;
            var keyTotal = Math.max(1, info.key_total || 1);
            var keyIdx = Math.max(0, info.key_index || 0);
            if (info.phase === 'property_done') {
                idx = Math.max(0, idx - 1);
                keyIdx = keyTotal;
            }
            var frac = Math.max(0, Math.min(1, keyIdx / keyTotal));
            var workFrac = Math.max(0, Math.min(1, (idx + frac) / total));
            var pct;
            if (info.phase === 'verify_structure') {
                pct = 88 + Math.round(4 * workFrac);
            } else if (info.phase === 'property_done') {
                pct = 92;
            } else {
                pct = 82 + Math.round(6 * workFrac);
            }
            var label = 'apply: ' + (info.phase || 'working') +
                ' ' + Math.min(total, idx + 1) + '/' + total;
            if (info.key_total > 0 && info.key_index >= 0) {
                label += ' key ' + info.key_index + '/' + info.key_total;
            }
            if (pct < applyLastPct) { pct = applyLastPct; }
            if (pct !== applyLastPct || label !== applyLastLabel) {
                _paintSolveElapsedDuringBusyWork();
                setProgress(pct);
                setStatusText(label + ' (' + pct + '%)');
                if (pct >= applyLastLogPct + 2 || label !== applyLastLabel) {
                    log('  ' + label + ' (' + pct + '%)');
                    applyLastLogPct = pct;
                }
                applyLastPct = pct;
                applyLastLabel = label;
            }
        }
        var writeOpts = {
            undoGroupName:       'bbsolver-test-harness: bake ' + requestId,
            disableExpression:   settings.disableExpression,
            archiveExpression:   false,
            archiveAsGuideLayer: false,
            overwriteExisting:   true,
            flattenParentedPositionMap: flattenParentedPositionMap,
            perPropertyResolver: function (propId) {
                var info = propInfoById[propId] || null;
                var prop = null;
                if (info) {
                    prop = _resolveSampledProperty(info, comp);
                    if (prop) {
                        var directProp = null;
                        try { directProp = parseSepId(propId, comp); } catch (directErr) {}
                        if (!directProp && !fallbackResolverLogged[propId]) {
                            log('WARN [writeback]: synthetic property id resolver failed for ' +
                                _propertyLabelForId(propId, _propertyLabelMap(bundle)) +
                                '; using sampled layer-id/path fallback.');
                            fallbackResolverLogged[propId] = true;
                        }
                        return prop;
                    }
                }
                if (prop) { return prop; }
                try { prop = parseSepId(propId, comp); } catch (parseErr) {}
                if (prop) { return prop; }
                prop = _resolvePropertyByLayerPath(info, comp);
                if (prop && !fallbackResolverLogged[propId]) {
                    log('WARN [writeback]: stable id resolver failed for ' +
                        _propertyLabelForId(propId, _propertyLabelMap(bundle)) +
                        '; using layer-path fallback.');
                    fallbackResolverLogged[propId] = true;
                }
                return prop;
            },
            progressCallback: applyProgress,
            holdAdjacentFrameKeys: true,
            frameDuration: comp.frameDuration,
            frameOrigin: (typeof comp.displayStartTime === 'number')
                ? comp.displayStartTime : 0,
            snapKeyTimesToFrameGrid: true,
            verifyShapeKeyStructure: false
        };
        var writeResult;
        phaseStart = setStatus('Applying keyframes to AE', 82, phaseStart);
        try { writeResult = applyKeyBundleMultiPath(mergedKeyBundle, writeOpts); }
        catch (ae) {
            log('ERROR (writeback): ' + ae.message);
            setStatus('Apply failed', 0, phaseStart);
            alert('Writeback failed:\n' + ae.message);
            return;
        }
        log('Applied: ' + writeResult.applied + ' prop(s).');
        if (writeResult.skipped.length > 0) {
            log('Skipped: ' + writeResult.skipped.join(', '));
        }
        var ei;
        for (ei = 0; ei < writeResult.errors.length; ei++) {
            log('  ERR ' + writeResult.errors[ei].id + ': ' +
                writeResult.errors[ei].msg);
        }
        if (writeResult.applied <= 0) {
            var firstWritebackErr = '';
            if (writeResult.errors && writeResult.errors.length > 0) {
                firstWritebackErr = '\nFirst error: ' +
                    writeResult.errors[0].id + ': ' +
                    writeResult.errors[0].msg;
            }
            log('ERROR (writeback): solver produced ' +
                mergedKeyBundle.total_keys +
                ' key(s), but AE applied 0 properties.');
            setStatus('Apply failed', 0, phaseStart);
            alert('Writeback failed: solver produced ' +
                mergedKeyBundle.total_keys +
                ' key(s), but AE applied 0 properties.' +
                firstWritebackErr +
                '\nSee the bbsolver-test-harness log for skipped property ids.');
            return;
        }
        if (writeResult.shape_key_structure &&
                writeResult.shape_key_structure.length > 0) {
            var sksLogFail = 0, logBez = 0, logLin = 0, logHold = 0, sksli;
            for (sksli = 0; sksli < writeResult.shape_key_structure.length; sksli++) {
                var sksle = writeResult.shape_key_structure[sksli];
                if (!sksle.ok) { sksLogFail = sksLogFail + 1; }
                if (sksle.interp_summary) {
                    logBez  = logBez  + (sksle.interp_summary.bezier_count || 0);
                    logLin  = logLin  + (sksle.interp_summary.linear_count || 0);
                    logHold = logHold + (sksle.interp_summary.hold_count   || 0);
                }
            }
            if (sksLogFail === 0) {
                log('  Shape key structure: PASS -- ' + logBez +
                    ' Bezier / ' + logLin + ' Linear' +
                    (logHold > 0 ? ' / ' + logHold + ' Hold' : ''));
            } else {
                log('  Shape key structure: FAIL (' + sksLogFail + '/' +
                    writeResult.shape_key_structure.length + ' props)');
            }
        }
        if (writeResult.multi_path_count && writeResult.multi_path_count > 0) {
            var diagCount = writeResult.diagnostic_subpath_count || 0;
            var visCount  = writeResult.visible_subpath_count    || 0;
            if (diagCount > 0) {
                log('  Landmark sub-paths created (diagnostic-only): ' +
                    diagCount + ' adjacent bb_lm_N path(s). Not applied to source path; ' +
                    'may be inert (MaskMode.NONE or shape-group skip).');
            }
            if (visCount > 0) {
                log('  Visible sub-paths created: ' + visCount +
                    ' adjacent bb_vc_N path(s). These render visibly and ' +
                    'are the actual user-testable improvement.');
            }
        }
        phaseStart = setStatus('Applied keyframes to AE', 92, phaseStart);

        var vResult = null;
        if (settings.verifyRoundTrip === false) {
            log('Round-trip verify skipped by Settings.');
            phaseStart = setStatus('Round-trip verify skipped', 98, phaseStart);
        } else {
            phaseStart = setStatus('Verifying round-trip', 90, phaseStart);
            var verifyLastPct = -1;
            var verifyLastLogPct = -1;
            var verifyLastLabel = '';
            function verifyProgress(info) {
                info = info || {};
                var total = Math.max(1, info.property_total || 1);
                var idx = info.property_index || 0;
                var sampleTotal = Math.max(1, info.sample_total || 1);
                var sampleIdx = Math.max(0, info.sample_index || 0);
                if (info.phase === 'property_done') {
                    idx = Math.max(0, idx - 1);
                    sampleIdx = sampleTotal;
                }
                var frac = Math.max(0, Math.min(1, sampleIdx / sampleTotal));
                var pct = 90 + Math.round(8 * Math.max(0, Math.min(1, (idx + frac) / total)));
                var label = 'verify: ' + (info.phase || 'checking') +
                    ' ' + Math.min(total, idx + 1) + '/' + total;
                if (info.sample_total > 0 && info.sample_index >= 0) {
                    label += ' sample ' + info.sample_index + '/' + info.sample_total;
                }
                if (pct < verifyLastPct) { pct = verifyLastPct; }
                if (pct !== verifyLastPct || label !== verifyLastLabel) {
                    setProgress(pct);
                    setStatusText(label + ' (' + pct + '%)');
                    if (pct >= verifyLastLogPct + 2 || label !== verifyLastLabel) {
                        log('  ' + label + ' (' + pct + '%)');
                        verifyLastLogPct = pct;
                    }
                    verifyLastPct = pct;
                    verifyLastLabel = label;
                }
            }
            // verify.jsx tightens propTolerance via
            //   propTolerance = Math.min(settings.tolerance, flattenParentedTolerance, rigRotationTolerance)
            // for parented-position, parented-rotation, and rotation-property
            // rows. The historical defaults (0.05 px, 0.01°) were UI-quality
            // gates that did NOT match the ε the solver was actually invoked
            // with, so the OK/FAIL flag on verify cards diverged from the
            // paper-ε criterion. Default both knobs to settings.tolerance so
            // the per-kind Math.min is a no-op unless the user explicitly
            // sets a tighter verify budget. Set
            //   settings.coupleVerifyToSamplerPrecision = true
            // to restore the old behavior. (Mirrors the solve-side
            // coupleSolveToSamplerPrecision opt-in in _effectiveGroupTolerances.)
            var verifyFlattenTol;
            var verifyRigRotTol;
            if (settings.coupleVerifyToSamplerPrecision === true) {
                verifyFlattenTol = (typeof settings.flattenParentedTolerance === 'number')
                    ? settings.flattenParentedTolerance : 0.05;
                verifyRigRotTol = (typeof settings.rigRotationTolerance === 'number')
                    ? settings.rigRotationTolerance : 0.01;
            } else {
                verifyFlattenTol = (typeof settings.flattenParentedTolerance === 'number')
                    ? settings.flattenParentedTolerance : settings.tolerance;
                verifyRigRotTol = (typeof settings.rigRotationTolerance === 'number')
                    ? settings.rigRotationTolerance : settings.tolerance;
            }
            try {
                vResult = verifyRoundTrip(bundle,
                    function (propId) { return parseSepId(propId, comp); },
                    settings.tolerance, settings.toleranceScreenPx,
                    {
                        verifyRigGaps: false,
                        rigGapTolerance: settings.rigGapTolerance,
                        flattenParentedTolerance: verifyFlattenTol,
                        rigRotationTolerance: verifyRigRotTol,
                        useRigRotationTolerance: (settings.coupleVerifyToSamplerPrecision === true),
                        progressCallback: verifyProgress
                    });
            } catch (ve) { log('WARN (verify): ' + ve.message); }

            if (vResult) {
                phaseStart = setStatus('Writing verify report', 99, phaseStart);
                var wroteJson = writeVerifyReport(vResult, reportPath);
                var wroteTxt  = writeVerifyCard(vResult, cardPath);
                if (wroteJson) { log('Verify report: ' + reportPath); }
                if (wroteTxt)  { log('Verify card  : ' + cardPath); }
            }
            phaseStart = setStatus('Checking shape endpoint diagnostics', 99, phaseStart);
            _logShapeEndpointDeviation(bundle, mergedKeyBundle, comp);
        }
        setStatus('Done', 100, phaseStart);
        _showResultsDialog(writeResult, vResult, mergedKeyBundle.total_keys,
            _propertyLabelMap(bundle), bundle, mergedKeyBundle);
    }

    // ---- Core bake flow --------------------------------------------------

    var _cooperativeBakeActive = false;

    function _runBake(comp, props) {
        if (_isMacOSPlatform()) {
            _runBakeCooperative(comp, props);
            return;
        }
        _runBakeBlocking(comp, props);
    }

    function _effectiveGroupTolerances(pinfo) {
        pinfo = pinfo || {};
        var tolVal = _effectiveTol(pinfo.id);
        var tolPxVal = _effectiveTolPx(pinfo.id);
        if (typeof pinfo.cleanup_tolerance === 'number' &&
                pinfo.cleanup_tolerance > 0) {
            tolVal = pinfo.cleanup_tolerance;
        }
        if (typeof pinfo.cleanup_tolerance_px === 'number' &&
                pinfo.cleanup_tolerance_px > 0) {
            tolPxVal = pinfo.cleanup_tolerance_px;
        }
        // Solver tolerance respects the UI Linf field directly. The
        // flattenParentedTolerance and rigRotationTolerance settings describe
        // SAMPLER-side unparenting precision (used by sampler.jsx in
        // sourcePointToComp + rotation flattening) and rig-drift VERIFY budgets
        // (used by verify.jsx after writeback). They must remain decoupled
        // from the solver's per-frame error budget. Earlier versions clamped
        // tolVal via Math.min(tolVal, flattenParentedTolerance) and
        // Math.min(tolVal, rigRotationTolerance), which silently prevented
        // loose-tolerance solves on parented rigs and on rotation properties.
        // For opt-in legacy behavior, callers can set
        // settings.coupleSolveToSamplerPrecision = true to restore the clamps.
        if (settings.coupleSolveToSamplerPrecision === true) {
            var flattenTol = (typeof settings.flattenParentedTolerance === 'number' &&
                              settings.flattenParentedTolerance > 0)
                ? settings.flattenParentedTolerance : 0.05;
            var rigRotTol = (typeof settings.rigRotationTolerance === 'number' &&
                             settings.rigRotationTolerance > 0)
                ? settings.rigRotationTolerance : 0.01;
            if (pinfo.flatten_parented_position === true) {
                tolVal = Math.min(tolVal, flattenTol);
            }
            if (pinfo.flatten_parented_rotation === true ||
                    pinfo.parent_flatten_strict_rotation === true) {
                tolVal = Math.min(tolVal, flattenTol);
            }
            if (_isRotationPropertyInfo(pinfo) ||
                    pinfo.flatten_parented_rotation === true ||
                    pinfo.parent_flatten_strict_rotation === true) {
                tolVal = Math.min(tolVal, rigRotTol);
                tolPxVal = Math.min(tolPxVal, rigRotTol);
            }
        }
        return { tol: tolVal, tolPx: tolPxVal };
    }

    function _buildToleranceGroups(bundle) {
        var tolGroups = {};
        var i, ps, pid, tolKey, pinfo, eff, solveMode, cleanupMode, smoothTol;
        var smoothBezier, smoothBezierLabel, smoothUseEase, smoothBezierOverride;
        var smoothSourceFidelity;
        for (i = 0; i < bundle.properties.length; i++) {
            ps     = bundle.properties[i];
            pinfo  = ps.property || {};
            pid    = pinfo.id;
            eff = _effectiveGroupTolerances(pinfo);
            solveMode = _effectiveSolveMode(pid);
            cleanupMode = _effectiveCleanupMode(pid);
            var motionSmoothMode = solveMode === 'motion_smooth';
            var motionPathMode = solveMode === 'motion_path_smooth';
            var smoothingMode = motionSmoothMode || motionPathMode;
            smoothTol = solveMode === 'motion_smooth'
                ? _effectiveMotionSmoothTolerance(pid)
                : _effectiveMotionSmoothTolerance('');
            smoothBezier = smoothingMode
                ? _effectiveMotionSmoothBezier(pid)
                : _effectiveMotionSmoothBezier('');
            smoothBezierLabel = _motionSmoothBezierLabel(smoothBezier);
            smoothBezierOverride =
                _propOverrideObject(_propMotionSmoothBezierMap, pid) !== null;
            smoothUseEase = smoothingMode &&
                (settings.motionSmoothUseEase === true || smoothBezierOverride);
            smoothSourceFidelity = motionSmoothMode &&
                settings.motionSmoothSourceFidelity === true;
            tolKey = String(eff.tol) + '|' + String(eff.tolPx) +
                '|' + solveMode + '|' + cleanupMode +
                '|smooth=' + String(smoothTol) +
                '|ease=' + (smoothUseEase ? '1' : '0') +
                '|sourceFidelity=' +
                    (smoothSourceFidelity ? '1' : '0') +
                '|bezier=' + smoothBezierLabel;
            if (!tolGroups[tolKey]) {
                tolGroups[tolKey] = {
                    tolStr: String(eff.tol),
                    tolPxStr: String(eff.tolPx),
                    solveMode: solveMode,
                    cleanupMode: cleanupMode,
                    motionSmoothTolStr: String(smoothTol),
                    motionSmoothUseEase: smoothUseEase,
                    motionSmoothSourceFidelity: smoothSourceFidelity,
                    motionSmoothBezier: smoothBezier,
                    groupIds: []
                };
            }
            tolGroups[tolKey].groupIds.push(pid);
        }
        return tolGroups;
    }

    function _cloneConfigForTolerance(config, tol, tolPx) {
        var cloned = {};
        var key;
        config = config || {};
        for (key in config) {
            if (Object.prototype.hasOwnProperty.call(config, key)) {
                cloned[key] = config[key];
            }
        }
        cloned.tolerance = tol;
        cloned.tolerance_screen_px = tolPx;
        return cloned;
    }

    function _filterBundleForIds(
            bundle, groupIds, tol, tolPx, solveMode, motionSmoothTolerance,
            motionSmoothBezier, motionSmoothUseEase,
            motionSmoothSourceFidelity) {
        var filteredBundle = {
            _schema:        bundle._schema,
            schema_version: bundle.schema_version,
            request_id:     bundle.request_id,
            comp:           bundle.comp,
            config:         _cloneConfigForTolerance(bundle.config, tol, tolPx),
            properties:     []
        };
        filteredBundle.config = _applySolveOptimizationModeToConfig(
            filteredBundle.config,
            solveMode || filteredBundle.config.solve_optimization_mode,
            motionSmoothTolerance,
            motionSmoothBezier,
            motionSmoothUseEase,
            motionSmoothSourceFidelity);
        var j, fi, k, inGroup;
        for (j = 0; j < bundle.properties.length; j++) {
            fi = bundle.properties[j];
            inGroup = false;
            for (k = 0; k < groupIds.length; k++) {
                if (groupIds[k] === fi.property.id) { inGroup = true; break; }
            }
            if (inGroup) { filteredBundle.properties.push(fi); }
        }
        return filteredBundle;
    }

    function _mergeSampleBundlesForSegments(bundles, requestId) {
        if (!bundles || bundles.length === 0) { return null; }
        var merged = {
            _schema:        bundles[0]._schema,
            schema_version: bundles[0].schema_version,
            request_id:     requestId || bundles[0].request_id,
            comp:           bundles[0].comp,
            config:         bundles[0].config,
            properties:     []
        };
        var byId = {};
        var order = [];
        var bi, bundle, pi, ps, id, target, si;
        for (bi = 0; bi < bundles.length; bi++) {
            bundle = bundles[bi];
            if (!bundle || !bundle.properties) { continue; }
            for (pi = 0; pi < bundle.properties.length; pi++) {
                ps = bundle.properties[pi];
                id = ps.property && ps.property.id ? ps.property.id : ('prop_' + pi);
                if (!byId[id]) {
                    target = {
                        property:             ps.property,
                        t_start_sec:          ps.t_start_sec,
                        t_end_sec:            ps.t_end_sec,
                        samples_per_frame:    ps.samples_per_frame,
                        samples:              [],
                        layer_xform_at_start: ps.layer_xform_at_start,
                        hash_of_expression:   ps.hash_of_expression
                    };
                    byId[id] = target;
                    order.push(id);
                } else {
                    target = byId[id];
                    if (ps.t_start_sec < target.t_start_sec) {
                        target.t_start_sec = ps.t_start_sec;
                    }
                    if (ps.t_end_sec > target.t_end_sec) {
                        target.t_end_sec = ps.t_end_sec;
                    }
                }
                for (si = 0; si < ps.samples.length; si++) {
                    target.samples.push(ps.samples[si]);
                }
            }
        }
        for (pi = 0; pi < order.length; pi++) {
            target = byId[order[pi]];
            target.samples.sort(function (a, b) {
                return a.t_sec < b.t_sec ? -1 : (a.t_sec > b.t_sec ? 1 : 0);
            });
            var deduped = [];
            for (si = 0; si < target.samples.length; si++) {
                if (deduped.length > 0 &&
                        Math.abs(deduped[deduped.length - 1].t_sec -
                                 target.samples[si].t_sec) < 0.000001) {
                    deduped[deduped.length - 1] = target.samples[si];
                } else {
                    deduped.push(target.samples[si]);
                }
            }
            target.samples = deduped;
            merged.properties.push(target);
        }
        return merged;
    }

    function _attachWriteRangesToPropertyResults(results, range) {
        if (!results || !range) { return; }
        var writeRange = _normaliseWriteRanges([{
            start: range.tStart,
            end:   range.tEnd
        }]);
        if (!writeRange || writeRange.length === 0) { return; }
        var i, pk;
        for (i = 0; i < results.length; i++) {
            pk = results[i];
            if (!pk || _isSubPathEntry(pk)) { continue; }
            pk.write_ranges = [{
                start: writeRange[0].start,
                end:   writeRange[0].end
            }];
        }
    }

    function _mergeSegmentedPropertyResults(results) {
        if (!results || results.length === 0) { return []; }
        var seenIds = {};
        var hasSegmentDuplicates = false;
        var si, sr, sid;
        for (si = 0; si < results.length; si++) {
            sr = results[si];
            if (_isSubPathEntry(sr)) { continue; }
            sid = sr.property_id || ('prop_' + si);
            if (seenIds[sid] === true) {
                hasSegmentDuplicates = true;
                break;
            }
            seenIds[sid] = true;
        }
        if (!hasSegmentDuplicates) { return results; }

        var byId = {};
        var order = [];
        var out = [];
        var i, r, id, target, k, notes, resultRange;
        for (i = 0; i < results.length; i++) {
            r = results[i];
            if (_isSubPathEntry(r)) {
                out.push(r);
                continue;
            }
            id = r.property_id || ('prop_' + i);
            if (!byId[id]) {
                target = {};
                for (k in r) {
                    if (Object.prototype.hasOwnProperty.call(r, k) && k !== 'keys') {
                        target[k] = r[k];
                    }
                }
                target.keys = [];
                target.write_ranges = [];
                target._bb_segment_count = 0;
                target._bb_notes = [];
                byId[id] = target;
                order.push(id);
            } else {
                target = byId[id];
                if (r.converged === false) { target.converged = false; }
                if (typeof r.max_err === 'number' &&
                        (typeof target.max_err !== 'number' || r.max_err > target.max_err)) {
                    target.max_err = r.max_err;
                }
                if (typeof r.max_err_screen_px === 'number' &&
                        (typeof target.max_err_screen_px !== 'number' ||
                         r.max_err_screen_px > target.max_err_screen_px)) {
                    target.max_err_screen_px = r.max_err_screen_px;
                }
            }
            target._bb_segment_count++;
            notes = _trim(r.notes || '');
            if (notes) { target._bb_notes.push(notes); }
            var sourceWriteRanges = _normaliseWriteRanges(
                r.write_ranges || r._bb_write_ranges);
            if (sourceWriteRanges.length > 0) {
                for (k = 0; k < sourceWriteRanges.length; k++) {
                    target.write_ranges.push({
                        start: sourceWriteRanges[k].start,
                        end:   sourceWriteRanges[k].end
                    });
                }
            } else {
                resultRange = _keyTimesRange(r.keys);
                if (resultRange) {
                    target.write_ranges.push({
                        start: resultRange.start,
                        end:   resultRange.end
                    });
                }
            }
            if (r.keys) {
                for (k = 0; k < r.keys.length; k++) {
                    target.keys.push(r.keys[k]);
                }
            }
        }
        for (i = 0; i < order.length; i++) {
            target = byId[order[i]];
            target.keys.sort(function (a, b) {
                return a.t_sec < b.t_sec ? -1 : (a.t_sec > b.t_sec ? 1 : 0);
            });
            var dedupedKeys = [];
            for (k = 0; k < target.keys.length; k++) {
                if (dedupedKeys.length > 0 &&
                        Math.abs(dedupedKeys[dedupedKeys.length - 1].t_sec -
                                 target.keys[k].t_sec) < 0.000001) {
                    dedupedKeys[dedupedKeys.length - 1] = target.keys[k];
                } else {
                    dedupedKeys.push(target.keys[k]);
                }
            }
            target.keys = dedupedKeys;
            target.notes = 'segment_mode_merged; segments=' +
                target._bb_segment_count +
                (target._bb_notes.length > 0
                    ? '; segment_notes=' + target._bb_notes.join(' | ')
                    : '');
            target.write_ranges = _normaliseWriteRanges(target.write_ranges);
            delete target._bb_segment_count;
            delete target._bb_notes;
            out.push(target);
        }
        return out;
    }

    function _copyFlatArray(arr) {
        var out = [];
        if (!arr) { return out; }
        var i;
        for (i = 0; i < arr.length; i++) { out.push(arr[i]); }
        return out;
    }

    function _copyPlainObject(obj) {
        var out = {};
        var k;
        obj = obj || {};
        for (k in obj) {
            if (Object.prototype.hasOwnProperty.call(obj, k)) {
                out[k] = obj[k];
            }
        }
        return out;
    }

    function _sourceSampleMap(bundle) {
        var map = {};
        if (!bundle || !bundle.properties) { return map; }
        var i, ps, id;
        for (i = 0; i < bundle.properties.length; i++) {
            ps = bundle.properties[i];
            id = ps && ps.property ? ps.property.id : '';
            if (id) { map[id] = ps; }
        }
        return map;
    }

    function _sourceFrameCountForProperty(ps, fps) {
        if (ps && ps.samples && ps.samples.length > 0) {
            return ps.samples.length;
        }
        if (ps && typeof ps.t_start_sec === 'number' &&
                typeof ps.t_end_sec === 'number' && fps > 0) {
            return Math.max(0, Math.round((ps.t_end_sec - ps.t_start_sec) * fps) + 1);
        }
        return 0;
    }

    function _sourceOriginalKeyCount(pinfo) {
        pinfo = pinfo || {};
        var fields = [
            'source_num_keys',
            'original_num_keys',
            'num_keys',
            'key_count'
        ];
        var i, v;
        for (i = 0; i < fields.length; i++) {
            v = pinfo[fields[i]];
            if (typeof v === 'number' && isFinite(v)) {
                return Math.max(0, Math.round(v));
            }
        }
        return null;
    }

    function _cleanupSourceSummaryMap(bundle) {
        var map = {};
        if (!bundle || !bundle.properties) { return map; }
        var fps = bundle.comp && typeof bundle.comp.fps === 'number'
            ? bundle.comp.fps : 0;
        var i, ps, pinfo, id, entry, vertices, frames, sourceKeys;
        for (i = 0; i < bundle.properties.length; i++) {
            ps = bundle.properties[i];
            pinfo = ps && ps.property ? ps.property : {};
            id = pinfo.id || '';
            if (!id) { continue; }
            if (!map[id]) {
                map[id] = {
                    frames: 0,
                    original_vertices: 0,
                    original_keys: null,
                    is_shape_path: false
                };
            }
            entry = map[id];
            frames = _sourceFrameCountForProperty(ps, fps);
            entry.frames += frames;
            if (pinfo.units_label === 'shape_flat') {
                entry.is_shape_path = true;
                vertices = pinfo.shape_max_vertex_count ||
                    pinfo.shape_canonical_vertex_count || 0;
                if (vertices > entry.original_vertices) {
                    entry.original_vertices = vertices;
                }
            }
            sourceKeys = _sourceOriginalKeyCount(pinfo);
            if (sourceKeys !== null) {
                entry.original_keys = entry.original_keys === null
                    ? sourceKeys : Math.max(entry.original_keys, sourceKeys);
            }
        }
        return map;
    }

    function _shapeFlatVertexCountFromValue(v) {
        if (!v || v.length < 2) { return 0; }
        var n = Math.round(v[1]);
        if (!isFinite(n) || n < 0) { return 0; }
        return n;
    }

    function _resultMaxVertexCount(pk) {
        if (!pk || !pk.keys || pk.keys.length === 0) { return 0; }
        var maxV = 0;
        var i, n;
        for (i = 0; i < pk.keys.length; i++) {
            n = _shapeFlatVertexCountFromValue(pk.keys[i].v);
            if (n > maxV) { maxV = n; }
        }
        return maxV;
    }

    function _shapeFlatValuesNearEqual(a, b, tol) {
        if (!a || !b || a.length !== b.length || a.length < 2) {
            return false;
        }
        if (Math.round(a[0]) !== Math.round(b[0]) ||
                Math.round(a[1]) !== Math.round(b[1])) {
            return false;
        }
        tol = (typeof tol === 'number' && isFinite(tol)) ? Math.max(0, tol) : 0;
        var i, av, bv;
        for (i = 2; i < a.length; i++) {
            av = Number(a[i]);
            bv = Number(b[i]);
            if (!isFinite(av) || !isFinite(bv) ||
                    Math.abs(av - bv) > tol) {
                return false;
            }
        }
        return true;
    }

    function _copyKeyObject(key) {
        var out = {};
        var k;
        key = key || {};
        for (k in key) {
            if (Object.prototype.hasOwnProperty.call(key, k)) {
                if (k === 'v' || k === 'spatial_in' || k === 'spatial_out') {
                    out[k] = _copyFlatArray(key[k] || []);
                } else if (k === 'temporal_ease_in' ||
                        k === 'temporal_ease_out') {
                    out[k] = _copyEaseArray(key[k] || []);
                } else {
                    out[k] = key[k];
                }
            }
        }
        return out;
    }

    function _copyPropertyResult(pk) {
        var out = {};
        var k, i;
        pk = pk || {};
        for (k in pk) {
            if (Object.prototype.hasOwnProperty.call(pk, k) && k !== 'keys') {
                out[k] = pk[k];
            }
        }
        out.keys = [];
        if (pk.keys) {
            for (i = 0; i < pk.keys.length; i++) {
                out.keys.push(_copyKeyObject(pk.keys[i]));
            }
        }
        return out;
    }

    function _staticRunPrunedKeys(keys, tol) {
        if (!keys || keys.length < 3) { return null; }
        var out = [];
        var removed = 0;
        var i = 0;
        var j;
        while (i < keys.length) {
            out.push(_copyKeyObject(keys[i]));
            j = i + 1;
            while (j < keys.length &&
                    _shapeFlatValuesNearEqual(keys[i].v, keys[j].v, tol)) {
                j++;
            }
            if (j - i > 2) {
                out.push(_copyKeyObject(keys[j - 1]));
                removed += (j - i - 2);
            } else if (j - i === 2) {
                out.push(_copyKeyObject(keys[i + 1]));
            }
            i = j;
        }
        if (removed <= 0) { return null; }
        return { keys: out, removed: removed };
    }

    function _pruneStaticCleanupKeys(firstResults, baselineResults) {
        var out = [];
        var acceptedCount = 0;
        var i, pk, pruned, copy, baselineMap, baselinePk, baseKeys, baseVertices;
        baselineMap = {};
        baselineResults = baselineResults || firstResults;
        for (i = 0; i < baselineResults.length; i++) {
            if (baselineResults[i] && !_isSubPathEntry(baselineResults[i])) {
                baselineMap[baselineResults[i].property_id] = baselineResults[i];
            }
        }
        for (i = 0; i < firstResults.length; i++) {
            pk = firstResults[i];
            if (!pk || _isSubPathEntry(pk) || !pk.keys || pk.keys.length < 3) {
                out.push(pk);
                continue;
            }
            pruned = _staticRunPrunedKeys(pk.keys, 1e-7);
            if (!pruned) {
                out.push(pk);
                continue;
            }
            copy = _copyPropertyResult(pk);
            copy.keys = pruned.keys;
            baselinePk = baselineMap[pk.property_id] || pk;
            baseKeys = baselinePk && baselinePk.keys ? baselinePk.keys.length :
                pk.keys.length;
            baseVertices = _resultMaxVertexCount(baselinePk || pk);
            copy.notes = _stripCleanupAcceptedSummary(copy.notes || '');
            copy.notes = (copy.notes ? copy.notes + '; ' : '') +
                'auto_cleanup_pass_accepted; cleanup_phase=static_prune' +
                '; first_pass_keys=' + baseKeys +
                '; cleanup_keys=' + copy.keys.length +
                '; first_pass_vertices=' + baseVertices +
                '; cleanup_vertices=' + _resultMaxVertexCount(copy) +
                '; static_cleanup_removed_keys=' + pruned.removed;
            out.push(copy);
            acceptedCount++;
        }
        if (acceptedCount > 0) {
            log('INFO [cleanup]: static key cleanup removed redundant same-value keys for ' +
                acceptedCount + ' propert' + (acceptedCount === 1 ? 'y' : 'ies') + '.');
        }
        return { results: out, acceptedCount: acceptedCount };
    }

    function _cleanupPassSummaryLines(sourceBundle, keyBundle, labelMap) {
        var lines = [];
        if (!keyBundle || !keyBundle.property_results) { return lines; }
        var sourceMap = _cleanupSourceSummaryMap(sourceBundle);
        var i, pk, notes, source, firstKeys, cleanupKeys;
        var firstVertices, cleanupVertices, label, originalParts;
        var cleanupAccepted, pass2Text, line, advisoryMsg, smoothMsg;
        for (i = 0; i < keyBundle.property_results.length; i++) {
            pk = keyBundle.property_results[i];
            if (!pk || _isSubPathEntry(pk)) { continue; }
            notes = pk.notes || '';
            cleanupAccepted = notes.indexOf('auto_cleanup_pass_accepted') >= 0;
            source = sourceMap[pk.property_id] || {};
            if (cleanupAccepted) {
                firstKeys = _noteFieldValue(notes, 'first_pass_keys',
                    'auto_cleanup_pass_accepted') || '?';
                cleanupKeys = _noteFieldValue(notes, 'cleanup_keys',
                    'auto_cleanup_pass_accepted') ||
                    (pk.keys ? String(pk.keys.length) : '?');
                if (source.is_shape_path) {
                    firstVertices = _noteFieldValue(notes, 'first_pass_vertices',
                        'auto_cleanup_pass_accepted') || '?';
                    cleanupVertices = _noteFieldValue(notes, 'cleanup_vertices',
                        'auto_cleanup_pass_accepted') ||
                        String(_resultMaxVertexCount(pk) || '?');
                    pass2Text = cleanupVertices + ' vtx / ' + cleanupKeys + ' keys';
                } else {
                    firstVertices = '';
                    pass2Text = cleanupKeys + ' keys';
                }
            } else {
                firstKeys = pk.keys ? String(pk.keys.length) : '?';
                firstVertices = source.is_shape_path
                    ? String(_resultMaxVertexCount(pk) || '?') : '';
                pass2Text = source.is_shape_path
                    ? 'not accepted/run'
                    : 'not applicable';
            }
            label = _propertyLabelForId(pk.property_id, labelMap);
            originalParts = [];
            if (source.original_vertices > 0) {
                originalParts.push('original ' + source.original_vertices + ' vtx');
            }
            if (source.frames > 0) {
                originalParts.push('workarea ' + source.frames + ' frames');
            }
            if (source.original_keys !== null) {
                originalParts.push('original ' + source.original_keys + ' keys');
            }
            if (originalParts.length === 0) {
                originalParts.push('original n/a');
            }
            line = label + ': ' + originalParts.join(', ') +
                '; pass 1 ' +
                (source.is_shape_path
                    ? (firstVertices + ' vtx / ' + firstKeys + ' keys')
                    : (firstKeys + ' keys')) +
                '; pass 2 ' + pass2Text;
            advisoryMsg = _pathOptimizationAdvisoryMessage(notes);
            if (advisoryMsg) { line += '; ' + advisoryMsg; }
            smoothMsg = _motionSmoothResultMessage(notes);
            if (smoothMsg) { line += '; ' + smoothMsg; }
            lines.push(line);
        }
        return lines;
    }

    function _copyEaseArray(arr) {
        var out = [];
        if (!arr || !arr.length) { return out; }
        var i, src, speed, influence;
        for (i = 0; i < arr.length; i++) {
            src = arr[i] || {};
            speed = (typeof src.speed === 'number') ? src.speed : 0;
            influence = (typeof src.influence === 'number') ? src.influence : 33.3;
            out.push({ speed: speed, influence: influence });
        }
        return out;
    }

    function _cleanupKeySampleFromResultKey(k) {
        var sample = {
            t_sec: k.t_sec,
            v: _copyFlatArray(k.v)
        };
        if (typeof k.interp_in === 'string') { sample.interp_in = k.interp_in; }
        if (typeof k.interp_out === 'string') { sample.interp_out = k.interp_out; }
        sample.temporal_ease_in = _copyEaseArray(k.temporal_ease_in);
        sample.temporal_ease_out = _copyEaseArray(k.temporal_ease_out);
        sample.spatial_in = _copyFlatArray(k.spatial_in || []);
        sample.spatial_out = _copyFlatArray(k.spatial_out || []);
        sample.temporal_continuous = !!k.temporal_continuous;
        sample.spatial_continuous = !!k.spatial_continuous;
        sample.temporal_auto_bezier = !!k.temporal_auto_bezier;
        sample.spatial_auto_bezier = !!k.spatial_auto_bezier;
        sample.roving = !!k.roving;
        return sample;
    }

    function _cleanupPassConfig(baseConfig, phase) {
        var config = _copyPlainObject(baseConfig);
        config.allow_shape_temporal_bezier = true;
        config.allow_path_spatial_fit = true;
        config.allow_path_replacement_fit = false;
        config.path_replacement_prefer_vertices = (phase !== 'temporal');
        config.path_preserve_sharp_corners =
            (settings.preserveSharpPathCorners !== false);
        config.path_specific_max_gap = config.path_specific_max_gap || 60;
        if (typeof config.shape_temporal_bezier_attempt_threshold_ratio !== 'number' ||
                config.shape_temporal_bezier_attempt_threshold_ratio < 0) {
            config.shape_temporal_bezier_attempt_threshold_ratio = 1.5;
        }
        return _applySolveOptimizationModeToConfig(
            config, _cleanupSolveOptimizationMode(phase));
    }

    function _isMotionSmoothPathResult(pk) {
        var notes = pk && pk.notes ? String(pk.notes) : '';
        return notes.indexOf('solve_mode_motion_smooth') >= 0 ||
            notes.indexOf('solve_mode_motion_path_smooth') >= 0 ||
            notes.indexOf('motion_smooth_shape_trajectory_filter=true') >= 0 ||
            notes.indexOf('motion_smooth_shape_rove_time=true') >= 0 ||
            notes.indexOf('motion_path_spatial_trajectory_filter=true') >= 0;
    }

    function _buildCleanupPassPlan(bundle, mergedResults, requestId, phase) {
        var sourceMap = _sourceSampleMap(bundle);
        phase = phase === 'temporal' ? 'temporal' : 'vertex';
        var cleanupBundle = {
            _schema:        'samples',
            schema_version: 1,
            request_id:     requestId + '_cleanup_' + phase,
            comp:           bundle ? bundle.comp : null,
            config:         _cleanupPassConfig(bundle ? bundle.config : {}, phase),
            properties:     []
        };
        var candidates = [];
        var i, pk, ps, pinfo, samples, k, sourceVertices, keyVertices, label;
        var motionSmoothCleanup, cleanupTol;
        if (!mergedResults) { mergedResults = []; }
        for (i = 0; i < mergedResults.length; i++) {
            pk = mergedResults[i];
            if (!pk || _isSubPathEntry(pk) || pk.converged === false ||
                    !pk.keys || pk.keys.length < 3) {
                continue;
            }
            if (_effectiveCleanupMode(pk.property_id) === 'off') {
                continue;
            }
            ps = sourceMap[pk.property_id];
            pinfo = ps && ps.property ? ps.property : null;
            if (!pinfo || pinfo.units_label !== 'shape_flat') { continue; }
            keyVertices = _resultMaxVertexCount(pk);
            if (keyVertices <= 4) { continue; }
            sourceVertices = pinfo.shape_max_vertex_count ||
                pinfo.shape_canonical_vertex_count || keyVertices;

            samples = [];
            for (k = 0; k < pk.keys.length; k++) {
                samples.push(_cleanupKeySampleFromResultKey(pk.keys[k]));
            }
            pinfo = _copyPlainObject(pinfo);
            pinfo.dimensions = samples[0].v.length;
            pinfo.shape_max_vertex_count = keyVertices;
            pinfo.shape_canonical_vertex_count = keyVertices;
            pinfo.source_key_times = [];
            for (k = 0; k < samples.length; k++) {
                pinfo.source_key_times.push(samples[k].t_sec);
            }
            motionSmoothCleanup = phase === 'temporal' &&
                _isMotionSmoothPathResult(pk);
            cleanupTol = 0;
            if (motionSmoothCleanup) {
                cleanupTol = _motionSmoothTemporalCleanupTolerance();
                pinfo.cleanup_tolerance = cleanupTol;
                pinfo.cleanup_tolerance_px = cleanupTol;
                pinfo.motion_smooth_temporal_cleanup = true;
            }
            label = _friendlyPropertyLabel(pinfo);
            cleanupBundle.properties.push({
                property:             pinfo,
                t_start_sec:          samples[0].t_sec,
                t_end_sec:            samples[samples.length - 1].t_sec,
                samples_per_frame:    1,
                samples:              samples,
                layer_xform_at_start: ps.layer_xform_at_start,
                hash_of_expression:   'cleanup_from_first_pass:' +
                    (ps.hash_of_expression || '')
            });
            candidates.push({
                property_id: pk.property_id,
                label:       label,
                keys:        pk.keys.length,
                vertices:    keyVertices,
                source_vertices: sourceVertices,
                motion_smooth_cleanup: motionSmoothCleanup,
                cleanup_tolerance: cleanupTol,
                cleanup_phase: phase,
                cleanup_mode: _effectiveCleanupMode(pk.property_id)
            });
        }
        return {
            bundle: cleanupBundle,
            candidates: candidates
        };
    }

    function _cleanupPlanNeedsPrompt(plan) {
        if (!plan || !plan.candidates) { return false; }
        var i;
        for (i = 0; i < plan.candidates.length; i++) {
            if (_normalizeCleanupMode(plan.candidates[i].cleanup_mode) ===
                    'prompt') {
                return true;
            }
        }
        return false;
    }

    function _cleanupPromptText(plan) {
        var count = plan && plan.candidates ? plan.candidates.length : 0;
        var keyCount = 0;
        var maxVertices = 0;
        var motionSmoothCount = 0;
        var motionSmoothCleanupTol = 0;
        var i, c;
        for (i = 0; i < count; i++) {
            c = plan.candidates[i];
            keyCount += c.keys || 0;
            if ((c.vertices || 0) > maxVertices) { maxVertices = c.vertices; }
            if (c.motion_smooth_cleanup === true) {
                motionSmoothCount++;
                if ((c.cleanup_tolerance || 0) > motionSmoothCleanupTol) {
                    motionSmoothCleanupTol = c.cleanup_tolerance;
                }
            }
        }
        var text = 'bbsolver can run cleanup passes before writing keys.\n\n' +
            'Eligible path properties: ' + count + '\n' +
            'First-pass path keys: ' + keyCount + '\n' +
            'Max first-pass vertices: ' + maxVertices + '\n';
        if (motionSmoothCount > 0) {
            text += 'Motion Smooth path candidates: ' + motionSmoothCount + '\n' +
                'Post-smooth temporal cleanup tolerance: ' +
                motionSmoothCleanupTol + '\n';
        }
        text += '\nThis re-solves the first-pass path keys without resampling AE. ' +
            'The temporal cleanup phase tries to remove redundant keys; the ' +
            'vertex cleanup phase then tries guarded vertex pruning. Each ' +
            'accepted result must verify against the previous accepted keys ' +
            'and improve key count or vertex count. ' +
            'Original-source error is logged as a diagnostic because this ' +
            'matches manually baking the already-baked result. ';
        if (motionSmoothCount > 0) {
            text += 'For Motion Smooth paths, temporal cleanup uses the ' +
                'post-smooth tolerance above so it can reduce the smoothed ' +
                'key schedule the same way a manual temporal-only pass would. ';
        }
        return text + '\n\n' +
            'Run cleanup passes now?';
    }

    function _cleanupVerifyJsonFromOutput(text) {
        text = text || '';
        var start = text.indexOf('{');
        var end = text.lastIndexOf('}');
        if (start < 0 || end <= start) { return null; }
        try { return JSON.parse(text.substring(start, end + 1)); } catch (e) {}
        return null;
    }

    function _runBbsolverVerify(bbsolver, keyPath, samplePath) {
        var cmd = _platformQuote(bbsolver) + ' verify ' +
            _platformQuote(keyPath) + ' ' + _platformQuote(samplePath);
        var out = '';
        try { out = system.callSystem(cmd); } catch (e) {
            return { ok: false, error: e.message || String(e) };
        }
        var parsed = _cleanupVerifyJsonFromOutput(out);
        if (!parsed) {
            return { ok: false, error: 'could not parse verify output' };
        }
        return parsed;
    }

    function _cleanupVerifyResultMap(verifyResult) {
        var map = {};
        if (!verifyResult || !verifyResult.property_results) { return map; }
        var i, pr;
        for (i = 0; i < verifyResult.property_results.length; i++) {
            pr = verifyResult.property_results[i];
            if (pr && pr.property_id) { map[pr.property_id] = pr; }
        }
        return map;
    }

    function _cleanupResultIsBetter(firstPass, cleanup, phase) {
        if (!firstPass || !cleanup || cleanup.converged === false ||
                !cleanup.keys || cleanup.keys.length === 0 ||
                !firstPass.keys || firstPass.keys.length === 0) {
            return false;
        }
        var firstKeys = firstPass.keys.length;
        var cleanupKeys = cleanup.keys.length;
        var firstVertices = _resultMaxVertexCount(firstPass);
        var cleanupVertices = _resultMaxVertexCount(cleanup);
        var verticesNoWorse = (firstVertices <= 0 || cleanupVertices <= firstVertices);
        var keysNoWorse = cleanupKeys <= firstKeys;
        var verticesBetter = firstVertices > 0 && cleanupVertices > 0 &&
            cleanupVertices < firstVertices;
        var keysBetter = cleanupKeys < firstKeys;
        if (phase === 'temporal') {
            return keysBetter && verticesNoWorse;
        }
        return keysNoWorse && verticesNoWorse && (keysBetter || verticesBetter);
    }

    function _stripCleanupAcceptedSummary(notes) {
        var parts = String(notes || '').split(';');
        var out = [];
        var i, part, trimmed;
        for (i = 0; i < parts.length; i++) {
            part = parts[i];
            trimmed = _trim(part);
            if (!trimmed) { continue; }
            if (trimmed === 'auto_cleanup_pass_accepted' ||
                    trimmed.indexOf('cleanup_phase=') === 0 ||
                    trimmed.indexOf('first_pass_keys=') === 0 ||
                    trimmed.indexOf('cleanup_keys=') === 0 ||
                    trimmed.indexOf('first_pass_vertices=') === 0 ||
                    trimmed.indexOf('cleanup_vertices=') === 0 ||
                    trimmed.indexOf('auto_cleanup_original_') === 0) {
                continue;
            }
            out.push(trimmed);
        }
        return out.join('; ');
    }

    function _cleanupAppendAcceptedNote(
            firstPass, cleanup, originalVerifyEntry, phase, baselinePass) {
        var baseline = baselinePass || firstPass;
        var firstKeys = baseline && baseline.keys ? baseline.keys.length : 0;
        var cleanupKeys = cleanup && cleanup.keys ? cleanup.keys.length : 0;
        var firstVertices = _resultMaxVertexCount(baseline);
        var cleanupVertices = _resultMaxVertexCount(cleanup);
        cleanup.notes = _stripCleanupAcceptedSummary(cleanup.notes || '');
        cleanup.notes = (cleanup.notes ? cleanup.notes + '; ' : '') +
            'auto_cleanup_pass_accepted; cleanup_phase=' +
            (phase === 'temporal' ? 'temporal' : 'vertex') +
            '; first_pass_keys=' + firstKeys +
            '; cleanup_keys=' + cleanupKeys +
            '; first_pass_vertices=' + firstVertices +
            '; cleanup_vertices=' + cleanupVertices;
        if (originalVerifyEntry) {
            var originalMaxErr = (typeof originalVerifyEntry.max_err === 'number')
                ? originalVerifyEntry.max_err : '?';
            var originalMaxErrPx =
                (typeof originalVerifyEntry.max_err_screen_px === 'number')
                    ? originalVerifyEntry.max_err_screen_px : '?';
            cleanup.notes += '; auto_cleanup_original_verify_ok=' +
                (originalVerifyEntry.ok === true ? 'true' : 'false') +
                '; auto_cleanup_original_max_err=' +
                originalMaxErr +
                '; auto_cleanup_original_max_err_screen_px=' +
                originalMaxErrPx;
            if (originalVerifyEntry.ok !== true) {
                cleanup.notes += '; auto_cleanup_original_verify_exceeded=true';
            }
        }
        if (firstPass && firstPass.write_ranges) {
            cleanup.write_ranges = _normaliseWriteRanges(firstPass.write_ranges);
        }
        return cleanup;
    }

    function _acceptCleanupResults(
            firstResults, cleanupKeyBundle, cleanupVerifyResult,
            originalVerifyResult, phase, baselineResults) {
        var firstMap = {};
        var baselineMap = {};
        var accepted = {};
        var acceptedCount = 0;
        var rejectedCount = 0;
        var cleanupVerifyMap = _cleanupVerifyResultMap(cleanupVerifyResult);
        var originalVerifyMap = _cleanupVerifyResultMap(originalVerifyResult);
        var i, pk, firstPk;
        for (i = 0; i < firstResults.length; i++) {
            pk = firstResults[i];
            if (pk && !_isSubPathEntry(pk)) { firstMap[pk.property_id] = pk; }
        }
        baselineResults = baselineResults || firstResults;
        for (i = 0; i < baselineResults.length; i++) {
            pk = baselineResults[i];
            if (pk && !_isSubPathEntry(pk)) { baselineMap[pk.property_id] = pk; }
        }
        if (cleanupKeyBundle && cleanupKeyBundle.property_results) {
            for (i = 0; i < cleanupKeyBundle.property_results.length; i++) {
                pk = cleanupKeyBundle.property_results[i];
                if (!pk || _isSubPathEntry(pk)) { continue; }
                firstPk = firstMap[pk.property_id];
                if (cleanupVerifyMap[pk.property_id] &&
                        cleanupVerifyMap[pk.property_id].ok === true &&
                        _cleanupResultIsBetter(firstPk, pk, phase)) {
                    accepted[pk.property_id] = _cleanupAppendAcceptedNote(
                        firstPk, pk, originalVerifyMap[pk.property_id],
                        phase, baselineMap[pk.property_id]);
                    acceptedCount++;
                } else {
                    rejectedCount++;
                }
            }
        }
        return {
            accepted: accepted,
            acceptedCount: acceptedCount,
            rejectedCount: rejectedCount
        };
    }

    function _mergeCleanupAcceptedResults(firstResults, acceptedMap) {
        var out = [];
        var i, pk;
        for (i = 0; i < firstResults.length; i++) {
            pk = firstResults[i];
            if (!pk) { continue; }
            if (acceptedMap[pk.property_id]) {
                if (_isSubPathEntry(pk)) { continue; }
                out.push(acceptedMap[pk.property_id]);
            } else {
                out.push(pk);
            }
        }
        return out;
    }

    function _hasShapeFlatProperties(bundle) {
        var i;
        for (i = 0; i < bundle.properties.length; i++) {
            if (bundle.properties[i].property &&
                    bundle.properties[i].property.units_label === 'shape_flat') {
                return true;
            }
        }
        return false;
    }

    function _hasMotionSmoothPathProperties(bundle) {
        if (!bundle || !bundle.config) {
            return false;
        }
        var mode = _normalizeSolveOptimizationMode(
            bundle.config.solve_optimization_mode);
        if (mode !== 'motion_smooth' && mode !== 'motion_path_smooth') {
            return false;
        }
        return _hasShapeFlatProperties(bundle);
    }

    function _confirmMotionSmoothPathWarning(bundle) {
        var count = 0;
        var i;
        if (bundle && bundle.properties) {
            for (i = 0; i < bundle.properties.length; i++) {
                if (bundle.properties[i].property &&
                        bundle.properties[i].property.units_label === 'shape_flat') {
                    count++;
                }
            }
        }
        log('WARN [motion smooth]: smoothing ' + count +
            ' path propert' + (count === 1 ? 'y' : 'ies') +
            ' can alter apparent path topology/overlap ordering.');
        return confirm(
            'Motion Smooth on Path properties rewrites the sampled path as a ' +
            'smoothed vertex/tangent trajectory.\n\n' +
            'AE still interpolates each Path key as one whole shape, not as ' +
            'independent roving vertices. That means individual vertices and ' +
            'tangents may advance at different rates than they did before ' +
            'Motion Smooth. On complex, self-overlapping, or tightly folded ' +
            'paths this can change the apparent topology, overlap ordering, or ' +
            'inside/outside relationship even when the vertex count stays the ' +
            'same.\n\n' +
            'Use this mode when smoothing motion is more important than ' +
            'preserving the exact original per-frame topology. Review the result ' +
            'before replacing production keys.\n\n' +
            'Continue with Motion Smooth?');
    }

    function _runCleanupGroupPostValidate(
            bbsolver, firstResults, originalBundle, cleanupGroupBundle,
            cleanupKeyBundle, cleanupInputPath, verifySourcePath, cleanupOutPath,
            groupIds, tol, tolPx, phase, baselineResults) {
        var verifyBundle = _filterBundleForIds(
            originalBundle, groupIds || [], tol, tolPx);
        writeSampleBundleJson(verifyBundle, verifySourcePath);
        var cleanupVerifyResult = _runBbsolverVerify(
            bbsolver, cleanupOutPath, cleanupInputPath);
        if (!cleanupVerifyResult.ok) {
            log('WARN [cleanup]: candidate rejected by first-pass verify' +
                (cleanupVerifyResult.error ? ' (' + cleanupVerifyResult.error + ')' : '') + '.');
            return { accepted: {}, acceptedCount: 0, rejectedCount: 1 };
        }
        var originalVerifyResult = _runBbsolverVerify(
            bbsolver, cleanupOutPath, verifySourcePath);
        if (originalVerifyResult && originalVerifyResult.ok === false) {
            log('WARN [cleanup]: cleanup candidate improves first-pass keys ' +
                'but exceeds original-source tolerance; keeping diagnostics in notes.');
        }
        return _acceptCleanupResults(
            firstResults, cleanupKeyBundle, cleanupVerifyResult,
            originalVerifyResult, phase, baselineResults);
    }

    function _runCleanupPhaseBlocking(
            comp, bbsolver, originalBundle, scratchDir, requestId,
            firstResults, baselineResults, phase, phaseStart) {
        var plan = _buildCleanupPassPlan(
            originalBundle, firstResults, requestId, phase);
        if (!plan || !plan.candidates || plan.candidates.length === 0) {
            return {
                results: firstResults,
                phaseStart: phaseStart,
                acceptedCount: 0,
                rejectedCount: 0
            };
        }

        phaseStart = setStatus('Preparing ' + _cleanupPhaseLabel(phase),
            phase === 'temporal' ? 80 : 82, phaseStart);
        log('INFO [cleanup]: running ' + _cleanupPhaseLabel(phase) +
            ' on accepted path keys without AE resampling.');
        var acceptedAll = {};
        var acceptedCount = 0;
        var rejectedCount = 0;
        var tolGroups = _buildToleranceGroups(plan.bundle);
        var groupIdx = 0;
        var tolStr, group, tol, tolPx, filteredBundle, inPath, outPath;
        var verifySourcePath, keyBundle, accepted;
        for (tolStr in tolGroups) {
            if (!Object.prototype.hasOwnProperty.call(tolGroups, tolStr)) { continue; }
            groupIdx++;
            group = tolGroups[tolStr];
            tol = parseFloat(group.tolStr);
            tolPx = parseFloat(group.tolPxStr);
            filteredBundle = _filterBundleForIds(
                plan.bundle, group.groupIds, tol, tolPx);
            filteredBundle.config = _cleanupPassConfig(
                filteredBundle.config, phase);
            inPath = scratchDir + '/' + requestId + '_cleanup_' +
                phase + '_g' +
                groupIdx + '.bbsm.json';
            outPath = scratchDir + '/' + requestId + '_cleanup_' +
                phase + '_g' +
                groupIdx + '.bbky.json';
            verifySourcePath = scratchDir + '/' + requestId + '_cleanup_' +
                phase + '_g' +
                groupIdx + '_verify_source.bbsm.json';
            writeSampleBundleJson(filteredBundle, inPath);
            phaseStart = setStatus(_cleanupPhaseLabel(phase) +
                ' group ' + groupIdx, phase === 'temporal' ? 80 : 82,
                phaseStart);
            keyBundle = _runBbsolver(
                bbsolver, inPath, outPath, tol, tolPx,
                phase === 'temporal' ? 80 : 82,
                phase === 'temporal' ? 82 : 84,
                filteredBundle.config);
            _logKeyBundleSummary(keyBundle, filteredBundle);
            accepted = _runCleanupGroupPostValidate(
                bbsolver, firstResults, originalBundle, filteredBundle,
                keyBundle, inPath, verifySourcePath, outPath,
                group.groupIds, tol, tolPx, phase, baselineResults);
            var aid;
            for (aid in accepted.accepted) {
                if (Object.prototype.hasOwnProperty.call(accepted.accepted, aid)) {
                    acceptedAll[aid] = accepted.accepted[aid];
                }
            }
            acceptedCount += accepted.acceptedCount;
            rejectedCount += accepted.rejectedCount;
        }
        if (acceptedCount <= 0) {
            log('INFO [cleanup]: ' + _cleanupPhaseLabel(phase) +
                ' found no verified improvement ' +
                '(' + rejectedCount + ' candidate' +
                (rejectedCount === 1 ? '' : 's') + ' rejected/unchanged).');
            return {
                results: firstResults,
                phaseStart: phaseStart,
                acceptedCount: acceptedCount,
                rejectedCount: rejectedCount
            };
        }
        log('INFO [cleanup]: accepted ' + _cleanupPhaseLabel(phase) +
            ' replacements for ' +
            acceptedCount + ' propert' + (acceptedCount === 1 ? 'y' : 'ies') + '.');
        return {
            results: _mergeCleanupAcceptedResults(firstResults, acceptedAll),
            phaseStart: phaseStart,
            acceptedCount: acceptedCount,
            rejectedCount: rejectedCount
        };
    }

    function _runPromptedCleanupPassBlocking(
            comp, bbsolver, originalBundle, scratchDir, requestId,
            firstResults, phaseStart) {
        var promptPlan = _buildCleanupPassPlan(
            originalBundle, firstResults, requestId, 'temporal');
        if (!promptPlan || !promptPlan.candidates ||
                promptPlan.candidates.length === 0) {
            return { results: firstResults, phaseStart: phaseStart };
        }
        if (_cleanupPlanNeedsPrompt(promptPlan) &&
                !confirm(_cleanupPromptText(promptPlan))) {
            log('INFO [cleanup]: user skipped cleanup passes.');
            return { results: firstResults, phaseStart: phaseStart };
        }

        var baselineResults = firstResults;
        var staticPruned = _pruneStaticCleanupKeys(firstResults, baselineResults);
        var cleanupStartResults = staticPruned.results;
        var temporal = _runCleanupPhaseBlocking(
            comp, bbsolver, originalBundle, scratchDir, requestId,
            cleanupStartResults, baselineResults, 'temporal', phaseStart);
        var vertex = _runCleanupPhaseBlocking(
            comp, bbsolver, originalBundle, scratchDir, requestId,
            temporal.results, baselineResults, 'vertex', temporal.phaseStart);
        return {
            results: vertex.results,
            phaseStart: vertex.phaseStart
        };
    }

    function _runCleanupPhaseAsync(
            state, originalBundle, firstResults, baselineResults, phase, done) {
        var plan = _buildCleanupPassPlan(
            originalBundle, firstResults, state.requestId, phase);
        if (!plan || !plan.candidates || plan.candidates.length === 0) {
            done(firstResults);
            return;
        }

        state.phaseStart = setStatus('Preparing ' + _cleanupPhaseLabel(phase),
            phase === 'temporal' ? 80 : 82, state.phaseStart);
        log('INFO [cleanup]: running ' + _cleanupPhaseLabel(phase) +
            ' on accepted path keys without AE resampling.');
        var tolGroups = _buildToleranceGroups(plan.bundle);
        var groupList = [];
        var tolStr;
        for (tolStr in tolGroups) {
            if (Object.prototype.hasOwnProperty.call(tolGroups, tolStr)) {
                groupList.push(tolGroups[tolStr]);
            }
        }
        var cleanupState = {
            groupList: groupList,
            groupIndex: 0,
            acceptedAll: {},
            acceptedCount: 0,
            rejectedCount: 0,
            planBundle: plan.bundle
        };

        function continueCleanup() {
            if (_isCancelled()) {
                log('Cancelled by user during ' + _cleanupPhaseLabel(phase) + '.');
                _cleanCancelFile();
                _cooperativeBakeActive = false;
                return;
            }
            if (cleanupState.groupIndex >= cleanupState.groupList.length) {
                if (cleanupState.acceptedCount <= 0) {
                    log('INFO [cleanup]: ' + _cleanupPhaseLabel(phase) +
                        ' found no verified improvement ' +
                        '(' + cleanupState.rejectedCount + ' candidate' +
                        (cleanupState.rejectedCount === 1 ? '' : 's') +
                        ' rejected/unchanged).');
                    done(firstResults);
                    return;
                }
                log('INFO [cleanup]: accepted ' + _cleanupPhaseLabel(phase) +
                    ' replacements for ' +
                    cleanupState.acceptedCount + ' propert' +
                    (cleanupState.acceptedCount === 1 ? 'y' : 'ies') + '.');
                done(_mergeCleanupAcceptedResults(
                    firstResults, cleanupState.acceptedAll));
                return;
            }

            cleanupState.groupIndex++;
            var groupIdx = cleanupState.groupIndex;
            var group = cleanupState.groupList[groupIdx - 1];
            var tol = parseFloat(group.tolStr);
            var tolPx = parseFloat(group.tolPxStr);
            var filteredBundle = _filterBundleForIds(
                cleanupState.planBundle, group.groupIds, tol, tolPx);
            filteredBundle.config = _cleanupPassConfig(
                filteredBundle.config, phase);
            var inPath = state.scratchDir + '/' + state.requestId +
                '_cleanup_' + phase + '_g' + groupIdx + '.bbsm.json';
            var outPath = state.scratchDir + '/' + state.requestId +
                '_cleanup_' + phase + '_g' + groupIdx + '.bbky.json';
            var verifySourcePath = state.scratchDir + '/' + state.requestId +
                '_cleanup_' + phase + '_g' + groupIdx + '_verify_source.bbsm.json';
            try { writeSampleBundleJson(filteredBundle, inPath); }
            catch (we) {
                log('WARN [cleanup]: could not write ' +
                    _cleanupPhaseLabel(phase) + ' group ' + groupIdx +
                    ': ' + we.message);
                cleanupState.rejectedCount++;
                continueCleanup();
                return;
            }

            state.phaseStart = setStatus(_cleanupPhaseLabel(phase) +
                ' group ' + groupIdx, phase === 'temporal' ? 80 : 82,
                state.phaseStart);
            var cancelArg = _cancelFile
                ? (' --cancel-file ' + _shellQuote(_cancelFile)) : '';
            var solverJobs = _solverJobsSetting();
            var cleanupSolveMode = _normalizeSolveOptimizationMode(
                filteredBundle.config.solve_optimization_mode);
            var cmdBase = _shellQuote(state.bbsolver) +
                ' solve ' + _shellQuote(inPath) + ' ' + _shellQuote(outPath) +
                ' --tolerance ' + tol +
                ' --screen-px ' + tolPx +
                ' --jobs ' + solverJobs +
                ' --solve-mode ' + cleanupSolveMode +
                ' --progress-fd 1' +
                cancelArg;
            log('  cleanup bbsolver: phase=' + phase +
                ', tol=' + tol + ', px=' + tolPx +
                ', jobs=' + (solverJobs === 0 ? 'auto' : solverJobs) +
                ', solve mode=' + cleanupSolveMode +
                ', second-pass vertex prune=' +
                (filteredBundle.config.path_replacement_prefer_vertices === true
                    ? 'on' : 'off') +
                ', replacement topology=off' +
                ', preserve sharp corners=' +
                (filteredBundle.config.path_preserve_sharp_corners !== false
                    ? 'on' : 'off') +
                (cancelArg ? ' [cancel-file set]' : ''));

            var launched = _runBbsolverAsync(
                state.bbsolver, inPath, outPath, tol, tolPx, cmdBase,
                phase === 'temporal' ? 80 : 82,
                phase === 'temporal' ? 82 : 84,
                function (keyBundle) {
                    var accepted;
                    _logKeyBundleSummary(keyBundle, filteredBundle);
                    try {
                        accepted = _runCleanupGroupPostValidate(
                            state.bbsolver, firstResults, originalBundle,
                            filteredBundle, keyBundle, inPath, verifySourcePath,
                            outPath, group.groupIds, tol, tolPx, phase,
                            baselineResults);
                    } catch (ve) {
                        log('WARN [cleanup]: verify failed for ' +
                            _cleanupPhaseLabel(phase) + ' group ' +
                            groupIdx + ': ' + ve.message);
                        accepted = { accepted: {}, acceptedCount: 0, rejectedCount: 1 };
                    }
                    var aid;
                    for (aid in accepted.accepted) {
                        if (Object.prototype.hasOwnProperty.call(
                                accepted.accepted, aid)) {
                            cleanupState.acceptedAll[aid] =
                                accepted.accepted[aid];
                        }
                    }
                    cleanupState.acceptedCount += accepted.acceptedCount;
                    cleanupState.rejectedCount += accepted.rejectedCount;
                    continueCleanup();
                },
                function (err) {
                    log('WARN [cleanup]: ' + _cleanupPhaseLabel(phase) +
                        ' solver failed for group ' +
                        groupIdx + ': ' + err.message);
                    cleanupState.rejectedCount++;
                    continueCleanup();
                });
            if (!launched) {
                log('WARN [cleanup]: async ' + _cleanupPhaseLabel(phase) +
                    ' launch failed; skipping group ' + groupIdx + '.');
                cleanupState.rejectedCount++;
                continueCleanup();
            }
        }
        continueCleanup();
    }

    function _runPromptedCleanupPassAsync(state, originalBundle, firstResults, done) {
        var promptPlan = _buildCleanupPassPlan(
            originalBundle, firstResults, state.requestId, 'temporal');
        if (!promptPlan || !promptPlan.candidates ||
                promptPlan.candidates.length === 0) {
            done(firstResults);
            return;
        }
        if (_cleanupPlanNeedsPrompt(promptPlan) &&
                !confirm(_cleanupPromptText(promptPlan))) {
            log('INFO [cleanup]: user skipped cleanup passes.');
            done(firstResults);
            return;
        }

        var baselineResults = firstResults;
        var staticPruned = _pruneStaticCleanupKeys(firstResults, baselineResults);
        var cleanupStartResults = staticPruned.results;
        _runCleanupPhaseAsync(
            state, originalBundle, cleanupStartResults, baselineResults, 'temporal',
            function (temporalResults) {
                _runCleanupPhaseAsync(
                    state, originalBundle, temporalResults, baselineResults,
                    'vertex', done);
            });
    }

    function _logKeyBundleSummary(keyBundle, filteredBundle) {
        var _kb_source = 0;
        var _kb_diag_n = 0, _kb_diag = 0;
        var _kb_vis_n  = 0, _kb_vis  = 0;
        var _kb_scout_n = 0;     // visible-marker rows missing visibility=
        var _kb_overclaim_n = 0; // visible rows whose vertex_range over-claims entry verts
        var _kbri;
        for (_kbri = 0; _kbri < keyBundle.property_results.length; _kbri++) {
            var _kbr = keyBundle.property_results[_kbri];
            var _kbnotes = (_kbr.notes || '').replace(/^\s+|\s+$/g, '');
            if (_kbnotes.indexOf('landmark_subpath;') === 0 ||
                    (typeof _isSubPathEntry === 'function' && _isSubPathEntry(_kbr))) {
                _kb_diag_n++;
                _kb_diag += (_kbr.keys ? _kbr.keys.length : 0);
            } else if (_kbnotes.indexOf('shape_channel_subpath;') === 0 ||
                       _kbnotes.indexOf('visible_channel_subpath;') === 0) {
                _kb_vis_n++;
                _kb_vis += (_kbr.keys ? _kbr.keys.length : 0);
                if (_kbnotes.indexOf('visibility=') < 0) {
                    _kb_scout_n++;
                }
                var _kbVrIdx = _kbnotes.indexOf('vertex_range=');
                if (_kbVrIdx >= 0 &&
                        _kbr.keys && _kbr.keys.length > 0 &&
                        _kbr.keys[0].v && _kbr.keys[0].v.length >= 2) {
                    var _kbVrStart = _kbVrIdx + 'vertex_range='.length;
                    var _kbVrEnd   = _kbnotes.indexOf(';', _kbVrStart);
                    var _kbVrRaw   = (_kbVrEnd >= 0)
                        ? _kbnotes.substring(_kbVrStart, _kbVrEnd)
                        : _kbnotes.substring(_kbVrStart);
                    var _kbVrDash = _kbVrRaw.indexOf('-');
                    if (_kbVrDash > 0) {
                        var _kbVrA = parseInt(
                            _kbVrRaw.substring(0, _kbVrDash), 10);
                        var _kbVrB = parseInt(
                            _kbVrRaw.substring(_kbVrDash + 1), 10);
                        var _kbFlatVerts = parseInt(_kbr.keys[0].v[1], 10);
                        if (!isNaN(_kbVrA) && !isNaN(_kbVrB) &&
                                !isNaN(_kbFlatVerts) &&
                                _kbVrB > _kbVrA &&
                                (_kbVrB - _kbVrA) !== _kbFlatVerts) {
                            _kb_overclaim_n++;
                        }
                    }
                }
            } else {
                _kb_source += (_kbr.keys ? _kbr.keys.length : 0);
            }
        }
        log('  source: ' + _kb_source + ' keys  ms=' +
            (keyBundle.solve_time_ms || 0).toFixed(0));
        if (_kb_diag_n > 0) {
            log('  diagnostic subpaths: ' + _kb_diag_n +
                ' entries, ' + _kb_diag + ' keys ' +
                '(landmark_subpath / reserved -- bb_lm_N, may be inert)');
        }
        if (_kb_vis_n > 0) {
            log('  visible subpaths: ' + _kb_vis_n +
                ' entries, ' + _kb_vis + ' keys ' +
                '(shape_channel_subpath -- bb_vc_N, must render visibly)');
            if (_kb_scout_n > 0) {
                log('  WARN: ' + _kb_scout_n + ' of those ' +
                    'shape_channel_subpath rows lack a visibility= token. ' +
                    'AE writeback is fail-closed for solver scout rows; ' +
                    'they will be skipped, not applied. Coordinate with the ' +
                    'solver to add visibility=mask_add | mask_subtract | ' +
                    'shape_group_full before treating these as visible ' +
                    'improvements.');
            }
            if (_kb_overclaim_n > 0) {
                log('  WARN: ' + _kb_overclaim_n + ' of those ' +
                    'shape_channel_subpath rows over-claim vertex_range ' +
                    'relative to the entry flat-shape header ' +
                    '(vr.end - vr.first != flat vertex count). The solver ' +
                    'is asserting coverage it has not actually produced; AE ' +
                    'writeback will fail-close before creating a visible sibling.');
            }
        }
        _logPathReplacementNotes(keyBundle, _propertyLabelMap(filteredBundle));
    }

    function _runBakeCooperative(comp, props) {
        if (_cooperativeBakeActive) {
            alert('bbsolver-test-harness: a bake is already running.');
            return;
        }

        var phaseStart = setStatus('Initializing bake', 0);
        var bbsolver     = _findBbsolver();
        var requestId  = 'req-' + (new Date().getTime());
        var scratchDir = _runScratchDir(bbsolver, requestId);
        log('  artifact dir: ' + scratchDir);

        _cancelFile       = scratchDir + '/' + requestId + '.cancel';
        _cancelRequested  = false;
        _cleanCancelFile();
        _cancelFile = scratchDir + '/' + requestId + '.cancel';
        log('  cancel file: ' + _cancelFile);
        _setAbortEnabled(true);
        _startSolveElapsedTimer();

        if (settings.autoSeparateForBake === true) {
            var needSepCount = _countPropsNeedingSeparation(props);
            if (needSepCount > 0) {
                if (!_confirmSeparation(needSepCount)) {
                    log('Bake cancelled by user (separation confirmation).');
                    _cleanCancelFile();
                    return;
                }
                log('User confirmed Separate Dimensions on ' + needSepCount + ' prop(s).');
            }
        }

        var segmentBatches = _buildBakeBatches(comp, props);
        if (!segmentBatches || segmentBatches.length === 0) {
            alert('bbsolver-test-harness: segment mode found no bbsolver marker ranges on the selected property layers.');
            setStatus('Idle', 0, phaseStart);
            _cleanCancelFile();
            return;
        }

        var state = {
            comp: comp,
            bbsolver: bbsolver,
            scratchDir: scratchDir,
            requestId: requestId,
            bundle: null,
            sampleBundles: [],
            segmentBatches: segmentBatches,
            segmentIndex: 0,
            groupList: [],
            groupIndex: 0,
            mergedResults: [],
            phaseStart: phaseStart,
            tolGroupTotal: 0
        };
        state.motionSmoothPathWarningShown = false;
        _cooperativeBakeActive = true;
        _startCooperativeSegment(state);
    }

    function _finishCooperativeBake(state) {
        var mergedBundle = _mergeSampleBundlesForSegments(
            state.sampleBundles, state.requestId);
        _runPromptedCleanupPassAsync(
            state, mergedBundle, state.mergedResults,
            function (finalResults) {
                try {
                    _finishMergedBake(state.comp, mergedBundle,
                        state.scratchDir, state.requestId,
                        finalResults, state.phaseStart);
                } catch (fe) {
                    log('ERROR (finish): ' + fe.message);
                    alert('Finish failed:\n' + fe.message);
                    setStatus('Idle', 0, state.phaseStart);
                } finally {
                    _cooperativeBakeActive = false;
                }
            });
    }

    function _startCooperativeSegment(state) {
        if (_isCancelled()) {
            log('Cancelled by user.');
            _cleanCancelFile();
            _cooperativeBakeActive = false;
            return;
        }
        if (state.segmentIndex >= state.segmentBatches.length) {
            _finishCooperativeBake(state);
            return;
        }

        var batch = state.segmentBatches[state.segmentIndex];
        var segmentLabel = (batch.segmentMode ? 'Segment ' +
            (state.segmentIndex + 1) + '/' + state.segmentBatches.length +
            ' ' : '') + batch.label;
        state.phaseStart = setStatus(
            'Sampling AE properties: ' + segmentLabel, 5, state.phaseStart);
        log('INFO [segment]: sampling ' + segmentLabel +
            ' (' + batch.props.length + ' prop' +
            (batch.props.length === 1 ? '' : 's') + ').');

        var bundle;
        var rangeOverride = { tStart: batch.tStart, tEnd: batch.tEnd };
        try {
            bundle = _sampleOrReuseForBake(
                state.comp, batch.props, settings.autoSeparateForBake === true,
                rangeOverride);
        } catch (e) {
            log('ERROR (sample): ' + e.message);
            alert('Sample failed:\n' + e.message);
            setStatus('Idle', 0, state.phaseStart);
            _cleanCancelFile();
            _cooperativeBakeActive = false;
            return;
        }
        if (!bundle || bundle.properties.length === 0) {
            log('No bakeable properties in ' + segmentLabel + '; skipping.');
            state.segmentIndex++;
            _startCooperativeSegment(state);
            return;
        }
        state.sampleBundles.push(bundle);
        log('Sampled ' + bundle.properties.length + ' propert' +
            (bundle.properties.length === 1 ? 'y' : 'ies') + ' for ' +
            segmentLabel + '.');
        var spatialBlocker = _unifiedSpatialSolveBlocker(bundle);
        if (spatialBlocker) {
            log('SKIP [spatial]: ' + spatialBlocker);
            alert(spatialBlocker);
            setStatus('Idle', 0, state.phaseStart);
            _cleanCancelFile();
            _cooperativeBakeActive = false;
            return;
        }
        state.phaseStart = setStatus('Grouping by tolerance', 10, state.phaseStart);

        var tolGroups = _buildToleranceGroups(bundle);
        var groupList = [];
        var tolStr;
        for (tolStr in tolGroups) {
            if (Object.prototype.hasOwnProperty.call(tolGroups, tolStr)) {
                groupList.push(tolGroups[tolStr]);
            }
        }
        state.bundle = bundle;
        state.groupList = groupList;
        state.groupIndex = 0;
        state.tolGroupTotal = groupList.length;
        _continueCooperativeBake(state);
    }

    function _cooperativeBakeError(state, err) {
        log('ERROR (bbsolver): ' + err.message);
        alert('bbsolver failed:\n' + err.message);
        _cleanCancelFile();
        _cooperativeBakeActive = false;
        setStatus('Idle', 0, state.phaseStart);
    }

    function _continueCooperativeBake(state) {
        if (_isCancelled()) {
            log('Cancelled by user.');
            _cleanCancelFile();
            _cooperativeBakeActive = false;
            return;
        }
        if (state.groupIndex >= state.groupList.length) {
            state.segmentIndex++;
            if (state.segmentIndex >= state.segmentBatches.length) {
                _finishCooperativeBake(state);
                return;
            }
            _startCooperativeSegment(state);
            return;
        }

        var groupIdx = state.groupIndex + 1;
        var group = state.groupList[state.groupIndex];
            group.tol = parseFloat(group.tolStr);
            group.tolPx = parseFloat(group.tolPxStr);
            group.filteredBundle = _filterBundleForIds(
                state.bundle, group.groupIds, group.tol, group.tolPx,
                group.solveMode, parseFloat(group.motionSmoothTolStr),
                group.motionSmoothBezier, group.motionSmoothUseEase,
                group.motionSmoothSourceFidelity);
        if (!state.motionSmoothPathWarningShown &&
                _hasMotionSmoothPathProperties(group.filteredBundle)) {
            if (!_confirmMotionSmoothPathWarning(group.filteredBundle)) {
                log('Bake cancelled by user (Motion Smooth path warning).');
                setStatus('Cancelled (motion smooth warning)', 0, state.phaseStart);
                _cleanCancelFile();
                _cooperativeBakeActive = false;
                return;
            }
            state.motionSmoothPathWarningShown = true;
        }
        var segmentPathTag = state.segmentBatches.length > 1
            ? ('_s' + (state.segmentIndex + 1)) : '';
        group.inPath = state.scratchDir + '/' + state.requestId +
            segmentPathTag + '_g' + groupIdx + '.bbsm.json';
        group.outPath = state.scratchDir + '/' + state.requestId +
            segmentPathTag + '_g' + groupIdx + '.bbky.json';
        var segmentTotal = Math.max(1, state.segmentBatches.length);
        var segmentStart = 12 + Math.round(
            (state.segmentIndex / segmentTotal) * 68);
        var segmentEnd = 12 + Math.round(
            ((state.segmentIndex + 1) / segmentTotal) * 68);
        var segmentSpan = Math.max(1, segmentEnd - segmentStart);
        group.solveProgressStart = segmentStart + Math.round(
            ((groupIdx - 1) / Math.max(1, state.tolGroupTotal)) * segmentSpan);
        group.solveProgressEnd = segmentStart + Math.round(
            (groupIdx / Math.max(1, state.tolGroupTotal)) * segmentSpan);
        group.attempt = 0;

        try { writeSampleBundleJson(group.filteredBundle, group.inPath); }
        catch (we) {
            log('ERROR (write): ' + we.message);
            state.groupIndex++;
            _continueCooperativeBake(state);
            return;
        }
        state.phaseStart = setStatus(
            'Group ' + groupIdx + ': wrote sample bundle',
            group.solveProgressStart, state.phaseStart);
        if (_hasShapeFlatProperties(group.filteredBundle)) {
            log('NOTE [path]: path solve can take 30 s - several minutes. ' +
                'Progress bar updates as the solver emits phase events.');
        }
        _runCooperativeAttempt(state, group);
    }

    function _runCooperativeAttempt(state, group) {
        var groupIdx = state.groupIndex + 1;
        if (group.attempt >= 3) {
            log('WARN: maximum solver attempts reached for group ' +
                groupIdx + '.');
            return;
        }
        state.phaseStart = setStatus(
            'Group ' + groupIdx + ': solver attempt ' + (group.attempt + 1) +
            ' (tol=' + group.tol + ', px=' + group.tolPx + ', ' +
            group.groupIds.length + ' props)',
            group.solveProgressStart, state.phaseStart);

        var cancelArg = _cancelFile ? (' --cancel-file ' + _shellQuote(_cancelFile)) : '';
        var lmArg = '';
        var solverConfig = group.filteredBundle && group.filteredBundle.config
            ? group.filteredBundle.config : {};
        var solverJobs = _solverJobsSetting();
        var solveMode = _normalizeSolveOptimizationMode(
            solverConfig.solve_optimization_mode);
        var cmdBase = _shellQuote(state.bbsolver) +
            ' solve ' + _shellQuote(group.inPath) + ' ' + _shellQuote(group.outPath) +
            ' --tolerance ' + group.tol +
            ' --screen-px ' + group.tolPx +
            ' --jobs ' + solverJobs +
            ' --solve-mode ' + solveMode +
            ' --progress-fd 1' +
            cancelArg +
            lmArg;
        log('  bbsolver: tol=' + group.tol + ', px=' + group.tolPx +
            ', jobs=' + (solverJobs === 0 ? 'auto' : solverJobs) +
            ', solve mode=' + solveMode +
            ', second-pass vertex prune=' +
            (solverConfig.path_replacement_prefer_vertices === true ? 'on' : 'off') +
            ', replacement topology=' +
            (solverConfig.allow_path_replacement_fit === true ? 'on' : 'off') +
            ', preserve sharp corners=' +
            (solverConfig.path_preserve_sharp_corners !== false ? 'on' : 'off') +
            (cancelArg ? ' [cancel-file set]' : ''));
        var launched = _runBbsolverAsync(
            state.bbsolver, group.inPath, group.outPath, group.tol, group.tolPx,
            cmdBase, group.solveProgressStart, group.solveProgressEnd,
            function (keyBundle) {
                _afterCooperativeSolve(state, group, keyBundle);
            },
            function (err) {
                _cooperativeBakeError(state, err);
            });
        if (!launched) {
            log('  solver: cooperative async launch failed; using blocking fallback.');
            try {
                var keyBundle = _runBbsolverSync(
                    state.bbsolver, group.inPath, group.outPath, group.tol,
                    group.tolPx, cmdBase, group.solveProgressStart,
                    group.solveProgressEnd);
                _afterCooperativeSolve(state, group, keyBundle);
            } catch (syncErr) {
                _cooperativeBakeError(state, syncErr);
            }
        }
    }

    function _afterCooperativeSolve(state, group, keyBundle) {
        var groupIdx = state.groupIndex + 1;
        if (_isCancelled()) {
            log('Cancelled by user (bbsolver exit or cancel flag).');
            _cleanCancelFile();
            _cooperativeBakeActive = false;
            return;
        }

        _logKeyBundleSummary(keyBundle, group.filteredBundle);
        state.phaseStart = setStatus(
            'Group ' + groupIdx + ': parsed solver output',
            group.solveProgressEnd, state.phaseStart);

        var failedResults = [];
        var kr;
        for (kr = 0; kr < keyBundle.property_results.length; kr++) {
            if (keyBundle.property_results[kr].converged === false) {
                failedResults.push(keyBundle.property_results[kr]);
            }
        }
        if (failedResults.length > 0) {
            var errAction = _showConvergenceErrorDialog(
                failedResults, keyBundle.property_results,
                _propertyLabelMap(group.filteredBundle));
            if (errAction === 'skipAll') {
                log('Skipping (user cancelled after convergence failures).');
                setStatus('Cancelled (convergence)', 0, state.phaseStart);
                _cleanCancelFile();
                _cooperativeBakeActive = false;
                return;
            }
            if (errAction === 'loosen') {
                group.tol = group.tol * 2;
                group.attempt++;
                log('Retrying with tolerance ' + group.tol);
                if (group.attempt < 3) {
                    _runCooperativeAttempt(state, group);
                    return;
                }
                log('WARN: maximum solver retry attempts reached; ' +
                    'continuing with latest solver result.');
            }
            if (errAction !== 'loosen') {
                var convergedOnly = [];
                for (kr = 0; kr < keyBundle.property_results.length; kr++) {
                    if (keyBundle.property_results[kr].converged !== false) {
                        convergedOnly.push(keyBundle.property_results[kr]);
                    }
                }
                keyBundle.property_results = convergedOnly;
            }
        }

        if (settings.showPreview) {
            var previewResult = _showPreviewDialog(
                group.filteredBundle, keyBundle, group.tol, group.tolPx);
            if (previewResult.action === 'cancel') {
                log('Cancelled (preview).');
                setStatus('Cancelled (preview)', 0, state.phaseStart);
                _cleanCancelFile();
                _cooperativeBakeActive = false;
                return;
            }
            if (previewResult.action === 'resolveAll') {
                group.tol = group.tol * 0.5;
                group.attempt++;
                log('Re-solving with tolerance ' + group.tol);
                if (group.attempt < 3) {
                    _runCooperativeAttempt(state, group);
                    return;
                }
                log('WARN: maximum solver retry attempts reached; ' +
                    'continuing with latest solver result.');
            }
            if (previewResult.action !== 'resolveAll') {
                keyBundle.property_results = previewResult.filteredResults;
            }
        }

        _attachWriteRangesToPropertyResults(
            keyBundle.property_results,
            state.segmentBatches[state.segmentIndex]);

        var mr;
        for (mr = 0; mr < keyBundle.property_results.length; mr++) {
            state.mergedResults.push(keyBundle.property_results[mr]);
        }
        state.groupIndex++;
        _continueCooperativeBake(state);
    }

    function _runBakeBlocking(comp, props) {
        var phaseStart = setStatus('Initializing bake', 0);
        var bbsolver     = _findBbsolver();
        var requestId  = 'req-' + (new Date().getTime());
        var scratchDir = _runScratchDir(bbsolver, requestId);
        log('  artifact dir: ' + scratchDir);

        // Set up cancel sentinel file for this bake session.
        _cancelFile       = scratchDir + '/' + requestId + '.cancel';
        _cancelRequested  = false;
        // Ensure no stale cancel file from a prior session.
        _cleanCancelFile();
        _cancelFile = scratchDir + '/' + requestId + '.cancel';
        log('  cancel file: ' + _cancelFile);
        _setAbortEnabled(true);
        _startSolveElapsedTimer();

        // 0. Pre-bake separation check: if any spatial property needs Separate Dimensions
        //    enabled for the first time, confirm with the user (it's permanent).
        if (settings.autoSeparateForBake === true) {
            var needSepCount = _countPropsNeedingSeparation(props);
            if (needSepCount > 0) {
                if (!_confirmSeparation(needSepCount)) {
                    log('Bake cancelled by user (separation confirmation).');
                    _cleanCancelFile();
                    return;
                }
                log('User confirmed Separate Dimensions on ' + needSepCount + ' prop(s).');
            }
        }

        var segmentBatches = _buildBakeBatches(comp, props);
        if (!segmentBatches || segmentBatches.length === 0) {
            alert('bbsolver-test-harness: segment mode found no bbsolver marker ranges on the selected property layers.');
            setStatus('Idle', 0, phaseStart);
            _cleanCancelFile();
            return;
        }

        var mergedResults = [];
        var sampleBundles = [];
        var batchIdx, batch, bundle, rangeOverride, spatialBlocker;
        var motionSmoothPathWarningShown = false;
        for (batchIdx = 0; batchIdx < segmentBatches.length; batchIdx++) {
            batch = segmentBatches[batchIdx];
            phaseStart = setStatus(
                'Sampling AE properties: ' + batch.label, 5, phaseStart);
            log('INFO [segment]: sampling ' + batch.label +
                ' (' + batch.props.length + ' prop' +
                (batch.props.length === 1 ? '' : 's') + ').');
            rangeOverride = { tStart: batch.tStart, tEnd: batch.tEnd };
            try {
                bundle = _sampleOrReuseForBake(
                    comp, batch.props, settings.autoSeparateForBake === true,
                    rangeOverride);
            }
            catch (e) {
                log('ERROR (sample): ' + e.message);
                alert('Sample failed:\n' + e.message);
                setStatus('Idle', 0, phaseStart);
                _cleanCancelFile();
                return;
            }
            if (!bundle || bundle.properties.length === 0) {
                log('No bakeable properties in ' + batch.label + '; skipping.');
                continue;
            }
            sampleBundles.push(bundle);
            log('Sampled ' + bundle.properties.length + ' propert' +
                (bundle.properties.length === 1 ? 'y' : 'ies') + ' for ' +
                batch.label + '.');
            spatialBlocker = _unifiedSpatialSolveBlocker(bundle);
            if (spatialBlocker) {
                log('SKIP [spatial]: ' + spatialBlocker);
                alert(spatialBlocker);
                setStatus('Idle', 0, phaseStart);
                _cleanCancelFile();
                return;
            }
            phaseStart = setStatus('Grouping by tolerance', 10, phaseStart);

            var tolGroups = _buildToleranceGroups(bundle);
            var groupIdx = 0;
            var tolGroupTotal = _countMapKeys(tolGroups);
            var tolStr, group, groupIds, filteredBundle, inPath, outPath, keyBundle;

            for (tolStr in tolGroups) {
                if (!Object.prototype.hasOwnProperty.call(tolGroups, tolStr)) { continue; }

                // Check cancel between groups.
                if (_isCancelled()) {
                    log('Cancelled by user (between tolerance groups).');
                    _cleanCancelFile();
                    return;
                }

                group = tolGroups[tolStr];
                groupIds = group.groupIds;
                var tol  = parseFloat(group.tolStr);
                var tolPx = parseFloat(group.tolPxStr);

                // Filter bundle to this group.
                filteredBundle = _filterBundleForIds(
                    bundle, groupIds, tol, tolPx, group.solveMode,
                    parseFloat(group.motionSmoothTolStr),
                    group.motionSmoothBezier, group.motionSmoothUseEase,
                    group.motionSmoothSourceFidelity);
                if (!motionSmoothPathWarningShown &&
                        _hasMotionSmoothPathProperties(filteredBundle)) {
                    if (!_confirmMotionSmoothPathWarning(filteredBundle)) {
                        log('Bake cancelled by user (Motion Smooth path warning).');
                        setStatus('Cancelled (motion smooth warning)', 0, phaseStart);
                        _cleanCancelFile();
                        return;
                    }
                    motionSmoothPathWarningShown = true;
                }

                groupIdx = groupIdx + 1;
                inPath  = scratchDir + '/' + requestId + '_s' +
                    (batchIdx + 1) + '_g' + groupIdx + '.bbsm.json';
                outPath = scratchDir + '/' + requestId + '_s' +
                    (batchIdx + 1) + '_g' + groupIdx + '.bbky.json';
                var segmentTotal = Math.max(1, segmentBatches.length);
                var segmentStart = 12 + Math.round(
                    (batchIdx / segmentTotal) * 68);
                var segmentEnd = 12 + Math.round(
                    ((batchIdx + 1) / segmentTotal) * 68);
                var segmentSpan = Math.max(1, segmentEnd - segmentStart);
                var solveProgressStart = segmentStart + Math.round(
                    ((groupIdx - 1) / Math.max(1, tolGroupTotal)) * segmentSpan);
                var solveProgressEnd = segmentStart + Math.round(
                    (groupIdx / Math.max(1, tolGroupTotal)) * segmentSpan);

                try { writeSampleBundleJson(filteredBundle, inPath); }
                catch (we) { log('ERROR (write): ' + we.message); continue; }

                phaseStart = setStatus(
                    'Segment ' + (batchIdx + 1) + ', group ' + groupIdx +
                    ': wrote sample bundle',
                    solveProgressStart, phaseStart);

                // Warn once per group when shape_flat (path) properties are present.
                var sfi, hasShapeFlatInGroup = false;
                for (sfi = 0; sfi < filteredBundle.properties.length; sfi++) {
                    if (filteredBundle.properties[sfi].property &&
                            filteredBundle.properties[sfi].property.units_label === 'shape_flat') {
                        hasShapeFlatInGroup = true;
                        break;
                    }
                }
                if (hasShapeFlatInGroup) {
                    log('NOTE [path]: path solve can take 30 s - several minutes. ' +
                        'Progress bar updates as the solver emits phase events.');
                }

                // Retry loop.
                var maxAttempts = 3;
                var attempt;
                for (attempt = 0; attempt < maxAttempts; attempt++) {
                    phaseStart = setStatus(
                        'Segment ' + (batchIdx + 1) + ', group ' + groupIdx +
                        ': solver attempt ' + (attempt + 1) +
                        ' (tol=' + tol + ', ' + groupIds.length + ' props)',
                        solveProgressStart, phaseStart);

                    try {
                        keyBundle = _runBbsolver(
                            bbsolver, inPath, outPath, tol, tolPx,
                            solveProgressStart, solveProgressEnd,
                            filteredBundle.config);
                    }
                    catch (be) {
                        log('ERROR (bbsolver): ' + be.message);
                        alert('bbsolver failed:\n' + be.message);
                        _cleanCancelFile();
                        return;
                    }

                    // bbsolver exited 5 (cancelled) or user flagged cancel.
                    if (_isCancelled()) {
                        log('Cancelled by user (bbsolver exit or cancel flag).');
                        _cleanCancelFile();
                        return;
                    }

                    _logKeyBundleSummary(keyBundle, filteredBundle);
                    phaseStart = setStatus(
                        'Segment ' + (batchIdx + 1) + ', group ' + groupIdx +
                        ': parsed solver output',
                        solveProgressEnd, phaseStart);

                    // Convergence check.
                    var failedResults = [];
                    var kr;
                    for (kr = 0; kr < keyBundle.property_results.length; kr++) {
                        if (keyBundle.property_results[kr].converged === false) {
                            failedResults.push(keyBundle.property_results[kr]);
                        }
                    }
                    if (failedResults.length > 0) {
                        var errAction = _showConvergenceErrorDialog(
                            failedResults, keyBundle.property_results,
                            _propertyLabelMap(filteredBundle));
                        if (errAction === 'skipAll') {
                            log('Skipping (user cancelled after convergence failures).');
                            setStatus('Cancelled (convergence)', 0, phaseStart);
                            _cleanCancelFile();
                            return;
                        }
                        if (errAction === 'loosen') {
                            tol = tol * 2;
                            log('Retrying with tolerance ' + tol);
                            continue;
                        }
                        // convergedOnly: keep only converged results.
                        var convergedOnly = [];
                        for (kr = 0; kr < keyBundle.property_results.length; kr++) {
                            if (keyBundle.property_results[kr].converged !== false) {
                                convergedOnly.push(keyBundle.property_results[kr]);
                            }
                        }
                        keyBundle.property_results = convergedOnly;
                    }

                    // Preview dialog.
                    if (settings.showPreview) {
                        var previewResult = _showPreviewDialog(filteredBundle, keyBundle, tol, tolPx);
                        if (previewResult.action === 'cancel') {
                            log('Cancelled (preview).');
                            setStatus('Cancelled (preview)', 0, phaseStart);
                            _cleanCancelFile();
                            return;
                        }
                        if (previewResult.action === 'resolveAll') {
                            tol = tol * 0.5;
                            log('Re-solving with tolerance ' + tol);
                            continue;
                        }
                        keyBundle.property_results = previewResult.filteredResults;
                    }

                    break;
                }

                _attachWriteRangesToPropertyResults(
                    keyBundle.property_results, batch);

                var mr;
                for (mr = 0; mr < keyBundle.property_results.length; mr++) {
                    mergedResults.push(keyBundle.property_results[mr]);
                }
            }
        }

        var mergedBundle = _mergeSampleBundlesForSegments(sampleBundles, requestId);
        var cleanupOutcome = _runPromptedCleanupPassBlocking(
            comp, bbsolver, mergedBundle, scratchDir, requestId,
            mergedResults, phaseStart);
        mergedResults = cleanupOutcome.results;
        phaseStart = cleanupOutcome.phaseStart;
        _finishMergedBake(comp, mergedBundle, scratchDir, requestId,
            mergedResults, phaseStart);
    }

    // ---- Preview dialog --------------------------------------------------

    function _showPreviewDialog(bundle, keyBundle, tol, tolPx) {
        var results     = keyBundle.property_results;
        var origSamples = bundle.properties;

        var sampleCountMap = {};
        var labelMap = {};
        var i, ps;
        for (i = 0; i < origSamples.length; i++) {
            ps = origSamples[i];
            sampleCountMap[ps.property.id] = ps.samples.length;
            labelMap[ps.property.id] = _friendlyPropertyLabel(ps.property);
        }

        var dlg = new Window('dialog', 'bbsolver-test-harness: Preview Keyframes');
        dlg.orientation = 'column';
        dlg.alignChildren = ['fill', 'top'];
        dlg.spacing = 8;
        var introText = dlg.add('statictext', undefined, 'Review before applying.');

        // Separate diagnostic sub-path entries before building checkboxes.
        // Sub-paths share the source property_id and are not individually
        // selectable; they are applied automatically with their source.
        var subPathEntries = [];
        var displayResults = [];
        var sp;
        for (sp = 0; sp < results.length; sp++) {
            if (_isSubPathEntry(results[sp])) {
                subPathEntries.push(results[sp]);
            } else {
                displayResults.push(results[sp]);
            }
        }

        var compactPreview = displayResults.length > 12;
        if (compactPreview) {
            introText.text = 'Review before applying. Deselect rows to skip.';
        } else {
            introText.text = 'Review before applying. Uncheck to skip a property.';
        }

        var checkboxes = [];
        var compactList = null;
        var compactFallbackMode = 'all';
        if (compactPreview) {
            var listPanel = dlg.add('panel', undefined, 'Properties');
            listPanel.orientation = 'column';
            listPanel.alignChildren = ['fill', 'top'];
            listPanel.margins = [8, 8, 8, 8];
            listPanel.add('statictext', undefined,
                'Large bake compact view. Selected rows will be applied.');
            compactList = listPanel.add('listbox', undefined, [], { multiselect: true });
            compactList.preferredSize = [760, 300];
            compactList.alignment = ['fill', 'top'];
            compactList.helpTip = 'Command-click to toggle individual rows; Shift-click for a range.';
            compactList.onChange = function () {
                compactFallbackMode = 'selection';
            };
        }

        function previewRowText(label, keyCount, sourceCount, err, errPx, st, summary) {
            var text = label +
                ' | keys ' + keyCount + ' / was ' + sourceCount +
                ' | err ' + err + ' u, ' + errPx + ' px' +
                ' | ' + st;
            if (summary) { text += ' | ' + summary; }
            return text;
        }

        var j, pk, origCount, maxErr, maxErrPx, status, gateMsg;
        var vertexGateMsg, vertexSummaryMsg, advisoryMsg, smoothMsg;
        for (j = 0; j < displayResults.length; j++) {
            pk        = displayResults[j];
            origCount = sampleCountMap[pk.property_id] || '?';
            maxErr    = (typeof pk.max_err          === 'number') ? pk.max_err.toFixed(4)          : '?';
            maxErrPx  = (typeof pk.max_err_screen_px === 'number') ? pk.max_err_screen_px.toFixed(3) : '?';
            status    = (pk.converged !== false) ? 'converged' : 'FAILED ***';
            vertexSummaryMsg = _pathVertexSummaryMessage(pk.notes || '');
            advisoryMsg = _pathOptimizationAdvisoryMessage(pk.notes || '');
            smoothMsg = _motionSmoothResultMessage(pk.notes || '');

            if (compactPreview) {
                var item = compactList.add('item',
                    previewRowText(
                        _propertyLabelForId(pk.property_id, labelMap),
                        pk.keys.length,
                        origCount,
                        maxErr,
                        maxErrPx,
                        status,
                        vertexSummaryMsg || advisoryMsg || smoothMsg));
                item.pk = pk;
                try { item.selected = true; } catch (eSelectItem) {}
                checkboxes.push({ item: item, pk: pk });
                continue;
            }

            var rowPanel = dlg.add('panel');
            rowPanel.orientation = 'column';
            rowPanel.alignChildren = ['fill', 'top'];
            rowPanel.margins = [8, 4, 8, 4];

            var applyCheck = rowPanel.add('checkbox', undefined,
                'Property: ' + _propertyLabelForId(pk.property_id, labelMap));
            applyCheck.value = true;
            checkboxes.push({ check: applyCheck, pk: pk });

            rowPanel.add('statictext', undefined,
                '  Keys: ' + pk.keys.length + '  (was: ' + origCount + ' samples)');
            rowPanel.add('statictext', undefined,
                '  Max error: ' + maxErr + ' units  (' + maxErrPx + ' screen px)');
            rowPanel.add('statictext', undefined, '  Status: ' + status);
            if (vertexSummaryMsg) {
                rowPanel.add('statictext', undefined, '  ' + vertexSummaryMsg);
            }
            if (advisoryMsg) {
                rowPanel.add('statictext', undefined, '  ' + advisoryMsg);
            }
            if (smoothMsg) {
                rowPanel.add('statictext', undefined, '  ' + smoothMsg);
            }
            gateMsg = _optimizationAccuracyGateMessage(pk.notes || '');
            if (gateMsg) {
                rowPanel.add('statictext', undefined, '  ' + gateMsg);
            }
            vertexGateMsg = _vertexOptimizationGateMessage(pk.notes || '');
            if (vertexGateMsg) {
                rowPanel.add('statictext', undefined, '  ' + vertexGateMsg);
            }
        }

        if (subPathEntries.length > 0) {
            dlg.add('statictext', undefined,
                '  (' + subPathEntries.length +
                ' diagnostic sub-path(s) will be created adjacent to the source.)');
        }

        var btnGrp = dlg.add('group');
        btnGrp.alignment = 'right';
        var selectAllBtn = null;
        var clearAllBtn = null;
        if (compactPreview) {
            selectAllBtn = btnGrp.add('button', undefined, 'Select All');
            clearAllBtn  = btnGrp.add('button', undefined, 'Clear');
        }
        var applyBtn   = btnGrp.add('button', undefined, 'Apply Selected', { name: 'ok' });
        var resolveBtn = btnGrp.add('button', undefined, 'Re-solve (\xF72 tol)');
        var cancelBtn  = btnGrp.add('button', undefined, 'Cancel All',     { name: 'cancel' });

        var dialogAction    = 'cancel';
        var filteredResults = [];

        function compactItemSelected(item) {
            if (!compactList || !item) { return false; }
            try {
                if (item.selected === true) { return true; }
            } catch (eItemSelected) {}
            try {
                var sel = compactList.selection;
                if (!sel) { return false; }
                if (typeof sel.length === 'number') {
                    var si;
                    for (si = 0; si < sel.length; si++) {
                        if (sel[si] === item) { return true; }
                    }
                    return false;
                }
                return sel === item;
            } catch (eListSelection) {}
            return false;
        }

        applyBtn.onClick = function () {
            dialogAction = 'apply';
            filteredResults = [];
            var k;
            for (k = 0; k < checkboxes.length; k++) {
                if ((checkboxes[k].check && checkboxes[k].check.value) ||
                        (checkboxes[k].item &&
                         compactItemSelected(checkboxes[k].item))) {
                    filteredResults.push(checkboxes[k].pk);
                }
            }
            if (compactPreview && filteredResults.length === 0 &&
                    compactFallbackMode === 'all') {
                for (k = 0; k < checkboxes.length; k++) {
                    filteredResults.push(checkboxes[k].pk);
                }
            }
            // Include diagnostic sub-paths whose source property was selected.
            var selIds = {};
            for (k = 0; k < filteredResults.length; k++) {
                selIds[filteredResults[k].property_id] = true;
            }
            for (k = 0; k < subPathEntries.length; k++) {
                if (selIds[subPathEntries[k].property_id]) {
                    filteredResults.push(subPathEntries[k]);
                }
            }
            dlg.close(1);
        };
        if (selectAllBtn) {
            selectAllBtn.onClick = function () {
                compactFallbackMode = 'all';
                var si;
                for (si = 0; si < checkboxes.length; si++) {
                    if (checkboxes[si].item) {
                        try { checkboxes[si].item.selected = true; } catch (eSelAll) {}
                    }
                }
            };
        }
        if (clearAllBtn) {
            clearAllBtn.onClick = function () {
                compactFallbackMode = 'none';
                var ci;
                for (ci = 0; ci < checkboxes.length; ci++) {
                    if (checkboxes[ci].item) {
                        try { checkboxes[ci].item.selected = false; } catch (eClearAll) {}
                    }
                }
            };
        }
        resolveBtn.onClick = function () { dialogAction = 'resolveAll'; dlg.close(2); };
        cancelBtn.onClick  = function () { dialogAction = 'cancel';     dlg.close(0); };

        dlg.show();
        return { action: dialogAction, filteredResults: filteredResults };
    }

    // ---- Results dialog --------------------------------------------------

    function _showResultsDialog(
            writeResult, vResult, totalKeys, labelMap, sourceBundle, keyBundle) {
        var dlg = new Window('dialog', 'bbsolver-test-harness: Bake Results');
        dlg.orientation = 'column';
        dlg.alignChildren = ['fill', 'top'];

        dlg.add('statictext', undefined,
            'Applied: ' + writeResult.applied + ' properties | Total keys: ' + totalKeys);

        var cleanupLines = _cleanupPassSummaryLines(sourceBundle, keyBundle, labelMap);
        if (cleanupLines.length > 0) {
            dlg.add('statictext', undefined, 'Bake result summary:');
            var cleanupTxt = dlg.add('edittext', undefined, '',
                { multiline: true, scrolling: true });
            cleanupTxt.preferredSize = [
                560,
                Math.min(140, Math.max(54, cleanupLines.length * 26))
            ];
            cleanupTxt.text = cleanupLines.join('\n');
        }

        if (vResult) {
            var propVerifyOk = true;
            var vi;
            for (vi = 0; vi < vResult.properties.length; vi++) {
                if (!vResult.properties[vi].ok) { propVerifyOk = false; }
            }
            dlg.add('statictext', undefined,
                'Round-trip verify: ' + (propVerifyOk ? 'PASS' : 'FAIL') +
                ' | Overall: ' + (vResult.overall_ok ? 'PASS' : 'FAIL'));
            var verTxt = dlg.add('edittext', undefined, '',
                { multiline: true, scrolling: true });
            verTxt.preferredSize = [440, 120];
            var lines = [];
            var i, pv;
            for (i = 0; i < vResult.properties.length; i++) {
                pv = vResult.properties[i];
                lines.push((pv.ok ? 'OK  ' : 'FAIL') + '  ' +
                    _propertyLabelForId(pv.id, labelMap) +
                    '  max_err=' + pv.max_err.toFixed(4) +
                    (!pv.ok ? '  worst_t=' + pv.worst_t_sec.toFixed(4) : '') +
                    '  checked=' + (pv.samples_checked || 0));
            }
            if (vResult.rig_gaps &&
                vResult.rig_gaps.pairs &&
                vResult.rig_gaps.pairs.length > 0) {
                lines.push('');
                lines.push('Rig gaps: ' +
                    (vResult.rig_gaps.overall_ok ? 'PASS' : 'FAIL') +
                    '  pairs=' + vResult.rig_gaps.pairs.length +
                    '  tol=' + vResult.rig_gaps.tolerance);
                var gi, gp;
                for (gi = 0; gi < vResult.rig_gaps.pairs.length; gi++) {
                    gp = vResult.rig_gaps.pairs[gi];
                    lines.push((gp.ok ? 'OK  ' : 'FAIL') + '  ' +
                        _propertyLabelForId(gp.id_a, labelMap) + ' <> ' +
                        _propertyLabelForId(gp.id_b, labelMap) +
                        '  gap_delta=' + gp.max_gap_delta.toFixed(4) +
                        (!gp.ok ? '  worst_t=' + gp.worst_t_sec.toFixed(4) : ''));
                }
            }
            verTxt.text = lines.join('\n');
        }

        // Shape key structure summary -- shown whenever shape paths were baked.
        // Reports PASS when all structure checks passed, or FAIL with a count of
        // how many properties had interp/ease mismatches after writeback.
        if (writeResult.shape_key_structure && writeResult.shape_key_structure.length > 0) {
            var sksTotal = writeResult.shape_key_structure.length;
            var sksFail  = 0;
            var si;
            for (si = 0; si < writeResult.shape_key_structure.length; si++) {
                if (!writeResult.shape_key_structure[si].ok) { sksFail = sksFail + 1; }
            }
            var sksLabel;
            if (sksFail === 0) {
                // Show Bezier/Linear distribution so user can confirm non-linear
                // ease from the solver actually reached AE (not just neutral 33.3).
                var dlgBez = 0, dlgLin = 0, dlgHold = 0, dsi;
                for (dsi = 0; dsi < writeResult.shape_key_structure.length; dsi++) {
                    var dms = writeResult.shape_key_structure[dsi].interp_summary;
                    if (dms) {
                        dlgBez  = dlgBez  + (dms.bezier_count || 0);
                        dlgLin  = dlgLin  + (dms.linear_count || 0);
                        dlgHold = dlgHold + (dms.hold_count   || 0);
                    }
                }
                sksLabel = 'Shape key structure: PASS -- ' +
                    dlgBez + ' Bezier / ' + dlgLin + ' Linear' +
                    (dlgHold > 0 ? ' / ' + dlgHold + ' Hold' : '');
            } else {
                sksLabel = 'Shape key structure: FAIL (' + sksFail + '/' + sksTotal + ' props)';
            }
            dlg.add('statictext', undefined, sksLabel);
        }

        if (writeResult.multi_path_count && writeResult.multi_path_count > 0) {
            var dlgDiagCount = writeResult.diagnostic_subpath_count || 0;
            var dlgVisCount  = writeResult.visible_subpath_count    || 0;
            if (dlgDiagCount > 0) {
                dlg.add('statictext', undefined,
                    'Landmark sub-paths (diagnostic-only): ' + dlgDiagCount +
                    ' adjacent bb_lm_N paths created.');
            }
            if (dlgVisCount > 0) {
                dlg.add('statictext', undefined,
                    'Visible sub-paths: ' + dlgVisCount +
                    ' adjacent bb_vc_N paths created (render visibly).');
            }
        }

        if (writeResult.errors && writeResult.errors.length > 0) {
            dlg.add('statictext', undefined, 'Errors:');
            var errTxt = dlg.add('edittext', undefined, '',
                { multiline: true, scrolling: true });
            errTxt.preferredSize = [440, 60];
            var errLines = [];
            var ei;
            for (ei = 0; ei < writeResult.errors.length; ei++) {
                errLines.push(_propertyLabelForId(writeResult.errors[ei].id, labelMap) +
                    ': ' + writeResult.errors[ei].msg);
            }
            errTxt.text = errLines.join('\n');
        }

        dlg.add('button', undefined, 'OK', { name: 'ok' });
        dlg.show();
    }

    // ---- Actions ---------------------------------------------------------

    function doSample() {
        if (_cooperativeBakeActive) {
            alert('bbsolver-test-harness: a bake is already running.');
            return;
        }
        clearLog();
        var comp = getActiveComp();
        if (!comp) { alert('bbsolver-test-harness: no active composition.'); return; }
        var props = getSelectedProperties(comp);
        if (props.length === 0) {
            alert('bbsolver-test-harness: select at least one animatable property in the Timeline.');
            return;
        }
        var segmentBatches = _buildBakeBatches(comp, props);
        if (!segmentBatches || segmentBatches.length === 0) {
            alert('bbsolver-test-harness: segment mode found no bbsolver marker ranges on the selected property layers.');
            return;
        }

        var bundles = [];
        var bi, batch, bundle, rangeOverride;
        for (bi = 0; bi < segmentBatches.length; bi++) {
            batch = segmentBatches[bi];
            setStatus('Sampling AE properties: ' + batch.label, 5);
            log('INFO [segment]: sampling ' + batch.label +
                ' (' + batch.props.length + ' prop' +
                (batch.props.length === 1 ? '' : 's') + ').');
            rangeOverride = { tStart: batch.tStart, tEnd: batch.tEnd };
            try {
                bundle = _sampleOrReuseForBake(comp, batch.props, undefined,
                    rangeOverride);
            }
            catch (e) {
                log('ERROR: ' + e.message);
                alert('Sample failed:\n' + e.message);
                return;
            }
            if (bundle && bundle.properties.length > 0) {
                bundles.push(bundle);
            } else {
                log('No bakeable properties in ' + batch.label + '; skipping.');
            }
        }

        bundle = _mergeSampleBundlesForSegments(
            bundles, 'sample-' + (new Date().getTime()));
        if (!bundle || bundle.properties.length === 0) {
            log('No bakeable properties after filtering - nothing sampled.');
            return;
        }
        _lastBundle = bundle;
        _lastBundleSignature = 'merged-segment-sample:' +
            _sampleCacheSignature(comp, props, undefined, _timeRange(comp));
        _refreshPropList();

        var totalSampleCount = 0;
        var pi;
        for (pi = 0; pi < bundle.properties.length; pi++) {
            totalSampleCount += bundle.properties[pi].samples.length;
        }
        log('Sampled ' + bundle.properties.length + ' propert' +
            (bundle.properties.length === 1 ? 'y' : 'ies') + ', ' +
            totalSampleCount + ' total samples.');

        var sampleId = 'sample-' + (new Date().getTime());
        var sampleDir = _runScratchDir(_findBbsolver(), sampleId);
        var outPath = sampleDir + '/' + sampleId + '.bbsm.json';
        try { writeSampleBundleJson(bundle, outPath); }
        catch (e) {
            log('ERROR: ' + e.message);
            alert('Write failed:\n' + e.message);
            return;
        }
        log('Wrote: ' + outPath);
    }

    function doSolveAndBake() {
        if (_cooperativeBakeActive) {
            alert('bbsolver-test-harness: a bake is already running.');
            return;
        }
        clearLog();
        var comp = getActiveComp();
        if (!comp) { alert('bbsolver-test-harness: no active composition.'); return; }
        var props = getSelectedProperties(comp);
        if (props.length === 0) {
            alert('bbsolver-test-harness: select at least one animatable property in the Timeline.');
            return;
        }
        _runBake(comp, props);
    }

    // ---- Settings dialog -------------------------------------------------

    function openSettings() {
        if (_cooperativeBakeActive) {
            alert('bbsolver-test-harness: settings are locked while a bake is running.');
            return;
        }
        var dlg = new Window('dialog', 'bbsolver-test-harness Settings');
        dlg.orientation = 'column';
        dlg.alignChildren = ['fill', 'top'];
        dlg.spacing = 6;

        var settingsTabs = dlg.add('tabbedpanel');
        settingsTabs.alignChildren = ['fill', 'top'];
        settingsTabs.preferredSize = [620, 640];
        var coreTab = settingsTabs.add('tab', undefined, 'Core');
        var spatialTab = settingsTabs.add('tab', undefined, 'Spatial');
        var motionTab = settingsTabs.add('tab', undefined, 'Motion / Path');
        coreTab.orientation = 'column';
        spatialTab.orientation = 'column';
        motionTab.orientation = 'column';
        coreTab.alignChildren = ['fill', 'top'];
        spatialTab.alignChildren = ['fill', 'top'];
        motionTab.alignChildren = ['fill', 'top'];
        settingsTabs.selection = coreTab;
        settingsTabs.onChange = function () {
            try { dlg.layout.layout(true); } catch (tabLayoutErr) {}
        };

        // Tolerance
        var tolGrp = coreTab.add('group');
        tolGrp.add('statictext', undefined, 'Linf tolerance (property units):');
        var tolEdit = tolGrp.add('edittext', undefined, String(settings.tolerance));
        tolEdit.preferredSize.width = 80;
        tolEdit.helpTip = 'Maximum allowed numeric error in the property value. Smaller values preserve motion more strictly and may produce more keys.';

        // Screen px
        var tolPxGrp = coreTab.add('group');
        tolPxGrp.add('statictext', undefined, 'Screen-px tolerance (0 = off):');
        var tolPxEdit = tolPxGrp.add('edittext', undefined, String(settings.toleranceScreenPx));
        tolPxEdit.preferredSize.width = 80;
        tolPxEdit.helpTip = 'Optional visual error gate in comp pixels. Use this for shape paths when property units are hard to interpret; 0 disables it.';

        // bbsolver path
        var bbsolverPanel = coreTab.add('panel', undefined, 'bbsolver');
        bbsolverPanel.orientation = 'column';
        bbsolverPanel.alignChildren = ['fill', 'top'];
        var bdPathGrp = bbsolverPanel.add('group');
        bdPathGrp.add('statictext', undefined, 'Path (empty = auto):');
        var bdEdit = bdPathGrp.add('edittext', undefined, settings.bbsolverPath || '');
        bdEdit.preferredSize.width = 200;
        bdEdit.helpTip = 'Path to the bbsolver binary. Leave empty to use BBSOLVER_BIN or the default macOS/Windows install locations.';
        var browseBtn = bdPathGrp.add('button', undefined, 'Browse...');
        browseBtn.helpTip = 'Choose a custom bbsolver executable.';
        browseBtn.onClick = function () {
            var chosen = File.openDialog('Select bbsolver executable');
            if (chosen) { bdEdit.text = chosen.fsName; }
        };
        var jobsGrp = bbsolverPanel.add('group');
        jobsGrp.add('statictext', undefined, 'Solver jobs:');
        var jobsEdit = jobsGrp.add('edittext', undefined,
            String(typeof settings.solverJobs === 'number' ? settings.solverJobs : 0));
        jobsEdit.preferredSize.width = 60;
        jobsEdit.helpTip = '0 = auto. Positive values cap bbsolver parallel workers. Use 1 for serial fallback when troubleshooting; the solver also applies a hard safety cap.';
        var logPathGrp = bbsolverPanel.add('group');
        logPathGrp.add('statictext', undefined, 'Log folder (empty = Desktop):');
        var logPathEdit = logPathGrp.add('edittext', undefined,
            settings.logOutputDir || '');
        logPathEdit.preferredSize.width = 200;
        logPathEdit.helpTip = 'Folder for exported harness logs. Empty writes to Desktop/bbsolver-test-harness-logs.';
        var browseLogBtn = logPathGrp.add('button', undefined, 'Browse...');
        browseLogBtn.helpTip = 'Choose where exported harness logs are written.';
        browseLogBtn.onClick = function () {
            var chosen = Folder.selectDialog('Choose log output folder');
            if (chosen) { logPathEdit.text = chosen.fsName; }
        };

        // Time range
        var rangePanel = coreTab.add('panel', undefined, 'Time Range');
        rangePanel.orientation = 'column';
        rangePanel.alignChildren = ['fill', 'top'];
        var waCheck = rangePanel.add('checkbox', undefined, 'Use work area');
        waCheck.value = settings.useWorkArea;
        waCheck.helpTip = 'Bake only the comp work area. Turn this off to use the manual start/end seconds below.';
        var segmentMarkerCheck = rangePanel.add('checkbox', undefined,
            'Use bbsolver layer markers as bake ranges');
        segmentMarkerCheck.value = (settings.useSegmentMarkers === true);
        segmentMarkerCheck.helpTip = 'When enabled, the harness scans selected property layers for bbsolver marker ranges. Marker duration defines the range; zero-duration markers run until the next matching marker or the current time-range end.';
        var manualGrp = rangePanel.add('group');
        manualGrp.add('statictext', undefined, 'Start (sec):');
        var startEdit = manualGrp.add('edittext', undefined, String(settings.startSec));
        startEdit.preferredSize.width = 60;
        startEdit.helpTip = 'Manual bake start time in seconds, used only when Use work area is off.';
        manualGrp.add('statictext', undefined, 'End (sec):');
        var endEdit = manualGrp.add('edittext', undefined, String(settings.endSec));
        endEdit.preferredSize.width = 60;
        endEdit.helpTip = 'Manual bake end time in seconds, used only when Use work area is off.';
        function syncManual() {
            startEdit.enabled = !waCheck.value;
            endEdit.enabled   = !waCheck.value;
        }
        syncManual();
        waCheck.onClick = syncManual;

        // Expression handling
        var sepPanel = spatialTab.add('panel', undefined, 'Spatial Properties');
        sepPanel.orientation = 'column';
        sepPanel.alignChildren = ['fill', 'top'];
        var sepCheck = sepPanel.add('checkbox', undefined,
            'Auto-separate unkeyed spatial props (changes Position into X/Y/Z axes)');
        sepCheck.value = (settings.autoSeparateForBake === true);
        sepCheck.helpTip = 'For unkeyed Position properties, the harness can separate axes so each axis can be optimized independently.';
        var flattenCheck = sepPanel.add('checkbox', undefined,
            'Flatten parented Position on 2D layers to comp space and unparent');
        flattenCheck.value = (settings.flattenParentedPosition === true);
        flattenCheck.helpTip = 'Samples parented 2D Position in comp space and writes baked comp-space keys. Use when parent transforms are part of the motion you want baked.';
        var preserveParentCheck = sepPanel.add('checkbox', undefined,
            'Preserve parenting between selected layers');
        preserveParentCheck.value = (settings.preserveSelectedParenting === true);
        preserveParentCheck.helpTip = 'When parent flatten is on, only unparents selected layers from unselected parents. If a selected layer is parented to another selected layer, the harness keeps that link and bakes local values for the child.';
        var flattenTolGrp = sepPanel.add('group');
        flattenTolGrp.add('statictext', undefined, 'Parent-flatten tolerance:');
        var flattenTolEdit = flattenTolGrp.add('edittext', undefined,
            String(settings.flattenParentedTolerance || 0.05));
        flattenTolEdit.preferredSize.width = 80;
        flattenTolEdit.helpTip = 'Tolerance for parent-flattened Position solves. Keep this low because small rig offsets are visible.';
        var rigRotTolGrp = sepPanel.add('group');
        rigRotTolGrp.add('statictext', undefined, 'Rig rotation tolerance (deg):');
        var rigRotTolEdit = rigRotTolGrp.add('edittext', undefined,
            String(settings.rigRotationTolerance || 0.01));
        rigRotTolEdit.preferredSize.width = 80;
        rigRotTolEdit.helpTip = 'Allowed rotation error in degrees for rig-related rotation checks.';
        function syncFlattenTol() {
            flattenTolEdit.enabled = flattenCheck.value;
            preserveParentCheck.enabled = flattenCheck.value;
            rigRotTolEdit.enabled = true;
        }
        syncFlattenTol();
        flattenCheck.onClick = syncFlattenTol;

        var verifyPanel = spatialTab.add('panel', undefined, 'Troubleshooting Verify');
        verifyPanel.orientation = 'column';
        verifyPanel.alignChildren = ['fill', 'top'];
        var verifyRoundTripCheck = verifyPanel.add('checkbox', undefined,
            'Run round-trip verify after apply (debug; slower)');
        verifyRoundTripCheck.value = (settings.verifyRoundTrip === true);
        verifyRoundTripCheck.helpTip = 'Debug option: after applying keys, re-sample AE valueAtTime() and write a verify report. Default is off because normal bakes use solver-side validation before writeback.';
        verifyRoundTripCheck.onClick = function () {};

        var exprPanel = spatialTab.add('panel', undefined, 'Expression Handling');
        exprPanel.orientation = 'column';
        exprPanel.alignChildren = ['fill', 'top'];
        var disCheck   = exprPanel.add('checkbox', undefined,
            'Disable expression after bake (keep text)');
        disCheck.value = settings.disableExpression;
        disCheck.helpTip = 'Turns off the original expression after applying keys, while leaving the expression text in place for reference.';
        // UI options
        var uiPanel = spatialTab.add('panel', undefined, 'UI');
        uiPanel.orientation = 'column';
        uiPanel.alignChildren = ['fill', 'top'];
        var previewCheck = uiPanel.add('checkbox', undefined,
            'Show preview dialog before applying keyframes');
        previewCheck.value = settings.showPreview;
        previewCheck.helpTip = 'Shows the key count/error summary before the harness writes keys back into AE.';

        var expPanel = motionTab.add('panel', undefined, 'Motion and Path Options');
        expPanel.orientation = 'column';
        expPanel.alignChildren = ['fill', 'top'];
        var solveModeGrp = expPanel.add('group');
        solveModeGrp.add('statictext', undefined, 'Optimization mode:');
        var solveModeDDL = solveModeGrp.add('dropdownlist', undefined,
            _solveModeLabels);
        solveModeDDL.preferredSize.width = 190;
        solveModeDDL.helpTip = 'Full allows temporal key reduction and selected vertex reducers. Temporal only suppresses vertex reducers. Vertex only requests guarded path vertex reduction. Motion smooth normalizes keyed motion. Motion path smooth is for spatial Position paths that need trajectory smoothing with optional sharp/keyed locks.';
        var solveModeMap = _solveModeValues;
        var currentSolveModeIdx = 0;
        var smi;
        for (smi = 0; smi < solveModeMap.length; smi++) {
            if (solveModeMap[smi] ===
                    _normalizeSolveOptimizationMode(settings.solveOptimizationMode)) {
                currentSolveModeIdx = smi;
                break;
            }
        }
        solveModeDDL.selection = currentSolveModeIdx;
        var motionSmoothEaseCheck = expPanel.add('checkbox', undefined,
            'Ease / rove Motion Smooth keys');
        motionSmoothEaseCheck.value = (settings.motionSmoothUseEase === true);
        motionSmoothEaseCheck.helpTip = 'Motion Smooth only: enables temporal auto-Bezier/roving on intermediate smoothed spatial keys for steadier speed.';
        var motionSmoothSourceFidelityCheck = expPanel.add('checkbox', undefined,
            'Preserve source key poses during Motion Smooth');
        motionSmoothSourceFidelityCheck.value =
            (settings.motionSmoothSourceFidelity === true);
        motionSmoothSourceFidelityCheck.helpTip = 'Motion Smooth only: keeps the original source key times and path poses as hard constraints, then generates smoothed in-between path keys. This gives extra fidelity at keyed poses without requiring extra/bounding keyframes.';
        var motionSmoothTolGrp = expPanel.add('group');
        motionSmoothTolGrp.add('statictext', undefined, 'Motion smooth tolerance:');
        var motionSmoothTolEdit = motionSmoothTolGrp.add('edittext', undefined,
            String(_effectiveMotionSmoothTolerance('')));
        motionSmoothTolEdit.preferredSize.width = 64;
        motionSmoothTolEdit.helpTip = 'Motion Smooth only: higher values make the spatial path smoother and allow more smoothed-point decimation. This is not the solve accuracy tolerance.';
        var motionSmoothCleanupTolGrp = expPanel.add('group');
        motionSmoothCleanupTolGrp.add('statictext', undefined,
            'Motion smooth cleanup tol:');
        var motionSmoothCleanupTolEdit = motionSmoothCleanupTolGrp.add(
            'edittext', undefined,
            String(_motionSmoothTemporalCleanupTolerance()));
        motionSmoothCleanupTolEdit.preferredSize.width = 64;
        motionSmoothCleanupTolEdit.helpTip = 'Post-Motion-Smooth temporal cleanup only: max error used when reducing the smoothed path keys before writeback.';
        var motionSmoothBezier = _effectiveMotionSmoothBezier('');
        var motionSmoothBezierGrp = expPanel.add('group');
        motionSmoothBezierGrp.add('statictext', undefined, 'Motion smooth ease x1,y1,x2,y2:');
        var motionSmoothBezierX1Edit = motionSmoothBezierGrp.add('edittext', undefined,
            String(motionSmoothBezier.x1));
        var motionSmoothBezierY1Edit = motionSmoothBezierGrp.add('edittext', undefined,
            String(motionSmoothBezier.y1));
        var motionSmoothBezierX2Edit = motionSmoothBezierGrp.add('edittext', undefined,
            String(motionSmoothBezier.x2));
        var motionSmoothBezierY2Edit = motionSmoothBezierGrp.add('edittext', undefined,
            String(motionSmoothBezier.y2));
        motionSmoothBezierX1Edit.preferredSize.width = 46;
        motionSmoothBezierY1Edit.preferredSize.width = 46;
        motionSmoothBezierX2Edit.preferredSize.width = 46;
        motionSmoothBezierY2Edit.preferredSize.width = 46;
        motionSmoothBezierGrp.helpTip = 'Motion Smooth only: cubic Bezier timing curve applied to existing source key times when easing is enabled.';
        var motionPathTolGrp = expPanel.add('group');
        motionPathTolGrp.add('statictext', undefined, '\u03b1 Smooth:');
        var motionPathSmoothTolEdit = motionPathTolGrp.add('edittext', undefined,
            String(_motionPathSmoothingTolerance()));
        motionPathSmoothTolEdit.preferredSize.width = 64;
        motionPathSmoothTolEdit.helpTip =
            'Motion Path Smooth only. Smoothing strength is unitless. ' +
            'Default 3.0; min 1.0; max 32.0. Higher values fair the sampled ' +
            'path farther before key reduction; 32 is aggressive smoothing. ' +
            'This is not an accuracy tolerance.';
        var motionPathBoundsCheck = expPanel.add('checkbox', undefined,
            'Preserve global path bounds');
        motionPathBoundsCheck.value = settings.motionPathPreserveBounds === true;
        motionPathBoundsCheck.helpTip =
            'Motion Path Smooth only. Replaces \u03b5 Fit with \u03b5 Bounds and keeps the smoothed target within the original sampled motion-path bounds.';
        var motionPathAccuracyGrp = expPanel.add('group');
        motionPathAccuracyGrp.add('statictext', undefined, '\u03b5 Fit:');
        var motionPathAccuracyTolEdit = motionPathAccuracyGrp.add('edittext', undefined,
            String(_motionPathAccuracyTolerance()));
        motionPathAccuracyTolEdit.preferredSize.width = 64;
        motionPathAccuracyTolEdit.helpTip =
            'Motion Path Smooth only. Spatiotemporal fit tolerance. Units: comp px for Position. ' +
            'Default 1.5; min 0.1; max 200. Lower values keep more keys closer to the smoothed path. ' +
            'Higher values allow more deviation and usually produce fewer keys.';
        var motionPathBoundsGrp = expPanel.add('group');
        motionPathBoundsGrp.add('statictext', undefined, '\u03b5 Bounds:');
        var motionPathBoundsTolEdit = motionPathBoundsGrp.add('edittext', undefined,
            String(_motionPathBoundsTolerance()));
        motionPathBoundsTolEdit.preferredSize.width = 64;
        motionPathBoundsTolEdit.helpTip =
            'Motion Path Smooth only. Global motion-path bounds tolerance. Units: comp px for Position. ' +
            'Default 0; min 0; max 500. At 0, the smoothed target keeps the original outer bounds. ' +
            'Higher values allow each bounds side to deviate farther from the original path.';
        var motionPathSharpCheck = expPanel.add('checkbox', undefined,
            'Preserve sharp motion-path reversals');
        motionPathSharpCheck.value =
            (settings.motionPathPreserveSharpPoints !== false);
        motionPathSharpCheck.helpTip = 'Motion Path Smooth only: keeps cusp-like direction changes, such as ball bounce impacts, anchored with sharp spatial tangents.';
        var motionPathSharpAngleGrp = expPanel.add('group');
        motionPathSharpAngleGrp.add('statictext', undefined, 'Sharp reversal angle:');
        var motionPathSharpAngleEdit = motionPathSharpAngleGrp.add('edittext', undefined,
            String(_motionPathSharpAngleDeg()));
        motionPathSharpAngleEdit.preferredSize.width = 64;
        motionPathSharpAngleEdit.helpTip =
            'Motion Path Smooth only. Sharp reversal angle threshold. Units: degrees. ' +
            'Default 75; min 1; max 179. Direction changes at or above this value are preserved. ' +
            'Higher values preserve only sharper reversals.';
        var motionPathKeyedCheck = expPanel.add('checkbox', undefined,
            'Respect keyed frames in Motion Path Smooth');
        motionPathKeyedCheck.value =
            (settings.motionPathRespectKeyedFrames === true);
        motionPathKeyedCheck.helpTip = 'Motion Path Smooth only: keeps source key times and poses as hard anchors while still reducing non-keyed samples.';
        var lmCheck = { value: false };
        var shapeTemporalFullGapCheck = { value: false, enabled: false };
        var shapeReplacementCheck = { value: false, enabled: false };
        var shapeReplacementPreferVerticesCheck = expPanel.add('checkbox', undefined,
            'Run second-pass vertex prune after key reduction');
        shapeReplacementPreferVerticesCheck.value =
            (settings.shapeReplacementPreferVertices === true);
        shapeReplacementPreferVerticesCheck.helpTip = 'After the keyframe solve is accepted, tries removing existing path vertices while preserving the same key times under the tolerance gate. This can run without the replacement topology ladder.';
        var cleanupModeGrp = expPanel.add('group');
        cleanupModeGrp.add('statictext', undefined, 'Cleanup pass:');
        var cleanupModeDDL = cleanupModeGrp.add('dropdownlist', undefined, [
            'Prompt before writing',
            'Auto-run before writing',
            'Off'
        ]);
        cleanupModeDDL.preferredSize.width = 190;
        cleanupModeDDL.selection = _cleanupModeIndex(_globalCleanupMode());
        cleanupModeDDL.helpTip = 'After the first solve, optional second solver pass uses accepted path keys as input, avoids AE resampling, and only accepts verified lower-key or lower-vertex replacements.';
        var preserveSharpCornersCheck = expPanel.add('checkbox', undefined,
            'Preserve sharp path corners under loose tolerances');
        preserveSharpCornersCheck.value =
            (settings.preserveSharpPathCorners !== false);
        preserveSharpCornersCheck.helpTip = 'Guards persistent high-angle path corners during replacement topology and second-pass vertex pruning. The lock scales with the active tolerance, so loose solves can still prune smooth bridge vertices without deleting sharp corners.';
        function syncShapeReplacementOptions() {
            var mode = solveModeMap[solveModeDDL.selection
                ? solveModeDDL.selection.index : 0] || 'full';
            var temporalOnly = (mode === 'temporal_only');
            var vertexOnly = (mode === 'vertex_only');
            var motionSmooth = (mode === 'motion_smooth');
            var motionPathSmooth = (mode === 'motion_path_smooth');
            var smoothingMode = motionSmooth || motionPathSmooth;
            shapeTemporalFullGapCheck.enabled = !vertexOnly && !smoothingMode;
            shapeReplacementCheck.enabled = (mode === 'full');
            if (vertexOnly) {
                shapeReplacementPreferVerticesCheck.value = true;
            }
            motionSmoothEaseCheck.enabled = smoothingMode;
            motionSmoothSourceFidelityCheck.enabled = motionSmooth;
            motionSmoothTolEdit.enabled = motionSmooth;
            motionSmoothCleanupTolEdit.enabled = motionSmooth;
            motionSmoothBezierX1Edit.enabled = smoothingMode;
            motionSmoothBezierY1Edit.enabled = smoothingMode;
            motionSmoothBezierX2Edit.enabled = smoothingMode;
            motionSmoothBezierY2Edit.enabled = smoothingMode;
            motionPathBoundsCheck.enabled = motionPathSmooth;
            motionPathSmoothTolEdit.enabled = motionPathSmooth;
            motionPathAccuracyTolEdit.enabled = motionPathSmooth;
            motionPathBoundsTolEdit.enabled = motionPathSmooth;
            motionPathSharpCheck.enabled = motionPathSmooth;
            motionPathSharpAngleEdit.enabled =
                motionPathSmooth && motionPathSharpCheck.value === true;
            motionPathKeyedCheck.enabled = motionPathSmooth;
            shapeReplacementPreferVerticesCheck.enabled =
                !temporalOnly && !smoothingMode;
            cleanupModeDDL.enabled = true;
            var cleanupMode = cleanupModeDDL.selection
                ? _cleanupModeFromIndex(cleanupModeDDL.selection.index)
                : _globalCleanupMode();
            preserveSharpCornersCheck.enabled =
                shapeReplacementCheck.value === true ||
                shapeReplacementPreferVerticesCheck.value === true ||
                cleanupMode !== 'off' ||
                vertexOnly;
            try { dlg.layout.layout(true); } catch (layoutErr) {}
        }
        syncShapeReplacementOptions();
        solveModeDDL.onChange = syncShapeReplacementOptions;
        shapeReplacementCheck.onClick = syncShapeReplacementOptions;
        shapeReplacementPreferVerticesCheck.onClick = syncShapeReplacementOptions;
        motionPathBoundsCheck.onClick = syncShapeReplacementOptions;
        motionPathSharpCheck.onClick = syncShapeReplacementOptions;
        cleanupModeDDL.onChange = syncShapeReplacementOptions;

        var btnGrp = dlg.add('group');
        btnGrp.alignment = 'right';
        var okBtn     = btnGrp.add('button', undefined, 'OK',     { name: 'ok' });
        var cancelBtn = btnGrp.add('button', undefined, 'Cancel', { name: 'cancel' });

        okBtn.onClick = function () {
            settings.tolerance          = parseFloat(tolEdit.text)   || DEFAULT_TOL;
            settings.toleranceScreenPx  = parseFloat(tolPxEdit.text) || 0.0;
            settings.sampleMode         = 'auto';
            settings.bbsolverPath         = bdEdit.text || '';
            settings.logOutputDir         = logPathEdit.text || '';
            settings.solverJobs         = parseInt(jobsEdit.text, 10);
            if (!isFinite(settings.solverJobs) || settings.solverJobs < 0) {
                settings.solverJobs = 0;
            }
            if (settings.solverJobs > 64) { settings.solverJobs = 64; }
            settings.useWorkArea        = waCheck.value;
            settings.useSegmentMarkers  = segmentMarkerCheck.value;
            settings.startSec           = parseFloat(startEdit.text) || 0;
            settings.endSec             = parseFloat(endEdit.text)   || 0;
            settings.autoSeparateForBake = sepCheck.value;
            settings.flattenParentedPosition = flattenCheck.value;
            settings.preserveSelectedParenting = preserveParentCheck.value;
            settings.flattenParentedTolerance =
                parseFloat(flattenTolEdit.text) || 0.05;
            settings.rigRotationTolerance =
                parseFloat(rigRotTolEdit.text) || 0.01;
            settings.verifyRoundTrip   = verifyRoundTripCheck.value;
            settings.verifyRigGaps      = false;
            settings.rigGapTolerance    = settings.tolerance;
            settings.disableExpression  = disCheck.value;
            settings.archiveExpression  = false;
            settings.archiveAsGuideLayer = false;
            settings.showPreview        = previewCheck.value;
            settings.solveOptimizationMode =
                solveModeMap[solveModeDDL.selection
                    ? solveModeDDL.selection.index : 0] || 'full';
            settings.motionSmoothUseEase = motionSmoothEaseCheck.value;
            settings.motionSmoothSourceFidelity =
                motionSmoothSourceFidelityCheck.value;
            settings.motionSmoothTolerance =
                parseFloat(motionSmoothTolEdit.text) || 3.0;
            if (!isFinite(settings.motionSmoothTolerance) ||
                    settings.motionSmoothTolerance <= 0) {
                settings.motionSmoothTolerance = 3.0;
            }
            settings.motionSmoothTemporalCleanupTolerance =
                parseFloat(motionSmoothCleanupTolEdit.text) || 2.0;
            if (!isFinite(settings.motionSmoothTemporalCleanupTolerance) ||
                    settings.motionSmoothTemporalCleanupTolerance <= 0) {
                settings.motionSmoothTemporalCleanupTolerance = 2.0;
            }
            settings.motionPathSmoothingTolerance =
                _clampMotionPathSmoothingTolerance(
                    motionPathSmoothTolEdit.text);
            settings.motionPathAccuracyTolerance =
                _clampMotionPathFitTolerance(
                    motionPathAccuracyTolEdit.text);
            settings.motionPathPreserveBounds =
                motionPathBoundsCheck.value === true;
            settings.motionPathBoundsTolerance =
                _clampMotionPathBoundsTolerance(
                    motionPathBoundsTolEdit.text);
            settings.motionPathPreserveSharpPoints =
                motionPathSharpCheck.value;
            settings.motionPathSharpAngleDeg =
                _clampMotionPathSharpAngleDeg(motionPathSharpAngleEdit.text);
            settings.motionPathRespectKeyedFrames =
                motionPathKeyedCheck.value;
            var savedMotionBezier = _sanitizeMotionSmoothBezier({
                x1: motionSmoothBezierX1Edit.text,
                y1: motionSmoothBezierY1Edit.text,
                x2: motionSmoothBezierX2Edit.text,
                y2: motionSmoothBezierY2Edit.text
            });
            settings.motionSmoothBezierX1 = savedMotionBezier.x1;
            settings.motionSmoothBezierY1 = savedMotionBezier.y1;
            settings.motionSmoothBezierX2 = savedMotionBezier.x2;
            settings.motionSmoothBezierY2 = savedMotionBezier.y2;
            settings.emitLandmarkSubpaths = false;
            settings.shapeTemporalFullGapFit = false;
            settings.shapeReplacementFit = false;
            settings.shapeReplacementPreferVertices =
                shapeReplacementPreferVerticesCheck.value;
            _setGlobalCleanupMode(_cleanupModeFromIndex(
                cleanupModeDDL.selection ? cleanupModeDDL.selection.index : 0));
            settings.preserveSharpPathCorners =
                preserveSharpCornersCheck.value;
            _saveCurrentSettings();
            _syncMainModeControls();
            dlg.close(1);
        };
        cancelBtn.onClick = function () { dlg.close(0); };
        try {
            dlg.layout.layout(true);
            dlg.layout.resize();
        } catch (finalLayoutErr) {}
        dlg.show();
    }

    // ---- Build UI --------------------------------------------------------

    function buildUI(parent) {
        var panel = (parent instanceof Panel)
            ? parent
            : new Window('palette', TITLE, undefined, { resizeable: true });

        panel.text = TITLE;
        panel.orientation = 'column';
        panel.alignChildren = ['fill', 'top'];
        panel.margins = 10;
        panel.spacing = 6;
        panel.minimumSize = [360, 360];

        // Row 1: primary actions.
        var row1 = panel.add('group');
        row1.orientation = 'row';
        row1.alignChildren = ['center', 'center'];
        var sampleBtn = row1.add('button', undefined, 'Sample');
        var solveBtn  = row1.add('button', undefined, 'Solve and Bake');
        var abortBtn  = row1.add('button', undefined, 'Abort Solve');
        abortBtn.enabled = false;

        // Row 2: secondary actions.
        var row2 = panel.add('group');
        row2.orientation = 'row';
        row2.alignChildren = ['center', 'center'];
        var settingsBtn   = row2.add('button', undefined, 'Settings');
        row2.add('statictext', undefined, 'Mode:');
        var mainSolveModeDDL = row2.add('dropdownlist', undefined,
            _solveModeLabels);
        mainSolveModeDDL.preferredSize.width = 160;
        mainSolveModeDDL.selection =
            _solveModeIndex(settings.solveOptimizationMode);
        mainSolveModeDDL.helpTip = 'Default solve mode for sampled rows. Double-click a row to override one property.';
        row2.add('statictext', undefined, 'Cleanup:');
        var mainCleanupModeDDL = row2.add('dropdownlist', undefined, [
            'Prompt',
            'Auto',
            'Off'
        ]);
        mainCleanupModeDDL.preferredSize.width = 90;
        mainCleanupModeDDL.selection =
            _cleanupModeIndex(_globalCleanupMode());
        mainCleanupModeDDL.helpTip = 'Default second-pass cleanup behavior. Auto runs eligible cleanup without prompting; row overrides are available by double-click.';

        sampleBtn.helpTip     = 'Sample selected props; populates the property list';
        solveBtn.helpTip      = 'Sample, solve, preview, apply. Select multiple timeline properties to solve them in one run.';
        abortBtn.helpTip      = 'Safely request the running external solve to stop. The current AE operation may finish before the abort is observed.';
        settingsBtn.helpTip   = 'Open bake accuracy, solver, range, spatial, expression, and path options.';
        mainSolveModeDDL.onChange = function () {
            if (!mainSolveModeDDL.selection) { return; }
            _setMainSolveMode(
                _solveModeFromIndex(mainSolveModeDDL.selection.index));
        };
        mainCleanupModeDDL.onChange = function () {
            if (!mainCleanupModeDDL.selection) { return; }
            _setMainCleanupMode(
                _cleanupModeFromIndex(mainCleanupModeDDL.selection.index));
        };
        panel.solveModeDDL = mainSolveModeDDL;
        panel.cleanupModeDDL = mainCleanupModeDDL;
        panel.abortBtn = abortBtn;

        // Property listbox.
        // Verified: onDoubleClick (js-tools-guide p.1966), selection (p.1185).
        var sampledHeader = panel.add('group');
        sampledHeader.orientation = 'row';
        sampledHeader.alignChildren = ['left', 'center'];
        sampledHeader.alignment = ['fill', 'top'];
        var sampledHeaderText = sampledHeader.add('statictext', undefined,
            'Sampled properties (double-click one row to edit tolerance / mode / cleanup):');
        sampledHeaderText.alignment = ['fill', 'center'];
        var refreshSampledBtn = sampledHeader.add('button', undefined, 'Refresh');
        refreshSampledBtn.helpTip = 'Re-resolve sampled rows against the active comp, remove deleted properties, and update labels after layer/property renames.';
        var propList = panel.add('listbox', undefined, [],
            { multiselect: false });
        propList.preferredSize = [440, 80];
        propList.alignment = ['fill', 'top'];
        propList.helpTip = 'Double-click a sampled row to edit that property tolerance, screen-px, solve mode, and cleanup override. A * means an override is active.';
        propList.onDoubleClick = function () {
            var sel = propList.selection;
            if (!sel) { return; }
            if (typeof sel.length === 'number' && sel.length > 0) { sel = sel[0]; }
            if (!sel) { return; }
            // Use the listbox item index to look up directly in _lastBundle.
            var selIdx = sel.index;
            if (_lastBundle && selIdx >= 0 && selIdx < _lastBundle.properties.length) {
                var propId = _lastBundle.properties[selIdx].property.id;
                _setToleranceForProp(propId);
            }
        };
        panel.propList = propList;

        // Status label + progress bar. The label echoes the current phase
        // (sampling, solver, parsing, applying, verifying) while the native
        // ScriptUI progressbar gives AE a real fill control to repaint.
        var statusLabel = panel.add('statictext', undefined, 'Idle');
        statusLabel.preferredSize = [440, 16];
        statusLabel.alignment = ['fill', 'top'];
        panel.statusLabel = statusLabel;

        var progressRow = panel.add('group');
        progressRow.orientation = 'row';
        progressRow.alignChildren = ['left', 'center'];
        progressRow.alignment = ['fill', 'top'];
        progressRow.spacing = 6;
        var progressView = progressRow.add('progressbar', undefined, 0, 100);
        progressView.preferredSize = [260, 14];
        progressView.minimumSize = [80, 14];
        progressView.alignment = ['fill', 'center'];
        progressView.value = 0;
        progressView._value = 0;
        var progressPctLabel = progressRow.add('statictext', undefined, '0%');
        progressPctLabel.preferredSize = [52, 14];
        progressPctLabel.justify = 'right';
        var elapsedLabel = progressRow.add('statictext', undefined, 'Elapsed 00:00');
        elapsedLabel.preferredSize = [140, 14];
        elapsedLabel.minimumSize = [120, 14];
        elapsedLabel.justify = 'right';
        progressRow.helpTip = 'Live bake progress. Blue fill advances on solver events and during long solver waits.';
        panel.progressView = progressView;
        panel.progressBar = progressView;
        panel.progressPctLabel = progressPctLabel;
        panel.elapsedLabel = elapsedLabel;

        // Log area.
        var logBox = panel.add('edittext', undefined, '',
            { multiline: true, scrolling: true });
        logBox.preferredSize = [440, 100];
        logBox.alignment = ['fill', 'fill'];
        panel.logBox = logBox;

        var footerRow = panel.add('group');
        footerRow.orientation = 'row';
        footerRow.alignChildren = ['left', 'center'];
        footerRow.alignment = ['fill', 'bottom'];
        var clearLogBtn = footerRow.add('button', undefined, 'Clear Log');
        clearLogBtn.helpTip = 'Clear the visible log and start a new exported log file.';
        var exportLogCheck = footerRow.add('checkbox', undefined, 'Export logs');
        exportLogCheck.value = (settings.exportLogs === true);
        exportLogCheck.helpTip = 'Write the panel log to the log folder selected in Settings.';
        var versionSpacer = footerRow.add('statictext', undefined, '');
        versionSpacer.alignment = ['fill', 'center'];
        var versionLabel = footerRow.add('statictext', undefined,
            'v' + PANEL_VERSION);
        versionLabel.justify = 'right';
        versionLabel.alignment = ['right', 'center'];

        sampleBtn.onClick     = doSample;
        solveBtn.onClick      = doSolveAndBake;
        abortBtn.onClick      = _requestSafeAbort;
        settingsBtn.onClick   = openSettings;
        clearLogBtn.onClick   = clearLog;
        exportLogCheck.onClick = function () {
            settings.exportLogs = exportLogCheck.value === true;
            if (!settings.exportLogs) { _exportLogPath = ''; }
            _saveCurrentSettings();
        };
        refreshSampledBtn.onClick = _refreshSampledPropertiesFromAE;

        panel.onResizing = panel.onResize = function () {
            _resizeMainLog(panel);
            try { panel.layout.resize(); } catch (eResize) {
                try { panel.layout.layout(true); } catch (eLayout) {}
            }
        };

        if (panel instanceof Window) { panel.center(); }
        return panel;
    }

    // ---- Init ------------------------------------------------------------

    ui = buildUI(thisObj);
    if (ui instanceof Window) {
        ui.show();
        _resizeMainLog(ui);
        try { ui.layout.layout(true); } catch (eShowLayout) {}
    } else {
        _resizeMainLog(ui);
        ui.layout.layout(true);
    }

    try { _checkBbsolverOnStartup(); } catch (e) {}

}(this));
