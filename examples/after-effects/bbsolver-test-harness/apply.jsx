// apply.jsx -- applyKeyBundleMultiPath
// Requires _polyfill.jsx, _lookup.jsx, parse_keys.jsx, writeback.jsx before this file.
//
// applyKeyBundleMultiPath is the top-level writeback entry point.
// For bundles with no sub-paths it delegates directly to applyKeyBundle -- existing
// single-path behavior is identical and the overhead is one groupKeyBundleBySource call.
// For bundles with sub-paths it routes each sub-path group to a new AE path
// property adjacent to the source (mask-path and shape-group sources supported).
//
// Two distinct sub-path protocols are supported:
//
// 1. Diagnostic-only marker -- entry notes begin with 'landmark_subpath;'.
//    These siblings exist so probes and the panel can inspect solver-emitted
//    landmark partitions. They MAY be inert in AE: partial-vertex mask siblings
//    are forced to MaskMode.NONE (zero alpha contribution); partial-vertex
//    shape-group siblings are skipped entirely (would render as degenerate
//    polygons under the group's fill/stroke operators). They MUST NOT be
//    counted as user-visible improvements.
//
// 2. Visible-channel marker -- entry notes begin with 'shape_channel_subpath;'
//    (canonical, matches the solver's emit) or 'visible_channel_subpath;'
//    (backward-compatible alias from build023; new code should not emit it).
//    These siblings assert they are intended to render visibly. The protocol
//    requires:
//      visibility=mask_add | mask_subtract | shape_group_full
//      vertex_range=0-N covering the full source vertex count, with the entry's
//      own flat-shape vertex count also equal to N (or vertex_range omitted,
//      meaning identical topology to the source)
//    AE writeback REJECTS any visible-channel entry whose vertex coverage
//    cannot be proven full-range, whose visibility token is missing/unknown,
//    or whose visibility mode is incompatible with the AE source kind. Visible
//    entries are NEVER set to MaskMode.NONE: a mask visible-channel sets
//    MaskMode.ADD or .SUBTRACT per the declared visibility, and a shape-group
//    visible-channel only proceeds when full coverage is proven.
//
// Notes format (semicolon-delimited):
//   "landmark_subpath; subpath_index=0; vertex_range=0-18; key_count=78; ..."
//   "shape_channel_subpath; subpath_index=0; visibility=mask_add; vertex_range=0-18"
// All sub-path entries share the property_id of the source they refine.
//
// Result fields populated by applyKeyBundleMultiPath:
//   multi_path_count            -- total AE properties created (diagnostic + visible)
//   diagnostic_subpath_count    -- properties from 'landmark_subpath' marker
//   visible_subpath_count       -- properties from the visible-channel marker
//                                  ('shape_channel_subpath' or the legacy alias)
//   visible_subpath_ids         -- AE labels of visible siblings (parallel to bb_lm_N)
// The two counts are strictly disjoint: a path either renders (visible) or it
// does not (diagnostic-only). Anything that gets MaskMode.NONE or is skipped
// is never counted as visible -- that is the property the panel and the
// path_panel_policy test rely on to prevent fake-win reporting.

// Parse a vertex_range="A-B" field from a semicolon-delimited notes string.
// Returns { first: <int>, end: <int> } when the field is present and valid,
// or null when absent or malformed.  Requires B > A >= 0.
// Uses _parseNoteField from parse_keys.jsx (included before this file).
function _parseVertexRange(notes) {
    var raw = _parseNoteField(notes, 'vertex_range');
    if (!raw) { return null; }
    var dash = raw.indexOf('-');
    if (dash <= 0) { return null; }
    var first = parseInt(raw.substring(0, dash), 10);
    var end   = parseInt(raw.substring(dash + 1), 10);
    if (isNaN(first) || isNaN(end) || first < 0 || end <= first) { return null; }
    return { first: first, end: end };
}

// Classify a SHAPE property as a mask path or shape-group path.
// Returns 'mask' | 'shape_group' | 'unknown'.
function _shapePropertyKind(prop) {
    var mn = '';
    try { mn = prop.matchName || ''; } catch (e) {}
    if (mn === 'ADBE Mask Shape')   { return 'mask'; }
    if (mn === 'ADBE Vector Shape') { return 'shape_group'; }
    return 'unknown';
}

function _entryShapeFlatVertexCount(entry) {
    var v;
    try {
        if (!entry || !entry.keys || !entry.keys.length) { return 0; }
        v = entry.keys[0].v;
        if (!v || v.length < 2) { return 0; }
        return parseInt(v[1], 10) || 0;
    } catch (e) { return 0; }
}

function _isSupportedLandmarkSubPathEntry(entry) {
    var notes = '';
    try { notes = entry.notes || ''; } catch (e) {}
    notes = notes.replace(/^\s+|\s+$/g, '');
    return notes.indexOf('landmark_subpath;') === 0;
}

// Classify a sub-path entry by its notes marker.
// Returns:
//   'diagnostic'  for 'landmark_subpath;'
//   'visible'     for 'shape_channel_subpath;' (canonical, matches solver emit)
//                 or 'visible_channel_subpath;' (deprecated alias from build023)
//   'unsupported' for any other prefix.
// Reserved marker 'non_contiguous_landmark_subpath;' currently has no AE
// composition story, so it classifies as 'unsupported' here -- writeback
// will record a skip with an explicit reason rather than silently degrade.
function _subPathProtocol(entry) {
    var notes = '';
    try { notes = entry.notes || ''; } catch (e) {}
    notes = notes.replace(/^\s+|\s+$/g, '');
    if (notes.indexOf('landmark_subpath;') === 0) { return 'diagnostic'; }
    if (notes.indexOf('shape_channel_subpath;') === 0) { return 'visible'; }
    if (notes.indexOf('visible_channel_subpath;') === 0) { return 'visible'; }
    return 'unsupported';
}

// Allowed visibility tokens for the visible-channel marker. Adding new
// tokens requires extending _applyVisibleMaskMode / _isVisibilityCompatible
// AND updating tests/path_panel_policy.py to match.
function _parseVisibility(entry) {
    var raw = _parseNoteField(entry.notes, 'visibility');
    if (!raw) { return null; }
    raw = raw.replace(/^\s+|\s+$/g, '');
    if (raw === 'mask_add' || raw === 'mask_subtract' ||
        raw === 'shape_group_full') {
        return raw;
    }
    return null;
}

function _isVisibilityCompatible(kind, visibility) {
    if (kind === 'mask') {
        return visibility === 'mask_add' || visibility === 'mask_subtract';
    }
    if (kind === 'shape_group') {
        return visibility === 'shape_group_full';
    }
    return false;
}

// Assign the mask atom's mode for a visible-channel mask sibling.
// MaskMode.NONE is NEVER set here -- that would defeat the protocol's
// purpose. Returns true on success; false if AE rejected the assignment
// (in which case the caller MUST treat the sub-path as skipped, never
// as a visible improvement).
function _applyVisibleMaskMode(newProp, visibility) {
    var maskAtom = null;
    try { maskAtom = newProp.parentProperty; } catch (e) { return false; }
    if (!maskAtom) { return false; }
    try {
        if (visibility === 'mask_add') {
            maskAtom.maskMode = MaskMode.ADD;
        } else if (visibility === 'mask_subtract') {
            maskAtom.maskMode = MaskMode.SUBTRACT;
        } else {
            return false;
        }
    } catch (e) { return false; }
    return true;
}

// Create a new mask path property on the same mask parade as sourceProp.
// sourceProp must be an 'ADBE Mask Shape' property.
// subPathLabel is the name for the new mask (e.g. 'bb_lm_0').
// Returns the new 'ADBE Mask Shape' property, or null on failure.
// Verified: maskParade.addProperty('ADBE Mask Atom'), mask.property('ADBE Mask Shape').
function _createAdjacentMaskPath(sourceProp, subPathLabel) {
    try {
        // Navigate: maskShape -> maskAtom -> maskParade.
        var maskAtom   = sourceProp.parentProperty;
        var maskParade = maskAtom ? maskAtom.parentProperty : null;
        if (!maskParade) { return null; }
        var newMask = maskParade.addProperty('ADBE Mask Atom');
        if (!newMask) { return null; }
        try { newMask.name = subPathLabel; } catch (nameErr) {}
        var newMaskShape = null;
        try { newMaskShape = newMask.property('ADBE Mask Shape'); } catch (mpErr) {}
        return newMaskShape;
    } catch (e) { return null; }
}

// Create a new Vector Shape path property adjacent to sourceProp within the
// same parent shape group.  sourceProp must be an 'ADBE Vector Shape' property
// (the path data property inside a 'ADBE Vector Shape - Group' path item).
// Returns the new 'ADBE Vector Shape' data property, or null on failure.
// Navigation: pathData (ADBE Vector Shape)
//           -> pathItem  (ADBE Vector Shape - Group)
//           -> groupContents (ADBE Vectors Group)
//           -> addProperty('ADBE Vector Shape - Group') -> newPathItem
//           -> property('ADBE Vector Shape') -> newPathData.
function _createAdjacentShapePath(sourceProp, subPathLabel) {
    try {
        var pathItem      = sourceProp.parentProperty;
        var groupContents = pathItem ? pathItem.parentProperty : null;
        if (!groupContents) { return null; }
        var newPathItem = groupContents.addProperty('ADBE Vector Shape - Group');
        if (!newPathItem) { return null; }
        try { newPathItem.name = subPathLabel; } catch (nameErr) {}
        var newVectorShape = null;
        try { newVectorShape = newPathItem.property('ADBE Vector Shape'); } catch (vpErr) {}
        return newVectorShape;
    } catch (e) { return null; }
}

// Write one sub-path group for a single source property.
// sourceProperty -- resolved AE SHAPE property for the source.
// subPathEntries -- [PropertyKeys] sorted by subpath_index. Each may use the
//                   diagnostic 'landmark_subpath;' marker or the visible
//                   'visible_channel_subpath;' marker.
// opts           -- passed from applyKeyBundleMultiPath (undoGroupName, etc).
// Returns: {
//   applied,                       -- TOTAL AE properties successfully written
//   diagnostic_applied,            -- subset of applied with landmark_subpath
//   visible_applied,               -- subset of applied with visible_channel_subpath
//   skipped_count, errors,
//   sub_path_ids,                  -- AE labels of all created siblings
//   visible_sub_path_ids,          -- AE labels that render visibly
//   shape_key_structure
// }
// Invariant: visible_applied counts ONLY entries that (a) used the
// visible_channel_subpath marker, (b) were not skipped, (c) did not have
// MaskMode.NONE applied. Anything else lands in diagnostic_applied or in
// skipped_count.
function _applySubPathGroup(sourceProperty, subPathEntries, opts) {
    var result = { applied: 0, diagnostic_applied: 0, visible_applied: 0,
                  skipped_count: 0, errors: [], sub_path_ids: [],
                  visible_sub_path_ids: [], shape_key_structure: [] };
    var kind = _shapePropertyKind(sourceProperty);

    var i, entry, subIdx, subLabel, newProp, capturedProp, subBundle, subResult,
        sksj, sksEntry, vr, entryVertexCount, isCountMismatch,
        isRangeEntryMismatch, isPartial, isShapeGroupSafe, srcVertexCount,
        srcShape, maskAtomForMode, protocol, visibility, visibleApplied;

    // Read source vertex count once for partial-range detection below.
    // A subpath is partial when its vertex_range covers fewer vertices than the source.
    srcVertexCount = 0;
    try {
        srcShape = sourceProperty.valueAtTime(0, false);
        srcVertexCount = (srcShape && srcShape.vertices) ? srcShape.vertices.length : 0;
    } catch (svce) {}

    for (i = 0; i < subPathEntries.length; i++) {
        entry  = subPathEntries[i];
        subIdx = parseInt(_parseNoteField(entry.notes, 'subpath_index') || String(i), 10);
        protocol = _subPathProtocol(entry);
        // bb_lm_  -- diagnostic (landmark, possibly inert)
        // bb_vc_  -- visible channel (must render). Distinct prefix is a
        //            hard signal in the AE timeline that the user can spot.
        subLabel = (protocol === 'visible' ? 'bb_vc_' : 'bb_lm_') + subIdx;
        visibility = null;
        visibleApplied = false;

        if (protocol === 'unsupported') {
            result.skipped_count = result.skipped_count + 1;
            result.errors.push({
                id:  entry.property_id,
                msg: 'sub-path ' + subIdx +
                    ': unsupported sub-path protocol marker skipped'
            });
            continue;
        }

        // Detect partial vertex_range: first > 0 is always partial; first == 0 with
        // end < source count is partial when the source count could be read.
        vr = _parseVertexRange(entry.notes);
        entryVertexCount = _entryShapeFlatVertexCount(entry);
        isCountMismatch = (vr === null && srcVertexCount > 0 &&
                          entryVertexCount > 0 &&
                          entryVertexCount !== srcVertexCount);
        isRangeEntryMismatch = (vr !== null && entryVertexCount > 0 &&
                                (vr.end - vr.first) !== entryVertexCount);
        isPartial = isCountMismatch ||
                    isRangeEntryMismatch ||
                    ((vr !== null) &&
                    (vr.first > 0 ||
                      (srcVertexCount > 0 && vr.end < srcVertexCount)));
        // For shape-group: require positive proof that vertex_range covers the full source.
        // When vertex_range is present but srcVertexCount could not be read, coverage
        // cannot be verified -- treat conservatively as unsafe rather than allowing through.
        isShapeGroupSafe = (vr === null && !isCountMismatch) ||
                          (vr !== null && !isRangeEntryMismatch &&
                            vr.first === 0 &&
                            srcVertexCount > 0 && vr.end === srcVertexCount);

        // Visible-channel protocol pre-checks. These run BEFORE any AE
        // property is created so a fake-win cannot leak through as an
        // applied-but-invisible path.
        if (protocol === 'visible') {
            visibility = _parseVisibility(entry);
            if (visibility === null) {
                // FAIL-CLOSED: a visible-channel marker without an explicit
                // visibility= token cannot be rendered safely -- AE would
                // have to guess between MaskMode.ADD, MaskMode.SUBTRACT,
                // and shape-group full coverage, and a wrong guess would
                // either change the visible composition or leave the
                // sibling inert. Solver "scout" rows that emit
                // shape_channel_subpath without committing to a render
                // mode are skipped here on purpose; they must be
                // re-emitted with visibility= before AE accepts them.
                result.skipped_count = result.skipped_count + 1;
                result.errors.push({
                    id:  entry.property_id,
                    msg: 'visible sub-path ' + subIdx +
                        ': missing or unknown visibility= field ' +
                        '(must be mask_add, mask_subtract, or ' +
                        'shape_group_full). Solver scout rows without ' +
                        'visibility= are not AE-ready visible writeback.'
                });
                continue;
            }
            if (!_isVisibilityCompatible(kind, visibility)) {
                result.skipped_count = result.skipped_count + 1;
                result.errors.push({
                    id:  entry.property_id,
                    msg: 'visible sub-path ' + subIdx +
                        ': visibility=' + visibility +
                        ' incompatible with source kind=' + kind
                });
                continue;
            }
            // Coverage requirement: visible-channel ALWAYS requires
            // full-range coverage, regardless of source kind. A partial
            // mask channel would have to be inert to be safe, and an
            // inert path is not a visible improvement. Reject explicitly.
            if (isPartial) {
                result.skipped_count = result.skipped_count + 1;
                result.errors.push({
                    id:  entry.property_id,
                    msg: 'visible sub-path ' + subIdx +
                        ': partial-vertex coverage rejected for visible-channel' +
                        ' (' + (vr === null ?
                            ('vertex count mismatch; missing vertex_range; entry vertices=' +
                            entryVertexCount + '; source vertices=' +
                            srcVertexCount) :
                            ('vertex_range=' + vr.first + '-' + vr.end +
                            (isRangeEntryMismatch ?
                                ('; entry vertices=' + entryVertexCount) : ''))) +
                        (srcVertexCount > 0 ? '' : '; source vertex count unreadable') + ')'
                });
                continue;
            }
        }

        if (kind === 'mask') {
            newProp = _createAdjacentMaskPath(sourceProperty, subLabel);
            if (!newProp) {
                result.skipped_count = result.skipped_count + 1;
                result.errors.push({
                    id:  entry.property_id,
                    msg: 'sub-path ' + subIdx + ': could not create mask path'
                });
                continue;
            }
            if (protocol === 'visible') {
                // Visible-channel mask: set the user-declared mask mode.
                // _applyVisibleMaskMode NEVER sets MaskMode.NONE; if it
                // fails, treat the entry as skipped rather than as an
                // applied-but-inert mask.
                if (!_applyVisibleMaskMode(newProp, visibility)) {
                    result.skipped_count = result.skipped_count + 1;
                    result.errors.push({
                        id:  entry.property_id,
                        msg: 'visible sub-path ' + subIdx +
                            ': could not assign MaskMode for visibility=' +
                            visibility
                    });
                    continue;
                }
                visibleApplied = true;
            } else if (isPartial) {
                // Diagnostic-only partial-vertex mask subpaths must not
                // affect layer alpha. A degenerate partial-shape mask in
                // Add mode creates an unintended alpha region; set the
                // mask atom to None so it is inert.
                maskAtomForMode = null;
                try { maskAtomForMode = newProp.parentProperty; } catch (maErr) {}
                if (maskAtomForMode) {
                    try { maskAtomForMode.maskMode = MaskMode.NONE; } catch (mmErr) {}
                }
            }
        } else if (kind === 'shape_group') {
            // Partial-vertex sibling paths in a shape group receive the group's
            // fill/stroke operators and produce visible degenerate polygon artefacts.
            // Skip unless coverage is proven full-range; includes the case where
            // vertex_range is present but the source vertex count could not be read.
            // The visible-channel branch above already rejected partial entries,
            // so we only reach this for diagnostic-only shape-group siblings.
            if (!isShapeGroupSafe) {
                result.skipped_count = result.skipped_count + 1;
                result.errors.push({
                    id:  entry.property_id,
                    msg: 'sub-path ' + subIdx +
                        ': partial-vertex subpath skipped for shape-group source' +
                        ' (' + (vr === null ?
                            ('vertex count mismatch; missing vertex_range; entry vertices=' +
                            entryVertexCount + '; source vertices=' +
                            srcVertexCount) :
                            ('vertex_range=' + vr.first + '-' + vr.end +
                            (isRangeEntryMismatch ?
                                ('; entry vertices=' + entryVertexCount) : ''))) +
                        (srcVertexCount > 0 ? '' : '; source vertex count unreadable') + ')'
                });
                continue;
            }
            newProp = _createAdjacentShapePath(sourceProperty, subLabel);
            if (!newProp) {
                result.skipped_count = result.skipped_count + 1;
                result.errors.push({
                    id:  entry.property_id,
                    msg: 'sub-path ' + subIdx +
                        ': could not create Vector Shape path ' +
                        '(addProperty(ADBE Vector Shape - Group) failed)'
                });
                continue;
            }
            // A shape-group visible-channel entry has already proven full
            // coverage above; the new sibling inherits the group's fill/
            // stroke operators and therefore renders. Nothing further to
            // assign at the property level.
            if (protocol === 'visible') { visibleApplied = true; }
        } else {
            result.skipped_count = result.skipped_count + 1;
            result.errors.push({
                id:  entry.property_id,
                msg: 'sub-path ' + subIdx + ': unrecognised source property kind'
            });
            continue;
        }

        // Write this sub-path's keys to the newly created property.
        // Build a synthetic single-property key bundle; the resolver always returns
        // the new property, so the property_id value is irrelevant to resolution.
        capturedProp = newProp;
        subBundle = {
            _schema:          'keys',
            schema_version:   1,
            request_id:       'subpath-' + subIdx,
            property_results: [entry],
            total_keys:       entry.keys ? entry.keys.length : 0
        };
        subResult = applyKeyBundle(subBundle, {
            undoGroupName:       opts.undoGroupName || 'bbsolver-test-harness: diagnostic sub-path',
            disableExpression:   false,
            deleteExpression:    false,
            archiveExpression:   false,
            overwriteExisting:   true,
            perPropertyResolver: (function (p) {
                return function (propId) { return p; };
            }(capturedProp)),
            progressCallback: opts.progressCallback,
            holdAdjacentFrameKeys: opts.holdAdjacentFrameKeys === true,
            frameDuration: opts.frameDuration || 0,
            frameOrigin: opts.frameOrigin || 0,
            snapKeyTimesToFrameGrid: opts.snapKeyTimesToFrameGrid === true
        });

        // Collect structure results regardless of apply success -- a structure
        // failure (ease or interp type not surviving round-trip) must surface even
        // when keys were written.
        if (subResult.shape_key_structure && subResult.shape_key_structure.length > 0) {
            for (sksj = 0; sksj < subResult.shape_key_structure.length; sksj++) {
                sksEntry = subResult.shape_key_structure[sksj];
                try { sksEntry.id = entry.property_id + '/' + subLabel; } catch (sksIdErr) {}
                result.shape_key_structure.push(sksEntry);
            }
        }

        if (subResult.applied > 0) {
            result.applied          = result.applied + 1;
            result.sub_path_ids.push(subLabel);
            if (visibleApplied) {
                result.visible_applied      = result.visible_applied + 1;
                result.visible_sub_path_ids.push(subLabel);
            } else {
                result.diagnostic_applied   = result.diagnostic_applied + 1;
            }
        } else {
            result.skipped_count = result.skipped_count + 1;
            if (subResult.errors && subResult.errors.length > 0) {
                result.errors.push({
                    id:  entry.property_id,
                    msg: 'sub-path ' + subIdx + ': ' + subResult.errors[0].msg
                });
            }
        }
    }

    return result;
}

// applyKeyBundleMultiPath(keyBundle, opts)
//
// Replacement top-level entry point for applyKeyBundle.
// Single-path output (existing behavior, opts interface unchanged):
//   delegates to applyKeyBundle -- zero overhead change for callers.
// Multi-path output (diagnostic sub-paths present):
//   1. Applies source-property entries via applyKeyBundle.
//   2. Applies each sub-path group by creating new AE path properties adjacent
//      to the source (mask paths or shape-group paths).
//
// Returns the same object as applyKeyBundle, extended with:
//   multi_path_count:            total sub-path AE properties successfully created.
//   diagnostic_subpath_count:    subset created from 'landmark_subpath' marker.
//   visible_subpath_count:       subset created from 'visible_channel_subpath' marker.
//   visible_subpath_ids:         AE labels of visible siblings ('bb_vc_N').
//   multi_path_results: [{source_id, applied, visible_applied, diagnostic_applied,
//                         skipped, errors, sub_path_ids, visible_sub_path_ids,
//                         shape_key_structure}]
// Disjoint-count invariant:
//   diagnostic_subpath_count + visible_subpath_count === multi_path_count
// A path that ended up with MaskMode.NONE, or that was skipped for any reason,
// is never counted in visible_subpath_count.
function applyKeyBundleMultiPath(keyBundle, opts) {
    var grouped = groupKeyBundleBySource(keyBundle);

    // Fast path: single-path bundle -- identical to old applyKeyBundle behavior.
    if (!grouped.has_sub_paths) {
        var singleResult = applyKeyBundle(keyBundle, opts);
        singleResult.multi_path_count         = 0;
        singleResult.diagnostic_subpath_count = 0;
        singleResult.visible_subpath_count    = 0;
        singleResult.visible_subpath_ids      = [];
        singleResult.multi_path_results       = [];
        return singleResult;
    }

    // Build a bundle containing only source entries for the standard writeback.
    var srcResults = grouped.source_entries;
    var srcTotal = 0;
    var si;
    for (si = 0; si < srcResults.length; si++) {
        srcTotal = srcTotal + (srcResults[si].keys ? srcResults[si].keys.length : 0);
    }
    var sourceBundle = {
        _schema:          keyBundle._schema,
        schema_version:   keyBundle.schema_version,
        request_id:       keyBundle.request_id,
        property_results: srcResults,
        total_keys:       srcTotal
    };

    var mainResult = applyKeyBundle(sourceBundle, opts);
    mainResult.multi_path_count         = 0;
    mainResult.diagnostic_subpath_count = 0;
    mainResult.visible_subpath_count    = 0;
    mainResult.visible_subpath_ids      = [];
    mainResult.multi_path_results       = [];

    var resolver = opts ? opts.perPropertyResolver : null;
    if (typeof resolver !== 'function') { return mainResult; }

    // Route each sub-path group.
    var gid, subEntries, sourceProp, groupResult, gpErrors, gei, gsks, vid;
    for (gid in grouped.sub_path_groups) {
        if (!Object.prototype.hasOwnProperty.call(grouped.sub_path_groups, gid)) {
            continue;
        }
        subEntries = grouped.sub_path_groups[gid];
        sourceProp = null;
        try { sourceProp = resolver(gid); } catch (re) {}

        if (!sourceProp) {
            mainResult.errors.push({
                id:  gid,
                msg: 'sub-path group: source property not found'
            });
            mainResult.multi_path_results.push({
                source_id: gid, applied: 0, visible_applied: 0,
                diagnostic_applied: 0, skipped: subEntries.length,
                errors: ['source property not found'], sub_path_ids: [],
                visible_sub_path_ids: [], shape_key_structure: []
            });
            continue;
        }

        groupResult = _applySubPathGroup(sourceProp, subEntries, opts);
        mainResult.multi_path_count =
            mainResult.multi_path_count + groupResult.applied;
        mainResult.diagnostic_subpath_count =
            mainResult.diagnostic_subpath_count + groupResult.diagnostic_applied;
        mainResult.visible_subpath_count =
            mainResult.visible_subpath_count + groupResult.visible_applied;
        for (vid = 0; vid < groupResult.visible_sub_path_ids.length; vid++) {
            mainResult.visible_subpath_ids.push(
                groupResult.visible_sub_path_ids[vid]);
        }
        gpErrors = groupResult.errors || [];
        for (gei = 0; gei < gpErrors.length; gei++) {
            mainResult.errors.push(gpErrors[gei]);
        }
        // Propagate shape key structure results from sub-path writeback into the
        // same shape_key_structure array as source-path results, so the panel log
        // and results dialog cover sub-paths without any additional code paths.
        if (groupResult.shape_key_structure && groupResult.shape_key_structure.length > 0) {
            if (!mainResult.shape_key_structure) { mainResult.shape_key_structure = []; }
            for (gsks = 0; gsks < groupResult.shape_key_structure.length; gsks++) {
                mainResult.shape_key_structure.push(groupResult.shape_key_structure[gsks]);
            }
        }
        mainResult.multi_path_results.push({
            source_id:            gid,
            applied:              groupResult.applied,
            visible_applied:      groupResult.visible_applied,
            diagnostic_applied:   groupResult.diagnostic_applied,
            skipped:              groupResult.skipped_count,
            errors:               gpErrors,
            sub_path_ids:         groupResult.sub_path_ids,
            visible_sub_path_ids: groupResult.visible_sub_path_ids,
            shape_key_structure:  groupResult.shape_key_structure || []
        });
    }

    return mainResult;
}
