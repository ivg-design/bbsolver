// writeback.jsx — applyKeyBundle (round-3: adds archiveAsGuideLayer)
// Requires _polyfill.jsx and _lookup.jsx to be #included before this file.

// --- Helpers ---

// Map an InterpType string from the KeyBundle to KeyframeInterpolationType enum.
// Verified enum values: KeyframeInterpolationType.HOLD, LINEAR, BEZIER.
// Per AE scripting guide (property.md setInterpolationTypeAtKey).
function _interpToAE(kind) {
    if (kind === 'Hold')   { return KeyframeInterpolationType.HOLD;   }
    if (kind === 'Linear') { return KeyframeInterpolationType.LINEAR; }
    // Default to Bezier; also catches "Bezier" explicitly.
    return KeyframeInterpolationType.BEZIER;
}

// Map a temporal_ease_in/out array from the KeyBundle to the array of
// KeyframeEase objects that AE expects.
//
// AE ease array size per setTemporalEaseAtKey docs (property.md p.807):
//   - TwoD:   2 objects
//   - ThreeD: 3 objects
//   - All other types (OneD, TwoD_SPATIAL, ThreeD_SPATIAL, COLOR, etc.): 1 object
//
// easeArr: array of {speed, influence} objects from the KeyBundle.
//          May be empty (solver defaults); we synthesise a neutral ease in that case.
// pvt: prop.propertyValueType (a PropertyValueType enum value).
function _mapEase(easeArr, pvt) {
    var count;
    if (pvt === PropertyValueType.TwoD) {
        count = 2;
    } else if (pvt === PropertyValueType.ThreeD) {
        count = 3;
    } else {
        // OneD, TwoD_SPATIAL, ThreeD_SPATIAL, COLOR, CUSTOM_VALUE, and all others: 1.
        count = 1;
    }

    var result = [];
    var i, src, speed, influence;
    for (i = 0; i < count; i++) {
        src = (easeArr && i < easeArr.length) ? easeArr[i] : null;
        speed     = (src && typeof src.speed     === 'number') ? src.speed     : 0;
        influence = (src && typeof src.influence === 'number') ? src.influence : 33.3;
        // AE clamps influence [0.1, 100.0]. Clamp here to match solver contract.
        if (influence < 0.1)   { influence = 0.1;   }
        if (influence > 100.0) { influence = 100.0; }
        result.push(new KeyframeEase(speed, influence));
    }
    return result;
}

function _friendlyPropPath(prop) {
    var parts = [];
    var cur = prop;
    while (cur) {
        var nm = '';
        try { nm = cur.name || ''; } catch (en) {}
        if (nm) { parts.unshift(nm); }
        try { cur = cur.parentProperty; } catch (ep) { cur = null; }
    }
    if (parts.length === 0) {
        try { return prop.name || ''; } catch (e) {}
    }
    return parts.join('/');
}

// Archive the expression text for 'prop' onto layer as a marker at t=0.
// This is intended only for destructive expression deletion. The normal bake
// path only disables expressions, which keeps the expression text on the prop.
// The expression is put in the visible marker comment and duplicated in marker
// params under key 'expr' for machine recovery.
// Verified: layer.marker is the marker PropertyGroup (layer.md p.188).
// Verified: MarkerValue(comment) + setParameters(obj) (markervalue.md p.15,206).
// Verified: setValueAtTime on marker property (layer.md example p.215).
function _archiveExpression(layer, prop, propId, exprText) {
    try {
        var propPath = _friendlyPropPath(prop);
        var comment = 'bbsolver-test-harness archived expression\n' +
            'Property: ' + propPath + '\n' +
            'ID: ' + propId + '\n' +
            'Expression:\n' + exprText;
        var mv = new MarkerValue(comment);
        mv.setParameters({ prop: propPath, id: propId, expr: exprText });
        layer.marker.setValueAtTime(0, mv);
    } catch (e) {
        // Non-fatal: some layer types may not support markers (lights, cameras).
        // VERIFY-IN-AE: camera/light layers may lack a writable marker property.
    }
}

// Remove all keyframes from a property, working backwards (required by AE).
// Verified: prop.removeKey(index) and "work down from highest index" (property.md p.1097).
function _removeAllKeys(prop) {
    var n = prop.numKeys;
    while (n > 0) {
        prop.removeKey(n);
        n = n - 1;
    }
}

function _keyTimesRange(keys) {
    if (!keys || keys.length === 0) {
        return null;
    }
    var minT = keys[0].t_sec;
    var maxT = keys[0].t_sec;
    var i, t;
    for (i = 1; i < keys.length; i++) {
        t = keys[i].t_sec;
        if (typeof t !== 'number' || !isFinite(t)) { continue; }
        if (t < minT) { minT = t; }
        if (t > maxT) { maxT = t; }
    }
    if (typeof minT !== 'number' || !isFinite(minT) ||
            typeof maxT !== 'number' || !isFinite(maxT)) {
        return null;
    }
    return { start: minT, end: maxT };
}

function _normaliseWriteRanges(ranges) {
    if (!ranges || !ranges.length) { return []; }
    var out = [];
    var i, r, start, end;
    for (i = 0; i < ranges.length; i++) {
        r = ranges[i] || {};
        start = (typeof r.start === 'number') ? r.start : r.tStart;
        end = (typeof r.end === 'number') ? r.end : r.tEnd;
        if (typeof start !== 'number' || !isFinite(start) ||
                typeof end !== 'number' || !isFinite(end) ||
                end < start) {
            continue;
        }
        out.push({ start: start, end: end });
    }
    if (out.length <= 1) { return out; }
    out.sort(function (a, b) {
        if (a.start < b.start) { return -1; }
        if (a.start > b.start) { return 1; }
        return a.end < b.end ? -1 : (a.end > b.end ? 1 : 0);
    });
    var merged = [];
    var eps = 0.000001;
    var cur, last;
    for (i = 0; i < out.length; i++) {
        cur = out[i];
        if (merged.length === 0) {
            merged.push(cur);
            continue;
        }
        last = merged[merged.length - 1];
        if (cur.start <= last.end + eps) {
            if (cur.end > last.end) { last.end = cur.end; }
        } else {
            merged.push(cur);
        }
    }
    return merged;
}

function _removeKeysInTimeRange(prop, startTime, endTime) {
    var n = prop.numKeys;
    var eps = 0.000001;
    while (n > 0) {
        var kt = 0;
        try { kt = prop.keyTime(n); } catch (eTime) { kt = null; }
        if (kt !== null && kt >= startTime - eps && kt <= endTime + eps) {
            prop.removeKey(n);
        }
        n = n - 1;
    }
}

function _removeKeysInTimeRanges(prop, ranges) {
    ranges = _normaliseWriteRanges(ranges);
    var i;
    for (i = 0; i < ranges.length; i++) {
        _removeKeysInTimeRange(prop, ranges[i].start, ranges[i].end);
    }
}

function _keyIndexAtTime(prop, t) {
    var n = prop.numKeys;
    var eps = 0.000001;
    var bestIdx = -1;
    var bestDist = 999999999;
    var minGap = 999999999;
    var prevKt = null;
    var i, kt, d, gap, relaxedEps;
    for (i = 1; i <= n; i++) {
        try { kt = prop.keyTime(i); } catch (eTime) { continue; }
        d = Math.abs(kt - t);
        if (d <= eps) { return i; }
        if (d < bestDist) {
            bestDist = d;
            bestIdx = i;
        }
        if (prevKt !== null) {
            gap = Math.abs(kt - prevKt);
            if (gap > eps && gap < minGap) { minGap = gap; }
        }
        prevKt = kt;
    }
    if (bestIdx < 1) { return -1; }
    // AE can normalize sub-frame shape-key times by a small amount after
    // setValueAtTime(). Accept the nearest key only inside a tight, local
    // spacing-aware tolerance so dense frame-by-frame keys do not collapse.
    relaxedEps = 0.02;
    if (minGap < 999999999) {
        relaxedEps = Math.min(relaxedEps, minGap * 0.45);
    }
    if (relaxedEps < eps) { relaxedEps = eps; }
    return bestDist <= relaxedEps ? bestIdx : -1;
}

function _useAdjacentFrameHold(keys, leftIndex, frameDuration) {
    if (!keys || leftIndex < 0 || leftIndex >= keys.length - 1) { return false; }
    if (!(frameDuration > 0)) { return false; }
    var t0 = keys[leftIndex].t_sec;
    var t1 = keys[leftIndex + 1].t_sec;
    if (typeof t0 !== 'number' || typeof t1 !== 'number' ||
            !isFinite(t0) || !isFinite(t1)) {
        return false;
    }
    var dt = t1 - t0;
    var eps = Math.max(0.000001, frameDuration * 0.05);
    return dt > 0 && dt <= frameDuration + eps;
}

function _snapTimeToFrameGrid(t, frameDuration, frameOrigin) {
    if (!(frameDuration > 0) || typeof t !== 'number' || !isFinite(t)) {
        return t;
    }
    if (typeof frameOrigin !== 'number' || !isFinite(frameOrigin)) {
        frameOrigin = 0;
    }
    return frameOrigin + Math.round((t - frameOrigin) / frameDuration) *
        frameDuration;
}

function _cloneKeyWithTime(k, t) {
    var out = {};
    var key;
    for (key in k) {
        if (Object.prototype.hasOwnProperty.call(k, key)) {
            out[key] = k[key];
        }
    }
    out.t_sec = t;
    return out;
}

function _keysWithFrameSnappedTimes(keys, frameDuration, frameOrigin) {
    if (!keys || keys.length === 0 || !(frameDuration > 0)) { return keys; }
    var out = [];
    var changed = false;
    var prev = null;
    var eps = Math.max(0.0000001, frameDuration * 0.0001);
    var i, t, snapped;
    for (i = 0; i < keys.length; i++) {
        t = keys[i].t_sec;
        snapped = _snapTimeToFrameGrid(t, frameDuration, frameOrigin);
        if (typeof snapped !== 'number' || !isFinite(snapped)) {
            return keys;
        }
        if (prev !== null && snapped <= prev + eps) {
            // Do not collapse two solver keys onto one AE frame. Keeping the
            // original sub-frame schedule is safer than losing a key.
            return keys;
        }
        if (Math.abs(snapped - t) > eps) { changed = true; }
        out.push(_cloneKeyWithTime(keys[i], snapped));
        prev = snapped;
    }
    return changed ? out : keys;
}

function _finiteShapeFlatNumber(v) {
    return typeof v === 'number' && isFinite(v);
}

function _shapeFromFlatArray(flat) {
    if (!flat || flat.length < 2) {
        throw new Error('shape_flat key has no header');
    }

    var n = Math.round(flat[1]);
    if (n < 1 || flat.length !== 2 + 6 * n) {
        throw new Error('shape_flat key has invalid vertex count/length');
    }
    if (!_finiteShapeFlatNumber(flat[0]) || !_finiteShapeFlatNumber(flat[1])) {
        throw new Error('shape_flat key has non-finite header');
    }

    var sh = new Shape();
    var vertices = [];
    var inTangents = [];
    var outTangents = [];
    var idx = 2;
    var i;

    sh.closed = (Math.round(flat[0]) !== 0);
    for (i = 0; i < n; i++) {
        if (!_finiteShapeFlatNumber(flat[idx]) ||
                !_finiteShapeFlatNumber(flat[idx + 1]) ||
                !_finiteShapeFlatNumber(flat[idx + 2]) ||
                !_finiteShapeFlatNumber(flat[idx + 3]) ||
                !_finiteShapeFlatNumber(flat[idx + 4]) ||
                !_finiteShapeFlatNumber(flat[idx + 5])) {
            throw new Error('shape_flat key has non-finite vertex/tangent values');
        }
        vertices.push([flat[idx], flat[idx + 1]]);
        inTangents.push([flat[idx + 2], flat[idx + 3]]);
        outTangents.push([flat[idx + 4], flat[idx + 5]]);
        idx = idx + 6;
    }
    sh.vertices = vertices;
    sh.inTangents = inTangents;
    sh.outTangents = outTangents;
    return sh;
}

function _expectedValueLengthForPropertyValueType(pvt) {
    if (pvt === PropertyValueType.TwoD ||
            pvt === PropertyValueType.TwoD_SPATIAL) {
        return 2;
    }
    if (pvt === PropertyValueType.ThreeD ||
            pvt === PropertyValueType.ThreeD_SPATIAL) {
        return 3;
    }
    if (pvt === PropertyValueType.COLOR) {
        return 4;
    }
    return 1;
}

function _validateKeyPayloadForWriteback(pk, pvt, isShapePath) {
    if (!pk || !pk.keys || pk.keys.length === 0) {
        return 'solver produced no keys; existing keyframes left untouched';
    }
    var expected = _expectedValueLengthForPropertyValueType(pvt);
    var i, j, k, v;
    for (i = 0; i < pk.keys.length; i++) {
        k = pk.keys[i];
        if (!k || typeof k.t_sec !== 'number' || !isFinite(k.t_sec)) {
            return 'key ' + i + ' has invalid time; existing keyframes left untouched';
        }
        v = k.v;
        if (!v || !v.length) {
            return 'key ' + i + ' has no value payload; existing keyframes left untouched';
        }
        if (isShapePath) {
            try { _shapeFromFlatArray(v); }
            catch (shapeErr) {
                return 'key ' + i + ' has invalid shape payload: ' +
                    shapeErr.message + '; existing keyframes left untouched';
            }
            continue;
        }
        if (v.length < expected) {
            return 'key ' + i + ' value length ' + v.length +
                ' is shorter than expected ' + expected +
                '; existing keyframes left untouched';
        }
        for (j = 0; j < expected; j++) {
            if (typeof v[j] !== 'number' || !isFinite(v[j])) {
                return 'key ' + i + ' has non-finite value at component ' +
                    j + '; existing keyframes left untouched';
            }
        }
    }
    return '';
}

// --- Guide-layer archive helper ---

// Duplicate the source layer, rename with 'bbsolver_orig_' prefix, mark as guide layer.
// The duplicate is placed immediately above the original so the original retains
// its index for perPropertyResolver.
//
// Verified: layer.duplicate() (layer.md p.466), layer.name (layer.md p.22 example),
//           layer.moveBefore(other) (layer.md p.505),
//           AVLayer.guideLayer read/write bool (avlayer.md p.269).
// VERIFY-IN-AE: guideLayer only exists on AVLayer — not on CameraLayer/LightLayer.
//               We guard with typeof to avoid exceptions on non-AV layers.
function _archiveAsGuideLayer(layer) {
    try {
        var dup = layer.duplicate();
        dup.name = 'bbsolver_orig_' + layer.name;
        dup.moveBefore(layer);
        // guideLayer is on AVLayer only; guard so cameras/lights don't throw.
        if (typeof dup.guideLayer !== 'undefined') {
            dup.guideLayer = true;
        }
    } catch (e) {
        // Non-fatal — log is not available here; caller should catch if needed.
    }
}

function _unparentLayerForFlatten(layer) {
    if (!layer) { return false; }
    var hasParent = false;
    try { hasParent = (layer.parent !== null); } catch (ep) {}
    if (!hasParent) { return true; }

    // Direct parent assignment asks AE to compensate transform values so the
    // layer does not visibly jump at the current time. The sampled Position keys
    // written below then replace Position with comp-space anchor coordinates.
    try {
        layer.parent = null;
        return true;
    } catch (eParent) {}

    try {
        layer.setParentWithJump(null);
        return true;
    } catch (eJump) {}

    return false;
}

function _unparentFlattenedPositionLayers(keyBundle, resolver, flattenMap, errors) {
    if (!flattenMap) { return; }
    var seenLayers = {};
    var results = keyBundle.property_results || [];
    var i, propId, prop, layer, layerKey;
    for (i = 0; i < results.length; i++) {
        propId = results[i].property_id;
        if (flattenMap[propId] !== true) { continue; }

        prop = null;
        try { prop = resolver(propId); } catch (er) {}
        if (!prop) {
            errors.push({ id: propId, msg: 'parent-flatten resolver failed before writeback' });
            continue;
        }

        layer = null;
        try { layer = prop.propertyGroup(prop.propertyDepth); } catch (el) {}
        if (!layer) {
            errors.push({ id: propId, msg: 'parent-flatten layer not found before writeback' });
            continue;
        }

        layerKey = '';
        try { layerKey = String(layer.index); } catch (ek) { layerKey = propId; }
        if (seenLayers[layerKey]) { continue; }
        seenLayers[layerKey] = true;

        if (!_unparentLayerForFlatten(layer)) {
            errors.push({ id: propId, msg: 'could not unparent layer for comp-space Position bake' });
            flattenMap[propId] = 'failed';
        }
    }
}

// --- Main ---

// applyKeyBundle(keyBundle, opts)
//
// opts = {
//   undoGroupName:       string   — default 'bbsolver-test-harness: bake expression'
//   disableExpression:   bool     — disable but keep expression text
//   deleteExpression:    bool     — clear expression text after archiving
//   archiveExpression:   bool     — copy expression to a layer marker before delete
//   archiveAsGuideLayer: bool     — duplicate layer as guide before baking
//   overwriteExisting:   bool     — remove existing keyframes first
//   perPropertyResolver: function(propertyId) -> Property | null
//   flattenParentedPositionMap: { propertyId: true } — unparent layers before writeback
//   verifyShapeKeyStructure: bool — debug-only post-write shape key readback
//   holdAdjacentFrameKeys: bool — mark one-frame adjacent key boundaries as Hold for export
//   frameDuration: number — comp frame duration in seconds for holdAdjacentFrameKeys
//   snapKeyTimesToFrameGrid: bool — snap solver t_sec values to comp frame stops
//   frameOrigin: number — timeline origin for frame-grid snapping
// }
//
// Returns { applied: <count>, skipped: [<id>], errors: [{id, msg}],
//           shape_key_structure: [{id, ok, keys_checked, mismatches}] }.
function applyKeyBundle(keyBundle, opts) {
    opts = opts || {};
    var undoName      = opts.undoGroupName       || 'bbsolver-test-harness: bake expression';
    var disableExpr   = (opts.disableExpression  !== false); // default true
    var deleteExpr    = (opts.deleteExpression   === true);  // default false
    var archiveExpr   = (opts.archiveExpression  === true);  // default false
    var guideArchive  = !!opts.archiveAsGuideLayer;           // default false
    var overwrite     = (opts.overwriteExisting  !== false); // default true
    var resolver      = opts.perPropertyResolver;
    var flattenMap    = opts.flattenParentedPositionMap || null;
    var progressCb    = opts.progressCallback;
    var verifyShapeStructure = (opts.verifyShapeKeyStructure === true);
    var holdAdjacentFrameKeys = (opts.holdAdjacentFrameKeys === true);
    var frameDuration = (typeof opts.frameDuration === 'number' &&
                         isFinite(opts.frameDuration) &&
                         opts.frameDuration > 0)
        ? opts.frameDuration : 0;
    var snapKeyTimesToFrameGrid = (opts.snapKeyTimesToFrameGrid === true);
    var frameOrigin = (typeof opts.frameOrigin === 'number' &&
                       isFinite(opts.frameOrigin)) ? opts.frameOrigin : 0;

    if (typeof resolver !== 'function') {
        throw new Error('applyKeyBundle: opts.perPropertyResolver must be a function');
    }

    var applied  = 0;
    var skipped  = [];
    var errors   = [];
    var shapeKeyStructure = [];
    var archivedPropMap = {};

    function _resolvedWritebackProp(propId) {
        if (archivedPropMap && archivedPropMap[propId]) {
            return archivedPropMap[propId];
        }
        return resolver(propId);
    }

    function _emitApplyProgress(stage, propertyIndex, propertyTotal,
            propId, keyIndex, keyTotal) {
        if (typeof progressCb !== 'function') { return; }
        try {
            progressCb({
                phase: stage,
                property_index: propertyIndex,
                property_total: propertyTotal,
                property_id: propId,
                key_index: (typeof keyIndex === 'number') ? keyIndex : -1,
                key_total: (typeof keyTotal === 'number') ? keyTotal : 0
            });
        } catch (progressErr) {}
    }

    app.beginUndoGroup(undoName);

    // Guide-layer archive: one duplicate per UNIQUE layer touched by this bundle.
    // We collect layer-part strings ('L<n>') from property ids, deduplicate,
    // then resolve one property per layer to obtain the Layer object.
    if (guideArchive) {
        var layersArchived = {};
        var ai, apkId, aSlash, aLayerPart, aLayerIdx, anyProp;
        for (ai = 0; ai < keyBundle.property_results.length; ai++) {
            apkId      = keyBundle.property_results[ai].property_id;
            anyProp = null;
            try { anyProp = resolver(apkId); } catch (resolveForArchiveErr) {}
            if (anyProp) { archivedPropMap[apkId] = anyProp; }

            aSlash     = apkId.indexOf('/');
            aLayerPart = (aSlash >= 0) ? apkId.substring(0, aSlash) : '';
            aLayerIdx  = (aLayerPart.charAt(0) === 'L')
                       ? parseInt(aLayerPart.substring(1), 10) : 0;
            if (aLayerIdx > 0 && !layersArchived[aLayerIdx]) {
                layersArchived[aLayerIdx] = true;
                try {
                    if (anyProp) {
                        // Walk from the property up to its containing layer.
                        // Verified: prop.propertyGroup(prop.propertyDepth) -> Layer
                        // (propertybase.md p.341 example).
                        var srcLayer = anyProp.propertyGroup(anyProp.propertyDepth);
                        _archiveAsGuideLayer(srcLayer);
                    }
                } catch (ge) {}
            }
        }
    }

    _unparentFlattenedPositionLayers(keyBundle, _resolvedWritebackProp,
        flattenMap, errors);

    try {
        var results = keyBundle.property_results;
        var i, pk, prop;

        for (i = 0; i < results.length; i++) {
            pk = results[i];
            var propId = pk.property_id;
            _emitApplyProgress('resolve', i, results.length, propId, 0, 0);

            if (flattenMap && flattenMap[propId] === 'failed') {
                skipped.push(propId);
                continue;
            }

            try {
                prop = _resolvedWritebackProp(propId);
            } catch (re) {
                skipped.push(propId);
                errors.push({ id: propId, msg: 'resolver threw: ' + re.message });
                continue;
            }

            if (!prop) {
                skipped.push(propId);
                errors.push({
                    id: propId,
                    msg: 'resolver returned no property; layer/property may have been renamed, deleted, moved, or refreshed after sampling'
                });
                continue;
            }

            try {
                var pvt  = prop.propertyValueType;
                var keys = snapKeyTimesToFrameGrid
                    ? _keysWithFrameSnappedTimes(
                        pk.keys, frameDuration, frameOrigin)
                    : pk.keys;
                var isShapePath = (pvt === PropertyValueType.SHAPE);
                // AE Shape paths can behave spatially in the UI, but their
                // PropertyValueType is SHAPE, not TwoD_SPATIAL/ThreeD_SPATIAL.
                // Spatial key APIs throw on SHAPE; Shape geometry already lives
                // inside the Shape value's vertices/tangents.
                var isAeSpatial = false;
                if (!isShapePath) {
                    try { isAeSpatial = !!prop.isSpatial; } catch (eSpatial) {}
                }

                var payloadError = _validateKeyPayloadForWriteback(
                    pk, pvt, isShapePath);
                if (payloadError) {
                    skipped.push(propId);
                    errors.push({ id: propId, msg: payloadError });
                    _emitApplyProgress('property_done', i + 1, results.length,
                        propId, 0, 0);
                    continue;
                }

                // 1. Optionally remove existing keyframes.
                if (overwrite && prop.numKeys > 0) {
                    var explicitWriteRanges = _normaliseWriteRanges(
                        pk.write_ranges || pk._bb_write_ranges);
                    var writeRange = null;
                    if (explicitWriteRanges.length > 0) {
                        _removeKeysInTimeRanges(prop, explicitWriteRanges);
                    } else {
                        writeRange = _keyTimesRange(keys);
                    }
                    if (writeRange) {
                        _removeKeysInTimeRange(prop, writeRange.start, writeRange.end);
                    } else if (explicitWriteRanges.length === 0) {
                        _removeAllKeys(prop);
                    }
                }

                // 2. Archive expression only before destructive expression deletion.
                // Disabling keeps the expression text on the property, so a marker
                // archive is redundant and clutters the layer.
                var exprText = '';
                try { exprText = prop.expression; } catch (ee) {}

                if (deleteExpr && archiveExpr && exprText) {
                    var layer = prop.propertyGroup(prop.propertyDepth);
                    _archiveExpression(layer, prop, propId, exprText);
                }

                // 3. Disable expression (keeps text per AE scripting guide p.263).
                if (disableExpr) {
                    try {
                        prop.expressionEnabled = false;
                    } catch (ex) {
                        // Some properties have no expression; ignore.
                    }
                }
                if (deleteExpr) {
                    try { prop.expression = ''; } catch (ed) {}
                }

                // 4. Build aligned times/values arrays and set all keyframes at once.
                var times  = [];
                var values = [];
                var j, k;
                var keyStride = Math.max(1, Math.floor(keys.length / 24));
                for (j = 0; j < keys.length; j++) {
                    k = keys[j];
                    times.push(k.t_sec);
                    // k.v is an array of length == dimensions.
                    // For OneD, setValuesAtTimes needs a plain number not an array.
                    // VERIFY-IN-AE: whether OneD setValuesAtTimes accepts [num] or num.
                    // The scripting guide says "value appropriate for the type"; for OneD
                    // that is a float. We pass the raw array for non-OneD, scalar for OneD.
                    if (isShapePath) {
                        values.push(_shapeFromFlatArray(k.v));
                    } else if (pvt === PropertyValueType.OneD) {
                        values.push(k.v[0]);
                    } else {
                        values.push(k.v);
                    }
                    if (j === 0 || j === keys.length - 1 || (j % keyStride) === 0) {
                        _emitApplyProgress('prepare_values', i, results.length,
                            propId, j + 1, keys.length);
                    }
                }
                if (isShapePath) {
                    for (j = 0; j < values.length; j++) {
                        prop.setValueAtTime(times[j], values[j]);
                        if (j === 0 || j === values.length - 1 || (j % keyStride) === 0) {
                            _emitApplyProgress('write_values', i, results.length,
                                propId, j + 1, values.length);
                        }
                    }
                } else {
                    _emitApplyProgress('write_values', i, results.length,
                        propId, values.length, values.length);
                    prop.setValuesAtTimes(times, values);
                }
                var numAfterWrite = -1;
                try { numAfterWrite = prop.numKeys; } catch (numAfterErr) {}
                if (keys.length > 0 && numAfterWrite >= 0 &&
                        numAfterWrite < keys.length) {
                    throw new Error('writeback wrote only ' + numAfterWrite +
                        '/' + keys.length + ' keyframes');
                }

                // 5. Per-key interpolation, ease, and tangent settings.
                for (j = 0; j < keys.length; j++) {
                    k = keys[j];
                    var keyIdx = _keyIndexAtTime(prop, k.t_sec);
                    if (keyIdx < 1) {
                        throw new Error('writeback could not find AE key at time ' + k.t_sec);
                    }

                    // Interpolation type.
                    var inInterp  = _interpToAE(k.interp_in);
                    var outInterp = _interpToAE(k.interp_out);
                    if (holdAdjacentFrameKeys) {
                        if (_useAdjacentFrameHold(keys, j - 1, frameDuration)) {
                            inInterp = KeyframeInterpolationType.HOLD;
                        }
                        if (_useAdjacentFrameHold(keys, j, frameDuration)) {
                            outInterp = KeyframeInterpolationType.HOLD;
                        }
                    }
                    prop.setInterpolationTypeAtKey(keyIdx, inInterp, outInterp);

                    // Temporal ease.
                    var inEase  = _mapEase(k.temporal_ease_in,  pvt);
                    var outEase = _mapEase(k.temporal_ease_out, pvt);
                    if (isShapePath) {
                        // Shape Path keys are especially sensitive to AE
                        // promoting Linear/Hold sides back to Bezier when ease
                        // is applied. Only apply ease for Bezier-sided keys,
                        // then reassert interpolation so Linear path segments
                        // stay linear for valueAtTime()/verify.
                        try {
                            prop.setTemporalContinuousAtKey(keyIdx, !!k.temporal_continuous);
                            prop.setTemporalAutoBezierAtKey(keyIdx, !!k.temporal_auto_bezier);
                        } catch (shapeTemporalFlagErr) {}
                        if (inInterp === KeyframeInterpolationType.BEZIER ||
                                outInterp === KeyframeInterpolationType.BEZIER) {
                            try { prop.setTemporalEaseAtKey(keyIdx, inEase, outEase); } catch (shapeEaseErr) {}
                        }
                        try { prop.setInterpolationTypeAtKey(keyIdx, inInterp, outInterp); } catch (shapeInterpErr) {}
                    } else {
                        prop.setTemporalEaseAtKey(keyIdx, inEase, outEase);

                        // AE can promote a Linear/Hold side back to Bezier when
                        // temporal ease is applied. Reassert interpolation after
                        // ease so mixed Bezier/Linear keys keep the solver's side
                        // types before spatial tangent writeback.
                        prop.setInterpolationTypeAtKey(keyIdx, inInterp, outInterp);

                        // Temporal continuity / auto-bezier.
                        // Verified: setTemporalContinuousAtKey and setTemporalAutoBezierAtKey
                        // per AE scripting guide (property.md p.1391, p.1377).
                        prop.setTemporalContinuousAtKey(keyIdx, !!k.temporal_continuous);
                        prop.setTemporalAutoBezierAtKey(keyIdx, !!k.temporal_auto_bezier);

                        // Spatial continuity / auto-bezier (spatial only).
                        // Verified: setSpatialContinuousAtKey and setSpatialAutoBezierAtKey
                        // per AE scripting guide (property.md p.1321, p.1300).
                        if (isAeSpatial) {
                            prop.setSpatialContinuousAtKey(keyIdx, !!k.spatial_continuous);
                            prop.setSpatialAutoBezierAtKey(keyIdx, !!k.spatial_auto_bezier);
                            if (k.spatial_in  && k.spatial_in.length  > 0 &&
                                    k.spatial_out && k.spatial_out.length > 0) {
                                prop.setSpatialTangentsAtKey(keyIdx, k.spatial_in, k.spatial_out);
                            }
                        }

                        // Roving (spatial only, not first/last key).
                        // Verified: setRovingAtKey(keyIndex, bool) — NOT setRovingAtTime.
                        // AE scripting guide (property.md p.1258).
                        if (k.roving && isAeSpatial && keys.length > 2 &&
                            j > 0 && j < keys.length - 1) {
                            prop.setRovingAtKey(keyIdx, true);
                        }
                    }
                    if (j === 0 || j === keys.length - 1 || (j % keyStride) === 0) {
                        _emitApplyProgress('set_interpolation', i, results.length,
                            propId, j + 1, keys.length);
                    }
                }

                if (verifyShapeStructure && isShapePath && typeof verifyShapeKeyStructure === 'function') {
                    try {
                        _emitApplyProgress('verify_structure', i, results.length,
                            propId, 0, keys.length);
                        var sks = verifyShapeKeyStructure(prop, keys, function (info) {
                            info = info || {};
                            _emitApplyProgress(info.phase || 'verify_structure',
                                i, results.length, propId,
                                info.key_index || 0,
                                info.key_total || keys.length);
                        });
                        _emitApplyProgress('verify_structure', i, results.length,
                            propId, keys.length, keys.length);
                        sks.id = propId;
                        shapeKeyStructure.push(sks);
                        if (!sks.ok) {
                            // Build a log-friendly summary: mismatch count and
                            // first-mismatch detail (field, key position, values).
                            var mm = (sks.mismatches && sks.mismatches.length > 0)
                                   ? sks.mismatches[0] : null;
                            var msg = 'shape key structure mismatch';
                            if (sks.mismatches) {
                                msg += ' (' + sks.mismatches.length + ')';
                            }
                            if (mm) {
                                msg += ': ' + mm.field;
                                if (mm.key_index >= 0) {
                                    msg += ' at key ' + mm.key_index;
                                }
                                if (mm.expected !== undefined && mm.actual !== undefined) {
                                    msg += ' (expected=' + mm.expected +
                                           ' actual=' + mm.actual + ')';
                                }
                            }
                            errors.push({ id: propId, msg: msg });
                        }
                    } catch (sksErr) {
                        errors.push({ id: propId, msg: 'shape key structure verify failed: ' + sksErr.message });
                    }
                }

                applied = applied + 1;
                _emitApplyProgress('property_done', i + 1, results.length,
                    propId, keys.length, keys.length);

            } catch (applyErr) {
                errors.push({ id: propId, msg: applyErr.message });
                skipped.push(propId);
            }
        }

    } finally {
        app.endUndoGroup();
    }

    return {
        applied: applied,
        skipped: skipped,
        errors: errors,
        shape_key_structure: shapeKeyStructure
    };
}
