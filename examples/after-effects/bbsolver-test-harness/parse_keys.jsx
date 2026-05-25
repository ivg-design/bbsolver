// parse_keys.jsx -- readKeyBundleJson (full round-2 implementation)
// Requires _polyfill.jsx to be #included before this file.

// Read a KeyBundle JSON from 'filepath', validate its shape, and return the
// parsed JS object matching the protocol/keys.fbs JSON structure.
function readKeyBundleJson(filepath) {
    var f = new File(filepath);
    if (!f.exists) {
        throw new Error('readKeyBundleJson: file not found: ' + filepath);
    }
    f.encoding = 'UTF-8';
    if (!f.open('r')) {
        throw new Error('readKeyBundleJson: cannot open file for reading: ' + filepath);
    }
    var text = f.read();
    f.close();

    if (!text || !text.length) {
        throw new Error('readKeyBundleJson: file is empty: ' + filepath);
    }

    var obj;
    try {
        obj = JSON.parse(text);
    } catch (e) {
        throw new Error('readKeyBundleJson: JSON parse failed: ' + e.message);
    }

    // --- Schema validation ---

    if (typeof obj.schema_version === 'undefined') {
        throw new Error('readKeyBundleJson: missing schema_version');
    }
    if (obj.schema_version !== 1) {
        throw new Error('readKeyBundleJson: unsupported schema_version ' +
            obj.schema_version + ' (expected 1)');
    }
    if (obj._schema !== 'keys') {
        throw new Error('readKeyBundleJson: expected _schema "keys", got "' +
            String(obj._schema) + '"');
    }
    if (!obj.property_results || typeof obj.property_results.length === 'undefined') {
        throw new Error('readKeyBundleJson: missing or invalid property_results array');
    }

    // Validate each PropertyKeys entry.
    var i, pk;
    for (i = 0; i < obj.property_results.length; i++) {
        pk = obj.property_results[i];
        if (typeof pk.property_id !== 'string') {
            throw new Error('readKeyBundleJson: property_results[' + i +
                '] missing property_id string');
        }
        if (!pk.keys || typeof pk.keys.length === 'undefined') {
            throw new Error('readKeyBundleJson: property_results[' + i +
                '] missing keys array');
        }
        // Validate each Key entry.
        var j, k;
        for (j = 0; j < pk.keys.length; j++) {
            k = pk.keys[j];
            if (typeof k.t_sec !== 'number') {
                throw new Error('readKeyBundleJson: property_results[' + i +
                    '].keys[' + j + '] missing t_sec number');
            }
            if (!k.v || typeof k.v.length === 'undefined') {
                throw new Error('readKeyBundleJson: property_results[' + i +
                    '].keys[' + j + '] missing v array');
            }
        }
    }

    return obj;
}

// --- Sub-path helpers (for landmark-bounded multi-path solver output) ---

// Extract a named field from a semicolon-delimited notes string.
// Example: _parseNoteField("landmark_subpath; subpath_index=2; t_start=1.5", "t_start") -> "1.5"
// Returns null if the field is absent.
function _parseNoteField(notes, fieldName) {
    if (!notes) { return null; }
    var prefix = fieldName + '=';
    var idx = notes.indexOf(prefix);
    if (idx < 0) { return null; }
    var start = idx + prefix.length;
    var end = notes.indexOf(';', start);
    var val = (end >= 0) ? notes.substring(start, end) : notes.substring(start);
    // Trim whitespace.
    return val.replace(/^\s+|\s+$/g, '');
}

// Return true when a PropertyKeys entry is a solver-emitted sub-path/channel
// companion for a source property. Experimental markers are routed away from
// source_entries so unsupported future protocols cannot overwrite the source path.
function _isSubPathEntry(pk) {
    if (!pk || typeof pk.notes !== 'string') { return false; }
    var notes = pk.notes.replace(/^\s+|\s+$/g, '');
    return notes.indexOf('landmark_subpath;') === 0 ||
           notes.indexOf('visible_channel_subpath;') === 0 ||
           notes.indexOf('non_contiguous_landmark_subpath;') === 0 ||
           notes.indexOf('shape_channel_subpath;') === 0;
}

// groupKeyBundleBySource(keyBundle)
//
// Partition a parsed key bundle into:
//   source_entries   -- standard single-path PropertyKeys (no leading landmark_subpath marker).
//   sub_path_groups  -- sub-path PropertyKeys keyed by property_id, sorted by
//                      subpath_index extracted from notes.
//   has_sub_paths    -- true when any sub-path entries exist.
//
// This is a pure parsing operation with no AE host calls. Call it before
// applyKeyBundleMultiPath (apply.jsx) to decide which routing path to use.
// Single-path bundles (has_sub_paths===false) take the fast path through the
// existing applyKeyBundle without any overhead.
function groupKeyBundleBySource(keyBundle) {
    var result = { source_entries: [], sub_path_groups: {}, has_sub_paths: false };

    if (!keyBundle || !keyBundle.property_results) { return result; }

    var i, pk, propId;
    for (i = 0; i < keyBundle.property_results.length; i++) {
        pk = keyBundle.property_results[i];
        propId = pk.property_id;
        if (_isSubPathEntry(pk)) {
            result.has_sub_paths = true;
            if (!result.sub_path_groups[propId]) {
                result.sub_path_groups[propId] = [];
            }
            result.sub_path_groups[propId].push(pk);
        } else {
            result.source_entries.push(pk);
        }
    }

    // Sort each sub-path group by subpath_index ascending.
    var gid;
    for (gid in result.sub_path_groups) {
        if (!Object.prototype.hasOwnProperty.call(result.sub_path_groups, gid)) {
            continue;
        }
        result.sub_path_groups[gid].sort(function (a, b) {
            var ia = parseInt(_parseNoteField(a.notes, 'subpath_index') || '0', 10);
            var ib = parseInt(_parseNoteField(b.notes, 'subpath_index') || '0', 10);
            return ia - ib;
        });
    }

    return result;
}
