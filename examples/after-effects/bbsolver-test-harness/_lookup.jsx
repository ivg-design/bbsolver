// _lookup.jsx — resolve a Property by stable id (round-5: numeric-index fallback)
//
// Id format: 'L<layerIndex>/<matchName1#index>/<matchName2#index>/...'
// e.g. 'L3/ADBE Transform Group#5/ADBE Position#2'
//      'L1/ADBE Effect Parade#2/Pseudo/LimbControlsV3#1/Pseudo/LimbControlsV3-0034#8'
// Older matchName-only ids are still accepted for backwards compatibility.
//
// ROBUSTNESS:
//  1. Greedy segment join: try single segment; if that fails, join with next to
//     handle '/' embedded in pseudo-effect matchNames (e.g. 'Pseudo/LimbControlsV3').
//  2. Linear scan: for INDEXED_GROUPs where property(matchName) returns null,
//     iterate children by index comparing matchName.
//  3. Numeric index fallback: if a segment is a pure integer string (e.g. '0001'),
//     try it as a 1-based child index. Handles some effect parameter paths
//     like 'ADBE Slider Control-0001/0001'.
//  4. Expression-only properties (no keyframes) are found by matchName just like
//     keyframed ones — the resolver does not check numKeys or expressionEnabled.
//
// Verified APIs:
//   layer.property(name)  — propertygroup.md p.134 ("Any match name")
//   group.numProperties   — propertygroup.md p.19
//   group.property(index) — propertygroup.md p.134
//   child.matchName       — propertybase.md p.172
//   child.propertyType    — propertybase.md p.247
//   PropertyType.PROPERTY / INDEXED_GROUP / NAMED_GROUP — propertybase.md p.257-261

function _parseIndexedIdSegment(name) {
    var hashIdx = name.lastIndexOf('#');
    if (hashIdx < 0) {
        return { base: name, index: 0 };
    }

    var base = name.substring(0, hashIdx);
    var idxStr = name.substring(hashIdx + 1);
    if (!idxStr || !/^[0-9]+$/.test(idxStr)) {
        return { base: name, index: 0 };
    }

    var idx = parseInt(idxStr, 10);
    if (isNaN(idx) || idx < 1) {
        return { base: name, index: 0 };
    }
    return { base: base, index: idx };
}

function _childMatchesIdBase(child, base) {
    if (!child) { return false; }
    if (!base) { return true; }
    try { if (child.matchName === base) { return true; } } catch (e1) {}
    try { if (child.name === base) { return true; } } catch (e2) {}
    return false;
}

function _lookupIdSegmentForPropertyBase(propBase) {
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

function _propertyInstanceKeyForDedup(prop) {
    var names = [];
    var cur = prop;
    try {
        while (cur !== null && cur.propertyDepth > 0) {
            names.unshift(_lookupIdSegmentForPropertyBase(cur));
            cur = cur.parentProperty;
        }
    } catch (e) {}
    if (names.length > 0) { return names.join('/'); }
    try { return prop.matchName + '#' + prop.propertyIndex; } catch (ef) {}
    return '';
}

// Attempt to resolve one matchName#index segment from 'group'.
// Returns the child PropertyBase, or null.
function _resolveSegment(group, name) {
    var result = null;
    var parsed = _parseIndexedIdSegment(name);
    var baseName = parsed.base;

    // 0. Exact property-index disambiguation. This is what prevents repeated
    // sibling Shape paths from resolving to the first matching matchName.
    if (parsed.index > 0) {
        try { result = group.property(parsed.index); } catch (ei) {}
        if (_childMatchesIdBase(result, baseName)) { return result; }
        result = null;
        var n0b = 0, i0b, c0b, ci0b = 0;
        try { n0b = group.numProperties; } catch (en0b) {}
        for (i0b = 1; i0b <= n0b; i0b++) {
            try { c0b = group.property(i0b); } catch (ec0b) { continue; }
            try { ci0b = c0b.propertyIndex; } catch (eci0b) { ci0b = 0; }
            if (ci0b === parsed.index && _childMatchesIdBase(c0b, baseName)) {
                return c0b;
            }
        }
    }

    // 1. Direct lookup by matchName (or display name — AE accepts both).
    try { result = group.property(baseName); } catch (e) {}
    if (result) { return result; }

    // 2. Linear scan of indexed children by matchName.
    //    Required for pseudo-effects and other INDEXED_GROUPs.
    var n = 0;
    try { n = group.numProperties; } catch (e) {}
    var i, child;
    for (i = 1; i <= n; i++) {
        try { child = group.property(i); } catch (e) { continue; }
        if (_childMatchesIdBase(child, baseName)) { return child; }
    }

    // 3. Numeric index fallback: if name is a pure decimal integer, try it as a
    //    1-based property index. Handles paths ending in e.g. '/0001'.
    //    VERIFY-IN-AE: confirm AE property indices are 1-based and ≤ numProperties.
    var asInt = parseInt(baseName, 10);
    if (/^[0-9]+$/.test(baseName) && !isNaN(asInt) && asInt >= 1) {
        try { result = group.property(asInt); } catch (e) {}
        if (result) { return result; }
    }

    return null;
}

// parseId(id, comp) -> Property | null
// Walks comp.layer(layerIndex) then descends by match-name segments.
// Returns the leaf Property, or null if not found.
function parseId(id, comp) {
    if (!id || !comp) { return null; }

    // Parse leading 'L<n>/'.
    var slashIdx = id.indexOf('/');
    if (slashIdx < 0) { return null; }

    var layerPart = id.substring(0, slashIdx);
    var pathPart  = id.substring(slashIdx + 1);

    if (layerPart.charAt(0) !== 'L') { return null; }
    var layerIndex = parseInt(layerPart.substring(1), 10);
    if (isNaN(layerIndex) || layerIndex < 1) { return null; }

    var layer;
    try { layer = comp.layer(layerIndex); } catch (e) { return null; }
    if (!layer) { return null; }

    // Walk the property path with greedy segment resolution.
    var segments = pathPart.split('/');
    var cur = layer;
    var i = 0;
    var seg, next;

    while (i < segments.length) {
        seg = segments[i];
        if (!seg) { return null; }

        next = _resolveSegment(cur, seg);

        if (!next && i + 1 < segments.length) {
            // Two-part join for '/' in match names (e.g. 'Pseudo/LimbControlsV3').
            var two = seg + '/' + segments[i + 1];
            next = _resolveSegment(cur, two);
            if (next) { i = i + 2; cur = next; continue; }

            // Three-part join (defensive for deeply-nested pseudo names).
            if (i + 2 < segments.length) {
                var three = two + '/' + segments[i + 2];
                next = _resolveSegment(cur, three);
                if (next) { i = i + 3; cur = next; continue; }
            }
        }

        if (!next) { return null; }
        cur = next;
        i = i + 1;
    }

    // Accept leaf properties only.
    // Expression-only props (no keyframes) pass this check the same as keyframed ones.
    try {
        if (cur.propertyType !== PropertyType.PROPERTY) { return null; }
    } catch (e) { return null; }

    return cur;
}

// parseSepId(id, comp) -> Property | null
// Handles the synthetic '/sep/<dim>' suffix emitted by the separated-dimensions
// sampler. Given an id like 'L1/ADBE Transform Group/ADBE Position/sep/1':
//   1. Detects the '/sep/<dim>' suffix.
//   2. Strips it to recover the parent id.
//   3. Resolves the parent via parseId().
//   4. Returns parent.getSeparationFollower(dim).
//
// Falls back to parseId() for ids that do not contain '/sep/'.
//
// Verified: Property.getSeparationFollower(dim) (property.md p.694) — returns the
// separated follower Property for the given 0-based dimension index.
// Verified: Property.isSeparationLeader (property.md p.361) — must be true on parent.
// Verified: Property.dimensionsSeparated read/write (property.md p.190).
function parseSepId(id, comp) {
    // Detect '/sep/<d>' suffix.
    var SEP_MARKER = '/sep/';
    var sepIdx = id.lastIndexOf(SEP_MARKER);
    if (sepIdx < 0) {
        // No separation suffix — ordinary path.
        return parseId(id, comp);
    }

    var parentId = id.substring(0, sepIdx);
    var dimStr   = id.substring(sepIdx + SEP_MARKER.length);
    var dim      = parseInt(dimStr, 10);
    if (isNaN(dim) || dim < 0) { return null; }

    // Resolve the parent spatial property.
    var parent = parseId(parentId, comp);
    if (!parent) { return null; }

    // Ensure it is a separation leader with dimensions separated.
    var isLeader = false;
    try { isLeader = parent.isSeparationLeader; } catch (e) {}
    if (!isLeader) { return null; }

    var separated = false;
    try { separated = parent.dimensionsSeparated; } catch (e) {}
    if (!separated) {
        // dimensionsSeparated should already be true (set during sampling).
        // Attempt to re-enable it defensively; only do so if no keys on parent.
        var nk = 0;
        try { nk = parent.numKeys; } catch (e) {}
        if (nk === 0) {
            try { parent.dimensionsSeparated = true; } catch (e) { return null; }
        } else {
            return null;
        }
    }

    // Get the follower for the requested dimension.
    var follower = null;
    try { follower = parent.getSeparationFollower(dim); } catch (e) {}
    return follower;
}

// ---- collectAnimatedProps ------------------------------------------------
// Recursively walk a layer and return all animated leaf properties:
// canVaryOverTime AND (expressionEnabled OR numKeys > 0).
// At the layer root, numProperties returns only 3 indexed groups (masks/effects/trackers).
// Named groups (Transform, Audio, Shape Contents, Text, etc.) must be probed by matchName.
// Verified: layer.numProperties returns 3 at root (propertygroup.md p.21).

var _LAYER_ROOT_GROUPS = [
    'ADBE Transform Group',
    'ADBE Effect Parade',
    'ADBE Mask Parade',
    'ADBE Audio Group',
    'ADBE Root Vectors Group',
    'ADBE Text Properties',
    'ADBE Camera Options Group',
    'ADBE Light Options Group',
    'ADBE Layer Styles'
];

function _recurseProps(node, results) {
    var pt;
    try { pt = node.propertyType; } catch (e) { return; }

    if (pt === PropertyType.PROPERTY) {
        var cvot = false, exprOn = false, nk = 0;
        try { cvot   = node.canVaryOverTime;   } catch (e) {}
        try { exprOn = node.expressionEnabled; } catch (e) {}
        try { nk     = node.numKeys;           } catch (e) {}
        if (cvot && (exprOn || nk > 0)) { results.push(node); }
        return;
    }

    var n = 0;
    try { n = node.numProperties; } catch (e) {}
    var i, child;
    for (i = 1; i <= n; i++) {
        try { child = node.property(i); } catch (e) { continue; }
        if (child) { _recurseProps(child, results); }
    }
}

function collectAnimatedProps(layer) {
    var results = [];
    var i, grp;

    // Named groups (not in numProperties).
    for (i = 0; i < _LAYER_ROOT_GROUPS.length; i++) {
        grp = null;
        try { grp = layer.property(_LAYER_ROOT_GROUPS[i]); } catch (e) {}
        if (grp) { _recurseProps(grp, results); }
    }

    // Indexed groups (mask, effect, tracker) — may duplicate named groups; de-dup below.
    var n = 0;
    try { n = layer.numProperties; } catch (e) {}
    var j, child;
    for (j = 1; j <= n; j++) {
        try { child = layer.property(j); } catch (e) { continue; }
        if (child) { _recurseProps(child, results); }
    }

    // De-duplicate by full indexed property path. Leaf matchName + propertyIndex
    // is ambiguous for repeated Shape paths under different vector groups.
    var seen = [];
    var unique = [];
    var k, key, dup, s;
    for (k = 0; k < results.length; k++) {
        key = _propertyInstanceKeyForDedup(results[k]);
        if (!key) { key = String(k); }
        dup = false;
        for (s = 0; s < seen.length; s++) {
            if (seen[s] === key) { dup = true; break; }
        }
        if (!dup) { seen.push(key); unique.push(results[k]); }
    }
    return unique;
}
