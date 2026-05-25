// sampler.jsx — collectSampleBundle
// Requires _polyfill.jsx to be #included before this file.

// djb2-style string hash, returned as 8-char hex — not SHA1, but unique per
// expression text and sufficient for cache invalidation in v1.
// VERIFY-IN-AE: If a stronger hash is required, swap in a proper SHA1 impl.
function _hashString(s) {
    var h = 5381;
    var i, c;
    for (i = 0; i < s.length; i++) {
        c = s.charCodeAt(i);
        h = ((h << 5) + h + c) | 0;
    }
    // Convert to unsigned hex
    return ('00000000' + ((h >>> 0).toString(16))).slice(-8);
}

// Determine PropertyInfo.kind and dimensions from prop.propertyValueType.
// Returns { kind, dimensions, is_spatial }.
function _classifyProperty(prop) {
    var pvt = prop.propertyValueType;
    if (pvt === PropertyValueType.OneD) {
        return { kind: 'Scalar',        dimensions: 1, is_spatial: false };
    } else if (pvt === PropertyValueType.TwoD) {
        return { kind: 'TwoD',          dimensions: 2, is_spatial: false };
    } else if (pvt === PropertyValueType.TwoD_SPATIAL) {
        return { kind: 'TwoD_Spatial',  dimensions: 2, is_spatial: true };
    } else if (pvt === PropertyValueType.ThreeD) {
        return { kind: 'ThreeD',        dimensions: 3, is_spatial: false };
    } else if (pvt === PropertyValueType.ThreeD_SPATIAL) {
        return { kind: 'ThreeD_Spatial',dimensions: 3, is_spatial: true };
    } else if (pvt === PropertyValueType.COLOR) {
        return { kind: 'Color',         dimensions: 4, is_spatial: false };
    } else if (pvt === PropertyValueType.SHAPE) {
        // Shape values are encoded as the solver's shape_flat numeric layout.
        // Verified: PropertyValueType.SHAPE (property.md p.495)
        return { kind: 'Custom',        dimensions: 0, is_spatial: false, isShape: true };
    } else {
        return { kind: 'Custom',        dimensions: 1, is_spatial: false };
    }
}

function _sourceKeyTimesInRange(prop, tStart, tEnd) {
    var times = [];
    var count = 0;
    var eps = 0.000001;
    try { count = prop.numKeys || 0; } catch (eCount) { count = 0; }
    var i, kt;
    for (i = 1; i <= count; i++) {
        try { kt = prop.keyTime(i); } catch (eTime) { continue; }
        if (typeof kt !== 'number' || !isFinite(kt)) { continue; }
        if (kt >= tStart - eps && kt <= tEnd + eps) {
            times.push(kt);
        }
    }
    return times;
}

// Walk from prop up to the containing layer using propertyGroup(depth).
// Returns the Layer object.
function _containingLayer(prop) {
    return prop.propertyGroup(prop.propertyDepth);
}

function _isPositionProperty(prop) {
    try { return prop.matchName === 'ADBE Position'; } catch (e) {}
    return false;
}

function _isRotationZProperty(prop) {
    try { return prop.matchName === 'ADBE Rotate Z'; } catch (e) {}
    return false;
}

function _layerHasParent(layer) {
    try { return !!layer.parent; } catch (e) {}
    return false;
}

function _is2DLayer(layer) {
    try { return layer.threeDLayer !== true; } catch (e) {}
    return true;
}

function _isParentedPositionFlattenCandidate(prop, classif) {
    if (!_isPositionProperty(prop)) { return false; }
    // AE Position values are three-component arrays even on 2D layers; the
    // third component is unused/zero. Keep the 2D check only as a defensive
    // fallback for non-standard Position-like properties.
    if (!classif || (classif.dimensions !== 2 && classif.dimensions !== 3)) { return false; }
    var layer = null;
    try { layer = _containingLayer(prop); } catch (e) {}
    if (!layer || !_is2DLayer(layer) || !_layerHasParent(layer)) { return false; }

    var isSepFollower = false;
    var separated = false;
    try { isSepFollower = prop.isSeparationFollower; } catch (esf) {}
    try { separated = prop.dimensionsSeparated; } catch (eds) {}
    return !isSepFollower && !separated;
}

function _parentedPositionFlattenDetails(prop, classif) {
    var layer = null;
    try { layer = _containingLayer(prop); } catch (e) {}
    var is3d = false;
    var hasParent = false;
    var isSepFollower = false;
    var isSepLeader = false;
    var separated = false;
    try { is3d = layer && layer.threeDLayer === true; } catch (e3) {}
    try { hasParent = !!(layer && layer.parent); } catch (ep) {}
    try { isSepFollower = prop.isSeparationFollower === true; } catch (ef) {}
    try { isSepLeader = prop.isSeparationLeader === true; } catch (el) {}
    try { separated = prop.dimensionsSeparated === true; } catch (ed) {}
    return 'kind=' + (classif ? classif.kind : '?') +
        ' dims=' + (classif ? classif.dimensions : '?') +
        ' spatial=' + (classif && classif.is_spatial === true ? 'true' : 'false') +
        ' layer3D=' + (is3d ? 'true' : 'false') +
        ' parent=' + (hasParent ? 'true' : 'false') +
        ' sepLeader=' + (isSepLeader ? 'true' : 'false') +
        ' sepFollower=' + (isSepFollower ? 'true' : 'false') +
        ' dimensionsSeparated=' + (separated ? 'true' : 'false');
}

function _idSegmentForPropertyBase(propBase) {
    var matchName = '';
    var idx = 0;
    try { matchName = propBase.matchName; } catch (em) {}
    try { idx = propBase.propertyIndex; } catch (ei) {}
    if (idx && idx > 0) { return matchName + '#' + idx; }
    var parent = null;
    try { parent = propBase.parentProperty; } catch (ep) {}
    if (parent) {
        var pn = 0, pi, pc;
        try { pn = parent.numProperties; } catch (epn) {}
        for (pi = 1; pi <= pn; pi++) {
            try { pc = parent.property(pi); } catch (epc) { continue; }
            if (pc === propBase) { return matchName + '#' + pi; }
        }
    }
    return matchName;
}

// Build the 'id' string: 'L<layerIndex>/<matchName#index path>'.
// The propertyIndex suffix disambiguates repeated sibling Shape groups/Paths.
// parseId remains backwards-compatible with older matchName-only IDs.
function _buildId(prop, layerIndex) {
    var names = [];
    var cur = prop;
    while (cur !== null && cur.propertyDepth > 0) {
        names.unshift(_idSegmentForPropertyBase(cur));
        cur = cur.parentProperty;
    }
    return 'L' + layerIndex + '/' + names.join('/');
}

// Build layer_path: 'CompName/LayerName/...propertyNames'.
// We walk parentProperty chain collecting display names.
function _buildLayerPath(prop, layerName, compName) {
    var names = [];
    var cur = prop;
    while (cur !== null && cur.propertyDepth > 0) {
        names.unshift(cur.name);
        cur = cur.parentProperty;
    }
    return compName + '/' + layerName + '/' + names.join('/');
}

function _layerStableId(layer) {
    try {
        if (typeof layer.id === 'number' && layer.id > 0) {
            return layer.id;
        }
    } catch (eLayerId) {}
    return 0;
}

// Capture layer transform at tStart. Returns a LayerXform object.
// 'layer' must be an AVLayer. Reads via valueAtTime to avoid keyframe noise.
function _captureLayerXform(layer, tSec) {
    var xform = {};
    var is3d = false;
    // VERIFY-IN-AE: AVLayer.threeDLayer exists per AE scripting guide.
    if (typeof layer.threeDLayer !== 'undefined') {
        is3d = layer.threeDLayer;
    }

    // Helper: safely sample a transform property by matchName.
    function sample(matchName) {
        try {
            var p = layer.property('ADBE Transform Group').property(matchName);
            return p.valueAtTime(tSec, false);
        } catch (e) {
            return null;
        }
    }

    var ap  = sample('ADBE Anchor Point');
    var pos = sample('ADBE Position');
    var sc  = sample('ADBE Scale');
    var op  = sample('ADBE Opacity');

    // Rotation: Z in 2D, or X/Y/Z for 3D layers.
    var rot;
    if (is3d) {
        var rx = sample('ADBE Rotate X');
        var ry = sample('ADBE Rotate Y');
        var rz = sample('ADBE Rotate Z');
        if (rx !== null && ry !== null && rz !== null) {
            rot = [rx, ry, rz];
        } else {
            rot = [0, 0, 0];
        }
    } else {
        var rz2 = sample('ADBE Rotate Z');
        rot = (rz2 !== null) ? [rz2] : [0];
    }

    xform.anchor_point = ap  ? [].concat(ap)  : (is3d ? [0,0,0] : [0,0]);
    xform.position     = pos ? [].concat(pos) : (is3d ? [0,0,0] : [0,0]);
    xform.scale        = sc  ? [].concat(sc)  : (is3d ? [100,100,100] : [100,100]);
    xform.rotation     = rot;
    xform.opacity      = (op !== null) ? op : 100.0;
    return xform;
}

// Normalize a value from valueAtTime into a flat JS array of numbers.
// For scalar, wrap in []. For arrays, return directly.
function _valueToArray(v, dimensions) {
    if (dimensions === 1) {
        return [v];
    } else {
        // AE returns an array for multi-dimensional properties.
        var arr = [];
        var i;
        for (i = 0; i < dimensions; i++) {
            arr.push(v[i]);
        }
        return arr;
    }
}

function _shapeToArray(shapeValue) {
    if (!shapeValue || !shapeValue.vertices) {
        throw new Error('shape/path value could not be read');
    }

    var arr = [];
    var verts = shapeValue.vertices;
    var ins = shapeValue.inTangents || [];
    var outs = shapeValue.outTangents || [];
    var n = verts.length;
    var i, v, tin, tout;

    arr.push(shapeValue.closed ? 1 : 0);
    arr.push(n);
    for (i = 0; i < n; i++) {
        v = verts[i] || [0, 0];
        tin = ins[i] || [0, 0];
        tout = outs[i] || [0, 0];
        arr.push(Number(v[0]) || 0);
        arr.push(Number(v[1]) || 0);
        arr.push(Number(tin[0]) || 0);
        arr.push(Number(tin[1]) || 0);
        arr.push(Number(tout[0]) || 0);
        arr.push(Number(tout[1]) || 0);
    }
    return arr;
}

function _shapeIsUsable(shapeValue) {
    try {
        return shapeValue &&
            shapeValue.vertices &&
            shapeValue.inTangents &&
            shapeValue.outTangents &&
            shapeValue.vertices.length > 0 &&
            shapeValue.vertices.length === shapeValue.inTangents.length &&
            shapeValue.vertices.length === shapeValue.outTangents.length;
    } catch (e) {
        return false;
    }
}

function _shapeTopologySignatureFromFlat(flat) {
    if (!flat || flat.length < 2) { return 'invalid'; }
    return 'closed=' + Math.round(flat[0]) + ',n=' + Math.round(flat[1]);
}

function _point2(p) {
    var x = Number(p[0]);
    var y = Number(p[1]);
    if (!isFinite(x)) { x = 0; }
    if (!isFinite(y)) { y = 0; }
    return [x, y];
}

function _add2(a, b) {
    return [a[0] + b[0], a[1] + b[1]];
}

function _sub2(a, b) {
    return [a[0] - b[0], a[1] - b[1]];
}

function _mul2(a, s) {
    return [a[0] * s, a[1] * s];
}

function _dist2(a, b) {
    var dx = a[0] - b[0];
    var dy = a[1] - b[1];
    return Math.sqrt(dx * dx + dy * dy);
}

function _dist2Sq(a, b) {
    var dx = a[0] - b[0];
    var dy = a[1] - b[1];
    return dx * dx + dy * dy;
}

function _cubicPoint2(p0, p1, p2, p3, t) {
    var u = 1.0 - t;
    var uu = u * u;
    var tt = t * t;
    var uuu = uu * u;
    var ttt = tt * t;
    return [
        p0[0] * uuu + 3.0 * p1[0] * uu * t + 3.0 * p2[0] * u * tt + p3[0] * ttt,
        p0[1] * uuu + 3.0 * p1[1] * uu * t + 3.0 * p2[1] * u * tt + p3[1] * ttt
    ];
}

function _pushDensePoint(points, p) {
    if (points.length > 0 && _dist2(points[points.length - 1], p) < 0.000001) {
        return;
    }
    points.push(p);
}

function _shapeToDensePolyline(shapeValue) {
    if (!_shapeIsUsable(shapeValue)) {
        throw new Error('shape/path value has malformed vertices or tangents');
    }

    var verts = shapeValue.vertices;
    var ins = shapeValue.inTangents || [];
    var outs = shapeValue.outTangents || [];
    var n = verts.length;
    var closed = shapeValue.closed === true;
    var segCount = closed ? n : Math.max(0, n - 1);
    var points = [];
    var i, next, p0, p1, p2, p3, controlLen, chordLen, divs, j, t;

    if (segCount <= 0) {
        points.push(_point2(verts[0]));
        return points;
    }

    for (i = 0; i < segCount; i++) {
        next = (i + 1) % n;
        p0 = _point2(verts[i]);
        p3 = _point2(verts[next]);
        p1 = _add2(p0, _point2(outs[i] || [0, 0]));
        p2 = _add2(p3, _point2(ins[next] || [0, 0]));
        controlLen = _dist2(p0, p1) + _dist2(p1, p2) + _dist2(p2, p3);
        chordLen = _dist2(p0, p3);
        divs = Math.ceil(Math.max(6, Math.min(32, (controlLen + Math.abs(controlLen - chordLen) * 2.0) / 10.0)));

        if (i === 0) { _pushDensePoint(points, p0); }
        for (j = 1; j <= divs; j++) {
            t = j / divs;
            if (closed && i === segCount - 1 && j === divs) { continue; }
            _pushDensePoint(points, _cubicPoint2(p0, p1, p2, p3, t));
        }
    }

    if (points.length < 1) { points.push(_point2(verts[0])); }
    return points;
}

function _polylineArea(points) {
    var area = 0;
    var n = points.length;
    var i, a, b;
    if (n < 3) { return 0; }
    for (i = 0; i < n; i++) {
        a = points[i];
        b = points[(i + 1) % n];
        area = area + (a[0] * b[1] - b[0] * a[1]);
    }
    return area * 0.5;
}

function _resamplePolylineByArc(points, closed, count) {
    var n = points.length;
    var out = [];
    var segCount = closed ? n : Math.max(0, n - 1);
    var segLens = [];
    var total = 0;
    var i, a, b, len;

    if (count < 1) { count = 1; }
    if (n < 1) {
        for (i = 0; i < count; i++) { out.push([0, 0]); }
        return out;
    }
    if (segCount <= 0) {
        for (i = 0; i < count; i++) { out.push([points[0][0], points[0][1]]); }
        return out;
    }

    for (i = 0; i < segCount; i++) {
        a = points[i];
        b = points[(i + 1) % n];
        len = _dist2(a, b);
        segLens.push(len);
        total = total + len;
    }
    if (total < 0.000001) {
        for (i = 0; i < count; i++) { out.push([points[0][0], points[0][1]]); }
        return out;
    }

    var targetCountDenom = closed ? count : Math.max(count - 1, 1);
    var segIdx = 0;
    var segStartDist = 0;
    var target, ratio, p0, p1;
    for (i = 0; i < count; i++) {
        target = total * i / targetCountDenom;
        while (segIdx < segLens.length - 1 &&
                segStartDist + segLens[segIdx] < target - 0.000001) {
            segStartDist = segStartDist + segLens[segIdx];
            segIdx = segIdx + 1;
        }
        ratio = segLens[segIdx] > 0.000001
            ? (target - segStartDist) / segLens[segIdx]
            : 0;
        if (ratio < 0) { ratio = 0; }
        if (ratio > 1) { ratio = 1; }
        p0 = points[segIdx];
        p1 = points[(segIdx + 1) % n];
        out.push([p0[0] + (p1[0] - p0[0]) * ratio,
                  p0[1] + (p1[1] - p0[1]) * ratio]);
    }
    return out;
}

function _reversePoints(points) {
    var out = [];
    var i;
    for (i = points.length - 1; i >= 0; i--) {
        out.push([points[i][0], points[i][1]]);
    }
    return out;
}

function _shiftPoints(points, shift) {
    var out = [];
    var n = points.length;
    var i, idx;
    for (i = 0; i < n; i++) {
        idx = (i + shift) % n;
        out.push([points[idx][0], points[idx][1]]);
    }
    return out;
}

function _pointSetCost(a, b) {
    var n = Math.min(a.length, b.length);
    var sum = 0;
    var i;
    if (n < 1) { return 999999999; }
    for (i = 0; i < n; i++) {
        sum = sum + _dist2Sq(a[i], b[i]);
    }
    return sum / n;
}

function _alignCanonicalPoints(points, closed, state) {
    if (!closed || !state || !state.prev || state.prev.length !== points.length) {
        return points;
    }

    var area = _polylineArea(points);
    var base = points;
    if (state.areaSign !== 0 && area !== 0 &&
            ((area > 0 ? 1 : -1) !== state.areaSign)) {
        base = _reversePoints(points);
    }

    var best = base;
    var bestCost = 999999999;
    var n = base.length;
    var shift, cand, cost;
    for (shift = 0; shift < n; shift++) {
        cand = _shiftPoints(base, shift);
        cost = _pointSetCost(cand, state.prev);
        if (cost < bestCost) {
            bestCost = cost;
            best = cand;
        }
    }
    return best;
}

function _canonicalVertexCount(maxVertices, requested) {
    var count = requested && requested > 0 ? Math.round(requested) : 0;
    if (count < 1) {
        count = Math.round(maxVertices || 0);
    }
    if (count < 8) { count = 8; }
    if (count > 256) { count = 256; }
    return count;
}

function _canonicalPointsToFlat(points, closed) {
    var n = points.length;
    var flat = [closed ? 1 : 0, n];
    var i, prev, next, cur, tangent, maxLen, tangentLen, scale;
    for (i = 0; i < n; i++) {
        cur = points[i];
        if (closed) {
            prev = points[(i + n - 1) % n];
            next = points[(i + 1) % n];
        } else {
            prev = (i > 0) ? points[i - 1] : cur;
            next = (i + 1 < n) ? points[i + 1] : cur;
        }

        tangent = _mul2(_sub2(next, prev), 1.0 / 6.0);
        maxLen = Math.min(_dist2(prev, cur), _dist2(cur, next)) * 0.45;
        tangentLen = Math.sqrt(tangent[0] * tangent[0] + tangent[1] * tangent[1]);
        if (tangentLen > maxLen && tangentLen > 0.000001) {
            scale = maxLen / tangentLen;
            tangent = _mul2(tangent, scale);
        }
        if (!closed && (i === 0 || i === n - 1)) {
            tangent = [0, 0];
        }

        flat.push(cur[0]);
        flat.push(cur[1]);
        flat.push(-tangent[0]);
        flat.push(-tangent[1]);
        flat.push(tangent[0]);
        flat.push(tangent[1]);
    }
    return flat;
}

function _canonicalShapeToFlat(shapeValue, count, state) {
    var closed = shapeValue.closed === true;
    var dense = _shapeToDensePolyline(shapeValue);
    var sampled = _resamplePolylineByArc(dense, closed, count);
    if (closed && state && !state.hasAreaSign) {
        var a = _polylineArea(sampled);
        state.areaSign = a < 0 ? -1 : (a > 0 ? 1 : 0);
        state.hasAreaSign = true;
    }
    sampled = _alignCanonicalPoints(sampled, closed, state);
    if (state) { state.prev = sampled; }
    return _canonicalPointsToFlat(sampled, closed);
}

function _prepareShapeFrameSamples(prop, tStart, tEnd, frameDur, opts) {
    opts = opts || {};
    var raw = [];
    var t = tStart;
    var maxVertices = 0;
    var topologies = {};
    var topologyList = [];
    var baselineFlat = null;
    var baselineClosed = null;
    var unstable = false;
    var progressCb = (typeof opts._shapeSamplingProgress === 'function')
        ? opts._shapeSamplingProgress : null;
    var progressTotal = _countFrameSamples(tStart, tEnd, frameDur);
    var progressDone = 0;
    var i, sh, flat, sig, nVerts, closedFlag;

    while (t <= tEnd + 1e-9) {
        sh = prop.valueAtTime(t, false);
        if (!_shapeIsUsable(sh)) {
            throw new Error('shape/path value is malformed at t=' + t.toFixed(4) + 's');
        }
        flat = _shapeToArray(sh);
        sig = _shapeTopologySignatureFromFlat(flat);
        if (!topologies[sig]) {
            topologies[sig] = true;
            topologyList.push(sig);
        }
        nVerts = Math.round(flat[1]);
        closedFlag = Math.round(flat[0]);
        if (nVerts > maxVertices) { maxVertices = nVerts; }
        if (baselineFlat === null) {
            baselineFlat = flat;
            baselineClosed = closedFlag;
        } else {
            if (_shapeTopologyMismatch(baselineFlat, flat)) { unstable = true; }
            if (baselineClosed !== closedFlag) {
                throw new Error('shape/path open-closed state changes at t=' +
                    t.toFixed(4) + 's; canonical path bake requires a stable closed flag');
            }
        }
        raw.push({ t_sec: t, shape: sh, flat: flat });
        progressDone = progressDone + 1;
        if (progressCb) {
            try { progressCb(progressDone, progressTotal); } catch (ep) {}
        }
        t = t + frameDur;
        if (t > tEnd + frameDur * 0.5) { break; }
    }

    if (raw.length < 1) {
        throw new Error('shape/path produced no samples');
    }

    if (!unstable) {
        var stableSamples = [];
        for (i = 0; i < raw.length; i++) {
            stableSamples.push({ t_sec: raw[i].t_sec, v: raw[i].flat });
        }
        return {
            samples: stableSamples,
            dimensions: stableSamples[0].v.length,
            canonicalized: false,
            variable_topology: false,
            max_vertex_count: Math.round(stableSamples[0].v[1]),
            canonical_vertex_count: Math.round(stableSamples[0].v[1]),
            source_topologies: topologyList,
            method: 'shape_flat_exact'
        };
    }

    // Keep AE on read-only sampling for variable topology. The external solver
    // owns all canonicalization/reduction decisions and can validate the final
    // reduced outline against these raw source frames. dimensions is the maximum
    // possible vector length; individual samples intentionally retain their
    // original shape_flat length.
    var rawSamples = [];
    for (i = 0; i < raw.length; i++) {
        rawSamples.push({
            t_sec: raw[i].t_sec,
            v: raw[i].flat
        });
    }
    return {
        samples: rawSamples,
        dimensions: 2 + 6 * maxVertices,
        canonicalized: false,
        variable_topology: true,
        max_vertex_count: maxVertices,
        canonical_vertex_count: 0,
        source_topologies: topologyList,
        method: 'shape_flat_raw_variable'
    };
}

function _shapeTopologyMismatch(baseFlat, currentFlat) {
    if (!baseFlat || !currentFlat || baseFlat.length < 2 || currentFlat.length < 2) {
        return 'invalid shape_flat header';
    }
    if (baseFlat.length !== currentFlat.length) {
        return 'flat length changed from ' + baseFlat.length + ' to ' + currentFlat.length;
    }
    if (Math.round(baseFlat[0]) !== Math.round(currentFlat[0])) {
        return 'closed flag changed';
    }
    if (Math.round(baseFlat[1]) !== Math.round(currentFlat[1])) {
        return 'vertex count changed from ' + Math.round(baseFlat[1]) +
            ' to ' + Math.round(currentFlat[1]);
    }
    return '';
}

function _assertShapeTopologyStable(baseFlat, currentFlat, propName, tSec) {
    var mismatch = _shapeTopologyMismatch(baseFlat, currentFlat);
    if (mismatch) {
        throw new Error('shape/path topology changed for "' + propName + '" at t=' +
            tSec.toFixed(4) + 's: ' + mismatch);
    }
}

function _angleDegFromVector(dx, dy) {
    return Math.atan2(dy, dx) * 180.0 / Math.PI;
}

function _unwrapAngleDeg(prev, current) {
    if (prev === null || typeof prev !== 'number') { return current; }
    while (current - prev > 180.0) { current = current - 360.0; }
    while (current - prev < -180.0) { current = current + 360.0; }
    return current;
}

function _sourcePointCacheKey(layer, tSec, point) {
    var layerIndex = 'L?';
    try { layerIndex = 'L' + layer.index; } catch (eli) {}
    return layerIndex + '@' + tSec.toFixed(9) + ':' +
        String(point[0]) + ',' + String(point[1]);
}

function _sourcePointsToCompAtTime(layer, comp, points, tSec, cache) {
    var out = [];
    var missing = [];
    var keys = [];
    var i, key, cached;
    for (i = 0; i < points.length; i++) {
        key = _sourcePointCacheKey(layer, tSec, points[i]);
        keys[i] = key;
        cached = cache ? cache[key] : null;
        if (cached && cached.length >= 2) {
            out[i] = [cached[0], cached[1]];
        } else {
            missing.push(i);
        }
    }
    if (missing.length === 0) { return out; }

    var oldTime = comp.time;
    var setOk = false;
    var errText = '';
    try {
        comp.time = tSec;
        setOk = (Math.abs(comp.time - tSec) < 0.000001);
    } catch (et) {
        errText = et.toString();
    }
    if (setOk) {
        for (i = 0; i < missing.length; i++) {
            var mi = missing[i];
            var ptOut = null;
            try {
                ptOut = layer.sourcePointToComp(points[mi]);
            } catch (es) {
                errText = es.toString();
            }
            if (ptOut && ptOut.length >= 2) {
                out[mi] = [ptOut[0], ptOut[1]];
                if (cache) { cache[keys[mi]] = [ptOut[0], ptOut[1]]; }
            }
        }
    }
    try { comp.time = oldTime; } catch (er) {}

    if (!setOk) {
        throw new Error('could not set comp.time while sampling parented transform in comp space' +
            (errText ? ': ' + errText : ''));
    }
    for (i = 0; i < out.length; i++) {
        if (!out[i] || out[i].length < 2) {
            throw new Error('sourcePointToComp failed while sampling parented transform in comp space' +
                (errText ? ': ' + errText : ''));
        }
    }
    return out;
}

function _sourcePointToCompAtTime(layer, comp, point, tSec, cache) {
    return _sourcePointsToCompAtTime(layer, comp, [point], tSec, cache)[0];
}

// Sample the comp-space location of a 2D layer's anchor point. AE scripting
// exposes sourcePointToComp() at the current comp time, so preserve and restore
// comp.time around the call.
function _sampleLayerAnchorInComp(layer, comp, tSec, dimensions, cache) {
    var anchor = [0, 0];
    try {
        var tr = layer.property('ADBE Transform Group');
        var ap = tr.property('ADBE Anchor Point').valueAtTime(tSec, false);
        anchor = [ap[0], ap[1]];
    } catch (ea) {}

    var out = _sourcePointToCompAtTime(layer, comp, anchor, tSec, cache);
    if (dimensions >= 3) { return [out[0], out[1], 0]; }
    return [out[0], out[1]];
}

function _sampleLayerRotationInComp(layer, comp, tSec, prevAngle, cache) {
    var anchor = [0, 0];
    try {
        var tr = layer.property('ADBE Transform Group');
        var ap = tr.property('ADBE Anchor Point').valueAtTime(tSec, false);
        anchor = [ap[0], ap[1]];
    } catch (ea) {}

    var pts = _sourcePointsToCompAtTime(
        layer, comp,
        [anchor, [anchor[0] + 100.0, anchor[1]]],
        tSec, cache);
    var p0 = pts[0];
    var p1 = pts[1];
    var dx = p1[0] - p0[0];
    var dy = p1[1] - p0[1];
    if (Math.sqrt(dx * dx + dy * dy) < 0.000001) {
        throw new Error('cannot sample comp-space rotation; transformed local X axis has zero length');
    }
    var angle = _angleDegFromVector(dx, dy);
    return _unwrapAngleDeg(prevAngle, angle);
}

// Resolve the actual samples-per-frame count from opts.sampleMode and comp state.
// sampleMode: 'auto' | 'frame' | 'sub3' | 'sub5' | undefined
//   'auto'  — 1 unless comp.motionBlur is true, then 3.
//   'frame' — always 1 (frame-centre only).
//   'sub3'  — always 3 sub-frame samples.
//   'sub5'  — always 5 sub-frame samples.
//   undefined / fallback — use opts.samplesPerFrame (backward-compatible).
// Verified: comp.motionBlur (compitem.md p.239), a boolean read-only attribute.
function resolveSamplesPerFrame(opts, comp) {
    var mode = opts.sampleMode;
    if (mode === 'frame') { return 1; }
    if (mode === 'sub3')  { return 3; }
    if (mode === 'sub5')  { return 5; }
    if (mode === 'auto') {
        var mblur = false;
        try { mblur = comp.motionBlur; } catch (e) {}
        return mblur ? 3 : 1;
    }
    // Fallback: honour explicit samplesPerFrame (or default 1).
    return opts.samplesPerFrame || 1;
}

function _countFrameSamples(tStart, tEnd, frameDur) {
    var total = 0;
    var t = tStart;
    while (t <= tEnd + 1e-9) {
        total = total + 1;
        t = t + frameDur;
        if (t > tEnd + frameDur * 0.5) { break; }
    }
    return total > 0 ? total : 1;
}

function _selectedLayerMapFromSampleProps(props) {
    var map = {};
    var i, layer;
    for (i = 0; i < props.length; i++) {
        try {
            layer = _containingLayer(props[i]);
            map[String(layer.index)] = true;
        } catch (eLayer) {}
    }
    return map;
}

function _sampleLayerParentIsSelected(layer, selectedLayerMap) {
    var parent = null;
    try { parent = layer.parent; } catch (eParent) {}
    if (!parent) { return false; }
    try { return selectedLayerMap[String(parent.index)] === true; } catch (eIdx) {}
    return false;
}

// Main entrypoint.
// props: Array of Property objects (AE property references).
// opts:  { sampleMode, samplesPerFrame, tStartSec, tEndSec, toleranceUnits,
//          toleranceScreenPx, requestId }
//   sampleMode: 'auto'|'frame'|'sub3'|'sub5' (see resolveSamplesPerFrame).
//   samplesPerFrame: used if sampleMode is absent (backward-compatible).
// Returns a SampleBundle JS object matching the JSON schema in docs/protocol/PROTOCOL.md.
function collectSampleBundle(props, opts) {
    // Resolve spf first so we can pass comp from firstLayer below.
    var firstLayer0 = _containingLayer(props[0]);
    var comp0       = firstLayer0.containingComp;
    var spf         = resolveSamplesPerFrame(opts, comp0);
    var tStart      = opts.tStartSec        || 0;
    var tEnd        = opts.tEndSec          || 0;
    var tolerance   = opts.toleranceUnits   || 0.5;
    var tolPx       = opts.toleranceScreenPx || 0.0;
    var requestId   = opts.requestId        || ('req-' + (new Date().getTime()));
    var flattenParentedPosition = (opts.flattenParentedPosition === true);
    var preserveSelectedParenting = (opts.preserveSelectedParenting === true);

    var firstLayer  = firstLayer0;
    var comp        = comp0;

    var fps         = comp.frameRate;
    var frameDur    = comp.frameDuration;
    var shutterAng  = comp.shutterAngle;   // integer degrees [0..720]
    var shutterPh   = comp.shutterPhase;   // integer degrees [-360..360]
    var mblur       = comp.motionBlur;
    var progressCb  = (typeof opts.progressCallback === 'function')
        ? opts.progressCallback : null;
    var frameTotal  = _countFrameSamples(tStart, tEnd, frameDur);
    var propTotal   = props.length;
    var totalSlots  = Math.max(1, (opts.sampleReadTotal || (propTotal * frameTotal * spf)));
    var slotsDone   = 0;
    var sampleStride = Math.max(1, Math.floor(frameTotal / 16));
    var compPointCache = {};

    function emitSamplingProgress(stage, propIndex, label, sampleIndex, sampleTotal, elapsedMs) {
        if (!progressCb) { return; }
        try {
            progressCb({
                stage: stage,
                property_index: propIndex,
                property_total: propTotal,
                property_label: label,
                sample_index: sampleIndex,
                sample_total: sampleTotal,
                read_index: slotsDone,
                read_total: totalSlots,
                elapsed_ms: elapsedMs || 0
            });
        } catch (epc) {}
    }

    // Build CompInfo.
    var compInfo = {
        fps:                 fps,
        duration_sec:        comp.duration,
        width:               comp.width,
        height:              comp.height,
        pixel_aspect:        comp.pixelAspect,
        shutter_angle_deg:   shutterAng,
        shutter_phase_deg:   shutterPh,
        motion_blur_enabled: mblur,
        work_area_start_sec: comp.workAreaStart,
        work_area_end_sec:   comp.workAreaStart + comp.workAreaDuration
    };

    // Build SolverConfig from opts.
    var solverConfig = {
        tolerance:             tolerance,
        tolerance_screen_px:   tolPx,
        weight_pos:            1.0,
        weight_vel:            0.1,
        weight_acc:            0.01,
        weight_curv:           0.0,
        weight_screen:         0.0,
        allow_hold:            true,
        allow_linear:          true,
        allow_bezier:          true,
        allow_shape_temporal_bezier: false,
        allow_path_spatial_fit: false,
        allow_path_replacement_fit: false,
        path_replacement_prefer_vertices: false,
        motion_smooth_source_fidelity:
            opts.motionSmoothSourceFidelity === true,
        motion_smooth_tolerance: opts.motionSmoothTolerance || 3.0,
        motion_smooth_bezier_x1: (typeof opts.motionSmoothBezierX1 === 'number') ? opts.motionSmoothBezierX1 : 0.33,
        motion_smooth_bezier_y1: (typeof opts.motionSmoothBezierY1 === 'number') ? opts.motionSmoothBezierY1 : 0.0,
        motion_smooth_bezier_x2: (typeof opts.motionSmoothBezierX2 === 'number') ? opts.motionSmoothBezierX2 : 0.67,
        motion_smooth_bezier_y2: (typeof opts.motionSmoothBezierY2 === 'number') ? opts.motionSmoothBezierY2 : 1.0,
        motion_path_smoothing_tolerance: (typeof opts.motionPathSmoothingTolerance === 'number') ? opts.motionPathSmoothingTolerance : 3.0,
        motion_path_accuracy_tolerance: (typeof opts.motionPathAccuracyTolerance === 'number') ? opts.motionPathAccuracyTolerance : 1.5,
        motion_path_preserve_bounds: opts.motionPathPreserveBounds === true,
        motion_path_bounds_tolerance: (typeof opts.motionPathBoundsTolerance === 'number') ? opts.motionPathBoundsTolerance : 0.0,
        motion_path_preserve_sharp_points: opts.motionPathPreserveSharpPoints !== false,
        motion_path_sharp_angle_deg: (typeof opts.motionPathSharpAngleDeg === 'number') ? opts.motionPathSharpAngleDeg : 75.0,
        motion_path_respect_keyed_frames: opts.motionPathRespectKeyedFrames === true,
        path_preserve_sharp_corners: true,
        path_sharp_corner_angle_deg: 90.0,
        path_sharp_corner_tolerance: 1.5,
        path_replacement_min_vertices: 4,
        path_replacement_max_vertices: 0,
        path_replacement_max_key_growth_ratio: 4.0,
        path_replacement_min_vertex_reduction_ratio: 0.20,
        path_specific_max_gap: 0,
        shape_temporal_bezier_attempt_threshold_ratio: -1.0,
        min_influence:         0.1,
        max_influence:         100.0,
        max_iters_per_segment: 100,
        min_segment_frames:    2,
        max_keys_hint:         0,
        parallel_jobs:         0,
        verbose:               false
    };

    // Collect PropertySamples for each property.
    var propertySamplesArr = [];
    var i, prop;
    var flattenLayerMap = {};
    var hasFlattenLayer = false;
    var hasShapeTemporalBezierCandidate = false;
    var selectedLayerMap = preserveSelectedParenting
        ? _selectedLayerMapFromSampleProps(props) : {};

    if (flattenParentedPosition) {
        var fp;
        for (i = 0; i < props.length; i++) {
            fp = props[i];
            var fpClassif = _classifyProperty(fp);
            try {
                var fpLayer = _containingLayer(fp);
                if (_isParentedPositionFlattenCandidate(fp, fpClassif) &&
                        !(preserveSelectedParenting &&
                          _sampleLayerParentIsSelected(fpLayer, selectedLayerMap))) {
                    flattenLayerMap[fpLayer.index] = true;
                    hasFlattenLayer = true;
                }
            } catch (efp) {}
        }
    }

    for (i = 0; i < props.length; i++) {
        prop = props[i];

        var layer      = _containingLayer(prop);
        var layerIndex = layer.index;
        var layerName  = layer.name;
        var compName   = comp.name;

        var classif    = _classifyProperty(prop);
        var dims       = classif.dimensions;
        var propLabel  = _buildLayerPath(prop, layerName, compName);
        var propStartMs = (new Date()).getTime();
        emitSamplingProgress('property_start', i, propLabel, 0, frameTotal * spf, 0);
        var shapeBaseline = null;
        var shapeFrameSamples = null;
        var shapeFrameIndex = 0;
        if (classif.isShape) {
            if (spf === 1) {
                hasShapeTemporalBezierCandidate = true;
            }
            if (spf === 1) {
                var shapeSlotBase = slotsDone;
                opts._shapeSamplingProgress = function (done, total) {
                    slotsDone = shapeSlotBase + done;
                    if (done === 1 || done >= total || done % sampleStride === 0) {
                        emitSamplingProgress(
                            'shape_sample', i, propLabel, done, total,
                            (new Date()).getTime() - propStartMs);
                    }
                };
                try {
                    shapeFrameSamples = _prepareShapeFrameSamples(
                        prop, tStart, tEnd, frameDur, opts);
                } finally {
                    opts._shapeSamplingProgress = null;
                }
                shapeBaseline = shapeFrameSamples.samples[0].v;
                dims = shapeFrameSamples.dimensions;
            } else {
                shapeBaseline = _shapeToArray(prop.valueAtTime(tStart, false));
                dims = shapeBaseline.length;
            }
        }

        var isSepFollower = false;
        var isSepLeader   = false;
        // Verified: isSeparationFollower and isSeparationLeader (property.md pp.344-363).
        try { isSepFollower = prop.isSeparationFollower; } catch (ef) {}
        try { isSepLeader   = prop.isSeparationLeader;   } catch (el) {}

        var keepSelectedParent =
            preserveSelectedParenting &&
            _sampleLayerParentIsSelected(layer, selectedLayerMap);
        var flattenThisPosition =
            flattenParentedPosition &&
            !keepSelectedParent &&
            _isParentedPositionFlattenCandidate(prop, classif);
        var flattenThisRotation =
            flattenParentedPosition &&
            _isRotationZProperty(prop) &&
            _is2DLayer(layer) &&
            flattenLayerMap[layerIndex] === true;
        var strictParentFlattenRotation =
            flattenParentedPosition &&
            hasFlattenLayer &&
            _isRotationZProperty(prop) &&
            _is2DLayer(layer);

        // Optional separated-dimensions path: for unkeyed spatial isSeparationLeader
        // properties, enable Separate Dimensions and sample each follower axis
        // independently. The default product path preserves unified Position and
        // lets the solver emit one shared temporal ease plus per-dimension spatial
        // tangents.
        //
        // opts.autoSeparateForBake = true enables this explicit destructive mode.
        // Verified APIs:
        //   prop.dimensionsSeparated read/write boolean (property.md p.190).
        //   prop.isSeparationLeader read-only boolean (property.md p.361).
        //   prop.getSeparationFollower(dim) -> Property (property.md p.694).
        //   prop.numKeys (property.md p.435) — guard against pre-existing keys.
        if (classif.is_spatial && isSepLeader &&
                opts.autoSeparateForBake === true && !flattenThisPosition) {
            var nk0 = 0;
            try { nk0 = prop.numKeys; } catch (enk) {}
            if (nk0 === 0) {
                // Enable separation (permanent for this bake — user was warned).
                try { prop.dimensionsSeparated = true; } catch (eSep) {}
            }
        }

        // Check if separation is now active so we can emit per-axis samples.
        var doSep = false;
        if (classif.is_spatial && isSepLeader && !flattenThisPosition) {
            try { doSep = prop.dimensionsSeparated; } catch (eds) {}
        }

        var parentId  = _buildId(prop, layerIndex);
        var exprText  = '';
        try { exprText = prop.expression; } catch (ee) {}
        var exprHash  = exprText ? _hashString(exprText) : '';
        var layerXform = (flattenThisPosition || classif.isShape)
            ? null
            : _captureLayerXform(layer, tStart);

        if (doSep) {
            // Emit one PropertySamples per axis (OneD followers).
            // Stable ID: '<parentId>/sep/<dim>'  where dim = 0-based index.
            var axisNames = (dims === 3) ? ['X', 'Y', 'Z'] : ['X', 'Y'];
            var d;
            var sepDone = 0;
            var sepTotal = Math.max(1, frameTotal * dims * spf);
            for (d = 0; d < dims; d++) {
                var follower = null;
                try { follower = prop.getSeparationFollower(d); } catch (egf) {}
                if (!follower) { continue; }

                var sepId = parentId + '/sep/' + d;
                var sepInfo = {
                    id:           sepId,
                    match_name:   follower.matchName,
                    display_name: prop.name + ' (' + axisNames[d] + ')',
                    layer_path:   _buildLayerPath(prop, layerName, compName) +
                                  '/' + axisNames[d],
                    layer_id:     _layerStableId(layer),
                    layer_index:  layerIndex,
                    kind:         'Scalar',
                    dimensions:   1,
                    is_spatial:   false,
                    is_separated: true,
                    source_key_times: _sourceKeyTimesInRange(
                        follower, tStart, tEnd),
                    units_label:  '',
                    min_value:    [],
                    max_value:    []
                };

                var sepSamples = [];
                var ts = tStart;
                while (ts <= tEnd + 1e-9) {
                    var sepSampleV;
                    if (spf === 1) {
                        var fsv;
                        try { fsv = follower.valueAtTime(ts, false); }
                        catch (efv) { fsv = 0; }
                        sepSampleV = [fsv];
                    } else {
                        // Sub-frame shutter sampling (mirrors unified path, docs/protocol/PROTOCOL.md).
                        var sepA = shutterAng / 360.0;
                        var sepP = shutterPh  / 360.0;
                        var sepOpen = ts - sepA * (sepP + 0.5) * frameDur;
                        sepSampleV = [];
                        var sk;
                        for (sk = 0; sk < spf; sk++) {
                            var subTs = sepOpen + (sk + 0.5) * frameDur * sepA / spf;
                            var fsvk;
                            try { fsvk = follower.valueAtTime(subTs, false); }
                            catch (esk) { fsvk = 0; }
                            sepSampleV.push(fsvk);
                        }
                    }
                    sepSamples.push({ t_sec: ts, v: sepSampleV });
                    sepDone = sepDone + spf;
                    slotsDone = slotsDone + spf;
                    if (sepDone === spf || sepDone >= sepTotal ||
                            sepDone % sampleStride === 0) {
                        emitSamplingProgress(
                            'sample', i, propLabel, sepDone, sepTotal,
                            (new Date()).getTime() - propStartMs);
                    }
                    ts = ts + frameDur;
                    if (ts > tEnd + frameDur * 0.5) { break; }
                }

                propertySamplesArr.push({
                    property:             sepInfo,
                    t_start_sec:          tStart,
                    t_end_sec:            tEnd,
                    samples_per_frame:    spf,
                    samples:              sepSamples,
                    layer_xform_at_start: layerXform,
                    hash_of_expression:   exprHash
                });
            }
            emitSamplingProgress(
                'property_done', i, propLabel, sepTotal, sepTotal,
                (new Date()).getTime() - propStartMs);
            continue; // Skip the unified-property path below.
        }

        var propInfo = {
            id:           parentId,
            match_name:   prop.matchName,
            display_name: prop.name,
            layer_path:   _buildLayerPath(prop, layerName, compName),
            layer_id:     _layerStableId(layer),
            layer_index:  layerIndex,
            kind:         classif.kind,
            dimensions:   dims,
            is_spatial:   classif.is_spatial,
            is_separated: (isSepFollower || doSep),
            source_key_times: _sourceKeyTimesInRange(prop, tStart, tEnd),
            flatten_parented_position: flattenThisPosition,
            flatten_parented_rotation: flattenThisRotation,
            parent_flatten_strict_rotation: strictParentFlattenRotation,
            preserve_selected_parent: keepSelectedParent,
            sample_space: flattenThisPosition ? 'comp' :
                (flattenThisRotation ? 'comp_angle' : 'property'),
            writeback_mode: flattenThisPosition ? 'unparent_position' :
                (flattenThisRotation ? 'unparent_rotation' : 'normal'),
            units_label:  classif.isShape ? 'shape_flat' : '',
            shape_canonicalized: shapeFrameSamples ? shapeFrameSamples.canonicalized : false,
            shape_variable_topology: shapeFrameSamples ? shapeFrameSamples.variable_topology : false,
            shape_canonical_method: shapeFrameSamples ? shapeFrameSamples.method : '',
            shape_canonical_vertex_count: shapeFrameSamples ? shapeFrameSamples.canonical_vertex_count : 0,
            shape_max_vertex_count: shapeFrameSamples ? shapeFrameSamples.max_vertex_count : 0,
            shape_source_topologies: shapeFrameSamples ? shapeFrameSamples.source_topologies : [],
            min_value:    [],
            max_value:    []
        };

        // Build samples array.
        var samples  = [];
        var t        = tStart;
        var lastFlattenRotation = null;
        var sampleDone = 0;
        var sampleTotal = Math.max(1, frameTotal * spf);

        // Frame center loop.
        while (t <= tEnd + 1e-9) {
            var sampleV = [];

            if (spf === 1) {
                if (flattenThisPosition) {
                    sampleV = _sampleLayerAnchorInComp(layer, comp, t, dims, compPointCache);
                } else if (flattenThisRotation) {
                    lastFlattenRotation = _sampleLayerRotationInComp(
                        layer, comp, t, lastFlattenRotation, compPointCache);
                    sampleV = [lastFlattenRotation];
                } else if (classif.isShape) {
                    if (shapeFrameSamples) {
                        if (!shapeFrameSamples.samples[shapeFrameIndex]) {
                            throw new Error('shape/path prepared sample missing at t=' +
                                t.toFixed(4) + 's');
                        }
                        sampleV = shapeFrameSamples.samples[shapeFrameIndex].v;
                        shapeFrameIndex = shapeFrameIndex + 1;
                    } else {
                        sampleV = _shapeToArray(prop.valueAtTime(t, false));
                        _assertShapeTopologyStable(shapeBaseline, sampleV, prop.name, t);
                    }
                } else {
                    var sv = prop.valueAtTime(t, false);
                    sampleV = _valueToArray(sv, dims);
                }
            } else {
                // Sub-frame sampling (docs/protocol/PROTOCOL.md shutter formula).
                var A = shutterAng / 360.0;
                var P = shutterPh  / 360.0;
                var shutterOpen = t - A * (P + 0.5) * frameDur;
                var k;
                for (k = 0; k < spf; k++) {
                    var sub_t = shutterOpen + (k + 0.5) * frameDur * A / spf;
                    var arr_k;
                    if (flattenThisPosition) {
                        arr_k = _sampleLayerAnchorInComp(layer, comp, sub_t, dims, compPointCache);
                    } else if (flattenThisRotation) {
                        lastFlattenRotation = _sampleLayerRotationInComp(
                            layer, comp, sub_t, lastFlattenRotation, compPointCache);
                        arr_k = [lastFlattenRotation];
                    } else if (classif.isShape) {
                        arr_k = _shapeToArray(prop.valueAtTime(sub_t, false));
                        _assertShapeTopologyStable(shapeBaseline, arr_k, prop.name, sub_t);
                    } else {
                        var sv_k = prop.valueAtTime(sub_t, false);
                        arr_k = _valueToArray(sv_k, dims);
                    }
                    var dd;
                    for (dd = 0; dd < arr_k.length; dd++) {
                        sampleV.push(arr_k[dd]);
                    }
                }
            }

            samples.push({ t_sec: t, v: sampleV });
            sampleDone = sampleDone + spf;
            if (!classif.isShape || !shapeFrameSamples) {
                slotsDone = slotsDone + spf;
            }
            if ((!classif.isShape || !shapeFrameSamples) &&
                    (sampleDone === spf || sampleDone >= sampleTotal ||
                    sampleDone % sampleStride === 0)) {
                emitSamplingProgress(
                    'sample', i, propLabel, sampleDone, sampleTotal,
                    (new Date()).getTime() - propStartMs);
            }

            t = t + frameDur;
            if (t > tEnd + frameDur * 0.5) { break; }
        }

        propertySamplesArr.push({
            property:             propInfo,
            t_start_sec:          tStart,
            t_end_sec:            tEnd,
            samples_per_frame:    spf,
            samples:              samples,
            layer_xform_at_start: layerXform,
            hash_of_expression:   exprHash
        });
        emitSamplingProgress(
            'property_done', i, propLabel, sampleTotal, sampleTotal,
            (new Date()).getTime() - propStartMs);
    }

    if (hasShapeTemporalBezierCandidate) {
        solverConfig.allow_shape_temporal_bezier = true;
        solverConfig.allow_path_replacement_fit = (opts.shapeReplacementFit === true);
        solverConfig.path_replacement_prefer_vertices =
            (opts.shapeReplacementPreferVertices === true);
        solverConfig.path_preserve_sharp_corners =
            (opts.preserveSharpPathCorners !== false);
        if (opts.shapeReplacementFit === true) {
            solverConfig.allow_path_spatial_fit = false;
            solverConfig.path_replacement_max_key_growth_ratio = 4.0;
            solverConfig.path_replacement_min_vertex_reduction_ratio = 0.20;
            solverConfig.path_specific_max_gap = 60;
            solverConfig.shape_temporal_bezier_attempt_threshold_ratio = 1.5;
        } else if (opts.shapeTemporalFullGapFit === true) {
            solverConfig.allow_path_spatial_fit = false;
            solverConfig.path_specific_max_gap = 60;
            solverConfig.shape_temporal_bezier_attempt_threshold_ratio = 1.5;
        } else {
            solverConfig.allow_path_spatial_fit = true;
        }
    }

    var bundle = {
        _schema:        'samples',
        schema_version: 1,
        request_id:     requestId,
        comp:           compInfo,
        properties:     propertySamplesArr,
        config:         solverConfig
    };

    return bundle;
}
