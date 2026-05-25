// verify.jsx — verifyRoundTrip + writeVerifyReport (round-4)
// Requires _polyfill.jsx (JSON) to be #included before this file.

// ISO 8601 timestamp using UTC Date methods (ES3-safe; toISOString not required).
function _isoNow() {
    var d = new Date();
    function pad(n) { return n < 10 ? '0' + n : String(n); }
    return d.getUTCFullYear() + '-' +
           pad(d.getUTCMonth() + 1) + '-' +
           pad(d.getUTCDate()) + 'T' +
           pad(d.getUTCHours()) + ':' +
           pad(d.getUTCMinutes()) + ':' +
           pad(d.getUTCSeconds()) + 'Z';
}

function _verifyComponentOrZero(v, idx) {
    if (v && idx < v.length && typeof v[idx] === 'number') { return v[idx]; }
    return 0;
}

function _verifyValueToArray(v, dims) {
    var arr = [];
    var d;
    if (dims === 1) { return [v]; }
    for (d = 0; d < dims; d++) { arr.push(v[d]); }
    return arr;
}

function _verifyShapeToArray(shapeValue) {
    var arr = [];
    if (!shapeValue || !shapeValue.vertices) { return arr; }

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

function _verifyShapeFlatMismatch(expected, actual) {
    if (!expected || !actual || expected.length < 2 || actual.length < 2) {
        return 'missing shape_flat header';
    }
    if (Math.round(expected[0]) !== Math.round(actual[0])) {
        return 'shape_flat closed flag changed';
    }
    return '';
}

function _verifyFlatPoint(flat, vertexIndex, offset) {
    var base = 2 + vertexIndex * 6 + offset;
    return [
        _verifyComponentOrZero(flat, base),
        _verifyComponentOrZero(flat, base + 1)
    ];
}

function _verifyAdd2(a, b) {
    return [a[0] + b[0], a[1] + b[1]];
}

function _verifyDist2(a, b) {
    var dx = a[0] - b[0];
    var dy = a[1] - b[1];
    return Math.sqrt(dx * dx + dy * dy);
}

function _verifyCubic2(p0, p1, p2, p3, t) {
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

function _verifyPushDense(points, p) {
    if (points.length > 0 && _verifyDist2(points[points.length - 1], p) < 0.000001) {
        return;
    }
    points.push(p);
}

function _verifyShapeFlatToDensePolyline(flat) {
    var n = Math.round(_verifyComponentOrZero(flat, 1));
    var closed = Math.round(_verifyComponentOrZero(flat, 0)) !== 0;
    var points = [];
    if (n < 1 || flat.length < 2 + 6 * n) {
        return points;
    }
    var segCount = closed ? n : Math.max(0, n - 1);
    var i, next, p0, p1, p2, p3, controlLen, chordLen, divs, j, t;
    if (segCount <= 0) {
        points.push(_verifyFlatPoint(flat, 0, 0));
        return points;
    }
    for (i = 0; i < segCount; i++) {
        next = (i + 1) % n;
        p0 = _verifyFlatPoint(flat, i, 0);
        p3 = _verifyFlatPoint(flat, next, 0);
        p1 = _verifyAdd2(p0, _verifyFlatPoint(flat, i, 4));
        p2 = _verifyAdd2(p3, _verifyFlatPoint(flat, next, 2));
        controlLen = _verifyDist2(p0, p1) + _verifyDist2(p1, p2) + _verifyDist2(p2, p3);
        chordLen = _verifyDist2(p0, p3);
        divs = Math.ceil(Math.max(6, Math.min(32,
            (controlLen + Math.abs(controlLen - chordLen) * 2.0) / 10.0)));
        if (i === 0) { _verifyPushDense(points, p0); }
        for (j = 1; j <= divs; j++) {
            t = j / divs;
            if (closed && i === segCount - 1 && j === divs) { continue; }
            _verifyPushDense(points, _verifyCubic2(p0, p1, p2, p3, t));
        }
    }
    return points;
}

function _verifyPointSegmentDistance(p, a, b) {
    var abx = b[0] - a[0];
    var aby = b[1] - a[1];
    var denom = abx * abx + aby * aby;
    if (!(denom > 0.000000000001)) {
        return _verifyDist2(p, a);
    }
    var u = ((p[0] - a[0]) * abx + (p[1] - a[1]) * aby) / denom;
    if (u < 0) { u = 0; }
    if (u > 1) { u = 1; }
    return _verifyDist2(p, [a[0] + abx * u, a[1] + aby * u]);
}

function _verifyDirectedPolylineDistance(aPoints, bPoints, closed) {
    if (!aPoints || !bPoints || aPoints.length < 1 || bPoints.length < 1) {
        return 999999;
    }
    var segCount = closed ? bPoints.length : Math.max(0, bPoints.length - 1);
    if (segCount < 1) {
        return _verifyDist2(aPoints[0], bPoints[0]);
    }
    var maxErr = 0;
    var i, s, a, b, err, best;
    for (i = 0; i < aPoints.length; i++) {
        best = 999999999;
        for (s = 0; s < segCount; s++) {
            a = bPoints[s];
            b = bPoints[(s + 1) % bPoints.length];
            err = _verifyPointSegmentDistance(aPoints[i], a, b);
            if (err < best) { best = err; }
        }
        if (best > maxErr) { maxErr = best; }
    }
    return maxErr;
}

function _verifyShapeFlatOutlineError(expected, actual) {
    var mismatch = _verifyShapeFlatMismatch(expected, actual);
    if (mismatch) {
        return { ok: false, err: 999999, note: mismatch };
    }
    var closed = Math.round(expected[0]) !== 0;
    var ep = _verifyShapeFlatToDensePolyline(expected);
    var ap = _verifyShapeFlatToDensePolyline(actual);
    var eToA = _verifyDirectedPolylineDistance(ep, ap, closed);
    var aToE = _verifyDirectedPolylineDistance(ap, ep, closed);
    return { ok: true, err: Math.max(eToA, aToE), note: 'outline distance' };
}

function _verifyVectorDelta(a, b, dims) {
    var out = [];
    var d;
    for (d = 0; d < dims; d++) {
        out.push(_verifyComponentOrZero(b, d) - _verifyComponentOrZero(a, d));
    }
    return out;
}

function _verifyVectorDrift(expected, actual, dims) {
    var sum = 0;
    var d, delta;
    for (d = 0; d < dims; d++) {
        delta = _verifyComponentOrZero(actual, d) - _verifyComponentOrZero(expected, d);
        sum = sum + delta * delta;
    }
    return Math.sqrt(sum);
}

function _verifyIsRotationPropertyInfo(pinfo) {
    var mn = '';
    try { mn = pinfo.match_name || pinfo.matchName || ''; } catch (e) {}
    return mn === 'ADBE Rotate X' || mn === 'ADBE Rotate Y' || mn === 'ADBE Rotate Z';
}

// verifyRigGaps(originalSampleBundle, propResolver, tolerance)
//
// Compares pairwise relative-offset vectors for spatial properties. This catches
// rig-chain drift where each Position passes individually but the joint relation
// changes enough to separate a limb or control pair.
function verifyRigGaps(originalSampleBundle, propResolver, tolerance) {
    tolerance = (typeof tolerance === 'number') ? tolerance : 0.5;
    var candidates = [];
    var propSamplesArr = originalSampleBundle.properties;
    var i, ps, propInfo;

    for (i = 0; i < propSamplesArr.length; i++) {
        ps = propSamplesArr[i];
        propInfo = ps.property;
        if (propInfo &&
            propInfo.is_spatial === true &&
            propInfo.dimensions >= 2 &&
            ps.samples &&
            ps.samples.length > 0) {
            candidates.push(ps);
        }
    }

    var pairResults = [];
    var overallOk = true;
    var aIdx, bIdx, aPs, bPs, aProp, bProp, dims, count;
    var j, t, aOrig, bOrig, aRaw, bRaw, aActual, bActual;
    var expectedDelta, actualDelta, drift, d;

    for (aIdx = 0; aIdx < candidates.length; aIdx++) {
        for (bIdx = aIdx + 1; bIdx < candidates.length; bIdx++) {
            aPs = candidates[aIdx];
            bPs = candidates[bIdx];
            aProp = null;
            bProp = null;
            try { aProp = propResolver(aPs.property.id); } catch (ae) {}
            try { bProp = propResolver(bPs.property.id); } catch (be) {}

            if (!aProp || !bProp) {
                pairResults.push({
                    id_a: aPs.property.id,
                    id_b: bPs.property.id,
                    max_gap_delta: -1,
                    worst_t_sec: 0,
                    ok: false,
                    samples_checked: 0,
                    note: 'property not found'
                });
                overallOk = false;
                continue;
            }

            dims = Math.max(aPs.property.dimensions || 1, bPs.property.dimensions || 1);
            count = Math.min(aPs.samples.length, bPs.samples.length);
            var maxGapDelta = 0;
            var worstT = 0;
            var worstExpectedDelta = [];
            var worstActualDelta = [];
            var worstDeltaDrift = [];
            var checked = 0;
            var mismatchedTimes = 0;

            for (j = 0; j < count; j++) {
                t = aPs.samples[j].t_sec;
                if (Math.abs((bPs.samples[j].t_sec || 0) - t) > 0.000001) {
                    mismatchedTimes = mismatchedTimes + 1;
                    continue;
                }
                try {
                    aRaw = aProp.valueAtTime(t, false);
                    bRaw = bProp.valueAtTime(t, false);
                } catch (re) {
                    continue;
                }
                checked = checked + 1;

                aOrig = _verifyValueToArray(aPs.samples[j].v, dims);
                bOrig = _verifyValueToArray(bPs.samples[j].v, dims);
                aActual = _verifyValueToArray(aRaw, dims);
                bActual = _verifyValueToArray(bRaw, dims);
                expectedDelta = _verifyVectorDelta(aOrig, bOrig, dims);
                actualDelta = _verifyVectorDelta(aActual, bActual, dims);
                drift = _verifyVectorDrift(expectedDelta, actualDelta, dims);

                if (drift > maxGapDelta) {
                    maxGapDelta = drift;
                    worstT = t;
                    worstExpectedDelta = expectedDelta;
                    worstActualDelta = actualDelta;
                    worstDeltaDrift = [];
                    for (d = 0; d < dims; d++) {
                        worstDeltaDrift.push(
                            _verifyComponentOrZero(actualDelta, d) -
                            _verifyComponentOrZero(expectedDelta, d));
                    }
                }
            }

            var pairOk = (checked > 0 && maxGapDelta <= tolerance);
            if (!pairOk) { overallOk = false; }
            pairResults.push({
                id_a: aPs.property.id,
                id_b: bPs.property.id,
                max_gap_delta: maxGapDelta,
                worst_t_sec: worstT,
                worst_expected_delta: worstExpectedDelta,
                worst_actual_delta: worstActualDelta,
                worst_delta_drift: worstDeltaDrift,
                ok: pairOk,
                samples_checked: checked,
                mismatched_times: mismatchedTimes,
                note: (checked > 0) ? '' : 'no matching sample times'
            });
        }
    }

    return {
        pairs: pairResults,
        overall_ok: overallOk,
        tolerance: tolerance
    };
}

// verifyRoundTrip(originalSampleBundle, propResolver, tolerance, screenPxTol, opts)
//
// originalSampleBundle: JS object from collectSampleBundle / readSampleBundleJson.
// propResolver:         function(propertyId) -> Property | null
// tolerance:            L∞ threshold in property units.
// screenPxTol:          screen-pixel tolerance; 0 disables (unused in v1).
// opts.verifyRigGaps:   when true, also verify pairwise spatial-property drift.
// opts.rigGapTolerance: max allowed relative-offset drift in property units.
//
// Returns:
//   { properties: [{ id, max_err, worst_t_sec, worst_dim, worst_expected,
//                    worst_actual, worst_delta, worst_sample_index, ok,
//                    samples_checked }],
//     overall_ok: bool,
//     rig_gaps: null | { pairs, overall_ok, tolerance } }
function verifyRoundTrip(originalSampleBundle, propResolver, tolerance, screenPxTol, opts) {
    if (typeof propResolver !== 'function') {
        throw new Error('verifyRoundTrip: propResolver must be a function');
    }
    opts = opts || {};
    tolerance   = (typeof tolerance   === 'number') ? tolerance   : 0.5;
    screenPxTol = (typeof screenPxTol === 'number') ? screenPxTol : 0.0;

    var propResults = [];
    var overallOk   = true;
    var progressCb  = opts.progressCallback;

    function _emitVerifyProgress(stage, propertyIndex, propertyTotal,
            propId, sampleIndex, sampleTotal) {
        if (typeof progressCb !== 'function') { return; }
        try {
            progressCb({
                phase: stage,
                property_index: propertyIndex,
                property_total: propertyTotal,
                property_id: propId,
                sample_index: (typeof sampleIndex === 'number') ? sampleIndex : -1,
                sample_total: (typeof sampleTotal === 'number') ? sampleTotal : 0
            });
        } catch (progressErr) {}
    }

    var propSamplesArr = originalSampleBundle.properties;
    var i, ps, prop;

    for (i = 0; i < propSamplesArr.length; i++) {
        ps = propSamplesArr[i];
        var propId      = ps.property.id;
        var dims        = ps.property.dimensions;
        var origSamples = ps.samples;
        var isShapeFlat = ps.property && ps.property.units_label === 'shape_flat';
        var sampleStride = Math.max(1, Math.floor(origSamples.length / 30));
        _emitVerifyProgress('property_start', i, propSamplesArr.length,
            propId, 0, origSamples.length);

        prop = null;
        try { prop = propResolver(propId); } catch (e) {}

        if (!prop) {
            propResults.push({
                id:             propId,
                max_err:        -1,
                worst_t_sec:    0,
                worst_dim:      0,
                ok:             false,
                samples_checked: 0,
                note:           'property not found'
            });
            overallOk = false;
            continue;
        }

        var maxErr       = 0;
        var worstT       = 0;
        var worstDim     = 0;
        var worstExpected = [];
        var worstActual   = [];
        var worstDelta    = [];
        var worstSampleIndex = -1;
        var checked      = 0;
        var j, s, t, resampledRaw, origV, resampledV, d, err, k;
        var shapeMismatch = '';
        var worstNote = '';

        for (j = 0; j < origSamples.length; j++) {
            if (j === 0 || j === origSamples.length - 1 || (j % sampleStride) === 0) {
                _emitVerifyProgress('sample', i, propSamplesArr.length,
                    propId, j + 1, origSamples.length);
            }
            s = origSamples[j];
            t = s.t_sec;

            try {
                resampledRaw = prop.valueAtTime(t, false);
            } catch (re) {
                continue;
            }
            checked = checked + 1;

            // Normalise to array.
            if (isShapeFlat) {
                resampledV = _verifyShapeToArray(resampledRaw);
            } else if (dims === 1) {
                resampledV = [resampledRaw];
            } else {
                resampledV = [];
                for (d = 0; d < dims; d++) { resampledV.push(resampledRaw[d]); }
            }

            // Frame-centre original values (first dims entries of s.v).
            origV = [];
            for (d = 0; d < dims; d++) { origV.push(s.v[d]); }

            if (isShapeFlat) {
                shapeMismatch = _verifyShapeFlatMismatch(origV, resampledV);
                if (shapeMismatch) {
                    maxErr = 999999;
                    worstT = t;
                    worstDim = 0;
                    worstSampleIndex = j;
                    worstExpected = origV;
                    worstActual = resampledV;
                    worstDelta = [];
                    for (k = 0; k < Math.max(origV.length, resampledV.length); k++) {
                        worstDelta.push(_verifyComponentOrZero(resampledV, k) -
                            _verifyComponentOrZero(origV, k));
                    }
                    worstNote = shapeMismatch;
                    break;
                }
                var outlineReport = _verifyShapeFlatOutlineError(origV, resampledV);
                err = outlineReport.err;
                if (err > maxErr) {
                    maxErr = err;
                    worstT = t;
                    worstDim = 0;
                    worstSampleIndex = j;
                    worstExpected = [
                        origV[0], origV[1],
                        origV.length
                    ];
                    worstActual = [
                        resampledV[0], resampledV[1],
                        resampledV.length
                    ];
                    worstDelta = [
                        err,
                        Math.round(resampledV[1]) - Math.round(origV[1]),
                        resampledV.length - origV.length
                    ];
                    worstNote = outlineReport.note;
                }
                continue;
            }

            // L∞ per dimension.
            for (d = 0; d < dims; d++) {
                err = Math.abs(_verifyComponentOrZero(resampledV, d) -
                    _verifyComponentOrZero(origV, d));
                if (err > maxErr) {
                    maxErr           = err;
                    worstT           = t;
                    worstDim         = d;
                    worstSampleIndex = j;
                    worstExpected    = [];
                    worstActual      = [];
                    worstDelta       = [];
                    for (k = 0; k < dims; k++) {
                        worstExpected.push(_verifyComponentOrZero(origV, k));
                        worstActual.push(_verifyComponentOrZero(resampledV, k));
                        worstDelta.push(_verifyComponentOrZero(resampledV, k) -
                            _verifyComponentOrZero(origV, k));
                    }
                }
            }
        }

        var propTolerance = tolerance;
        if (ps.property) {
            if (ps.property.flatten_parented_position === true &&
                    typeof opts.flattenParentedTolerance === 'number' &&
                    opts.flattenParentedTolerance > 0) {
                propTolerance = Math.min(propTolerance, opts.flattenParentedTolerance);
            }
            if ((ps.property.flatten_parented_rotation === true ||
                    ps.property.parent_flatten_strict_rotation === true)) {
                if (typeof opts.flattenParentedTolerance === 'number' &&
                        opts.flattenParentedTolerance > 0) {
                    propTolerance = Math.min(propTolerance, opts.flattenParentedTolerance);
                }
            }
            if ((opts.useRigRotationTolerance === true &&
                    _verifyIsRotationPropertyInfo(ps.property)) ||
                    ps.property.flatten_parented_rotation === true ||
                    ps.property.parent_flatten_strict_rotation === true) {
                if (typeof opts.rigRotationTolerance === 'number' &&
                        opts.rigRotationTolerance > 0) {
                    propTolerance = Math.min(propTolerance, opts.rigRotationTolerance);
                }
            }
        }

        var propOk = (maxErr <= propTolerance);
        if (!propOk) { overallOk = false; }

        propResults.push({
            id:              propId,
            max_err:         maxErr,
            worst_t_sec:     worstT,
            worst_dim:       worstDim,
            worst_expected:  worstExpected,
            worst_actual:    worstActual,
            worst_delta:     worstDelta,
            worst_sample_index: worstSampleIndex,
            tolerance:       propTolerance,
            ok:              propOk,
            samples_checked: checked,
            note:            worstNote
        });
        _emitVerifyProgress('property_done', i + 1, propSamplesArr.length,
            propId, origSamples.length, origSamples.length);
    }

    var rigGaps = null;
    if (opts.verifyRigGaps === true) {
        _emitVerifyProgress('rig_gaps_start', propSamplesArr.length,
            propSamplesArr.length, 'rig_gaps', 0, 1);
        rigGaps = verifyRigGaps(originalSampleBundle,
            propResolver,
            (typeof opts.rigGapTolerance === 'number') ? opts.rigGapTolerance : tolerance);
        _emitVerifyProgress('rig_gaps_done', propSamplesArr.length,
            propSamplesArr.length, 'rig_gaps', 1, 1);
        if (rigGaps && rigGaps.overall_ok === false) { overallOk = false; }
    }

    return { properties: propResults, overall_ok: overallOk, rig_gaps: rigGaps };
}

// writeVerifyCard(vResult, cardPath)
// Write a human-readable one-page summary to cardPath (plain text, .txt).
// Format:
//   bbsolver-test-harness verify  2026-05-14 20:30:00
//   4 properties, 3 OK, 1 FAILED
//   worst: L1/.../Position  max_err=2.1000  @ t=1.2340s
// Returns true on success, false on failure (never throws).
function _verifyFmtArray(a) {
    if (!a || typeof a.length !== 'number') { return '[]'; }
    var parts = [];
    var i, v;
    for (i = 0; i < a.length; i++) {
        v = a[i];
        if (typeof v === 'number') {
            parts.push(v.toFixed(4));
        } else {
            parts.push(String(v));
        }
    }
    return '[' + parts.join(', ') + ']';
}

function writeVerifyCard(vResult, cardPath) {
    var now = _isoNow().replace('T', ' ').replace('Z', '');
    var total  = vResult.properties.length;
    var passed = 0;
    var failed = 0;
    var worstErr   = -1;
    var worstId    = '';
    var worstT     = 0;
    var worstDim   = 0;
    var worstExpected = null;
    var worstActual   = null;
    var worstDelta    = null;
    var worstSampleIndex = -1;
    var i, pv;
    for (i = 0; i < vResult.properties.length; i++) {
        pv = vResult.properties[i];
        if (pv.ok) { passed = passed + 1; } else { failed = failed + 1; }
        if (typeof pv.max_err === 'number' && pv.max_err > worstErr) {
            worstErr = pv.max_err;
            worstId  = pv.id;
            worstT   = pv.worst_t_sec || 0;
            worstDim = pv.worst_dim || 0;
            worstExpected = pv.worst_expected || null;
            worstActual = pv.worst_actual || null;
            worstDelta = pv.worst_delta || null;
            worstSampleIndex = (typeof pv.worst_sample_index === 'number') ? pv.worst_sample_index : -1;
        }
    }

    var lines = [
        'bbsolver-test-harness verify  ' + now,
        total + ' propert' + (total === 1 ? 'y' : 'ies') + ', ' +
            passed + ' OK, ' + failed + ' FAILED',
        (vResult.overall_ok ? 'overall: PASS' : 'overall: FAIL')
    ];
    if (worstId) {
        lines.push('worst: ' + worstId +
            '  max_err=' + worstErr.toFixed(4) +
            '  @ t=' + worstT.toFixed(4) + 's');
        if (worstExpected && worstActual && worstDelta) {
            lines.push('       dim=' + worstDim +
                '  sample=' + worstSampleIndex +
                '  expected=' + _verifyFmtArray(worstExpected));
            lines.push('       actual=' + _verifyFmtArray(worstActual) +
                '  delta=' + _verifyFmtArray(worstDelta));
        }
    }
    if (vResult.rig_gaps && vResult.rig_gaps.pairs && vResult.rig_gaps.pairs.length > 0) {
        var rg = vResult.rig_gaps;
        var rgWorst = -1;
        var rgPair = null;
        var rgi, pair;
        for (rgi = 0; rgi < rg.pairs.length; rgi++) {
            pair = rg.pairs[rgi];
            if (typeof pair.max_gap_delta === 'number' && pair.max_gap_delta > rgWorst) {
                rgWorst = pair.max_gap_delta;
                rgPair = pair;
            }
        }
        lines.push('rig gaps: ' + (rg.overall_ok ? 'PASS' : 'FAIL') +
            '  pairs=' + rg.pairs.length +
            '  tol=' + rg.tolerance);
        if (rgPair) {
            lines.push('rig worst: ' + rgPair.id_a + ' <> ' + rgPair.id_b +
                '  gap_delta=' + rgPair.max_gap_delta.toFixed(4) +
                '  @ t=' + rgPair.worst_t_sec.toFixed(4) + 's');
            lines.push('           delta_drift=' + _verifyFmtArray(rgPair.worst_delta_drift));
        }
    }

    var f = new File(cardPath);
    f.encoding = 'UTF-8';
    if (!f.open('w')) { return false; }
    var ok = false;
    try { f.write(lines.join('\n') + '\n'); ok = true; } catch (e) {}
    f.close();
    return ok;
}

// verifyShapeKeyStructure(prop, expectedKeys, progressCallback)
//
// After applyKeyBundle, call this for each shape_flat property to confirm the
// key structure (interpolation types, ease influence) matches the solver's intent.
// Detects writeback corruption — e.g., AE promoting a Linear key to Bezier when
// setTemporalEaseAtKey fires — independently of the slower geometry verify.
//
// prop:         AE Property of type SHAPE, obtained via perPropertyResolver.
// expectedKeys: pk.keys array from the solver key bundle for this property.
// progressCallback: optional function receiving
//                   { phase, key_index, key_total } during long shape checks.
//
// Returns: { ok, keys_checked, mismatches }
//   ok:            false if key count differs or any interp/influence mismatch.
//   keys_checked:  number of keys actually examined.
//   mismatches:    [{key_index (0-based), field, expected, actual}]
//                  field is one of: 'num_keys', 'interp_in', 'interp_out',
//                                   'ease_in_influence', 'ease_out_influence'.
//
// Interp types are compared by canonical name: 'Hold', 'Linear', 'Bezier'.
// Ease influence is checked only for Bezier-sided keys, with ±1.0 tolerance
// (AE rounds internally; this catches silent ease loss, not sub-unit rounding).
// Speed is not checked — AE resets it when influence is written for Shape paths.
function verifyShapeKeyStructure(prop, expectedKeys, progressCallback) {
    var result = { ok: true, keys_checked: 0, mismatches: [] };

    if (!prop || !expectedKeys || !expectedKeys.length) {
        result.ok = false;
        result.mismatches.push({
            key_index: -1, field: 'input',
            expected: 'non-empty prop and keys', actual: 'missing'
        });
        return result;
    }

    var actualCount = 0;
    try { actualCount = prop.numKeys; } catch (ncErr) {}
    if (actualCount !== expectedKeys.length) {
        result.ok = false;
        result.mismatches.push({
            key_index: -1, field: 'num_keys',
            expected: expectedKeys.length, actual: actualCount
        });
        return result;
    }

    var j, keyIdx, ek, expIn, expOut, actIn, actOut;
    var actInEases, actOutEases, expInInfl, expOutInfl, actInInfl, actOutInfl;
    var inT, outT;
    var progressStride = Math.max(1, Math.floor(expectedKeys.length / 24));
    // Counters for interp_summary — tallied from actual AE readback, not expected.
    var bezierCount = 0, linearCount = 0, holdCount = 0, mixedCount = 0;

    for (j = 0; j < expectedKeys.length; j++) {
        keyIdx = j + 1;  // AE is 1-based
        ek = expectedKeys[j];
        result.keys_checked = result.keys_checked + 1;
        if (progressCallback && (j === 0 ||
                j === expectedKeys.length - 1 ||
                (j % progressStride) === 0)) {
            try {
                progressCallback({
                    phase: 'verify_structure',
                    key_index: keyIdx,
                    key_total: expectedKeys.length
                });
            } catch (progressErr) {}
        }

        // Normalize expected interp name — matches _interpToAE in writeback.jsx.
        expIn  = (ek.interp_in  === 'Hold')   ? 'Hold'
               : (ek.interp_in  === 'Linear') ? 'Linear'
               : 'Bezier';
        expOut = (ek.interp_out === 'Hold')   ? 'Hold'
               : (ek.interp_out === 'Linear') ? 'Linear'
               : 'Bezier';

        // Read actual interp type from AE.
        actIn  = 'ERR';
        actOut = 'ERR';
        try {
            inT   = prop.keyInInterpolationType(keyIdx);
            actIn = (inT === KeyframeInterpolationType.HOLD)   ? 'Hold'
                  : (inT === KeyframeInterpolationType.LINEAR) ? 'Linear'
                  : 'Bezier';
        } catch (e1) {}
        try {
            outT   = prop.keyOutInterpolationType(keyIdx);
            actOut = (outT === KeyframeInterpolationType.HOLD)   ? 'Hold'
                   : (outT === KeyframeInterpolationType.LINEAR) ? 'Linear'
                   : 'Bezier';
        } catch (e2) {}

        if (actIn !== expIn) {
            result.ok = false;
            result.mismatches.push({
                key_index: j, field: 'interp_in',
                expected: expIn, actual: actIn
            });
        }
        if (actOut !== expOut) {
            result.ok = false;
            result.mismatches.push({
                key_index: j, field: 'interp_out',
                expected: expOut, actual: actOut
            });
        }

        // Classify key for interp_summary using actual AE-stored types.
        // Only classify when both sides were readable (not 'ERR').
        if (actIn !== 'ERR' && actOut !== 'ERR') {
            if (actIn === 'Bezier' || actOut === 'Bezier') {
                bezierCount = bezierCount + 1;
            } else if (actIn === 'Linear' && actOut === 'Linear') {
                linearCount = linearCount + 1;
            } else if (actIn === 'Hold' && actOut === 'Hold') {
                holdCount = holdCount + 1;
            } else {
                mixedCount = mixedCount + 1;
            }
        }

        // Ease influence for Bezier-in side — only when BOTH expected and actual
        // interp are Bezier. Skipping when actIn disagrees avoids compound
        // mismatches on an already-flagged key (wrong type + wrong ease is noise).
        if (expIn === 'Bezier' && actIn === 'Bezier' &&
                ek.temporal_ease_in && ek.temporal_ease_in.length > 0 &&
                typeof ek.temporal_ease_in[0].influence === 'number') {
            expInInfl = ek.temporal_ease_in[0].influence;
            actInInfl = -1;
            try {
                actInEases = prop.keyInTemporalEase(keyIdx);
                if (actInEases && actInEases.length > 0) {
                    actInInfl = actInEases[0].influence;
                }
            } catch (e3) {}
            if (actInInfl >= 0 && Math.abs(actInInfl - expInInfl) > 1.0) {
                result.ok = false;
                result.mismatches.push({
                    key_index: j, field: 'ease_in_influence',
                    expected: expInInfl, actual: actInInfl
                });
            }
        }

        // Ease influence for Bezier-out side — same guard: skip when actOut
        // disagrees so a type mismatch doesn't cascade into a redundant ease error.
        if (expOut === 'Bezier' && actOut === 'Bezier' &&
                ek.temporal_ease_out && ek.temporal_ease_out.length > 0 &&
                typeof ek.temporal_ease_out[0].influence === 'number') {
            expOutInfl = ek.temporal_ease_out[0].influence;
            actOutInfl = -1;
            try {
                actOutEases = prop.keyOutTemporalEase(keyIdx);
                if (actOutEases && actOutEases.length > 0) {
                    actOutInfl = actOutEases[0].influence;
                }
            } catch (e4) {}
            if (actOutInfl >= 0 && Math.abs(actOutInfl - expOutInfl) > 1.0) {
                result.ok = false;
                result.mismatches.push({
                    key_index: j, field: 'ease_out_influence',
                    expected: expOutInfl, actual: actOutInfl
                });
            }
        }
    }

    result.interp_summary = {
        bezier_count: bezierCount,
        linear_count: linearCount,
        hold_count:   holdCount,
        mixed_count:  mixedCount
    };

    return result;
}

// verifyShapeKeyEndpoints(prop, keys, sourceSamples)
//
// For each key in the solver key bundle, reads the AE-stored key value and
// computes the outline (Hausdorff) error against the nearest original source
// sample. This distinguishes "exact" endpoints (key value == source, err ≤ 1e-3)
// from "relaxed" endpoints (solver chose a different shape, err > 1e-3).
//
// prop:          AE Property of type SHAPE.
// keys:          pk.keys from the solver key bundle (each has t_sec).
// sourceSamples: [{t_sec, v}] from the original sample bundle for this property.
//
// Returns: { max_endpoint_err, relaxed_key_count, exact_key_count, keys_checked }
// Skips keys with no source sample at the same sampled time. Never throws.
function verifyShapeKeyEndpoints(prop, keys, sourceSamples) {
    var result = {
        max_endpoint_err:  0,
        relaxed_key_count: 0,
        exact_key_count:   0,
        keys_checked:      0
    };

    if (!prop || !keys || !keys.length || !sourceSamples || !sourceSamples.length) {
        return result;
    }

    var EXACT_THRESH = 1e-3;
    var j, keyIdx, tKey, bestSample, bestDt, si, dt;
    var keyShape, keyFlat, srcFlat, outlineResult, err;

    for (j = 0; j < keys.length; j++) {
        keyIdx = j + 1;
        tKey = keys[j].t_sec;

        // Find source sample nearest to this key time.
        bestSample = null;
        bestDt = 999999;
        for (si = 0; si < sourceSamples.length; si++) {
            dt = Math.abs((sourceSamples[si].t_sec || 0) - tKey);
            if (dt < bestDt) { bestDt = dt; bestSample = sourceSamples[si]; }
        }
        if (!bestSample || bestDt > 0.001) { continue; }  // no matching sample time

        // Read AE-stored key value — prop.keyValue(1-based).
        keyShape = null;
        try { keyShape = prop.keyValue(keyIdx); } catch (kvErr) {}
        if (!keyShape) { continue; }

        try {
            keyFlat = _verifyShapeToArray(keyShape);
            srcFlat = bestSample.v;
            if (!keyFlat.length || !srcFlat || !srcFlat.length) { continue; }

            outlineResult = _verifyShapeFlatOutlineError(srcFlat, keyFlat);
            if (!outlineResult.ok) { continue; }
        } catch (shapeErr) {
            continue;
        }

        err = outlineResult.err;
        result.keys_checked = result.keys_checked + 1;
        if (err > result.max_endpoint_err) { result.max_endpoint_err = err; }
        if (err > EXACT_THRESH) {
            result.relaxed_key_count = result.relaxed_key_count + 1;
        } else {
            result.exact_key_count = result.exact_key_count + 1;
        }
    }

    return result;
}

// writeVerifyReport(vResult, reportPath)
// Write a JSON verification report to reportPath.
// Report shape:
//   { properties: [{id, max_err, ok, samples_checked}],
//     rig_gaps: null | { pairs, overall_ok, tolerance },
//     overall_ok: bool, generated: <iso8601> }
// Returns true on success, false on failure (never throws).
function writeVerifyReport(vResult, reportPath) {
    var report = {
        properties:  vResult.properties,
        rig_gaps:    vResult.rig_gaps || null,
        overall_ok:  vResult.overall_ok,
        generated:   _isoNow()
    };

    var f = new File(reportPath);
    f.encoding = 'UTF-8';
    if (!f.open('w')) { return false; }
    var ok = false;
    try {
        f.write(JSON.stringify(report, null, 2));
        ok = true;
    } catch (e) {}
    f.close();
    return ok;
}
